#include "app/document/Document.h"
#include "core/modeling/SelectionTopologyResolver.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Tool.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <ShapeUpgrade_ShapeDivideClosed.hxx>
#include <ShapeUpgrade_ShapeDivideClosedEdges.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

#include <QCoreApplication>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>
#include <unordered_set>
#include <vector>

namespace {

using onecad::kernel::elementmap::ElementId;
using onecad::kernel::elementmap::ElementKind;

TopoDS_Shape makeSplitCylinder() {
    TopoDS_Shape shape = BRepPrimAPI_MakeCylinder(1.0, 2.0).Shape();

    ShapeUpgrade_ShapeDivideClosed divideFaces(shape);
    divideFaces.SetNbSplitPoints(1);
    divideFaces.Perform();
    TopoDS_Shape splitFaces = divideFaces.Result();
    if (!splitFaces.IsNull()) {
        shape = splitFaces;
    }

    ShapeUpgrade_ShapeDivideClosedEdges divideEdges(shape);
    divideEdges.SetNbSplitPoints(1);
    divideEdges.Perform();
    TopoDS_Shape splitEdges = divideEdges.Result();
    if (!splitEdges.IsNull()) {
        shape = splitEdges;
    }

    return shape;
}

std::optional<std::string> firstIdForShapeOfKind(const onecad::kernel::elementmap::ElementMap& elementMap,
                                                 const TopoDS_Shape& shape,
                                                 ElementKind kind) {
    std::vector<std::string> ids;
    for (const auto& id : elementMap.findIdsByShape(shape)) {
        const auto* entry = elementMap.find(id);
        if (!entry || entry->kind != kind || entry->shape.IsNull()) {
            continue;
        }
        ids.push_back(id.value);
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    if (ids.empty()) {
        return std::nullopt;
    }
    return ids.front();
}

gp_Pnt curveMidpoint(const TopoDS_Edge& edge) {
    BRepAdaptor_Curve curve(edge);
    const double first = curve.FirstParameter();
    const double last = curve.LastParameter();
    return curve.Value(0.5 * (first + last));
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    onecad::app::Document document;
    const TopoDS_Shape shape = makeSplitCylinder();
    const std::string bodyId = document.addBody(shape);
    if (bodyId.empty()) {
        std::cerr << "Failed to add split cylinder body.\n";
        return 1;
    }

    const TopoDS_Shape* bodyShape = document.getBodyShape(bodyId);
    if (!bodyShape || bodyShape->IsNull()) {
        std::cerr << "Body shape missing.\n";
        return 1;
    }

    const auto topology = onecad::core::modeling::SelectionTopologyResolver::resolve(
        *bodyShape, document.elementMap());

    std::vector<std::string> cylindricalFaceIds;
    std::vector<std::string> planarFaceIds;
    for (TopExp_Explorer faceExp(*bodyShape, TopAbs_FACE); faceExp.More(); faceExp.Next()) {
        const TopoDS_Face face = TopoDS::Face(faceExp.Current());
        const auto faceId = firstIdForShapeOfKind(document.elementMap(), face, ElementKind::Face);
        if (!faceId.has_value()) {
            continue;
        }
        BRepAdaptor_Surface surface(face, true);
        if (surface.GetType() == GeomAbs_Cylinder) {
            cylindricalFaceIds.push_back(*faceId);
        } else if (surface.GetType() == GeomAbs_Plane) {
            planarFaceIds.push_back(*faceId);
        }
    }

    if (cylindricalFaceIds.size() < 2) {
        std::cerr << "Expected split cylindrical side faces.\n";
        return 1;
    }

    const std::string cylinderLeader = topology.faceLeaderByFaceId.at(cylindricalFaceIds.front());
    for (const auto& faceId : cylindricalFaceIds) {
        if (topology.faceLeaderByFaceId.at(faceId) != cylinderLeader) {
            std::cerr << "Cylindrical side faces were not promoted into one selectable face.\n";
            return 1;
        }
    }
    for (const auto& faceId : planarFaceIds) {
        if (topology.faceLeaderByFaceId.at(faceId) == cylinderLeader) {
            std::cerr << "Face promotion crossed a sharp cap boundary.\n";
            return 1;
        }
    }

    std::vector<std::string> topCircleEdgeIds;
    std::unordered_set<std::string> topCircleVertexIds;
    for (TopExp_Explorer edgeExp(*bodyShape, TopAbs_EDGE); edgeExp.More(); edgeExp.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(edgeExp.Current());
        const auto edgeId = firstIdForShapeOfKind(document.elementMap(), edge, ElementKind::Edge);
        if (!edgeId.has_value()) {
            continue;
        }
        BRepAdaptor_Curve curve(edge);
        if (curve.GetType() != GeomAbs_Circle) {
            continue;
        }
        const gp_Pnt midpoint = curveMidpoint(edge);
        if (std::abs(midpoint.Z() - 2.0) > 1e-5) {
            continue;
        }
        topCircleEdgeIds.push_back(*edgeId);

        TopoDS_Vertex firstVertex;
        TopoDS_Vertex lastVertex;
        TopExp::Vertices(edge, firstVertex, lastVertex);
        for (const auto& vertex : {firstVertex, lastVertex}) {
            if (vertex.IsNull()) {
                continue;
            }
            const auto vertexId = firstIdForShapeOfKind(document.elementMap(), vertex, ElementKind::Vertex);
            if (vertexId.has_value()) {
                topCircleVertexIds.insert(*vertexId);
            }
        }
    }

    if (topCircleEdgeIds.size() < 2) {
        std::cerr << "Expected split top circular boundary edges.\n";
        return 1;
    }

    const std::string topCircleLeader = topology.edgeLeaderByEdgeId.at(topCircleEdgeIds.front());
    for (const auto& edgeId : topCircleEdgeIds) {
        if (topology.edgeLeaderByEdgeId.at(edgeId) != topCircleLeader) {
            std::cerr << "Top circular boundary was not promoted into one selectable edge.\n";
            return 1;
        }
    }

    if (!topology.edgeGroupClosedByLeader.contains(topCircleLeader) ||
        !topology.edgeGroupClosedByLeader.at(topCircleLeader)) {
        std::cerr << "Promoted top circular edge should be marked closed.\n";
        return 1;
    }

    if (topCircleVertexIds.empty()) {
        std::cerr << "Expected split vertices on the top circular boundary.\n";
        return 1;
    }
    for (const auto& vertexId : topCircleVertexIds) {
        if (!topology.suppressedVertexIds.contains(vertexId)) {
            std::cerr << "Split top-circle vertex was not suppressed: " << vertexId << "\n";
            return 1;
        }
    }

    std::cout << "Selection topology resolver prototype passed.\n";
    return 0;
}
