#include "Viewport.h"
#include "ViewportCommon.h"
#include "../../render/Camera3D.h"
#include "../../render/Grid3D.h"
#include "../../core/sketch/SketchRenderer.h"
#include "../../core/sketch/Sketch.h"
#include "../../core/sketch/SketchPoint.h"
#include "../../core/sketch/tools/SketchToolManager.h"
#include "../../app/commands/CommandProcessor.h"
#include "../../app/commands/SketchDragGestureCommand.h"
#include "../../app/document/Document.h"
#include "../../app/selection/SelectionManager.h"
#include "../../app/selection/SelectionTypes.h"
#include "../tools/ModelingToolManager.h"
#include "../selection/DeepSelectPopup.h"
#include "../selection/SketchPickerAdapter.h"

#include <QLoggingCategory>
#include <QtMath>
#include <algorithm>
#include <cmath>

#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Vertex.hxx>
#include <BRep_Tool.hxx>
#include <gp_Pnt.hxx>

namespace onecad {
namespace ui {

Q_DECLARE_LOGGING_CATEGORY(logUiInput)

namespace sketch = core::sketch;
namespace sketchTools = core::sketch::tools;

using namespace viewport_common;

void Viewport::setSketchInteractionState(SketchInteractionState newState, const char* reason) {
    if (m_sketchInteractionState == newState) {
        return;
    }
    auto stateName = [](SketchInteractionState state) {
        switch (state) {
        case SketchInteractionState::Idle:
            return "Idle";
        case SketchInteractionState::PendingPointSingleDrag:
            return "PendingPointSingleDrag";
        case SketchInteractionState::PointSingleDragging:
            return "PointSingleDragging";
        case SketchInteractionState::PendingPointGroupDrag:
            return "PendingPointGroupDrag";
        case SketchInteractionState::PointGroupDragging:
            return "PointGroupDragging";
        case SketchInteractionState::SketchMoving:
            return "SketchMoving";
        }
        return "Unknown";
    };
    qCDebug(logUiInput) << "sketch_interaction_transition"
                        << stateName(m_sketchInteractionState)
                        << "->"
                        << stateName(newState)
                        << "reason="
                        << reason;
    m_sketchInteractionState = newState;
}

void Viewport::setSketchDragIntent(SketchDragIntent newIntent, const char* reason) {
    if (m_sketchDragIntent == newIntent) {
        return;
    }
    auto intentName = [](SketchDragIntent intent) {
        switch (intent) {
        case SketchDragIntent::None:
            return "None";
        case SketchDragIntent::PointSingle:
            return "PointSingle";
        case SketchDragIntent::PointGroup:
            return "PointGroup";
        case SketchDragIntent::SelectionBox:
            return "SelectionBox";
        }
        return "Unknown";
    };
    qCDebug(logUiInput) << "sketch_drag_intent_transition"
                        << intentName(m_sketchDragIntent)
                        << "->"
                        << intentName(newIntent)
                        << "reason="
                        << reason;
    m_sketchDragIntent = newIntent;
}

void Viewport::beginPlaneSelection() {
    if (m_inSketchMode) {
        return;
    }
    if (m_selectionManager) {
        m_selectionManager->clearSelection();
    }
    if (m_modelingToolManager) {
        m_modelingToolManager->cancelActiveTool();
    }
    setExtrudeToolActive(false);
    setRevolveToolActive(false);
    setFilletToolActive(false);
    setShellToolActive(false);
    if (m_deepSelectPopup && m_deepSelectPopup->isVisible()) {
        m_deepSelectPopup->hide();
    }
    m_pendingCandidates.clear();
    m_pendingClickPos = QPoint();
    m_pendingModifiers = {};
    m_pendingShellFaceToggle = false;
    m_planeSelectionActive = true;
    m_planeHoverIndex = -1;
    update();
}

void Viewport::cancelPlaneSelection() {
    if (!m_planeSelectionActive) {
        return;
    }
    m_planeSelectionActive = false;
    m_planeHoverIndex = -1;
    setCursor(Qt::ArrowCursor);
    update();
    emit planeSelectionCancelled();
}

void Viewport::updateSketchRenderingState() {
    if (!m_inSketchMode || !m_activeSketch || !m_sketchRenderer) {
        return;
    }

    int dof = m_activeSketch->getDegreesOfFreedom();
    bool overConstrained = m_activeSketch->isOverConstrained();
    m_sketchRenderer->setDOF(overConstrained ? -1 : dof);
    m_sketchRenderer->updateConstraints();
}

void Viewport::enterSketchMode(sketch::Sketch* sketch) {
    if (m_inSketchMode || !sketch) return;

    if (m_modelingToolManager) {
        m_modelingToolManager->cancelActiveTool();
    }
    setExtrudeToolActive(false);
    setRevolveToolActive(false);

    m_activeSketch = sketch;
    m_activeSketchId.clear();
    m_activeSketchId = resolveActiveSketchId();
    m_inSketchMode = true;
    m_planeSelectionActive = false;
    m_planeHoverIndex = -1;
    if (m_selectionManager) {
        m_selectionManager->setMode(app::selection::SelectionMode::Sketch);
        m_selectionManager->setDeepSelectEnabled(true);
        app::selection::SelectionFilter filter;
        filter.allowedKinds = {
            app::selection::SelectionKind::SketchPoint,
            app::selection::SelectionKind::SketchEdge,
            app::selection::SelectionKind::SketchRegion,
            app::selection::SelectionKind::SketchConstraint
        };
        m_selectionManager->setFilter(filter);
    }

    if (m_toolManager) {
        m_toolManager->deactivateTool();
        m_toolManager->setSketch(m_activeSketch);
        m_toolManager->setRenderer(m_sketchRenderer.get());
    }

    m_savedCameraPosition = m_camera->position();
    m_savedCameraTarget = m_camera->target();
    m_savedCameraUp = m_camera->up();
    m_savedCameraAngle = m_camera->cameraAngle();

    float currentVisualScale;
    float currentDist = m_camera->distance();
    if (m_camera->projectionType() == render::Camera3D::ProjectionType::Perspective) {
        float halfFovRad = qDegreesToRadians(m_camera->fov() * 0.5f);
        currentVisualScale = 2.0f * currentDist * qTan(halfFovRad);
        qDebug() << "[EnterSketch] Perspective→Ortho: dist=" << currentDist
                 << "FOV=" << m_camera->fov() << "visualScale=" << currentVisualScale;
    } else {
        currentVisualScale = m_camera->orthoScale();
        qDebug() << "[EnterSketch] Ortho→Ortho: preserving orthoScale=" << currentVisualScale;
    }

    const auto& plane = sketch->getPlane();
    QVector3D normal(plane.normal.x, plane.normal.y, plane.normal.z);
    QVector3D up(plane.yAxis.x, plane.yAxis.y, plane.yAxis.z);
    normal.normalize();
    up.normalize();

    QVector3D target(plane.origin.x, plane.origin.y, plane.origin.z);
    float dist = m_camera->distance();

    CameraState targetState;
    targetState.target = target;
    targetState.up = up;
    targetState.position = target + normal * dist;
    targetState.angle = 0.0f;
    targetState.orthoScale = currentVisualScale;

    animateCamera(targetState);

    if (m_sketchRenderer) {
        m_sketchRenderer->setSketch(sketch);
        m_sketchRenderer->updateGeometry();
        updateSketchRenderingState();
    }

    m_toolManager = std::make_unique<sketchTools::SketchToolManager>(this);
    m_toolManager->setSketch(sketch);
    m_toolManager->setRenderer(m_sketchRenderer.get());
    syncSnapGridSizeFromCamera();
    updateSnapGeometry();

    connect(m_toolManager.get(), &sketchTools::SketchToolManager::geometryCreated, this, [this]() {
        if (m_sketchRenderer) {
            m_sketchRenderer->updateGeometry();
            updateSketchRenderingState();
        }
        update();
        emit sketchUpdated();
    });
    connect(m_toolManager.get(), &sketchTools::SketchToolManager::updateRequested, this, [this]() {
        update();
    });

    update();
    emit sketchModeChanged(true);
}

void Viewport::exitSketchMode() {
    if (!m_inSketchMode) return;

    if (m_activeSketch) {
        m_activeSketch->endPointDrag();
        m_activeSketch->endGroupDrag();
    }
    endSketchDragGestureCapture(false);
    if (m_sketchRenderer) {
        m_sketchRenderer->clearDragGuides();
    }
    m_groupDragPointIds.clear();
    m_groupDragStartPositions.clear();
    m_groupDragFailureFeedbackShown = false;

    m_inSketchMode = false;
    m_activeSketch = nullptr;
    m_activeSketchId.clear();
    if (m_selectionManager) {
        m_selectionManager->setMode(app::selection::SelectionMode::Model);
        m_selectionManager->setDeepSelectEnabled(false);
        updateModelSelectionFilter();
    }

    if (m_toolManager) {
        m_toolManager->deactivateTool();
        m_toolManager->setSketch(m_activeSketch);
        m_toolManager->setRenderer(m_sketchRenderer.get());
    }

    updateSnapGeometry();

    if (m_sketchRenderer) {
        m_sketchRenderer->setSketch(nullptr);
    }

    m_documentSketchesDirty = true;

    CameraState savedState;
    savedState.position = m_savedCameraPosition;
    savedState.target = m_savedCameraTarget;
    savedState.up = m_savedCameraUp;
    savedState.angle = m_savedCameraAngle;

    if (m_savedCameraAngle < 0.01f) {
        savedState.orthoScale = m_camera->orthoScale();
        qDebug() << "[ExitSketch] Ortho→Ortho: preserving orthoScale=" << savedState.orthoScale;
    } else {
        float savedDistance = (m_savedCameraPosition - m_savedCameraTarget).length();
        float halfFovRad = qDegreesToRadians(m_savedCameraAngle * 0.5f);
        savedState.orthoScale = 2.0f * savedDistance * qTan(halfFovRad);
        qDebug() << "[ExitSketch] Ortho→Perspective: savedDist=" << savedDistance
                 << "savedFOV=" << m_savedCameraAngle << "requiredOrthoScale=" << savedState.orthoScale;
    }

    animateCamera(savedState);

    update();
    emit sketchModeChanged(false);
}

sketch::Vec2d Viewport::screenToSketch(const QPoint& screenPos) const {
    if (!m_activeSketch || !m_camera) {
        return {0.0, 0.0};
    }
    return screenToSketchPlane(screenPos, m_activeSketch->getPlane());
}

sketch::Vec2d Viewport::screenToSketchPlane(const QPoint& screenPos,
                                            const sketch::SketchPlane& plane) const {
    if (!m_camera) {
        return {0.0, 0.0};
    }

    float aspectRatio = static_cast<float>(m_width) / static_cast<float>(m_height);
    QMatrix4x4 view = m_camera->viewMatrix();
    QMatrix4x4 projection = m_camera->projectionMatrix(aspectRatio);
    QMatrix4x4 viewProj = projection * view;
    bool invertible = false;
    QMatrix4x4 invViewProj = viewProj.inverted(&invertible);

    if (!invertible) {
        return {0.0, 0.0};
    }

    float ndcX = (2.0f * screenPos.x() / m_width) - 1.0f;
    float ndcY = 1.0f - (2.0f * screenPos.y() / m_height);

    QVector4D nearPoint = invViewProj * QVector4D(ndcX, ndcY, -1.0f, 1.0f);
    QVector4D farPoint = invViewProj * QVector4D(ndcX, ndcY, 1.0f, 1.0f);

    if (std::abs(nearPoint.w()) < 1e-8f || std::abs(farPoint.w()) < 1e-8f) {
        return {0.0, 0.0};
    }

    QVector3D rayOrigin = nearPoint.toVector3D() / nearPoint.w();
    QVector3D rayEnd = farPoint.toVector3D() / farPoint.w();
    QVector3D rayDir = (rayEnd - rayOrigin).normalized();

    QVector3D planeOrigin(plane.origin.x, plane.origin.y, plane.origin.z);
    QVector3D planeNormal(plane.normal.x, plane.normal.y, plane.normal.z);

    float denom = QVector3D::dotProduct(rayDir, planeNormal);
    if (std::abs(denom) < 1e-8f) {
        QVector3D toPlane = planeOrigin - rayOrigin;
        float distToPlane = QVector3D::dotProduct(toPlane, planeNormal);
        QVector3D closestPoint = rayOrigin + planeNormal * distToPlane;
        sketch::Vec3d worldPt{closestPoint.x(), closestPoint.y(), closestPoint.z()};
        return plane.toSketch(worldPt);
    }

    float t = QVector3D::dotProduct(planeOrigin - rayOrigin, planeNormal) / denom;
    QVector3D intersection = rayOrigin + rayDir * t;

    sketch::Vec3d worldPt{intersection.x(), intersection.y(), intersection.z()};
    return plane.toSketch(worldPt);
}

void Viewport::updatePlaneSelectionHover(const QPoint& screenPos) {
    int hitIndex = -1;
    if (pickPlaneSelection(screenPos, &hitIndex)) {
        if (m_planeHoverIndex != hitIndex) {
            m_planeHoverIndex = hitIndex;
            update();
        }
        if (!m_isOrbiting && !m_isPanning) {
            setCursor(Qt::PointingHandCursor);
        }
        return;
    }

    if (m_planeHoverIndex != -1) {
        m_planeHoverIndex = -1;
        update();
    }
    if (!m_isOrbiting && !m_isPanning) {
        setCursor(Qt::ArrowCursor);
    }
}

std::unordered_set<sketch::EntityID> Viewport::selectedSketchPointIds() const {
    std::unordered_set<sketch::EntityID> pointIds;
    if (!m_selectionManager) {
        return pointIds;
    }

    for (const auto& item : m_selectionManager->selection()) {
        if (item.kind != app::selection::SelectionKind::SketchPoint) {
            continue;
        }
        if (!item.id.elementId.empty()) {
            pointIds.insert(item.id.elementId);
        }
    }
    return pointIds;
}

std::vector<std::string> Viewport::visibleModelSketchIds() const {
    std::vector<std::string> visibleIds;
    if (!m_document || m_inSketchMode) {
        return visibleIds;
    }

    const auto sketchIds = m_document->getSketchIds();
    visibleIds.reserve(sketchIds.size());
    for (const auto& sketchId : sketchIds) {
        if (!m_document->isSketchVisible(sketchId)) {
            continue;
        }
        if (m_document->getSketch(sketchId) == nullptr) {
            continue;
        }
        visibleIds.push_back(sketchId);
    }
    return visibleIds;
}

bool Viewport::hasVisibleModelSketches() const {
    if (!m_document || m_inSketchMode) {
        return false;
    }
    for (const auto& sketchId : m_document->getSketchIds()) {
        if (m_document->isSketchVisible(sketchId) && m_document->getSketch(sketchId) != nullptr) {
            return true;
        }
    }
    return false;
}

void Viewport::applySketchOverlayStateForSketch(const std::string& sketchId) {
    if (!m_sketchRenderer) {
        return;
    }

    m_sketchRenderer->clearSelection();
    m_sketchRenderer->clearRegionSelection();
    m_sketchRenderer->setSelectedConstraint({});
    m_sketchRenderer->setHoverEntity("");
    m_sketchRenderer->setHoverConstraint({});
    m_sketchRenderer->clearRegionHover();

    if (m_inSketchMode) {
        syncSuppressedConstraintMarkers();
    } else {
        m_sketchRenderer->setSuppressedConstraints({});
    }

    if (!m_selectionManager || sketchId.empty()) {
        return;
    }

    for (const auto& item : m_selectionManager->selection()) {
        if (item.id.ownerId != sketchId) {
            continue;
        }
        switch (item.kind) {
            case app::selection::SelectionKind::SketchRegion:
                m_sketchRenderer->toggleRegionSelection(item.id.elementId);
                break;
            case app::selection::SelectionKind::SketchConstraint:
                m_sketchRenderer->setSelectedConstraint(item.id.elementId);
                break;
            case app::selection::SelectionKind::SketchPoint:
            case app::selection::SelectionKind::SketchEdge:
                m_sketchRenderer->setEntitySelection(item.id.elementId,
                                                     sketch::SelectionState::Selected);
                break;
            default:
                break;
        }
    }

    const auto hover = m_selectionManager->hover();
    if (!hover.has_value() || hover->id.ownerId != sketchId) {
        return;
    }

    switch (hover->kind) {
        case app::selection::SelectionKind::SketchRegion:
            m_sketchRenderer->setRegionHover(hover->id.elementId);
            break;
        case app::selection::SelectionKind::SketchConstraint:
            m_sketchRenderer->setHoverConstraint(hover->id.elementId);
            break;
        case app::selection::SelectionKind::SketchPoint:
        case app::selection::SelectionKind::SketchEdge:
            m_sketchRenderer->setHoverEntity(hover->id.elementId);
            break;
        default:
            break;
    }
}

void Viewport::updateSketchSelectionFromManager() {
    if (!m_sketchRenderer) {
        return;
    }

    if (!m_inSketchMode) {
        update();
        return;
    }

    applySketchOverlayStateForSketch(resolveActiveSketchId());
    update();
}

void Viewport::updateSketchHoverFromManager() {
    if (!m_sketchRenderer) {
        return;
    }

    if (!m_inSketchMode) {
        update();
        return;
    }

    applySketchOverlayStateForSketch(resolveActiveSketchId());
    update();
}

void Viewport::selectSketchConstraint(const QString& constraintId) {
    if (!m_selectionManager || !m_sketchRenderer || !m_inSketchMode || constraintId.isEmpty()) {
        return;
    }

    app::selection::SelectionItem item;
    item.kind = app::selection::SelectionKind::SketchConstraint;
    item.id.ownerId = resolveActiveSketchId();
    item.id.elementId = constraintId.toStdString();
    m_selectionManager->replaceSelection({item});
    m_sketchRenderer->setSelectedConstraint(item.id.elementId);
    update();
}

void Viewport::suppressConstraintMarker(const QString& constraintId) {
    if (constraintId.isEmpty()) {
        return;
    }
    m_suppressedConstraintMarkers.insert(constraintId.toStdString());
    syncSuppressedConstraintMarkers();
    emit sketchSelectionChanged();
    update();
}

void Viewport::unsuppressConstraintMarker(const QString& constraintId) {
    if (constraintId.isEmpty()) {
        return;
    }
    m_suppressedConstraintMarkers.erase(constraintId.toStdString());
    syncSuppressedConstraintMarkers();
    emit sketchSelectionChanged();
    update();
}

void Viewport::clearSuppressedConstraintMarkers() {
    if (m_suppressedConstraintMarkers.empty()) {
        return;
    }
    m_suppressedConstraintMarkers.clear();
    syncSuppressedConstraintMarkers();
    emit sketchSelectionChanged();
    update();
}

void Viewport::syncSuppressedConstraintMarkers() {
    if (!m_sketchRenderer) {
        return;
    }

    std::vector<core::sketch::ConstraintID> suppressed;
    suppressed.reserve(m_suppressedConstraintMarkers.size());
    for (const auto& id : m_suppressedConstraintMarkers) {
        suppressed.push_back(id);
    }
    std::sort(suppressed.begin(), suppressed.end());
    m_sketchRenderer->setSuppressedConstraints(suppressed);
}

app::selection::PickResult Viewport::buildSketchPickResult(const QPoint& screenPos) const {
    if (!m_sketchRenderer || !m_activeSketch || !m_sketchPicker) {
        return {};
    }

    const double pixelScale = (m_pixelScale > 0.0) ? m_pixelScale : 1.0;
    const double tolerancePixels = static_cast<double>(sketch::constants::PICK_TOLERANCE_PIXELS);
    const std::string sketchId = resolveActiveSketchId();

    selection::SketchPickerAdapter::Options options;
    options.allowConstraints = true;
    options.allowRegions = true;

    sketch::Vec2d sketchPos = screenToSketch(screenPos);
    return m_sketchPicker->pick(*m_sketchRenderer,
                                *m_activeSketch,
                                sketchPos,
                                sketchId,
                                pixelScale,
                                tolerancePixels,
                                options);
}

bool Viewport::buildScreenRay(const QPointF& screenPos,
                              QVector3D* outOrigin,
                              QVector3D* outDirection) const {
    if (!m_camera || !outOrigin || !outDirection || m_width <= 0 || m_height <= 0) {
        return false;
    }

    const float aspectRatio = static_cast<float>(m_width) / static_cast<float>(m_height);
    const QMatrix4x4 viewProjection = m_camera->projectionMatrix(aspectRatio) * m_camera->viewMatrix();
    bool invertible = false;
    const QMatrix4x4 inverse = viewProjection.inverted(&invertible);
    if (!invertible) {
        return false;
    }

    const float ndcX = (2.0f * static_cast<float>(screenPos.x()) / static_cast<float>(m_width)) - 1.0f;
    const float ndcY = 1.0f - (2.0f * static_cast<float>(screenPos.y()) / static_cast<float>(m_height));
    const QVector4D nearPoint = inverse * QVector4D(ndcX, ndcY, -1.0f, 1.0f);
    const QVector4D farPoint = inverse * QVector4D(ndcX, ndcY, 1.0f, 1.0f);
    if (std::abs(nearPoint.w()) < 1e-8f || std::abs(farPoint.w()) < 1e-8f) {
        return false;
    }

    const QVector3D origin = nearPoint.toVector3D() / nearPoint.w();
    const QVector3D farPos = farPoint.toVector3D() / farPoint.w();
    const QVector3D direction = (farPos - origin).normalized();
    if (direction.lengthSquared() < 1e-8f) {
        return false;
    }

    *outOrigin = origin;
    *outDirection = direction;
    return true;
}

app::selection::PickResult Viewport::buildModelSketchPickResult(const QPoint& screenPos,
                                                                const std::string& sketchId,
                                                                sketch::Sketch* sketch) {
    if (!m_sketchRenderer || !sketch || !m_sketchPicker || sketchId.empty()) {
        return {};
    }

    const double pixelScale = (m_pixelScale > 0.0) ? m_pixelScale : 1.0;
    const double tolerancePixels = static_cast<double>(sketch::constants::PICK_TOLERANCE_PIXELS);

    selection::SketchPickerAdapter::Options options;
    options.allowConstraints = false;
    options.allowRegions = true;

    m_sketchRenderer->setSketch(sketch);
    m_sketchRenderer->updateGeometry();

    QVector3D rayOrigin;
    QVector3D rayDirection;
    if (!buildScreenRay(screenPos, &rayOrigin, &rayDirection)) {
        return {};
    }

    const auto& plane = sketch->getPlane();
    sketch::Vec2d sketchPos = screenToSketchPlane(screenPos, plane);
    const sketch::Vec3d sketchWorld = sketch->toWorld(sketchPos);
    QVector3D worldPos(static_cast<float>(sketchWorld.x),
                       static_cast<float>(sketchWorld.y),
                       static_cast<float>(sketchWorld.z));

    QVector3D planeHit;
    const QVector3D planeOrigin(static_cast<float>(plane.origin.x),
                                static_cast<float>(plane.origin.y),
                                static_cast<float>(plane.origin.z));
    const QVector3D planeNormal(static_cast<float>(plane.normal.x),
                                static_cast<float>(plane.normal.y),
                                static_cast<float>(plane.normal.z));
    if (intersectScreenPointWithPlane(screenPos, planeOrigin, planeNormal, &planeHit)) {
        worldPos = planeHit;
    }

    app::selection::PickResult result = m_sketchPicker->pick(*m_sketchRenderer,
                                                             *sketch,
                                                             sketchPos,
                                                             sketchId,
                                                             pixelScale,
                                                             tolerancePixels,
                                                             options);

    const double depth = std::max(
        0.0,
        static_cast<double>(QVector3D::dotProduct(worldPos - rayOrigin, rayDirection)));
    for (auto& hit : result.hits) {
        hit.depth = depth;
        hit.worldPos = {worldPos.x(), worldPos.y(), worldPos.z()};
        hit.normal = {planeNormal.x(), planeNormal.y(), planeNormal.z()};

        if (hit.kind != app::selection::SelectionKind::SketchPoint) {
            continue;
        }
        const auto* point = sketch->getEntityAs<sketch::SketchPoint>(hit.id.elementId);
        if (!point) {
            continue;
        }
        const sketch::Vec3d pointWorld = sketch->toWorld({point->position().X(), point->position().Y()});
        const QVector3D pointPos(static_cast<float>(pointWorld.x),
                                 static_cast<float>(pointWorld.y),
                                 static_cast<float>(pointWorld.z));
        hit.worldPos = {pointPos.x(), pointPos.y(), pointPos.z()};
        hit.depth = std::max(
            0.0,
            static_cast<double>(QVector3D::dotProduct(pointPos - rayOrigin, rayDirection)));
    }

    return result;
}

bool Viewport::pickPlaneSelection(const QPoint& screenPos, int* outIndex) const {
    if (!m_planeSelectionActive || !m_camera || !outIndex) {
        return false;
    }

    float aspectRatio = static_cast<float>(m_width) / static_cast<float>(m_height);
    QMatrix4x4 view = m_camera->viewMatrix();
    QMatrix4x4 projection = m_camera->projectionMatrix(aspectRatio);
    QMatrix4x4 viewProjection = projection * view;

    bool invertible = false;
    QMatrix4x4 inverse = viewProjection.inverted(&invertible);
    if (!invertible) {
        return false;
    }

    float ndcX = (2.0f * screenPos.x() / m_width) - 1.0f;
    float ndcY = 1.0f - (2.0f * screenPos.y() / m_height);

    QVector4D nearPoint = inverse * QVector4D(ndcX, ndcY, -1.0f, 1.0f);
    QVector4D farPoint = inverse * QVector4D(ndcX, ndcY, 1.0f, 1.0f);
    if (std::abs(nearPoint.w()) < 1e-8f || std::abs(farPoint.w()) < 1e-8f) {
        return false;
    }

    QVector3D rayOrigin = nearPoint.toVector3D() / nearPoint.w();
    QVector3D rayEnd = farPoint.toVector3D() / farPoint.w();
    QVector3D rayDir = (rayEnd - rayOrigin).normalized();

    const ThemeViewportPlaneColors& planeColors =
        ThemeManager::instance().currentTheme().viewport.planes;
    const auto selections = planeSelections(planeColors);

    float bestDist = std::numeric_limits<float>::max();
    int bestIndex = -1;

    for (int i = 0; i < static_cast<int>(selections.size()); ++i) {
        const auto& selection = selections[i];
        PlaneAxes axes = buildPlaneAxes(selection.plane);
        QVector3D origin(selection.plane.origin.x, selection.plane.origin.y, selection.plane.origin.z);
        QVector3D normal = axes.normal;

        QVector3D hit;
        float dist = 0.0f;
        if (!intersectRayWithPlane(rayOrigin, rayDir, origin, normal, &hit, &dist)) {
            continue;
        }

        QVector3D local = hit - origin;
        float u = QVector3D::dotProduct(local, axes.xAxis);
        float v = QVector3D::dotProduct(local, axes.yAxis);

        if (std::abs(u) <= kPlaneSelectHalf && std::abs(v) <= kPlaneSelectHalf) {
            if (dist < bestDist) {
                bestDist = dist;
                bestIndex = i;
            }
        }
    }

    if (bestIndex >= 0) {
        *outIndex = bestIndex;
        return true;
    }
    return false;
}

void Viewport::activateLineTool() {
    setMoveSketchMode(false);
    if (m_toolManager) {
        m_toolManager->activateTool(sketchTools::ToolType::Line);
    }
}

void Viewport::activateCircleTool() {
    setMoveSketchMode(false);
    if (m_toolManager) {
        m_toolManager->activateTool(sketchTools::ToolType::Circle);
    }
}

void Viewport::activateRectangleTool() {
    setMoveSketchMode(false);
    if (m_toolManager) {
        m_toolManager->activateTool(sketchTools::ToolType::Rectangle);
    }
}

void Viewport::activateArcTool() {
    setMoveSketchMode(false);
    if (m_toolManager) {
        m_toolManager->activateTool(sketchTools::ToolType::Arc);
    }
}

void Viewport::activateEllipseTool() {
    setMoveSketchMode(false);
    if (m_toolManager) {
        m_toolManager->activateTool(sketchTools::ToolType::Ellipse);
    }
}

void Viewport::activateTrimTool() {
    setMoveSketchMode(false);
    if (m_toolManager) {
        m_toolManager->activateTool(sketchTools::ToolType::Trim);
    }
}

void Viewport::activateMirrorTool() {
    setMoveSketchMode(false);
    if (m_toolManager) {
        m_toolManager->activateTool(sketchTools::ToolType::Mirror);
    }
}

void Viewport::deactivateTool() {
    if (m_toolManager) {
        m_toolManager->deactivateTool();
    }
}

void Viewport::beginSketchDragGestureCapture() {
    if (!m_sketchDragGestureCaptureEnabled || !m_document || !m_activeSketch) {
        return;
    }
    if (m_sketchDragGestureCommand) {
        return;
    }

    const std::string activeSketchId = resolveActiveSketchId();
    if (activeSketchId.empty()) {
        return;
    }

    auto command = std::make_unique<app::commands::SketchDragGestureCommand>(m_document,
                                                                              activeSketchId);
    if (!command->beginGesture()) {
        return;
    }
    m_sketchDragGestureCommand = std::move(command);
}

void Viewport::endSketchDragGestureCapture(bool commit) {
    if (!m_sketchDragGestureCommand) {
        return;
    }
    if (commit) {
        const bool finalized = m_sketchDragGestureCommand->finalizeGesture();
        if (finalized && m_sketchDragGestureCommand->hasCapturedChange() && m_commandProcessor) {
            auto command = std::move(m_sketchDragGestureCommand);
            m_commandProcessor->execute(std::move(command));
            return;
        }
    } else {
        m_sketchDragGestureCommand->cancelGesture();
    }
    m_sketchDragGestureCommand.reset();
}

void Viewport::notifySketchUpdated() {
    emit sketchUpdated();
}

void Viewport::setMoveSketchMode(bool active) {
    if (m_moveSketchModeActive == active) {
        return;
    }
    m_moveSketchModeActive = active;
    if (active) {
        setCursor(Qt::SizeAllCursor);
    } else {
        if (m_activeSketch) {
            m_activeSketch->endPointDrag();
            m_activeSketch->endGroupDrag();
        }
        if (m_sketchRenderer) {
            m_sketchRenderer->clearDragGuides();
        }
        if (m_sketchInteractionState == SketchInteractionState::SketchMoving) {
            endSketchDragGestureCapture(false);
        }
        setSketchInteractionState(SketchInteractionState::Idle, "move-sketch mode disabled");
        setSketchDragIntent(SketchDragIntent::None, "move-sketch mode disabled");
        m_pointDragCandidateId.clear();
        m_moveSketchLastSketchPos = sketch::Vec2d{0.0, 0.0};
        m_groupDragPointIds.clear();
        m_groupDragStartPositions.clear();
        m_groupDragFailureFeedbackShown = false;
        setCursor(Qt::ArrowCursor);
    }
    if (m_moveSketchModeChangedCallback) {
        m_moveSketchModeChangedCallback(m_moveSketchModeActive);
    }
    update();
}

double Viewport::currentPixelScaleForSnapping() const {
    if (!m_camera) {
        return 1.0;
    }

    const int viewportHeight = (m_height > 0) ? m_height : height();
    const qreal ratio = devicePixelRatio();
    if (viewportHeight <= 0 || ratio <= 0.0) {
        return 1.0;
    }

    double pixelScale = 1.0;
    if (m_camera->projectionType() == render::Camera3D::ProjectionType::Orthographic) {
        const double worldHeight = static_cast<double>(m_camera->orthoScale());
        pixelScale = worldHeight / (static_cast<double>(viewportHeight) * ratio);
    } else {
        const double halfFov = qDegreesToRadians(m_camera->fov() * 0.5f);
        const double worldHeight = 2.0 * static_cast<double>(m_camera->distance()) * std::tan(halfFov);
        pixelScale = worldHeight / (static_cast<double>(viewportHeight) * ratio);
    }

    if (!std::isfinite(pixelScale) || pixelScale <= 0.0) {
        return 1.0;
    }
    return pixelScale;
}

void Viewport::syncSnapGridSizeFromCamera() {
    if (!m_toolManager) {
        return;
    }

    const double pixelScale = currentPixelScaleForSnapping();
    m_pixelScale = pixelScale;

    const float gridSpacing = render::Grid3D::adaptiveSpacing(static_cast<float>(pixelScale));
    if (!std::isfinite(gridSpacing) || gridSpacing <= 0.0f) {
        return;
    }

    m_toolManager->snapManager().setGridSize(static_cast<double>(gridSpacing));
}

void Viewport::updateSnapSettings(const SnapSettingsPanel::SnapSettings& settings) {
    if (!m_toolManager) return;

    auto& sm = m_toolManager->snapManager();
    sm.setGridSnapEnabled(settings.grid);
    sm.setSnapEnabled(core::sketch::SnapType::SketchGuide, settings.sketchGuideLines);
    sm.setSnapEnabled(core::sketch::SnapType::Vertex, settings.sketchGuidePoints);
    sm.setSnapEnabled(core::sketch::SnapType::Endpoint, settings.sketchGuidePoints);
    sm.setSnapEnabled(core::sketch::SnapType::Midpoint, settings.sketchGuidePoints);
    sm.setSnapEnabled(core::sketch::SnapType::Center, settings.sketchGuidePoints);
    sm.setSnapEnabled(core::sketch::SnapType::Quadrant, settings.sketchGuidePoints);
    sm.setSnapEnabled(core::sketch::SnapType::Intersection, settings.sketchGuidePoints);

    sm.setSnapEnabled(core::sketch::SnapType::ActiveLayer3D, settings.activeLayer3DPoints || settings.activeLayer3DEdges);

    sm.setShowGuidePoints(settings.showGuidePoints);
    sm.setShowSnappingHints(settings.showSnappingHints);
    syncSnapGridSizeFromCamera();

    update();
}

void Viewport::updateSnapGeometry() {
    if (!m_inSketchMode || !m_activeSketch || !m_toolManager || !m_document) return;

    std::vector<core::sketch::Vec2d> points;
    std::vector<std::pair<core::sketch::Vec2d, core::sketch::Vec2d>> lines;

    const auto& plane = m_activeSketch->getPlane();

    auto bodyIds = m_document->getBodyIds();
    for (const auto& bodyId : bodyIds) {
        const auto* body = m_document->getBodyShape(bodyId);
        if (!body || !m_document->isBodyVisible(bodyId)) continue;

        TopExp_Explorer exV;
        for (exV.Init(*body, TopAbs_VERTEX); exV.More(); exV.Next()) {
            const TopoDS_Vertex& v = TopoDS::Vertex(exV.Current());
            gp_Pnt p = BRep_Tool::Pnt(v);
            core::sketch::Vec3d p3d{p.X(), p.Y(), p.Z()};
            points.push_back(plane.toSketch(p3d));
        }

        TopExp_Explorer exE;
        for (exE.Init(*body, TopAbs_EDGE); exE.More(); exE.Next()) {
             const TopoDS_Edge& e = TopoDS::Edge(exE.Current());
             (void)e;
        }
    }

    m_toolManager->snapManager().setExternalGeometry(points, lines);
}

} // namespace ui
} // namespace onecad
