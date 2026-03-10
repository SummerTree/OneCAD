#include "app/document/Document.h"
#include "app/selection/SelectionTypes.h"
#include "core/modeling/SelectionTopologyResolver.h"
#include "ui/selection/ModelPickerAdapter.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <ShapeUpgrade_ShapeDivideClosed.hxx>
#include <ShapeUpgrade_ShapeDivideClosedEdges.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

#include <QCoreApplication>
#include <QMatrix4x4>
#include <QVector4D>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>
#include <vector>

namespace {

using onecad::app::selection::SelectionKind;
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

gp_Pnt faceMidpoint(const TopoDS_Face& face) {
    BRepAdaptor_Surface surface(face, true);
    const double uMid = 0.5 * (surface.FirstUParameter() + surface.LastUParameter());
    const double vMid = 0.5 * (surface.FirstVParameter() + surface.LastVParameter());
    return surface.Value(uMid, vMid);
}

gp_Pnt curveMidpoint(const TopoDS_Edge& edge) {
    BRepAdaptor_Curve curve(edge);
    const double first = curve.FirstParameter();
    const double last = curve.LastParameter();
    return curve.Value(0.5 * (first + last));
}

QPoint projectToScreen(const QMatrix4x4& viewProjection,
                       const QVector3D& point,
                       const QSize& viewport) {
    const QVector4D clip = viewProjection * QVector4D(point, 1.0f);
    const QVector3D ndc = clip.toVector3D() / clip.w();
    const float x = (ndc.x() * 0.5f + 0.5f) * viewport.width();
    const float y = (1.0f - (ndc.y() * 0.5f + 0.5f)) * viewport.height();
    return QPoint(static_cast<int>(std::lround(x)), static_cast<int>(std::lround(y)));
}

std::optional<onecad::app::selection::SelectionItem> firstHitOfKind(
    const onecad::app::selection::PickResult& result,
    SelectionKind kind) {
    for (const auto& hit : result.hits) {
        if (hit.kind == kind) {
            return hit;
        }
    }
    return std::nullopt;
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
    const auto* mesh = document.meshStore().findMesh(bodyId);
    if (!bodyShape || bodyShape->IsNull() || !mesh) {
        std::cerr << "Body mesh missing.\n";
        return 1;
    }

    const auto promotedTopology = onecad::core::modeling::SelectionTopologyResolver::resolve(
        *bodyShape, document.elementMap());

    onecad::ui::selection::ModelPickerAdapter picker;
    onecad::ui::selection::ModelPickerAdapter::Mesh pickMesh;
    pickMesh.bodyId = mesh->bodyId;
    pickMesh.vertices = mesh->vertices;
    pickMesh.faceGroupByFaceId = promotedTopology.faceLeaderByFaceId;
    pickMesh.edgeGroupByEdgeId = promotedTopology.edgeLeaderByEdgeId;
    pickMesh.suppressedVertexIds = promotedTopology.suppressedVertexIds;

    pickMesh.triangles.reserve(mesh->triangles.size());
    for (const auto& tri : mesh->triangles) {
        pickMesh.triangles.push_back({tri.i0, tri.i1, tri.i2, tri.faceId});
    }
    for (const auto& [faceId, topo] : mesh->topologyByFace) {
        onecad::ui::selection::ModelPickerAdapter::FaceTopology faceTopo;
        for (const auto& edge : topo.edges) {
            faceTopo.edges.push_back({edge.edgeId, edge.points});
        }
        for (const auto& vertex : topo.vertices) {
            faceTopo.vertices.push_back({vertex.vertexId, vertex.position});
        }
        pickMesh.topologyByFace[faceId] = std::move(faceTopo);
    }
    picker.setMeshes({std::move(pickMesh)});

    QSize viewport(800, 600);
    QMatrix4x4 projection;
    projection.perspective(35.0f, static_cast<float>(viewport.width()) / viewport.height(), 0.1f, 100.0f);
    QMatrix4x4 view;
    view.lookAt(QVector3D(0.0f, -5.0f, 3.0f), QVector3D(0.0f, 0.0f, 1.0f), QVector3D(0.0f, 0.0f, 1.0f));
    const QMatrix4x4 viewProjection = projection * view;

    std::vector<std::string> cylindricalFaceIds;
    TopoDS_Face seedCylinderFace;
    std::vector<std::string> topCircleEdgeIds;
    TopoDS_Edge seedTopEdge;
    TopoDS_Vertex seedTopVertex;

    for (TopExp_Explorer faceExp(*bodyShape, TopAbs_FACE); faceExp.More(); faceExp.Next()) {
        const TopoDS_Face face = TopoDS::Face(faceExp.Current());
        BRepAdaptor_Surface surface(face, true);
        if (surface.GetType() != GeomAbs_Cylinder) {
            continue;
        }
        const auto faceId = firstIdForShapeOfKind(document.elementMap(), face, ElementKind::Face);
        if (!faceId.has_value()) {
            continue;
        }
        cylindricalFaceIds.push_back(*faceId);
        if (seedCylinderFace.IsNull()) {
            seedCylinderFace = face;
        }
    }

    for (TopExp_Explorer edgeExp(*bodyShape, TopAbs_EDGE); edgeExp.More(); edgeExp.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(edgeExp.Current());
        BRepAdaptor_Curve curve(edge);
        if (curve.GetType() != GeomAbs_Circle) {
            continue;
        }
        const gp_Pnt midpoint = curveMidpoint(edge);
        if (std::abs(midpoint.Z() - 2.0) > 1e-5) {
            continue;
        }
        const auto edgeId = firstIdForShapeOfKind(document.elementMap(), edge, ElementKind::Edge);
        if (!edgeId.has_value()) {
            continue;
        }
        topCircleEdgeIds.push_back(*edgeId);
        if (seedTopEdge.IsNull()) {
            seedTopEdge = edge;
            TopoDS_Vertex firstVertex;
            TopoDS_Vertex lastVertex;
            TopExp::Vertices(edge, firstVertex, lastVertex);
            seedTopVertex = firstVertex.IsNull() ? lastVertex : firstVertex;
        }
    }

    if (cylindricalFaceIds.size() < 2 || seedCylinderFace.IsNull() ||
        topCircleEdgeIds.size() < 2 || seedTopEdge.IsNull() || seedTopVertex.IsNull()) {
        std::cerr << "Failed to collect split cylinder topology for picker prototype.\n";
        return 1;
    }

    const std::string promotedCylinderFaceId =
        promotedTopology.faceLeaderByFaceId.at(cylindricalFaceIds.front());
    const std::string promotedTopEdgeId =
        promotedTopology.edgeLeaderByEdgeId.at(topCircleEdgeIds.front());

    const gp_Pnt facePoint = faceMidpoint(seedCylinderFace);
    const auto facePick = picker.pick(
        projectToScreen(viewProjection,
                        QVector3D(static_cast<float>(facePoint.X()),
                                  static_cast<float>(facePoint.Y()),
                                  static_cast<float>(facePoint.Z())),
                        viewport),
        8.0,
        viewProjection,
        viewport);
    const auto pickedFace = firstHitOfKind(facePick, SelectionKind::Face);
    if (!pickedFace.has_value() || pickedFace->id.elementId != promotedCylinderFaceId) {
        std::cerr << "Expected promoted cylindrical face selection.\n";
        return 1;
    }

    std::vector<std::array<QVector3D, 3>> promotedFaceTriangles;
    if (!picker.getFaceTriangles(bodyId, promotedCylinderFaceId, promotedFaceTriangles)) {
        std::cerr << "Failed to fetch promoted cylindrical face triangles.\n";
        return 1;
    }
    std::size_t expectedCylinderTriangleCount = 0;
    for (const auto& tri : mesh->triangles) {
        if (std::find(cylindricalFaceIds.begin(), cylindricalFaceIds.end(), tri.faceId) !=
            cylindricalFaceIds.end()) {
            expectedCylinderTriangleCount++;
        }
    }
    if (promotedFaceTriangles.size() != expectedCylinderTriangleCount) {
        std::cerr << "Promoted cylindrical face highlight does not cover all member triangles.\n";
        return 1;
    }

    const gp_Pnt edgePoint = curveMidpoint(seedTopEdge);
    const auto edgePick = picker.pick(
        projectToScreen(viewProjection,
                        QVector3D(static_cast<float>(edgePoint.X()),
                                  static_cast<float>(edgePoint.Y()),
                                  static_cast<float>(edgePoint.Z())),
                        viewport),
        10.0,
        viewProjection,
        viewport);
    const auto pickedEdge = firstHitOfKind(edgePick, SelectionKind::Edge);
    if (!pickedEdge.has_value() || pickedEdge->id.elementId != promotedTopEdgeId) {
        std::cerr << "Expected promoted top circular edge selection.\n";
        return 1;
    }

    std::vector<std::vector<QVector3D>> promotedEdgePolylines;
    if (!picker.getEdgePolylines(bodyId, promotedTopEdgeId, promotedEdgePolylines) ||
        promotedEdgePolylines.size() < 2) {
        std::cerr << "Promoted top circular edge did not return all member segments.\n";
        return 1;
    }

    const gp_Pnt vertexPoint = BRep_Tool::Pnt(seedTopVertex);
    const auto vertexPick = picker.pick(
        projectToScreen(viewProjection,
                        QVector3D(static_cast<float>(vertexPoint.X()),
                                  static_cast<float>(vertexPoint.Y()),
                                  static_cast<float>(vertexPoint.Z())),
                        viewport),
        10.0,
        viewProjection,
        viewport);
    if (firstHitOfKind(vertexPick, SelectionKind::Vertex).has_value()) {
        std::cerr << "Internal split vertex on promoted top edge should not be selectable.\n";
        return 1;
    }
    const auto pickedEdgeAtVertex = firstHitOfKind(vertexPick, SelectionKind::Edge);
    if (!pickedEdgeAtVertex.has_value() || pickedEdgeAtVertex->id.elementId != promotedTopEdgeId) {
        std::cerr << "Clicking the split point should promote to the full top circular edge.\n";
        return 1;
    }

    std::cout << "Pick topology promotion prototype passed.\n";
    return 0;
}
