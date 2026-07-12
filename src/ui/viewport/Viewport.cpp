#include "Viewport.h"
#include "ViewportCommon.h"
#include "../../render/BodyRenderer.h"
#include "../../render/Camera3D.h"
#include "../../render/Grid3D.h"
#include "../../core/sketch/SketchRenderer.h"
#include "../../core/sketch/Sketch.h"
#include "../../core/sketch/SketchPoint.h"
#include "../../core/sketch/SketchConstraint.h"
#include "../../core/sketch/constraints/Constraints.h"
#include "../../core/sketch/tools/SketchToolManager.h"
#include "../../core/loop/RegionUtils.h"
#include "../../core/modeling/FacePatchResolver.h"
#include "../../app/commands/CommandProcessor.h"
#include "../../app/commands/SketchDragGestureCommand.h"
#include "../../app/document/Document.h"
#include "../../app/selection/SelectionManager.h"
#include "../../app/selection/SelectionTypes.h"
#include "../viewcube/ViewCube.h"
#include "../theme/ThemeManager.h"
#include "../sketch/DimensionEditor.h"
#include "../viewport/SnapSettingsPanel.h"
#include "../selection/DeepSelectPopup.h"
#include "../selection/SketchPickerAdapter.h"
#include "../selection/ModelPickerAdapter.h"
#include "../tools/ModelingToolManager.h"

#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QGestureEvent>
#include <QPinchGesture>
#include <QPanGesture>
#include <QNativeGestureEvent>
#include <QApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QLoggingCategory>
#include <QOpenGLContext>
#include <QSizePolicy>
#include <QVector2D>
#include <QEasingCurve>
#include <QtMath>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>

#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Edge.hxx>
#include <BRep_Tool.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <gp_Pnt.hxx>

namespace onecad {
namespace ui {

namespace sketch = core::sketch;
namespace sketchTools = core::sketch::tools;
using namespace viewport_common;
Q_LOGGING_CATEGORY(logUiInput, "onecad.ui.input")

namespace {
constexpr float kTrackpadPanScale = 1.0f;
constexpr float kTrackpadOrbitScale = 0.35f;
constexpr float kPinchZoomScale = 1000.0f;
constexpr float kWheelZoomShiftScale = 0.2f;
constexpr float kAngleDeltaToPixels = 1.0f / 8.0f;
constexpr qint64 kNativeZoomPostSuppressMs = 120;
constexpr std::array<sketch::SnapType, 8> kPointDragSnapTypes = {
    sketch::SnapType::Vertex,
    sketch::SnapType::Endpoint,
    sketch::SnapType::Midpoint,
    sketch::SnapType::Center,
    sketch::SnapType::Quadrant,
    sketch::SnapType::Intersection,
    sketch::SnapType::OnCurve,
    sketch::SnapType::Grid
};

std::vector<sketch::SketchRenderer::GuideLineInfo> buildFeasibleDragGuides(
    const sketch::Sketch* sketchModel,
    const sketch::EntityID& pointId) {
    if (!sketchModel || pointId.empty()) {
        return {};
    }

    const auto* point = sketchModel->getEntityAs<sketch::SketchPoint>(pointId);
    if (!point) {
        return {};
    }

    const auto directions = sketchModel->getPointFreeDirections(pointId);
    if (directions.size() != 1) {
        return {};
    }

    const sketch::Vec2d& direction = directions.front();
    const double length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length <= 1e-9) {
        return {};
    }

    const sketch::Vec2d origin{point->position().X(), point->position().Y()};
    const sketch::Vec2d target{origin.x + (direction.x / length),
                               origin.y + (direction.y / length)};
    return {{origin, target}};
}

} // namespace
Viewport::Viewport(QWidget* parent)
    : QOpenGLWidget(parent) {

    m_camera = std::make_unique<render::Camera3D>();
    m_grid = std::make_unique<render::Grid3D>();
    m_sketchPicker = std::make_unique<selection::SketchPickerAdapter>();
    m_modelPicker = std::make_unique<selection::ModelPickerAdapter>();
    // Note: SketchRenderer is created in initializeGL() when OpenGL context is ready

    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    
    // CRITICAL: Allow viewport to expand and fill available space
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // Prevent partial updates which can cause compositing artifacts on macOS
    setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);
    setAcceptDrops(true);
    
    // Gesture handling:
    // - macOS: rely on NativeGesture to avoid duplicate pinch streams
    // - other platforms: keep pinch/pan gesture recognizers
#if defined(Q_OS_MACOS)
    qCDebug(logUiInput) << "Using macOS native gesture pipeline";
#else
    grabGesture(Qt::PinchGesture);
    grabGesture(Qt::PanGesture);
#endif

    m_nativeGestureTimer.start();

    // Setup navigation quality timer (debounce end of navigation)
    m_navigationTimer = new QTimer(this);
    m_navigationTimer->setSingleShot(true);
    connect(m_navigationTimer, &QTimer::timeout, this, [this]() {
        m_isNavigating = false;
        update();  // Final high-quality redraw
    });

    // Setup ViewCube
    m_viewCube = new ViewCube(this);
    m_viewCube->setCamera(m_camera.get());

    // Connect ViewCube -> Viewport
    connect(m_viewCube, &ViewCube::viewChanged, this, [this]() {
        update();
        emit cameraChanged();
    });

    // Connect Viewport -> ViewCube
    connect(this, &Viewport::cameraChanged, m_viewCube, &ViewCube::updateRotation);

    // Setup DimensionEditor for inline constraint editing
    m_dimensionEditor = new DimensionEditor(this);
    m_dimensionEditor->hide();
    connect(m_dimensionEditor, &DimensionEditor::valueConfirmed,
            this, [this](const QString& constraintId, double newValue) {
        if (!m_activeSketch) return;

        auto* constraint = m_activeSketch->getConstraint(constraintId.toStdString());
        if (!constraint) return;

        // Check if it's a dimensional constraint
        auto* dimConstraint = dynamic_cast<core::sketch::DimensionalConstraint*>(constraint);
        if (dimConstraint) {
            dimConstraint->setValue(newValue);
            m_activeSketch->solve();
            if (m_sketchRenderer) {
                m_sketchRenderer->updateGeometry();
                m_sketchRenderer->setConflictingConstraints(m_activeSketch->getConflictingConstraints());
                m_sketchRenderer->updateConstraints();
            }
            update();
            emit sketchUpdated();
        }
    });

    // Theme integration
    m_themeConnection = connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
                                this, &Viewport::updateTheme, Qt::UniqueConnection);
    updateTheme();

    // Selection manager
    m_selectionManager = new app::selection::SelectionManager(this);
    m_selectionManager->setMode(app::selection::SelectionMode::Model);
    m_selectionManager->setDeepSelectEnabled(false);
    {
        app::selection::SelectionFilter filter;
        filter.allowedKinds = {
            app::selection::SelectionKind::Body,
            app::selection::SelectionKind::Vertex,
            app::selection::SelectionKind::Edge,
            app::selection::SelectionKind::Face
        };
        m_selectionManager->setFilter(filter);
    }
    connect(m_selectionManager, &app::selection::SelectionManager::selectionChanged,
            this, &Viewport::updateSketchSelectionFromManager);
    connect(m_selectionManager, &app::selection::SelectionManager::hoverChanged,
            this, &Viewport::updateSketchHoverFromManager);
    connect(m_selectionManager, &app::selection::SelectionManager::selectionChanged,
            this, &Viewport::handleModelSelectionChanged);

    m_modelingToolManager = std::make_unique<tools::ModelingToolManager>(this);

    // Deep select popup
    m_deepSelectPopup = new selection::DeepSelectPopup(this);
    m_deepSelectPopup->hide();
    connect(m_deepSelectPopup, &selection::DeepSelectPopup::candidateHovered,
            this, [this](int index) {
        if (index < 0 || index >= static_cast<int>(m_pendingCandidates.size())) {
            return;
        }
        m_selectionManager->setHoverItem(m_pendingCandidates[static_cast<size_t>(index)]);
        update();
    });
    connect(m_deepSelectPopup, &selection::DeepSelectPopup::candidateSelected,
            this, [this](int index) {
        if (index < 0 || index >= static_cast<int>(m_pendingCandidates.size())) {
            return;
        }
        const auto candidate = m_pendingCandidates[static_cast<size_t>(index)];
        m_selectionManager->applySelectionCandidate(
            candidate,
            m_pendingModifiers,
            m_pendingClickPos
        );
        if (m_pendingShellFaceToggle && m_modelingToolManager) {
            m_modelingToolManager->toggleShellOpenFace(candidate);
        }
        m_pendingShellFaceToggle = false;
        update();
    });
    connect(m_deepSelectPopup, &selection::DeepSelectPopup::popupClosed,
            this, [this]() {
        m_pendingCandidates.clear();
        m_pendingClickPos = QPoint();
        m_pendingShellFaceToggle = false;
        if (m_selectionManager) {
            m_selectionManager->setHoverItem(std::nullopt);
        }
        update();
    });

    // Initialize camera animation
    m_cameraAnimation = new QVariantAnimation(this);
    m_cameraAnimation->setDuration(500); // 500ms transition
    m_cameraAnimation->setEasingCurve(QEasingCurve::OutCubic);
}

Viewport::~Viewport() {
    if (context()) {
        makeCurrent();
        if (m_bodyRenderer) {
            m_bodyRenderer->cleanup();
            m_bodyRenderer.reset();
        }
        if (m_sketchRenderer) {
            m_sketchRenderer->cleanup();
            m_sketchRenderer.reset();
        }
        if (m_grid) {
            m_grid->cleanup();
        }
        doneCurrent();
    }
}

void Viewport::resetTransientState() {
    if (m_inSketchMode) {
        exitSketchMode();
    }

    if (m_cameraAnimation) {
        m_cameraAnimation->stop();
    }

    if (m_activeSketch) {
        m_activeSketch->endPointDrag();
        m_activeSketch->endGroupDrag();
    }
    endSketchDragGestureCapture(false);

    if (m_modelingToolManager) {
        m_modelingToolManager->cancelActiveTool();
    }
    setExtrudeToolActive(false);
    setRevolveToolActive(false);
    setLinearPatternToolActive(false);
    setFilletToolActive(false);
    setShellToolActive(false);

    if (m_toolManager) {
        m_toolManager->deactivateTool();
        m_toolManager->setSketch(nullptr);
    }

    if (m_deepSelectPopup && m_deepSelectPopup->isVisible()) {
        m_deepSelectPopup->hide();
    }
    m_pendingCandidates.clear();
    m_pendingModifiers = {};
    m_pendingClickPos = QPoint();
    m_pendingShellFaceToggle = false;

    if (m_selectionManager) {
        m_selectionManager->clearSelection();
        m_selectionManager->setHoverItem(std::nullopt);
    }

    m_planeSelectionActive = false;
    m_planeHoverIndex = -1;
    m_indicatorHovered = false;
    m_referenceSketchId.clear();
    m_referenceSketch = nullptr;
    m_activeSketch = nullptr;
    m_activeSketchId.clear();
    m_selectedRegionId.clear();
    m_pointDragCandidateId.clear();

    setMoveSketchMode(false);
    setSketchInteractionState(SketchInteractionState::Idle, "transient reset");
    setSketchDragIntent(SketchDragIntent::None, "transient reset");
    m_groupDragPointIds.clear();
    m_groupDragStartPositions.clear();
    m_groupDragFailureFeedbackShown = false;
    m_pointDragFailureFeedbackShown = false;
    m_boxSelectionActive = false;
    m_boxSelectionStart = QPoint();
    m_boxSelectionEnd = QPoint();

    if (m_sketchRenderer) {
        m_sketchRenderer->setSketch(nullptr);
        m_sketchRenderer->clearSelection();
        m_sketchRenderer->setHoverEntity({});
        m_sketchRenderer->setSelectedConstraint({});
        m_sketchRenderer->setHoverConstraint({});
        m_sketchRenderer->clearRegionSelection();
        m_sketchRenderer->clearRegionHover();
        m_sketchRenderer->clearPreview();
        m_sketchRenderer->clearPreviewDimensions();
        m_sketchRenderer->hideSnapIndicator();
        m_sketchRenderer->clearAnchorIndicators();
        m_sketchRenderer->clearGhostConstraints();
        m_sketchRenderer->clearDragGuides();
    }

    m_suppressedConstraintMarkers.clear();
    m_draftDimensionLabels.clear();
    m_activeDraftDimensionId.clear();
    if (m_dimensionEditor) {
        m_dimensionEditor->hide();
    }

    clearModelPreviewMeshes();
    clearPreviewHiddenBody();
    clearZoomAnchor();
    m_documentSketchesDirty = true;
    updateModelSelectionFilter();
    syncModelMeshes();
    emit selectionContextChanged(0);
    update();
}

void Viewport::mousePressEvent(QMouseEvent* event) {
    m_lastMousePos = event->pos();

    if (event->button() == Qt::LeftButton && m_deepSelectPopup &&
        m_deepSelectPopup->isVisible()) {
        m_deepSelectPopup->hide();
        m_pendingCandidates.clear();
        m_pendingClickPos = QPoint();
        m_pendingModifiers = {};
        m_pendingShellFaceToggle = false;
    }

    if (m_inSketchMode && m_moveSketchModeActive && event->button() == Qt::LeftButton &&
        m_activeSketch && (!m_toolManager || !m_toolManager->hasActiveTool())) {
        m_groupDragPointIds.clear();
        m_groupDragStartPositions.clear();
        m_groupDragFailureFeedbackShown = false;

        for (const auto& entity : m_activeSketch->getAllEntities()) {
            if (!entity || entity->type() != sketch::EntityType::Point) {
                continue;
            }
            const auto& pointId = entity->id();
            const auto* point = dynamic_cast<const sketch::SketchPoint*>(entity.get());
            if (!point) {
                continue;
            }
            m_groupDragPointIds.insert(pointId);
            m_groupDragStartPositions[pointId] =
                sketch::Vec2d{point->position().X(), point->position().Y()};
        }

        if (m_groupDragPointIds.empty()) {
            setSketchInteractionState(SketchInteractionState::Idle,
                                      "move-sketch mode press without points");
            setSketchDragIntent(SketchDragIntent::None,
                                "move-sketch mode press without points");
            setCursor(Qt::SizeAllCursor);
            update();
            return;
        }

        m_activeSketch->beginGroupDrag(m_groupDragPointIds);
        beginSketchDragGestureCapture();
        setSketchInteractionState(SketchInteractionState::SketchMoving,
                                  "move-sketch mode immediate drag");
        setSketchDragIntent(SketchDragIntent::PointGroup,
                            "move-sketch mode immediate drag");
        m_moveSketchLastSketchPos = screenToSketch(event->pos());
        setCursor(Qt::SizeAllCursor);
        update();
        return;
    }

    if (m_inSketchMode && event->button() == Qt::LeftButton && m_activeSketch) {
        m_groupDragPointIds.clear();
        m_groupDragStartPositions.clear();
    }

    if (m_inSketchMode && m_sketchRenderer && event->button() == Qt::LeftButton &&
        (!m_toolManager || !m_toolManager->hasActiveTool())) {
        if (!m_selectionManager) {
            return;
        }
        app::selection::ClickModifiers modifiers;
        modifiers.shift = event->modifiers() & Qt::ShiftModifier;
        modifiers.toggle = event->modifiers() & (Qt::MetaModifier | Qt::ControlModifier);

        auto pickResult = buildSketchPickResult(event->pos());

        std::optional<app::selection::SelectionItem> bestPointForDrag;
        for (const auto& hit : pickResult.hits) {
            if (hit.kind == app::selection::SelectionKind::SketchPoint) {
                if (!bestPointForDrag.has_value() ||
                    hit.screenDistance < bestPointForDrag->screenDistance) {
                    bestPointForDrag = hit;
                }
            }
        }

        // Point drag must win over deep-select ambiguity at line endpoints.
        if (!m_moveSketchModeActive && bestPointForDrag.has_value()) {
            m_selectionManager->applySelectionCandidate(*bestPointForDrag, modifiers, event->pos());
            beginSketchDragGestureCapture();

            const auto selectedPointIds = selectedSketchPointIds();
            const bool pressedPointSelected =
                selectedPointIds.find(bestPointForDrag->id.elementId) != selectedPointIds.end();
            if (pressedPointSelected && selectedPointIds.size() >= 2) {
                m_groupDragPointIds.reserve(selectedPointIds.size());
                m_groupDragStartPositions.reserve(selectedPointIds.size());
                for (const auto& pointId : selectedPointIds) {
                    const auto* point = m_activeSketch->getEntityAs<sketch::SketchPoint>(pointId);
                    if (!point) {
                        continue;
                    }
                    m_groupDragPointIds.insert(pointId);
                    m_groupDragStartPositions[pointId] =
                        sketch::Vec2d{point->position().X(), point->position().Y()};
                }
            }

            if (m_groupDragPointIds.size() >= 2) {
                setSketchInteractionState(SketchInteractionState::PendingPointGroupDrag,
                                          "press selected point candidate for group drag");
                setSketchDragIntent(SketchDragIntent::PointGroup,
                                    "press selected point candidate for group drag");
            } else {
                setSketchInteractionState(SketchInteractionState::PendingPointSingleDrag,
                                          "press point candidate");
                setSketchDragIntent(SketchDragIntent::PointSingle, "press point candidate");
            }
            m_sketchPressPos = event->pos();
            m_moveSketchLastSketchPos = screenToSketch(event->pos());
            m_pointDragCandidateId = bestPointForDrag->id.elementId;
            m_groupDragFailureFeedbackShown = false;
            m_pointDragFailureFeedbackShown = false;
            m_selectedRegionId.clear();
            update();
            return;
        }

        auto action = m_selectionManager->handleClick(pickResult, modifiers, event->pos());

        if (action.needsDeepSelect) {
            m_pendingCandidates = action.candidates;
            m_pendingModifiers = modifiers;
            m_pendingClickPos = event->pos();
            QStringList labels = buildDeepSelectLabels(m_pendingCandidates);
            m_deepSelectPopup->setCandidateLabels(labels);
            QPoint popupPos = mapToGlobal(event->pos() + QPoint(12, 12));
            m_deepSelectPopup->showAt(popupPos);
            if (!m_pendingCandidates.empty()) {
                m_selectionManager->setHoverItem(m_pendingCandidates.front());
            }
            setSketchInteractionState(SketchInteractionState::Idle, "deep select popup");
            setSketchDragIntent(SketchDragIntent::None, "deep select popup");
            m_pointDragCandidateId.clear();
            return;
        }

        // Candidate for point drag, region move, or sketch move
        if (!m_moveSketchModeActive) {
            auto top = m_selectionManager->topCandidate(pickResult);
            bool hitInSelectedRegion = false;
            bool hitSelectedRegionFill = false;
            if (!m_selectedRegionId.empty() && top.has_value()) {
                if (top->kind == app::selection::SelectionKind::SketchEdge) {
                    std::vector<sketch::EntityID> regionEntityIds =
                        core::loop::getEntityIdsInRegion(*m_activeSketch, m_selectedRegionId);
                    hitInSelectedRegion =
                        std::find(regionEntityIds.begin(), regionEntityIds.end(), top->id.elementId) !=
                        regionEntityIds.end();
                } else if (top->kind == app::selection::SelectionKind::SketchRegion &&
                           top->id.elementId == m_selectedRegionId) {
                    hitSelectedRegionFill = true;
                }
            }

            if (!m_selectedRegionId.empty() &&
                (!top.has_value() || hitInSelectedRegion || hitSelectedRegionFill)) {
                setSketchInteractionState(SketchInteractionState::Idle,
                                          "press selected region no-translate");
                setSketchDragIntent(SketchDragIntent::SelectionBox,
                                    "press selected region no-translate");
                m_pointDragCandidateId.clear();
            } else if (!top.has_value()) {
                setSketchInteractionState(SketchInteractionState::Idle,
                                          "press empty sketch no-translate");
                setSketchDragIntent(SketchDragIntent::SelectionBox,
                                    "press empty sketch no-translate");
                m_pointDragCandidateId.clear();
                m_selectedRegionId.clear();
            } else {
                setSketchInteractionState(SketchInteractionState::Idle, "press selectable non-drag target");
                setSketchDragIntent(SketchDragIntent::None, "press selectable non-drag target");
                m_pointDragCandidateId.clear();
                m_selectedRegionId.clear();
            }
        } else {
            setSketchInteractionState(SketchInteractionState::Idle, "move-sketch mode press");
            setSketchDragIntent(SketchDragIntent::None, "move-sketch mode press");
            m_pointDragCandidateId.clear();
        }

        update();
        return;
    }

    // Check for indicator drag FIRST, before generic model picking.
    // This ensures that clicking "near" the arrow (tolerance zone) works even if
    // hovering over empty space or unrelated faces.
    if (!m_inSketchMode && !m_planeSelectionActive &&
        m_modelingToolManager && m_modelingToolManager->hasActiveTool()) {
        
        if (isMouseOverIndicator(event->pos())) {
            // Force start dragging
            if (m_modelingToolManager->handleMousePress(event->pos(), event->button())) {
                event->accept();
                return;
            }
        }
    }

    if (!m_inSketchMode && event->button() == Qt::LeftButton && !m_planeSelectionActive) {
        if (m_selectionManager && m_modelPicker) {
            app::selection::ClickModifiers modifiers;
            modifiers.shift = event->modifiers() & Qt::ShiftModifier;
            modifiers.toggle = event->modifiers() & (Qt::MetaModifier | Qt::ControlModifier);

            auto pickResult = buildModelPickResult(event->pos());

            auto topCandidate = m_selectionManager->topCandidate(pickResult);

            bool allowTool = false;
            if (!modifiers.shift && !modifiers.toggle &&
                m_modelingToolManager && m_modelingToolManager->hasActiveTool()) {
                const auto& selection = m_selectionManager->selection();
                if (topCandidate.has_value() && selection.size() == 1) {
                    app::selection::SelectionKey topKey{topCandidate->kind, topCandidate->id};
                    app::selection::SelectionKey selKey{selection.front().kind, selection.front().id};
                    if (topKey == selKey) {
                        allowTool = true;
                    }
                }
                if (!allowTool && topCandidate.has_value()) {
                    auto shellBodyId = m_modelingToolManager->activeShellBodyId();
                    if (shellBodyId.has_value() &&
                        (topCandidate->kind == app::selection::SelectionKind::Face ||
                         topCandidate->kind == app::selection::SelectionKind::Body) &&
                        topCandidate->id.ownerId == shellBodyId.value()) {
                        allowTool = true;
                    }
                }
            }

            if (allowTool && m_modelingToolManager->handleMousePress(event->pos(), event->button())) {
                event->accept();
                return;
            }

            bool shellTogglePending = false;
            if (modifiers.shift && m_modelingToolManager && topCandidate.has_value()) {
                auto shellBodyId = m_modelingToolManager->activeShellBodyId();
                if (shellBodyId.has_value() &&
                    topCandidate->kind == app::selection::SelectionKind::Face) {
                    shellTogglePending = true;
                    modifiers.toggle = true;
                    modifiers.shift = false;
                }
            }

            auto action = m_selectionManager->handleClick(pickResult, modifiers, event->pos());

            if (action.needsDeepSelect) {
                m_pendingCandidates = action.candidates;
                m_pendingModifiers = modifiers;
                m_pendingClickPos = event->pos();
                m_pendingShellFaceToggle = shellTogglePending;
                QStringList labels = buildDeepSelectLabels(m_pendingCandidates);
                m_deepSelectPopup->setCandidateLabels(labels);
                QPoint popupPos = mapToGlobal(event->pos() + QPoint(12, 12));
                m_deepSelectPopup->showAt(popupPos);
                if (!m_pendingCandidates.empty()) {
                    m_selectionManager->setHoverItem(m_pendingCandidates.front());
                }
                return;
            }

            if (shellTogglePending && topCandidate.has_value() && m_modelingToolManager) {
                m_modelingToolManager->toggleShellOpenFace(*topCandidate);
            }
            m_pendingShellFaceToggle = false;
            update();
            return;
        }
    }

    // Forward to sketch tool if active and left-click (or right-click for cancel)
    if (m_inSketchMode && m_toolManager && m_toolManager->hasActiveTool()) {
        if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) {
            syncSnapGridSizeFromCamera();
            sketch::Vec2d sketchPos = screenToSketch(event->pos());
            m_toolManager->handleMousePress(sketchPos, event->button());
            // Still allow right-click to orbit if tool is in Idle state
            if (event->button() == Qt::RightButton && !m_toolManager->activeTool()->isActive()) {
                m_isOrbiting = true;
                setCursor(Qt::ClosedHandCursor);
            }
            return;
        }
    }

    if (!m_inSketchMode && m_planeSelectionActive && event->button() == Qt::LeftButton) {
        int hitIndex = -1;
        if (pickPlaneSelection(event->pos(), &hitIndex)) {
            m_planeSelectionActive = false;
            m_planeHoverIndex = -1;
            setCursor(Qt::ArrowCursor);
            update();
            emit sketchPlanePicked(hitIndex);
            return;
        }
    }

    if (event->button() == Qt::RightButton) {
        m_isOrbiting = true;
        setCursor(Qt::ClosedHandCursor);
    } else if (event->button() == Qt::MiddleButton) {
        m_isPanning = true;
        setCursor(Qt::SizeAllCursor);
    }

    QOpenGLWidget::mousePressEvent(event);
}

void Viewport::mouseDoubleClickEvent(QMouseEvent* event) {
    if (!m_inSketchMode) {
        if (!m_selectionManager || !m_modelPicker) {
            QOpenGLWidget::mouseDoubleClickEvent(event);
            return;
        }

        // Handle double-click: sketch part → open for edit; otherwise body selection
        if (event->button() == Qt::LeftButton) {
            auto pickResult = buildModelPickResult(event->pos());

            auto topCandidate = m_selectionManager->topCandidate(pickResult);
            if (topCandidate.has_value()) {
                const auto kind = topCandidate->kind;
                if (kind == app::selection::SelectionKind::SketchRegion) {
                    event->accept();
                    return;
                }
                if (kind == app::selection::SelectionKind::SketchPoint ||
                    kind == app::selection::SelectionKind::SketchEdge) {
                    const std::string sketchId = topCandidate->id.ownerId;
                    if (m_document && !sketchId.empty()) {
                        core::sketch::Sketch* sketch = m_document->getSketch(sketchId);
                        if (sketch) {
                            emit openSketchForEditRequested(QString::fromStdString(sketchId));
                            return;
                        }
                    }
                }

                // Construct a Body selection item from the hit
                app::selection::SelectionItem bodyItem;
                bodyItem.kind = app::selection::SelectionKind::Body;
                bodyItem.id = {topCandidate->id.ownerId, topCandidate->id.ownerId};
                bodyItem.priority = topCandidate->priority; // Keep priority or set custom
                bodyItem.depth = topCandidate->depth;
                bodyItem.worldPos = topCandidate->worldPos;
                
                // Replace current selection with this body
                app::selection::ClickModifiers modifiers;
                m_selectionManager->applySelectionCandidate(bodyItem, modifiers, event->pos());
                update();
                return;
            }
        }
    }

    if (!m_inSketchMode || !m_sketchRenderer || !m_activeSketch) {
        QOpenGLWidget::mouseDoubleClickEvent(event);
        return;
    }

    if (event->button() != Qt::LeftButton) {
        QOpenGLWidget::mouseDoubleClickEvent(event);
        return;
    }

    // Check if double-clicked on a constraint
    sketch::Vec2d sketchPos = screenToSketch(event->pos());
    sketch::ConstraintID constraintId = m_sketchRenderer->pickConstraint(sketchPos, 5.0);

    if (!constraintId.empty()) {
        auto* constraint = m_activeSketch->getConstraint(constraintId);
        if (constraint) {
            // Check if it's a dimensional constraint
            auto* dimConstraint = dynamic_cast<sketch::DimensionalConstraint*>(constraint);
            if (dimConstraint) {
                // Get the units based on constraint type
                QString units;
                switch (constraint->type()) {
                    case sketch::ConstraintType::Distance:
                    case sketch::ConstraintType::HorizontalDistance:
                    case sketch::ConstraintType::VerticalDistance:
                    case sketch::ConstraintType::Radius:
                    case sketch::ConstraintType::Diameter:
                        units = "mm";
                        break;
                    case sketch::ConstraintType::Angle:
                        units = QString::fromUtf8("°");
                        break;
                    default:
                        units = "";
                        break;
                }

                // Show the dimension editor at the click position
                if (constraint->type() == sketch::ConstraintType::HorizontalDistance ||
                    constraint->type() == sketch::ConstraintType::VerticalDistance) {
                    m_dimensionEditor->showForSignedConstraint(
                        QString::fromStdString(constraintId),
                        dimConstraint->value(),
                        units,
                        event->pos()
                    );
                } else {
                    m_dimensionEditor->showForConstraint(
                        QString::fromStdString(constraintId),
                        dimConstraint->value(),
                        units,
                        event->pos()
                    );
                }
                return;
            }
        }
    }

    // Double-click on region fill, edge, or point: select whole region (all connected edges and vertices)
    auto pickResult = buildSketchPickResult(event->pos());
    auto top = m_selectionManager->topCandidate(pickResult);
    std::string regionIdToSelect;
    if (top.has_value() && top->kind == app::selection::SelectionKind::SketchRegion) {
        regionIdToSelect = top->id.elementId;
    } else if (top.has_value() &&
               (top->kind == app::selection::SelectionKind::SketchEdge ||
                top->kind == app::selection::SelectionKind::SketchPoint)) {
        auto regionIdOpt = core::loop::getRegionIdContainingEntity(*m_activeSketch, top->id.elementId);
        if (regionIdOpt.has_value()) {
            regionIdToSelect = *regionIdOpt;
        }
    }
    if (!regionIdToSelect.empty()) {
        std::vector<sketch::EntityID> entityIds =
            core::loop::getEntityIdsInRegion(*m_activeSketch, regionIdToSelect);
        std::string sketchIdStr = resolveActiveSketchId();
        std::vector<app::selection::SelectionItem> regionItems;
        regionItems.reserve(entityIds.size());
        for (const auto& eid : entityIds) {
            const auto* entity = m_activeSketch->getEntity(eid);
            if (!entity) {
                continue;
            }
            app::selection::SelectionItem item;
            item.id = {sketchIdStr, eid};
            item.priority = (entity->type() == sketch::EntityType::Point) ? 0 : 1;
            item.screenDistance = 0.0;
            if (entity->type() == sketch::EntityType::Point) {
                item.kind = app::selection::SelectionKind::SketchPoint;
            } else {
                item.kind = app::selection::SelectionKind::SketchEdge;
            }
            regionItems.push_back(item);
        }
        if (!regionItems.empty()) {
            m_selectionManager->replaceSelection(regionItems);
            m_selectedRegionId = regionIdToSelect;
            if (m_sketchRenderer) {
                m_sketchRenderer->clearRegionSelection();
                m_sketchRenderer->toggleRegionSelection(regionIdToSelect);
            }
            updateSketchSelectionFromManager();
            update();
            return;
        }
    }

    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void Viewport::mouseMoveEvent(QMouseEvent* event) {
    QPoint delta = event->pos() - m_lastMousePos;
    m_lastMousePos = event->pos();

    if (!m_inSketchMode && m_modelingToolManager && m_modelingToolManager->isDragging() &&
        (event->buttons() & Qt::LeftButton)) {
        setCursor(Qt::ClosedHandCursor);
        if (m_modelingToolManager->handleMouseMove(event->pos())) {
            update();
            return;
        }
    }

    if (m_inSketchMode && m_sketchInteractionState == SketchInteractionState::SketchMoving &&
        m_moveSketchModeActive &&
        m_activeSketch && (event->buttons() & Qt::LeftButton)) {
        if (m_groupDragStartPositions.empty()) {
            update();
            return;
        }

        sketch::Vec2d sketchPos = screenToSketch(event->pos());
        const sketch::Vec2d dragDelta{
            sketchPos.x - m_moveSketchLastSketchPos.x,
            sketchPos.y - m_moveSketchLastSketchPos.y,
        };

        std::unordered_map<sketch::EntityID, sketch::Vec2d> targetPositions;
        targetPositions.reserve(m_groupDragStartPositions.size());
        for (const auto& [pointId, startPos] : m_groupDragStartPositions) {
            targetPositions[pointId] =
                sketch::Vec2d{startPos.x + dragDelta.x, startPos.y + dragDelta.y};
        }

        const auto result = m_activeSketch->solveWithGroupDrag(targetPositions);
        if (result.success) {
            if (m_sketchRenderer) {
                m_sketchRenderer->updateGeometry();
                updateSketchRenderingState();
            }
            notifySketchUpdated();
        } else if (!m_groupDragFailureFeedbackShown) {
            m_groupDragFailureFeedbackShown = true;
            const QString message = result.errorMessage.empty()
                ? tr("Constrained or unsolved group drag")
                : QString::fromStdString(result.errorMessage);
            emit statusMessageRequested(message);
        }
        update();
        return;
    }

    // Forward to sketch tool if active
    if (m_inSketchMode && m_toolManager && m_toolManager->hasActiveTool()) {
        syncSnapGridSizeFromCamera();
        sketch::Vec2d sketchPos = screenToSketch(event->pos());
        m_toolManager->handleMouseMove(sketchPos);
        if (m_selectionManager) {
            m_selectionManager->setHoverItem(std::nullopt);
        }
    } else if (m_inSketchMode && m_sketchRenderer && !m_isOrbiting && !m_isPanning &&
               (!m_deepSelectPopup || !m_deepSelectPopup->isVisible())) {
        // Point drag state machine
        if (m_sketchInteractionState == SketchInteractionState::PendingPointSingleDrag &&
            m_activeSketch && !m_pointDragCandidateId.empty()) {
            QPoint delta = event->pos() - m_sketchPressPos;
            int distSq = delta.x() * delta.x() + delta.y() * delta.y();
            if (distSq >= kPointDragThresholdPixels * kPointDragThresholdPixels) {
                if (m_activeSketch->hasFixedConstraint(m_pointDragCandidateId)) {
                    emit statusMessageRequested(tr("Point is fixed"));
                    endSketchDragGestureCapture(false);
                    if (m_sketchRenderer) {
                        m_sketchRenderer->clearDragGuides();
                    }
                    setSketchInteractionState(SketchInteractionState::Idle, "fixed point drag blocked");
                    setSketchDragIntent(SketchDragIntent::None, "fixed point drag blocked");
                    m_pointDragCandidateId.clear();
                } else {
                    m_activeSketch->beginPointDrag(m_pointDragCandidateId);
                    setSketchInteractionState(SketchInteractionState::PointSingleDragging,
                                              "point drag threshold reached");
                    m_sketchRenderer->setEntitySelection(m_pointDragCandidateId,
                        sketch::SelectionState::Dragging);
                    m_sketchRenderer->setDragGuides(
                        buildFeasibleDragGuides(m_activeSketch, m_pointDragCandidateId));
                }
            }
        }
        if (m_sketchInteractionState == SketchInteractionState::PendingPointGroupDrag &&
            m_activeSketch && !m_groupDragPointIds.empty()) {
            QPoint delta = event->pos() - m_sketchPressPos;
            int distSq = delta.x() * delta.x() + delta.y() * delta.y();
            if (distSq >= kPointDragThresholdPixels * kPointDragThresholdPixels) {
                m_activeSketch->beginGroupDrag(m_groupDragPointIds);
                setSketchInteractionState(SketchInteractionState::PointGroupDragging,
                                          "group drag threshold reached");
                if (m_sketchRenderer) {
                    m_sketchRenderer->setDragGuides(
                        buildFeasibleDragGuides(m_activeSketch, m_pointDragCandidateId));
                }
            }
        }
        // Box selection tracking
        if (m_sketchDragIntent == SketchDragIntent::SelectionBox &&
            (event->buttons() & Qt::LeftButton)) {
            QPoint delta = event->pos() - m_sketchPressPos;
            int distSq = delta.x() * delta.x() + delta.y() * delta.y();
            if (distSq >= kPointDragThresholdPixels * kPointDragThresholdPixels) {
                m_boxSelectionActive = true;
                m_boxSelectionStart = m_sketchPressPos;
                m_boxSelectionEnd = event->pos();
                update();
            }
        }

        if (m_sketchInteractionState == SketchInteractionState::PointSingleDragging &&
            m_activeSketch && m_sketchRenderer && !m_pointDragCandidateId.empty()) {
            m_sketchRenderer->setDragGuides(
                buildFeasibleDragGuides(m_activeSketch, m_pointDragCandidateId));
            sketch::Vec2d sketchPos = screenToSketch(event->pos());
            sketch::Vec2d targetPos = sketchPos;
            if (m_toolManager) {
                syncSnapGridSizeFromCamera();
                const auto& baseSnapManager = m_toolManager->snapManager();
                if (baseSnapManager.isEnabled()) {
                    sketch::SnapManager dragSnapManager;
                    dragSnapManager.setAllSnapsEnabled(false);
                    dragSnapManager.setEnabled(baseSnapManager.isEnabled());
                    dragSnapManager.setSnapRadius(baseSnapManager.getSnapRadius());
                    dragSnapManager.setGridSize(baseSnapManager.getGridSize());
                    dragSnapManager.setGridSnapEnabled(baseSnapManager.isGridSnapEnabled() &&
                                                       baseSnapManager.isSnapEnabled(sketch::SnapType::Grid));
                    dragSnapManager.setSpatialHashEnabled(baseSnapManager.isSpatialHashEnabled());

                    for (const auto type : kPointDragSnapTypes) {
                        dragSnapManager.setSnapEnabled(type, baseSnapManager.isSnapEnabled(type));
                    }

                    std::unordered_set<sketch::EntityID> excludeFromDragSnap{m_pointDragCandidateId};
                    const sketch::SnapResult dragSnap = dragSnapManager.findBestSnap(
                        sketchPos, *m_activeSketch, excludeFromDragSnap);
                    if (dragSnap.snapped) {
                        targetPos = dragSnap.position;
                    }
                }
            }
            auto result = m_activeSketch->solveWithDrag(m_pointDragCandidateId, targetPos);
            if (result.success) {
                m_sketchRenderer->updateGeometry();
                updateSketchRenderingState();
                update();
            } else {
                if (!m_pointDragFailureFeedbackShown) {
                    m_pointDragFailureFeedbackShown = true;
                    QString msg = result.errorMessage.empty()
                        ? tr("Constrained or unsolved drag")
                        : QString::fromStdString(result.errorMessage);
                    emit statusMessageRequested(msg);
                }
                update();
            }
            return;
        }
        if (m_sketchInteractionState == SketchInteractionState::PointGroupDragging &&
            m_activeSketch && m_sketchRenderer && !m_groupDragStartPositions.empty()) {
            m_sketchRenderer->setDragGuides(
                buildFeasibleDragGuides(m_activeSketch, m_pointDragCandidateId));
            sketch::Vec2d sketchPos = screenToSketch(event->pos());
            const sketch::Vec2d dragDelta{
                sketchPos.x - m_moveSketchLastSketchPos.x,
                sketchPos.y - m_moveSketchLastSketchPos.y,
            };

            std::unordered_map<sketch::EntityID, sketch::Vec2d> targetPositions;
            targetPositions.reserve(m_groupDragStartPositions.size());
            for (const auto& [pointId, startPos] : m_groupDragStartPositions) {
                targetPositions[pointId] = sketch::Vec2d{startPos.x + dragDelta.x, startPos.y + dragDelta.y};
            }

            if (!targetPositions.empty()) {
                const auto result = m_activeSketch->solveWithGroupDrag(targetPositions);
                if (result.success) {
                    m_sketchRenderer->updateGeometry();
                    updateSketchRenderingState();
                    update();
                } else {
                    if (!m_groupDragFailureFeedbackShown) {
                        m_groupDragFailureFeedbackShown = true;
                        const QString message = result.errorMessage.empty()
                            ? tr("Constrained or unsolved group drag")
                            : QString::fromStdString(result.errorMessage);
                        emit statusMessageRequested(message);
                    }
                    update();
                }
            }
            return;
        }
        if (m_sketchInteractionState != SketchInteractionState::PointSingleDragging &&
            m_sketchInteractionState != SketchInteractionState::PointGroupDragging) {
            if (m_moveSketchModeActive) {
                setCursor(Qt::SizeAllCursor);
            }
            auto pickResult = buildSketchPickResult(event->pos());
            m_selectionManager->updateHover(pickResult);
            update();
        }
    }

    if (!m_inSketchMode && !m_planeSelectionActive && !m_isOrbiting && !m_isPanning &&
        (!m_deepSelectPopup || !m_deepSelectPopup->isVisible())) {
        
        // Check if hovering over tool indicator
        bool overIndicator = isMouseOverIndicator(event->pos());
        if (m_indicatorHovered != overIndicator) {
            m_indicatorHovered = overIndicator;
            update();
        }

        if (overIndicator) {
            setCursor(Qt::PointingHandCursor);
            if (m_selectionManager) {
                m_selectionManager->setHoverItem(std::nullopt);
            }
            update(); // Ensure redraw for color change
        } else {
            if (m_modelingToolManager && !m_modelingToolManager->isDragging()) {
                setCursor(Qt::ArrowCursor);
            }
            if (m_selectionManager && m_modelPicker) {
                auto pickResult = buildModelPickResult(event->pos());
                m_selectionManager->updateHover(pickResult);
            }
        }
    }

    if (m_planeSelectionActive && !m_inSketchMode && !m_isOrbiting && !m_isPanning) {
        updatePlaneSelectionHover(event->pos());
    }

    if (m_isOrbiting) {
        handleOrbit(delta.x(), delta.y());
    } else if (m_isPanning) {
        handlePan(delta.x(), delta.y());
    }

    // Emit sketch coordinates if in sketch mode, otherwise screen coords
    if (m_inSketchMode && m_activeSketch) {
        sketch::Vec2d sketchPos = screenToSketch(event->pos());
        emit mousePositionChanged(sketchPos.x, sketchPos.y, 0.0);
    } else {
        emit mousePositionChanged(event->pos().x(), event->pos().y(), 0.0);
    }

    QOpenGLWidget::mouseMoveEvent(event);
}

void Viewport::mouseReleaseEvent(QMouseEvent* event) {
    // End Move Sketch gesture
    if (m_inSketchMode && event->button() == Qt::LeftButton &&
        m_sketchInteractionState == SketchInteractionState::SketchMoving) {
        if (m_activeSketch) {
            m_activeSketch->endGroupDrag();
            m_activeSketch->solve();
        }
        if (m_sketchRenderer) {
            m_sketchRenderer->updateGeometry();
            m_sketchRenderer->updateConstraints();
            updateSketchRenderingState();
            updateSketchSelectionFromManager();
        }
        notifySketchUpdated();
        endSketchDragGestureCapture(true);
        setSketchInteractionState(SketchInteractionState::Idle, "release sketch move");
        setSketchDragIntent(SketchDragIntent::None, "release sketch move");
        m_moveSketchLastSketchPos = sketch::Vec2d{0.0, 0.0};
        m_groupDragPointIds.clear();
        m_groupDragStartPositions.clear();
        m_groupDragFailureFeedbackShown = false;
        setCursor(Qt::ArrowCursor);
        update();
        return;
    }

    // Finalize box selection
    if (m_inSketchMode && event->button() == Qt::LeftButton && m_boxSelectionActive) {
        m_boxSelectionActive = false;

        if (m_activeSketch && m_selectionManager) {
            QRect rect = QRect(m_boxSelectionStart, m_boxSelectionEnd).normalized();
            sketch::Vec2d topLeft = screenToSketch(rect.topLeft());
            sketch::Vec2d bottomRight = screenToSketch(rect.bottomRight());
            sketch::Vec2d minPt{std::min(topLeft.x, bottomRight.x),
                                std::min(topLeft.y, bottomRight.y)};
            sketch::Vec2d maxPt{std::max(topLeft.x, bottomRight.x),
                                std::max(topLeft.y, bottomRight.y)};

            auto entityIds = m_activeSketch->findInRect(minPt, maxPt);

            std::vector<app::selection::SelectionItem> items;
            for (const auto& eid : entityIds) {
                auto* entity = m_activeSketch->getEntity(eid);
                if (!entity) continue;
                app::selection::SelectionItem item;
                if (entity->type() == sketch::EntityType::Point) {
                    item.kind = app::selection::SelectionKind::SketchPoint;
                } else {
                    item.kind = app::selection::SelectionKind::SketchEdge;
                }
                item.id.elementId = eid;
                items.push_back(item);
            }
            m_selectionManager->replaceSelection(items);
            updateSketchSelectionFromManager();
        }

        setSketchDragIntent(SketchDragIntent::None, "release box selection");
        update();
        return;
    }

    if (m_inSketchMode && event->button() == Qt::LeftButton &&
        m_sketchInteractionState == SketchInteractionState::PointGroupDragging) {
        if (m_activeSketch) {
            m_activeSketch->endGroupDrag();
            m_activeSketch->solve();
        }
        if (m_sketchRenderer) {
            m_sketchRenderer->updateGeometry();
            m_sketchRenderer->updateConstraints();
            updateSketchRenderingState();
            updateSketchSelectionFromManager();
        }
        notifySketchUpdated();
        endSketchDragGestureCapture(true);
        if (m_sketchRenderer) {
            m_sketchRenderer->clearDragGuides();
        }
        setSketchInteractionState(SketchInteractionState::Idle, "release point group drag");
        setSketchDragIntent(SketchDragIntent::None, "release point group drag");
        m_groupDragPointIds.clear();
        m_groupDragStartPositions.clear();
        m_groupDragFailureFeedbackShown = false;
        update();
        return;
    }

    // Finalize point drag (sketch mode, no tool)
    if (m_inSketchMode && event->button() == Qt::LeftButton &&
        m_sketchInteractionState == SketchInteractionState::PointSingleDragging) {
        if (m_activeSketch) {
            m_activeSketch->endPointDrag();
        }
        if (m_activeSketch && m_sketchRenderer && !m_pointDragCandidateId.empty()) {
            m_activeSketch->solve();
            m_sketchRenderer->updateGeometry();
            m_sketchRenderer->updateConstraints();
            updateSketchRenderingState();
            updateSketchSelectionFromManager();
            notifySketchUpdated();
        }
        endSketchDragGestureCapture(true);
        if (m_sketchRenderer) {
            m_sketchRenderer->clearDragGuides();
        }
        setSketchInteractionState(SketchInteractionState::Idle, "release point drag");
        setSketchDragIntent(SketchDragIntent::None, "release point drag");
        m_pointDragCandidateId.clear();
        m_pointDragFailureFeedbackShown = false;
        update();
        return;
    }
    if (m_inSketchMode && event->button() == Qt::LeftButton &&
        m_sketchInteractionState == SketchInteractionState::PendingPointSingleDrag) {
        endSketchDragGestureCapture(false);
        if (m_sketchRenderer) {
            m_sketchRenderer->clearDragGuides();
        }
        setSketchInteractionState(SketchInteractionState::Idle, "release pending drag intent");
        setSketchDragIntent(SketchDragIntent::None, "release pending drag intent");
        m_pointDragCandidateId.clear();
    }
    if (m_inSketchMode && event->button() == Qt::LeftButton &&
        m_sketchInteractionState == SketchInteractionState::PendingPointGroupDrag) {
        endSketchDragGestureCapture(false);
        if (m_sketchRenderer) {
            m_sketchRenderer->clearDragGuides();
        }
        setSketchInteractionState(SketchInteractionState::Idle,
                                  "release pending group drag intent");
        setSketchDragIntent(SketchDragIntent::None,
                            "release pending group drag intent");
        m_groupDragPointIds.clear();
        m_groupDragStartPositions.clear();
        m_groupDragFailureFeedbackShown = false;
    }

    // Forward to sketch tool if active
    if (m_inSketchMode && m_toolManager && m_toolManager->hasActiveTool()) {
        syncSnapGridSizeFromCamera();
        sketch::Vec2d sketchPos = screenToSketch(event->pos());
        m_toolManager->handleMouseRelease(sketchPos, event->button());
    }

    if (!m_inSketchMode && m_modelingToolManager && m_modelingToolManager->isDragging()) {
        if (m_modelingToolManager->handleMouseRelease(event->pos(), event->button())) {
            if (!m_modelingToolManager->hasActiveTool()) {
                m_modelingToolManager->cancelActiveTool();
                setExtrudeToolActive(false);
                setRevolveToolActive(false);
                setLinearPatternToolActive(false);
                setFilletToolActive(false);
                setShellToolActive(false);
            }
            update();
            return;
        }
    }

    if (event->button() == Qt::RightButton) {
        m_isOrbiting = false;
    } else if (event->button() == Qt::MiddleButton) {
        m_isPanning = false;
    }

    setCursor(Qt::ArrowCursor);
    if (m_planeSelectionActive && !m_inSketchMode) {
        updatePlaneSelectionHover(event->pos());
    }
    QOpenGLWidget::mouseReleaseEvent(event);
}

void Viewport::wheelEvent(QWheelEvent* event) {
    const QPoint pixelDelta = event->pixelDelta();
    const QPoint angleDelta = event->angleDelta();
    const bool hasPixelDelta = !pixelDelta.isNull();
    const bool hasAngleDelta = !angleDelta.isNull();
    const bool isTrackpad = isTrackpadScrollEvent(event);
    const bool pinchActive = m_pinchActive || isNativeZoomActive();
    const bool zoomModifier = (event->modifiers() & Qt::ControlModifier) != 0;

    if (isTrackpad && pinchActive) {
        qCDebug(logUiInput) << "wheel_suppressed_during_zoom"
                            << "phase=" << static_cast<int>(event->phase())
                            << "pixelDelta=" << pixelDelta
                            << "angleDelta=" << angleDelta;
        event->accept();
        return;
    }

    if (isTrackpad && zoomModifier && (hasPixelDelta || hasAngleDelta)) {
        QPointF delta = hasPixelDelta
            ? QPointF(pixelDelta)
            : QPointF(angleDelta) * kAngleDeltaToPixels;
        handleZoom(static_cast<float>(delta.y()));
        event->accept();
        return;
    }

    if (isTrackpad && (hasPixelDelta || hasAngleDelta)) {
        QPointF delta = hasPixelDelta
            ? QPointF(pixelDelta)
            : QPointF(angleDelta) * kAngleDeltaToPixels;

        if (event->modifiers() & Qt::ShiftModifier) {
            handleOrbit(delta.x() * kTrackpadOrbitScale, 
                        delta.y() * kTrackpadOrbitScale);
        } else {
            handlePan(delta.x() * kTrackpadPanScale, 
                      delta.y() * kTrackpadPanScale);
        }

        event->accept();
        return;
    }

    if (!hasAngleDelta) {
        event->ignore();
        return;
    }

    float delta = static_cast<float>(angleDelta.y());

    // Shift for slower zoom
    if (event->modifiers() & Qt::ShiftModifier) {
        delta *= kWheelZoomShiftScale;
    }

    handleZoom(delta);
    event->accept();
}

void Viewport::leaveEvent(QEvent* event) {
    // Clear hover state when mouse leaves viewport
    if (m_selectionManager) {
        m_selectionManager->setHoverItem(std::nullopt);
        update();
    }
    QOpenGLWidget::leaveEvent(event);
}

void Viewport::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        for (const QUrl& url : event->mimeData()->urls()) {
            QString path = url.toLocalFile().toLower();
            if (path.endsWith(".step") || path.endsWith(".stp")) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void Viewport::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasUrls()) {
        for (const QUrl& url : event->mimeData()->urls()) {
            QString path = url.toLocalFile().toLower();
            if (path.endsWith(".step") || path.endsWith(".stp")) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void Viewport::dropEvent(QDropEvent* event) {
    if (!event->mimeData()->hasUrls()) {
        return;
    }
    for (const QUrl& url : event->mimeData()->urls()) {
        QString path = url.toLocalFile();
        QString lower = path.toLower();
        if (lower.endsWith(".step") || lower.endsWith(".stp")) {
            emit fileDropped(path);
        }
    }
    event->acceptProposedAction();
}

bool Viewport::event(QEvent* event) {
    if (event->type() == QEvent::NativeGesture) {
        auto* gestureEvent = static_cast<QNativeGestureEvent*>(event);
        const Qt::NativeGestureType gestureType = gestureEvent->gestureType();

#if defined(Q_OS_MACOS)
        if (gestureType == Qt::BeginNativeGesture) {
            m_nativeGestureSequenceActive = true;
            m_nativeZoomSuppressUntilMs = -1;
            qCDebug(logUiInput) << "native_gesture_begin"
                                << "fingers=" << gestureEvent->fingerCount();
            return true;
        }

        if (gestureType == Qt::EndNativeGesture) {
            m_nativeGestureSequenceActive = false;
            if (m_nativeZoomActive && m_nativeGestureTimer.isValid()) {
                m_nativeZoomActive = false;
                m_nativeZoomSuppressUntilMs =
                    m_nativeGestureTimer.elapsed() + kNativeZoomPostSuppressMs;
                clearZoomAnchor();
                qCDebug(logUiInput) << "native_zoom_end"
                                    << "suppressUntilMs=" << m_nativeZoomSuppressUntilMs;
            }
            return true;
        }
#endif

        if (gestureType == Qt::ZoomNativeGesture && !m_pinchActive) {
            if (!m_nativeGestureTimer.isValid()) {
                m_nativeGestureTimer.start();
            }
            if (!m_nativeGestureSequenceActive) {
                // Some drivers skip explicit BeginNativeGesture.
                m_nativeGestureSequenceActive = true;
            }

            if (!m_nativeZoomActive) {
                m_nativeZoomActive = true;
                m_nativeZoomSuppressUntilMs = -1;
                m_lastNativeZoomEventMs = m_nativeGestureTimer.elapsed();
                initializeZoomAnchor(gestureEvent->position());
                qCDebug(logUiInput) << "native_zoom_start"
                                    << "position=" << gestureEvent->position()
                                    << "rawValue=" << gestureEvent->value();
            } else {
                m_lastNativeZoomEventMs = m_nativeGestureTimer.elapsed();
            }

            float factor = 1.0f;
            if (!normalizeNativeZoomFactor(gestureEvent->value(), &factor)) {
                return true;
            }

            applyAnchoredZoomFactor(factor, gestureEvent->position());
            qCDebug(logUiInput) << "native_zoom_update"
                                << "factor=" << factor
                                << "position=" << gestureEvent->position();
            return true;
        }
    }

    if (event->type() == QEvent::Gesture) {
#if defined(Q_OS_MACOS)
        return QOpenGLWidget::event(event);
#else
        QGestureEvent* gestureEvent = static_cast<QGestureEvent*>(event);
        
        // Handle pinch gesture (zoom)
        if (QPinchGesture* pinch = static_cast<QPinchGesture*>(
                gestureEvent->gesture(Qt::PinchGesture))) {
            
            if (pinch->state() == Qt::GestureStarted) {
                m_lastPinchScale = 1.0;
                m_pinchActive = true;
            }
            
            qreal scaleFactor = pinch->scaleFactor();
            qreal delta = (scaleFactor - m_lastPinchScale) * kPinchZoomScale;
            m_lastPinchScale = scaleFactor;
            
            handleZoom(static_cast<float>(delta));

            if (pinch->state() == Qt::GestureFinished ||
                pinch->state() == Qt::GestureCanceled) {
                m_pinchActive = false;
            }
            
            return true;
        }
        
        // Handle pan gesture (two-finger drag)
        if (QPanGesture* pan = static_cast<QPanGesture*>(
                gestureEvent->gesture(Qt::PanGesture))) {
            if (m_pinchActive || isNativeZoomActive()) {
                return true;
            }
            
            QPointF delta = pan->delta();
            
            // Check if Shift is held for orbit mode
            bool shiftHeld = QApplication::keyboardModifiers() & Qt::ShiftModifier;
            
            if (shiftHeld) {
                // Shift + two-finger = orbit
                handleOrbit(static_cast<float>(delta.x()) * kTrackpadOrbitScale,
                           static_cast<float>(delta.y()) * kTrackpadOrbitScale);
            } else {
                // Two-finger = pan
                handlePan(static_cast<float>(delta.x()) * kTrackpadPanScale,
                         static_cast<float>(delta.y()) * kTrackpadPanScale);
            }
            
            return true;
        }
#endif
    }
    
    return QOpenGLWidget::event(event);
}

bool Viewport::activateExtrudeTool() {
    if (m_inSketchMode || !m_selectionManager || !m_modelingToolManager) {
        setExtrudeToolActive(false);
        setLinearPatternToolActive(false);
        setFilletToolActive(false);
        setShellToolActive(false);
        return false;
    }

    if (m_extrudeToolActive) {
        setRevolveToolActive(false);
        setLinearPatternToolActive(false);
        setFilletToolActive(false);
        setShellToolActive(false);
        return true;
    }

    const auto& selection = m_selectionManager->selection();
    if (selection.size() == 1 &&
        selection.front().kind == app::selection::SelectionKind::SketchRegion) {
        m_modelingToolManager->activateExtrude(selection.front());
        setRevolveToolActive(false);
        setLinearPatternToolActive(false);
        setFilletToolActive(false);
        setShellToolActive(false);
        const bool activated = m_modelingToolManager->hasActiveTool();
        setExtrudeToolActive(activated);
        return activated;
    }

    setExtrudeToolActive(false);
    setLinearPatternToolActive(false);
    setFilletToolActive(false);
    setShellToolActive(false);
    return false;
}
void Viewport::keyPressEvent(QKeyEvent* event) {
    if (m_planeSelectionActive && !m_inSketchMode && event->key() == Qt::Key_Escape) {
        cancelPlaneSelection();
        event->accept();
        return;
    }

    // Debug visualization toggles (F1-F5)
    switch (event->key()) {
    case Qt::Key_F1:
        setDebugToggles(!m_debugNormals, false, m_wireframeOnly, m_disableGamma, m_useMatcap);
        qDebug() << "Debug normals:" << (m_debugNormals ? "ON" : "OFF");
        event->accept();
        return;
    case Qt::Key_F2:
        setDebugToggles(false, !m_debugDepth, m_wireframeOnly, m_disableGamma, m_useMatcap);
        qDebug() << "Debug depth:" << (m_debugDepth ? "ON" : "OFF");
        event->accept();
        return;
    case Qt::Key_F3:
        setDebugToggles(m_debugNormals, m_debugDepth, !m_wireframeOnly, m_disableGamma, m_useMatcap);
        qDebug() << "Wireframe only:" << (m_wireframeOnly ? "ON" : "OFF");
        event->accept();
        return;
    case Qt::Key_F4:
        setDebugToggles(m_debugNormals, m_debugDepth, m_wireframeOnly, !m_disableGamma, m_useMatcap);
        qDebug() << "Gamma correction:" << (m_disableGamma ? "DISABLED" : "ENABLED");
        event->accept();
        return;
    case Qt::Key_F5:
        setDebugToggles(m_debugNormals, m_debugDepth, m_wireframeOnly, m_disableGamma, !m_useMatcap);
        qDebug() << "MatCap shading:" << (m_useMatcap ? "ON" : "OFF");
        event->accept();
        return;
    default:
        break;
    }

    // Esc exits Move Sketch mode (and cancels in-progress move)
    if (m_inSketchMode && m_moveSketchModeActive && event->key() == Qt::Key_Escape) {
        setMoveSketchMode(false);
        event->accept();
        return;
    }

    // G toggles Move Sketch mode when in sketch mode
    if (m_inSketchMode && event->key() == Qt::Key_G) {
        setMoveSketchMode(!m_moveSketchModeActive);
        event->accept();
        return;
    }

    // Forward to sketch tool if active
    if (m_inSketchMode && m_toolManager && m_toolManager->hasActiveTool()) {
        m_toolManager->handleKeyPress(static_cast<Qt::Key>(event->key()));
        event->accept();
        return;
    }

    if (!m_inSketchMode && m_modelingToolManager && m_modelingToolManager->hasActiveTool()) {
        if (event->key() == Qt::Key_Tab) {
            if (m_modelingToolManager->toggleFilletMode()) {
                update();
                event->accept();
                return;
            }
        }
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            if (m_modelingToolManager->confirmShellFaceSelection()) {
                event->accept();
                return;
            }
            if (m_modelingToolManager->confirmLinearPattern()) {
                setLinearPatternToolActive(false);
                update();
                event->accept();
                return;
            }
        }
    }

    if (!m_inSketchMode && m_modelingToolManager &&
        m_modelingToolManager->hasActiveTool() &&
        event->key() == Qt::Key_Escape) {
        m_modelingToolManager->cancelActiveTool();
        setExtrudeToolActive(false);
        setRevolveToolActive(false);
        setLinearPatternToolActive(false);
        setFilletToolActive(false);
        setShellToolActive(false);
        event->accept();
        return;
    }

    QOpenGLWidget::keyPressEvent(event);
}

// Tool management
sketchTools::SketchToolManager* Viewport::toolManager() const {
    return m_toolManager.get();
}

sketch::SketchRenderer* Viewport::sketchRenderer() const {
    return m_sketchRenderer.get();
}

QMatrix4x4 Viewport::buildViewProjection() const {
    if (!m_camera) {
        return QMatrix4x4();
    }
    float aspectRatio = static_cast<float>(m_width) / static_cast<float>(m_height);
    QMatrix4x4 projection = buildProjectionMatrix(aspectRatio);
    QMatrix4x4 view = m_camera->viewMatrix();
    return projection * view;
}

Viewport::RenderClipPlanes Viewport::computeRenderClipPlanes(float aspectRatio) const {
    RenderClipPlanes clipPlanes;
    if (!m_camera) {
        return clipPlanes;
    }

    clipPlanes.nearPlane = m_camera->nearPlane();
    clipPlanes.farPlane = m_camera->farPlane();

    if (m_camera->projectionType() != render::Camera3D::ProjectionType::Perspective ||
        m_width <= 0 || m_height <= 0) {
        return clipPlanes;
    }

    const QVector3D cameraPosition = m_camera->position();
    float nearestPlaneDistance = std::numeric_limits<float>::max();

    // Sample sketch plane distances
    std::vector<sketch::SketchPlane> planes;
    if (m_inSketchMode && m_activeSketch) {
        planes.push_back(m_activeSketch->getPlane());
    } else if (m_document) {
        const auto visibleSketches = visibleModelSketchIds();
        planes.reserve(visibleSketches.size());
        for (const auto& sketchId : visibleSketches) {
            const auto* sketchModel = m_document->getSketch(sketchId);
            if (!sketchModel) {
                continue;
            }
            planes.push_back(sketchModel->getPlane());
        }
    }

    if (!planes.empty()) {
        const QMatrix4x4 baseProjection = m_camera->projectionMatrix(aspectRatio);
        const QMatrix4x4 baseViewProjection = baseProjection * m_camera->viewMatrix();
        for (const auto& plane : planes) {
            const PlaneDepthRangeInfo depthRange = samplePerspectivePlaneDepthRange(
                baseViewProjection,
                cameraPosition,
                QVector3D(plane.origin.x, plane.origin.y, plane.origin.z),
                QVector3D(plane.normal.x, plane.normal.y, plane.normal.z)
            );
            if (!depthRange.hasIntersection) {
                continue;
            }
            nearestPlaneDistance = std::min(nearestPlaneDistance, depthRange.minDistance);
        }
    }

    // Adaptive near/far from visible body bounding boxes
    if (m_document) {
        for (const auto& bodyId : m_document->getBodyIds()) {
            if (!m_document->isBodyVisible(bodyId)) continue;
            const auto* shape = m_document->getBodyShape(bodyId);
            if (!shape || shape->IsNull()) continue;
            Bnd_Box box;
            BRepBndLib::Add(*shape, box);
            if (box.IsVoid()) continue;
            double xmin, ymin, zmin, xmax, ymax, zmax;
            box.Get(xmin, ymin, zmin, xmax, ymax, zmax);
            QVector3D bCenter(static_cast<float>((xmin + xmax) * 0.5),
                              static_cast<float>((ymin + ymax) * 0.5),
                              static_cast<float>((zmin + zmax) * 0.5));
            float bRadius = QVector3D(static_cast<float>(xmax - xmin),
                                      static_cast<float>(ymax - ymin),
                                      static_cast<float>(zmax - zmin)).length() * 0.5f;
            float distToCenter = (cameraPosition - bCenter).length();
            float bodyNear = std::max(1.0f, distToCenter - bRadius);
            float bodyFar = distToCenter + bRadius + 100.0f;
            nearestPlaneDistance = std::min(nearestPlaneDistance, bodyNear);
            clipPlanes.farPlane = std::max(clipPlanes.farPlane, bodyFar);
        }
    }

    if (nearestPlaneDistance >= std::numeric_limits<float>::max() * 0.5f) {
        return clipPlanes;
    }

    constexpr float kAdaptiveNearFactor = 0.25f;
    constexpr float kAdaptiveNearMin = 1.0f;
    const float adaptiveNear = std::max(kAdaptiveNearMin, nearestPlaneDistance * kAdaptiveNearFactor);
    clipPlanes.nearPlane = std::min(clipPlanes.nearPlane, adaptiveNear);
    clipPlanes.farPlane = std::max(clipPlanes.farPlane, clipPlanes.nearPlane + 1.0f);
    return clipPlanes;
}

QMatrix4x4 Viewport::buildProjectionMatrix(float aspectRatio,
                                           RenderClipPlanes* outClipPlanes) const {
    if (!m_camera) {
        if (outClipPlanes) {
            *outClipPlanes = {};
        }
        return QMatrix4x4();
    }

    const RenderClipPlanes clipPlanes = computeRenderClipPlanes(aspectRatio);
    if (outClipPlanes) {
        *outClipPlanes = clipPlanes;
    }

    QMatrix4x4 projection;
    if (m_camera->projectionType() == render::Camera3D::ProjectionType::Orthographic) {
        const float halfHeight = m_camera->orthoScale() * 0.5f;
        const float halfWidth = halfHeight * aspectRatio;
        projection.ortho(-halfWidth, halfWidth,
                         -halfHeight, halfHeight,
                         clipPlanes.nearPlane, clipPlanes.farPlane);
    } else {
        projection.perspective(m_camera->fov(), aspectRatio,
                               clipPlanes.nearPlane, clipPlanes.farPlane);
    }
    return projection;
}

QSize Viewport::viewportSize() const {
    return QSize(m_width, m_height);
}

std::string Viewport::resolveActiveSketchId() const {
    if (!m_activeSketchId.empty()) {
        return m_activeSketchId;
    }
    if (!m_document || !m_activeSketch) {
        return {};
    }
    for (const auto& id : m_document->getSketchIds()) {
        if (m_document->getSketch(id) == m_activeSketch) {
            return id;
        }
    }
    return {};
}

std::vector<app::selection::SelectionItem> Viewport::modelSelection() const {
    std::vector<app::selection::SelectionItem> result;
    if (!m_selectionManager) {
        return result;
    }

    const auto& current = m_selectionManager->selection();
    result.reserve(current.size());
    for (const auto& item : current) {
        switch (item.kind) {
            case app::selection::SelectionKind::Vertex:
            case app::selection::SelectionKind::Edge:
            case app::selection::SelectionKind::Face:
            case app::selection::SelectionKind::Body:
                result.push_back(item);
                break;
            default:
                break;
        }
    }
    return result;
}

std::vector<app::selection::SelectionItem> Viewport::sketchSelection() const {
    std::vector<app::selection::SelectionItem> result;
    if (!m_selectionManager) {
        return result;
    }

    const auto& current = m_selectionManager->selection();
    result.reserve(current.size());
    for (const auto& item : current) {
        switch (item.kind) {
            case app::selection::SelectionKind::SketchPoint:
            case app::selection::SelectionKind::SketchEdge:
            case app::selection::SelectionKind::SketchRegion:
            case app::selection::SelectionKind::SketchConstraint:
                result.push_back(item);
                break;
            default:
                break;
        }
    }
    return result;
}
int Viewport::suppressedConstraintMarkerCount() const {
    return static_cast<int>(m_suppressedConstraintMarkers.size());
}

QMatrix4x4 Viewport::currentViewProjection() const {
    return buildViewProjection();
}

QSize Viewport::currentViewportSize() const {
    return viewportSize();
}

void Viewport::handleModelSelectionChanged() {
    if (m_inSketchMode || !m_selectionManager || !m_modelingToolManager) {
        return;
    }

    const auto& selection = m_selectionManager->selection();
    // Forward selection change to tool manager (e.g. for Revolve axis picking)
    m_modelingToolManager->onSelectionChanged(selection);

    // Emit context based on selection kind
    int context = 0;  // Default
    if (!selection.empty()) {
        switch (selection.front().kind) {
            case app::selection::SelectionKind::Edge:
                context = 1;
                break;
            case app::selection::SelectionKind::Face:
                context = 2;
                break;
            case app::selection::SelectionKind::Body:
                context = 3;
                break;
            case app::selection::SelectionKind::SketchRegion:
                context = 4;
                break;
            default:
                context = 0;
                break;
        }
    }
    emit selectionContextChanged(context);

    if (m_revolveToolActive) {
        return;
    }
    if (m_linearPatternToolActive) {
        return;
    }
    if (m_shellToolActive) {
        auto shellBodyId = m_modelingToolManager
            ? m_modelingToolManager->activeShellBodyId()
            : std::nullopt;
        if (!shellBodyId.has_value()) {
            m_modelingToolManager->cancelActiveTool();
            setShellToolActive(false);
        } else if (!selection.empty()) {
            const auto& front = selection.front();
            const bool sameBody = (front.id.ownerId == shellBodyId.value()) ||
                (front.id.elementId == shellBodyId.value());
            if (!sameBody) {
                m_modelingToolManager->cancelActiveTool();
                setShellToolActive(false);
            } else {
                return;
            }
        } else {
            return;
        }
    }

    // Auto-activate Extrude only for a closed sketch region. A selected planar face
    // routes through the auto-sketch-on-face fast path instead (see MainWindow).
    bool canExtrude = false;
    if (selection.size() == 1 &&
        selection.front().kind == app::selection::SelectionKind::SketchRegion) {
        canExtrude = true;
    }

    if (canExtrude) {
        m_modelingToolManager->activateExtrude(selection.front());
        setLinearPatternToolActive(false);
        setFilletToolActive(false);
        setShellToolActive(false);
        setExtrudeToolActive(m_modelingToolManager->hasActiveTool());
        return;
    }

    m_modelingToolManager->cancelActiveTool();
    setExtrudeToolActive(false);
    setLinearPatternToolActive(false);
    setFilletToolActive(false);
    setShellToolActive(false);
}

void Viewport::setExtrudeToolActive(bool active) {
    if (m_extrudeToolActive == active) {
        return;
    }
    m_extrudeToolActive = active;
    emit extrudeToolActiveChanged(active);
}

void Viewport::setFilletToolActive(bool active) {
    if (m_filletToolActive == active) {
        return;
    }
    m_filletToolActive = active;
    emit filletToolActiveChanged(active);
}

void Viewport::setShellToolActive(bool active) {
    if (m_shellToolActive == active) {
        return;
    }
    m_shellToolActive = active;
    emit shellToolActiveChanged(active);
}
app::selection::PickResult Viewport::buildModelPickResult(const QPoint& screenPos) {
    app::selection::PickResult result;
    if (m_modelPicker) {
        result = m_modelPicker->pick(screenPos,
                                     static_cast<double>(sketch::constants::PICK_TOLERANCE_PIXELS),
                                     buildViewProjection(),
                                     viewportSize());
    }

    std::optional<double> frontModelDepth;
    for (const auto& hit : result.hits) {
        switch (hit.kind) {
            case app::selection::SelectionKind::Vertex:
            case app::selection::SelectionKind::Edge:
            case app::selection::SelectionKind::Face:
                if (!frontModelDepth.has_value() || hit.depth < *frontModelDepth) {
                    frontModelDepth = hit.depth;
                }
                break;
            default:
                break;
        }
    }

    for (const auto& sketchId : visibleModelSketchIds()) {
        auto* sketch = m_document ? m_document->getSketch(sketchId) : nullptr;
        if (!sketch) {
            continue;
        }

        auto sketchResult = buildModelSketchPickResult(screenPos, sketchId, sketch);
        if (sketchResult.hits.empty()) {
            continue;
        }

        for (const auto& hit : sketchResult.hits) {
            if (frontModelDepth.has_value() &&
                hit.depth > *frontModelDepth + std::max(1e-2, *frontModelDepth * 1e-3)) {
                continue;
            }
            result.hits.push_back(hit);
        }
    }

    return result;
}

QStringList Viewport::buildDeepSelectLabels(
    const std::vector<app::selection::SelectionItem>& candidates) const {
    QStringList labels;
    labels.reserve(static_cast<int>(candidates.size()));

    std::unordered_map<std::string, core::sketch::RegionPickHit> sketchRegionInfoById;
    std::unordered_map<std::string, int> sketchRegionBaseCounts;
    std::unordered_map<std::string, int> sketchRegionBaseIndex;

    if (m_inSketchMode && m_sketchRenderer) {
        for (const auto& item : candidates) {
            if (item.kind != app::selection::SelectionKind::SketchRegion) {
                continue;
            }
            auto info = m_sketchRenderer->getRegionPickHit(item.id.elementId);
            if (!info.has_value()) {
                continue;
            }
            sketchRegionInfoById.emplace(item.id.elementId, *info);

            std::string baseLabel = "Profile";
            if (info->hasHoles) {
                baseLabel = "Ring profile";
            } else if (info->containmentDepth > 0) {
                baseLabel = "Inner profile";
            }
            sketchRegionBaseCounts[baseLabel]++;
        }
    }

    for (const auto& item : candidates) {
        QString label;
        switch (item.kind) {
            case app::selection::SelectionKind::SketchPoint:
                label = tr("Sketch Point");
                break;
            case app::selection::SelectionKind::SketchEdge:
                label = tr("Sketch Edge");
                break;
            case app::selection::SelectionKind::SketchRegion:
                if (auto it = sketchRegionInfoById.find(item.id.elementId);
                    it != sketchRegionInfoById.end()) {
                    const auto& info = it->second;
                    std::string baseLabel = "Profile";
                    if (info.hasHoles) {
                        baseLabel = "Ring profile";
                    } else if (info.containmentDepth > 0) {
                        baseLabel = "Inner profile";
                    }

                    int labelIndex = ++sketchRegionBaseIndex[baseLabel];
                    if (baseLabel == "Profile") {
                        label = tr("Profile %1").arg(labelIndex);
                    } else if (sketchRegionBaseCounts[baseLabel] > 1) {
                        label = QString::fromStdString(baseLabel) + " " +
                                QString::number(labelIndex);
                    } else {
                        label = QString::fromStdString(baseLabel);
                    }
                } else {
                    label = tr("Sketch Region");
                }
                break;
            case app::selection::SelectionKind::SketchConstraint:
                label = tr("Sketch Constraint");
                break;
            case app::selection::SelectionKind::Vertex:
                label = tr("Vertex");
                break;
            case app::selection::SelectionKind::Edge:
                label = tr("Edge");
                break;
            case app::selection::SelectionKind::Face:
                label = tr("Face");
                break;
            case app::selection::SelectionKind::Body:
                label = tr("Body");
                break;
            default:
                label = tr("Entity");
                break;
        }

        if (item.kind != app::selection::SelectionKind::SketchRegion &&
            !item.id.elementId.empty()) {
            label += " (" + QString::fromStdString(item.id.elementId) + ")";
        }
        labels.append(label);
    }
    return labels;
}
void Viewport::setDocument(app::Document* document) {
    for (const auto& connection : m_documentConnections) {
        disconnect(connection);
    }
    m_documentConnections.clear();

    m_document = document;
    m_documentSketchesDirty = true;
    if (m_document && !m_referenceSketchId.empty()) {
        m_referenceSketch = m_document->getSketch(m_referenceSketchId);
    } else {
        m_referenceSketch = nullptr;
    }
    if (!m_inSketchMode && m_sketchRenderer) {
        m_sketchRenderer->setSketch(nullptr);
    }
    if (m_modelingToolManager) {
        m_modelingToolManager->setDocument(m_document);
    }

    // Connect to document signals to mark geometry dirty
    if (m_document) {
        auto trackConnection = [this](const QMetaObject::Connection& connection) {
            m_documentConnections.push_back(connection);
        };

        trackConnection(connect(m_document, &app::Document::sketchAdded, this, [this]() {
            m_documentSketchesDirty = true;
            updateModelSelectionFilter();
            update();
        }));
        trackConnection(connect(m_document, &app::Document::sketchRemoved, this, [this]() {
            if (!m_referenceSketchId.empty() &&
                m_document &&
                m_document->getSketch(m_referenceSketchId) == nullptr) {
                m_referenceSketchId.clear();
                m_referenceSketch = nullptr;
                updateModelSelectionFilter();
            }
            m_documentSketchesDirty = true;
            updateModelSelectionFilter();
            if (!m_inSketchMode && m_sketchRenderer) {
                m_sketchRenderer->setSketch(nullptr);
            }
            update();
        }));
        trackConnection(connect(m_document, &app::Document::sketchVisibilityChanged, this, [this]() {
            m_documentSketchesDirty = true;
            updateModelSelectionFilter();
            if (!m_inSketchMode && m_sketchRenderer) {
                m_sketchRenderer->setSketch(nullptr);
            }
            update();
        }));
        trackConnection(connect(m_document, &app::Document::bodyAdded, this, [this]() {
            syncModelMeshes();
            update();
        }));
        trackConnection(connect(m_document, &app::Document::bodyRemoved, this, [this]() {
            syncModelMeshes();
            update();
        }));
        trackConnection(connect(m_document, &app::Document::bodyUpdated, this, [this]() {
            syncModelMeshes();
            update();
        }));
        trackConnection(connect(m_document, &app::Document::bodyVisibilityChanged, this, [this]() {
            syncModelMeshes();
            update();
        }));
        trackConnection(connect(m_document, &app::Document::isolationChanged, this, [this]() {
            syncModelMeshes();
            update();
        }));
        trackConnection(connect(m_document, &app::Document::documentCleared, this, [this]() {
            resetTransientState();
            syncModelMeshes();
            update();
        }));
    }

    updateModelSelectionFilter();
    syncModelMeshes();
    update();
}

void Viewport::setCommandProcessor(app::commands::CommandProcessor* processor) {
    m_commandProcessor = processor;
    if (m_modelingToolManager) {
        m_modelingToolManager->setCommandProcessor(processor);
    }
}

void Viewport::setReferenceSketch(const QString& sketchId) {
    const std::string id = sketchId.toStdString();
    if (id == m_referenceSketchId &&
        ((id.empty() && m_referenceSketch == nullptr) || (!id.empty() && m_referenceSketch))) {
        return;
    }
    m_referenceSketchId = id;
    if (m_document && !m_referenceSketchId.empty()) {
        m_referenceSketch = m_document->getSketch(m_referenceSketchId);
    } else {
        m_referenceSketch = nullptr;
    }
    if (!m_inSketchMode && m_sketchRenderer) {
        m_sketchRenderer->setSketch(nullptr);
    }
    m_documentSketchesDirty = true;
    updateModelSelectionFilter();
    update();
}

void Viewport::updateModelSelectionFilter() {
    if (!m_selectionManager || m_inSketchMode) {
        return;
    }

    app::selection::SelectionFilter filter;
    filter.allowedKinds = {
        app::selection::SelectionKind::Body,
        app::selection::SelectionKind::Vertex,
        app::selection::SelectionKind::Edge,
        app::selection::SelectionKind::Face
    };
    if (hasVisibleModelSketches()) {
        filter.allowedKinds.insert(app::selection::SelectionKind::SketchRegion);
    }
    m_selectionManager->setFilter(filter);
}
void Viewport::setMoveSketchModeChangedCallback(std::function<void(bool)> callback) {
    m_moveSketchModeChangedCallback = std::move(callback);
}

} // namespace ui
} // namespace onecad
