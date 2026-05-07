/**
 * @file LinearPatternTool.cpp
 */
#include "LinearPatternTool.h"

#include "PatternOptionsOverlay.h"

#include "../viewport/Viewport.h"
#include "../../app/commands/AddOperationCommand.h"
#include "../../app/commands/CommandProcessor.h"
#include "../../app/document/Document.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <gp.hxx>
#include <QUuid>
#include <QLoggingCategory>
#include <QMatrix4x4>
#include <QObject>
#include <QVector2D>
#include <QVector4D>
#include <QString>
#include <cmath>

namespace onecad::ui::tools {

Q_LOGGING_CATEGORY(logLinearPatternTool, "onecad.ui.tools.linearpattern")

namespace {
constexpr double kMinSpacingMagnitude = 1e-4;

std::optional<QPointF> projectPoint(const gp_Pnt& point,
                                    const QMatrix4x4& viewProjection,
                                    const QSize& viewportSize) {
    QVector4D clip = viewProjection * QVector4D(point.X(), point.Y(), point.Z(), 1.0f);
    if (std::abs(clip.w()) < 1e-8f) {
        return std::nullopt;
    }

    const float ndcX = clip.x() / clip.w();
    const float ndcY = clip.y() / clip.w();
    const float sx = (ndcX * 0.5f + 0.5f) * static_cast<float>(viewportSize.width());
    const float sy = (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(viewportSize.height());
    return QPointF(sx, sy);
}

QString axisLabel(const gp_Dir& dir) {
    return QStringLiteral("(%1, %2, %3)")
        .arg(dir.X(), 0, 'f', 2)
        .arg(dir.Y(), 0, 'f', 2)
        .arg(dir.Z(), 0, 'f', 2);
}
} // namespace

LinearPatternTool::LinearPatternTool(Viewport* viewport, app::Document* document)
    : viewport_(viewport)
    , document_(document) {
}

LinearPatternTool::~LinearPatternTool() = default;

void LinearPatternTool::setDocument(app::Document* document) {
    document_ = document;
}

void LinearPatternTool::setCommandProcessor(app::commands::CommandProcessor* processor) {
    commandProcessor_ = processor;
}

void LinearPatternTool::begin(const app::selection::SelectionItem& selection) {
    qCDebug(logLinearPatternTool) << "begin"
                                  << "selectionKind=" << static_cast<int>(selection.kind)
                                  << "ownerId=" << QString::fromStdString(selection.id.ownerId)
                                  << "elementId=" << QString::fromStdString(selection.id.elementId);
    cancel();
    ensureOverlay();

    if (!prepareSource(selection)) {
        qCWarning(logLinearPatternTool) << "begin:prepare-source-failed";
        cancel();
        return;
    }

    active_ = true;
    if (selection.kind == app::selection::SelectionKind::Edge ||
        selection.kind == app::selection::SelectionKind::Face) {
        if (!trySetDirectionFromSelection(selection)) {
            state_ = State::WaitingForDirection;
        }
    }

    if (state_ != State::Ready) {
        state_ = State::WaitingForDirection;
    }

    updateOverlay();
    if (overlay_) {
        overlay_->show();
        overlay_->raise();
    }
}

void LinearPatternTool::cancel() {
    clearPreview();
    resetState();
    if (overlay_) {
        overlay_->hide();
    }
}

bool LinearPatternTool::handleMousePress(const QPoint& screenPos, Qt::MouseButton button) {
    if (!active_ || button != Qt::LeftButton) {
        return false;
    }
    if (state_ != State::Ready) {
        return false;
    }
    if (!viewport_ || !viewport_->isMouseOverIndicator(screenPos)) {
        return false;
    }

    dragging_ = true;
    state_ = State::Dragging;
    dragStart_ = screenPos;
    dragStartSpacing_ = currentSpacing_;
    return true;
}

bool LinearPatternTool::handleMouseMove(const QPoint& screenPos) {
    if (!active_ || !dragging_) {
        return false;
    }

    currentSpacing_ = computeDraggedSpacing(screenPos);
    updateOverlay();
    updatePreview();
    return true;
}

bool LinearPatternTool::handleMouseRelease(const QPoint& /*screenPos*/, Qt::MouseButton button) {
    if (!active_ || !dragging_ || button != Qt::LeftButton) {
        return false;
    }

    dragging_ = false;
    state_ = State::Ready;
    updateOverlay();
    updatePreview();
    return true;
}

bool LinearPatternTool::confirm() {
    if (!active_ || !document_ || !directionValid_ || sourceBodyId_.empty()) {
        return false;
    }
    if (instanceCount_ < 2 || std::abs(currentSpacing_) < kMinSpacingMagnitude) {
        return false;
    }

    app::OperationRecord record;
    record.opId = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    record.type = app::OperationType::LinearPattern;
    record.input = app::BodyRef{sourceBodyId_};

    app::LinearPatternParams params;
    params.sourceBodyId = sourceBodyId_;
    params.dirX = direction_.X();
    params.dirY = direction_.Y();
    params.dirZ = direction_.Z();
    params.spacing = currentSpacing_;
    params.count = instanceCount_;
    params.fuseResult = fuseResult_;
    record.params = params;
    record.resultBodyIds.push_back(sourceBodyId_);

    auto command = std::make_unique<app::commands::AddOperationCommand>(document_, record);
    const bool success = commandProcessor_
        ? commandProcessor_->execute(std::move(command))
        : command->execute();
    if (!success) {
        qCWarning(logLinearPatternTool) << "confirm:add-operation-failed"
                                        << "sourceBodyId=" << QString::fromStdString(sourceBodyId_);
        return false;
    }

    cancel();
    return true;
}

std::optional<ModelingTool::Indicator> LinearPatternTool::indicator() const {
    if (!active_ || !directionValid_) {
        return std::nullopt;
    }

    Indicator indicator;
    indicator.origin = QVector3D(directionOrigin_.X(), directionOrigin_.Y(), directionOrigin_.Z());
    indicator.direction = QVector3D(direction_.X(), direction_.Y(), direction_.Z());
    indicator.distance = currentSpacing_;
    indicator.showDistance = std::abs(currentSpacing_) >= kMinSpacingMagnitude;
    indicator.isDoubleSided = true;
    indicator.booleanMode = app::BooleanMode::NewBody;
    return indicator;
}

void LinearPatternTool::onSelectionChanged(const std::vector<app::selection::SelectionItem>& selection) {
    if (!active_ || selection.empty()) {
        return;
    }
    if (state_ != State::WaitingForDirection) {
        return;
    }

    if (trySetDirectionFromSelection(selection.front())) {
        state_ = State::Ready;
        updateOverlay();
        updatePreview();
    }
}

void LinearPatternTool::resetState() {
    active_ = false;
    dragging_ = false;
    state_ = State::WaitingForSource;
    sourceSelection_ = {};
    directionSelection_ = {};
    sourceBodyId_.clear();
    sourceShape_.Nullify();
    directionOrigin_ = gp_Pnt();
    direction_ = gp::DZ();
    directionValid_ = false;
    directionSummary_.clear();
    dragStart_ = QPoint();
    dragStartSpacing_ = 10.0;
    currentSpacing_ = 10.0;
    instanceCount_ = 2;
    fuseResult_ = true;
}

bool LinearPatternTool::prepareSource(const app::selection::SelectionItem& selection) {
    if (!document_) {
        return false;
    }

    std::string bodyId;
    switch (selection.kind) {
        case app::selection::SelectionKind::Body:
            bodyId = !selection.id.elementId.empty() ? selection.id.elementId : selection.id.ownerId;
            break;
        case app::selection::SelectionKind::Edge:
        case app::selection::SelectionKind::Face:
            bodyId = selection.id.ownerId;
            break;
        default:
            return false;
    }

    const TopoDS_Shape* shape = document_->getBodyShape(bodyId);
    if (!shape || shape->IsNull()) {
        return false;
    }

    sourceSelection_ = selection;
    sourceBodyId_ = bodyId;
    sourceShape_ = *shape;
    currentSpacing_ = 10.0;
    instanceCount_ = 2;
    fuseResult_ = true;
    return true;
}

bool LinearPatternTool::trySetDirectionFromSelection(const app::selection::SelectionItem& selection) {
    if (sourceBodyId_.empty()) {
        return false;
    }

    gp_Pnt origin;
    gp_Dir dir;
    QString summary;
    bool ok = false;

    if (selection.kind == app::selection::SelectionKind::Edge &&
        selection.id.ownerId == sourceBodyId_) {
        ok = extractEdgeDirection(selection, &origin, &dir, &summary);
    } else if (selection.kind == app::selection::SelectionKind::Face &&
               selection.id.ownerId == sourceBodyId_) {
        ok = extractFaceDirection(selection, &origin, &dir, &summary);
    }

    if (!ok) {
        return false;
    }

    directionSelection_ = selection;
    directionOrigin_ = origin;
    direction_ = dir;
    directionValid_ = true;
    directionSummary_ = summary;
    return true;
}

bool LinearPatternTool::extractEdgeDirection(const app::selection::SelectionItem& selection,
                                             gp_Pnt* outOrigin,
                                             gp_Dir* outDirection,
                                             QString* outSummary) const {
    if (!document_) {
        return false;
    }
    const auto* entry = document_->elementMap().find(
        kernel::elementmap::ElementId::From(selection.id.elementId));
    if (!entry || entry->shape.IsNull()) {
        return false;
    }

    TopoDS_Edge edge = TopoDS::Edge(entry->shape);
    BRepAdaptor_Curve curve(edge);
    const double first = curve.FirstParameter();
    const double last = curve.LastParameter();
    if (!std::isfinite(first) || !std::isfinite(last) || std::abs(last - first) < 1e-9) {
        return false;
    }

    const gp_Pnt p0 = curve.Value(first);
    const gp_Pnt p1 = curve.Value(last);
    gp_Vec vec(p0, p1);
    if (vec.Magnitude() < 1e-9) {
        return false;
    }
    vec.Normalize();

    if (outOrigin) {
        outOrigin->SetXYZ((p0.XYZ() + p1.XYZ()) * 0.5);
    }
    if (outDirection) {
        *outDirection = gp_Dir(vec);
    }
    if (outSummary) {
        *outSummary = QObject::tr("Edge tangent %1").arg(axisLabel(gp_Dir(vec)));
    }
    return true;
}

bool LinearPatternTool::extractFaceDirection(const app::selection::SelectionItem& selection,
                                             gp_Pnt* outOrigin,
                                             gp_Dir* outDirection,
                                             QString* outSummary) const {
    if (!document_) {
        return false;
    }
    const auto* entry = document_->elementMap().find(
        kernel::elementmap::ElementId::From(selection.id.elementId));
    if (!entry || entry->shape.IsNull()) {
        return false;
    }

    TopoDS_Face face = TopoDS::Face(entry->shape);
    BRepAdaptor_Surface surface(face, true);
    if (surface.GetType() != GeomAbs_Plane) {
        return false;
    }

    gp_Dir normal = surface.Plane().Axis().Direction();
    if (face.Orientation() == TopAbs_REVERSED) {
        normal.Reverse();
    }

    GProp_GProps props;
    BRepGProp::SurfaceProperties(face, props);
    if (outOrigin) {
        *outOrigin = props.CentreOfMass();
    }
    if (outDirection) {
        *outDirection = normal;
    }
    if (outSummary) {
        *outSummary = QObject::tr("Face normal %1").arg(axisLabel(normal));
    }
    return true;
}

void LinearPatternTool::ensureOverlay() {
    if (overlay_ || !viewport_) {
        return;
    }

    auto overlay = std::make_unique<PatternOptionsOverlay>(viewport_);
    overlay->move(16, 16);
    QObject::connect(overlay.get(), &PatternOptionsOverlay::spacingChanged, viewport_,
            [this](double spacing) {
                currentSpacing_ = spacing;
                updatePreview();
            });
    QObject::connect(overlay.get(), &PatternOptionsOverlay::countChanged, viewport_,
            [this](int count) {
                instanceCount_ = count;
                updatePreview();
            });
    QObject::connect(overlay.get(), &PatternOptionsOverlay::fuseChanged, viewport_,
            [this](bool fuse) {
                fuseResult_ = fuse;
                updatePreview();
            });
    overlay_ = std::move(overlay);
}

void LinearPatternTool::updateOverlay() {
    if (!overlay_ || !document_) {
        return;
    }

    QString sourceName = QString::fromStdString(document_->getBodyName(sourceBodyId_));
    if (sourceName.isEmpty()) {
        sourceName = QString::fromStdString(sourceBodyId_);
    }
    overlay_->setSourceLabel(sourceName);
    overlay_->setDirectionLabel(directionValid_ ? directionSummary_ : QObject::tr("Not set"));
    overlay_->setSpacingValue(currentSpacing_);
    overlay_->setCountValue(instanceCount_);
    overlay_->setFuseResult(fuseResult_);
    overlay_->setDirectionReady(directionValid_);
}

void LinearPatternTool::updatePreview() {
    if (!viewport_) {
        return;
    }
    if (!active_ || !directionValid_ || sourceBodyId_.empty() ||
        instanceCount_ < 2 || std::abs(currentSpacing_) < kMinSpacingMagnitude) {
        clearPreview();
        return;
    }

    viewport_->setPreviewHiddenBody(sourceBodyId_);
    viewport_->setModelPreviewMeshes(buildPreviewMeshes());
}

void LinearPatternTool::clearPreview() {
    if (viewport_) {
        viewport_->clearModelPreviewMeshes();
        viewport_->clearPreviewHiddenBody();
    }
}

double LinearPatternTool::computeDraggedSpacing(const QPoint& screenPos) const {
    if (!viewport_ || !directionValid_) {
        return currentSpacing_;
    }

    const QMatrix4x4 viewProjection = viewport_->currentViewProjection();
    const QSize size = viewport_->currentViewportSize();
    const gp_Pnt startPoint = directionOrigin_;
    const gp_Pnt endPoint(directionOrigin_.X() + direction_.X(),
                          directionOrigin_.Y() + direction_.Y(),
                          directionOrigin_.Z() + direction_.Z());
    const auto screenStart = projectPoint(startPoint, viewProjection, size);
    const auto screenEnd = projectPoint(endPoint, viewProjection, size);
    if (screenStart.has_value() && screenEnd.has_value()) {
        QVector2D axis(static_cast<float>(screenEnd->x() - screenStart->x()),
                       static_cast<float>(screenEnd->y() - screenStart->y()));
        const float axisLength = axis.length();
        if (axisLength > 2.0f) {
            axis /= axisLength;
            const QVector2D delta(static_cast<float>(screenPos.x() - dragStart_.x()),
                                  static_cast<float>(screenPos.y() - dragStart_.y()));
            const double deltaPixels = QVector2D::dotProduct(delta, axis);
            return dragStartSpacing_ + (deltaPixels / axisLength);
        }
    }

    return dragStartSpacing_ - static_cast<double>(screenPos.y() - dragStart_.y()) * viewport_->pixelScale();
}

std::vector<render::SceneMeshStore::Mesh> LinearPatternTool::buildPreviewMeshes() const {
    std::vector<render::SceneMeshStore::Mesh> meshes;
    if (sourceShape_.IsNull() || instanceCount_ < 2) {
        return meshes;
    }

    previewElementMap_.clear();
    meshes.reserve(static_cast<std::size_t>(instanceCount_));

    for (int i = 0; i < instanceCount_; ++i) {
        TopoDS_Shape instanceShape = sourceShape_;
        if (i > 0) {
            gp_Trsf transform;
            transform.SetTranslation(gp_Vec(direction_.X() * currentSpacing_ * i,
                                            direction_.Y() * currentSpacing_ * i,
                                            direction_.Z() * currentSpacing_ * i));
            BRepBuilderAPI_Transform xform(sourceShape_, transform, true);
            if (!xform.IsDone()) {
                continue;
            }
            instanceShape = xform.Shape();
        }
        render::SceneMeshStore::Mesh mesh;
        std::string meshError;
        const std::string previewId = QStringLiteral("pattern_preview_%1").arg(i).toStdString();
        if (!previewTessellator_.tryBuildMesh(previewId,
                                              instanceShape,
                                              previewElementMap_,
                                              mesh,
                                              nullptr,
                                              &meshError)) {
            qCWarning(logLinearPatternTool) << "buildPreviewMeshes:tessellation-failed"
                                            << "previewId=" << QString::fromStdString(previewId)
                                            << "error=" << QString::fromStdString(meshError);
            continue;
        }
        meshes.push_back(std::move(mesh));
    }
    return meshes;
}

} // namespace onecad::ui::tools
