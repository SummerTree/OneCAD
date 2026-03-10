#include "MeasureTool.h"
#include "../viewport/Viewport.h"
#include "../../app/document/Document.h"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <BRep_Tool.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopExp_Explorer.hxx>

#include <cmath>

namespace onecad::ui::tools {

MeasureTool::MeasureTool(Viewport* viewport, QObject* parent)
    : QObject(parent)
    , m_viewport(viewport) {
}

void MeasureTool::setDocument(app::Document* document) {
    m_document = document;
}

void MeasureTool::setMode(Mode mode) {
    m_mode = mode;
    m_firstPoint.reset();
}

void MeasureTool::activate() {
    m_active = true;
    m_firstPoint.reset();
}

void MeasureTool::deactivate() {
    m_active = false;
    m_firstPoint.reset();
}

bool MeasureTool::handleMousePress(const QPoint& /*screenPos*/, Qt::MouseButton button) {
    if (!m_active || button != Qt::LeftButton) {
        return false;
    }

    // TODO: Integrate with selection/picking to get 3D hit points
    // For now, measurement relies on selection-based workflow
    return false;
}

bool MeasureTool::handleMouseMove(const QPoint& /*screenPos*/) {
    if (!m_active) {
        return false;
    }
    return false;
}

void MeasureTool::clearMeasurements() {
    m_measurements.clear();
    emit measurementsCleared();
}

double MeasureTool::computeEdgeLength(const TopoDS_Shape& edge) {
    if (edge.IsNull() || edge.ShapeType() != TopAbs_EDGE) {
        return 0.0;
    }
    BRepAdaptor_Curve curve(TopoDS::Edge(edge));
    return GCPnts_AbscissaPoint::Length(curve);
}

double MeasureTool::computeFaceArea(const TopoDS_Shape& face) {
    if (face.IsNull() || face.ShapeType() != TopAbs_FACE) {
        return 0.0;
    }
    GProp_GProps props;
    BRepGProp::SurfaceProperties(face, props);
    return props.Mass();
}

QVector3D MeasureTool::computeFaceCentroid(const TopoDS_Shape& face) {
    if (face.IsNull() || face.ShapeType() != TopAbs_FACE) {
        return {};
    }
    GProp_GProps props;
    BRepGProp::SurfaceProperties(face, props);
    gp_Pnt center = props.CentreOfMass();
    return {static_cast<float>(center.X()),
            static_cast<float>(center.Y()),
            static_cast<float>(center.Z())};
}

} // namespace onecad::ui::tools
