#include "TessellationCache.h"

#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepLProp_SLProps.hxx>
#include <Poly_Triangulation.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <Geom2d_Curve.hxx>
#include <GeomAbs_Shape.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <GCPnts_UniformAbscissa.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopExp.hxx>
#include <TopLoc_Location.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>

#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace {

constexpr double kSmoothEdgeAngleDeg = 5.0;
constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
const double kSmoothEdgeCos = std::cos(kSmoothEdgeAngleDeg * kDegToRad);

bool sampleFaceNormal(const TopoDS_Edge& edge, const TopoDS_Face& face, gp_Dir* outNormal) {
    if (!outNormal) {
        return false;
    }
    Standard_Real first = 0.0;
    Standard_Real last = 0.0;
    Handle(Geom2d_Curve) curve2d = BRep_Tool::CurveOnSurface(edge, face, first, last);
    if (curve2d.IsNull()) {
        return false;
    }
    Standard_Real mid = 0.5 * (first + last);
    gp_Pnt2d uv;
    gp_Vec2d d1;
    curve2d->D1(mid, uv, d1);

    BRepAdaptor_Surface surface(face, true);
    BRepLProp_SLProps props(surface, uv.X(), uv.Y(), 1, Precision::Confusion());
    if (!props.IsNormalDefined()) {
        return false;
    }
    gp_Dir normal = props.Normal();
    if (face.Orientation() == TopAbs_REVERSED) {
        normal.Reverse();
    }
    *outNormal = normal;
    return true;
}

bool isSharpEdgeByAngle(const TopoDS_Edge& edge, const TopoDS_Face& f1, const TopoDS_Face& f2) {
    gp_Dir n1;
    gp_Dir n2;
    if (!sampleFaceNormal(edge, f1, &n1) || !sampleFaceNormal(edge, f2, &n2)) {
        return true;
    }
    double dot = std::abs(n1.Dot(n2));
    return dot < kSmoothEdgeCos;
}

bool isVisibleEdge(const TopoDS_Edge& edge, const TopTools_ListOfShape& faces) {
    std::vector<TopoDS_Face> adjacentFaces;
    adjacentFaces.reserve(static_cast<size_t>(faces.Extent()));
    for (TopTools_ListIteratorOfListOfShape it(faces); it.More(); it.Next()) {
        TopoDS_Face face = TopoDS::Face(it.Value());
        if (!face.IsNull()) {
            adjacentFaces.push_back(face);
        }
    }

    if (adjacentFaces.empty()) {
        return false;
    }
    if (adjacentFaces.size() == 1) {
        if (BRep_Tool::IsClosed(edge, adjacentFaces[0])) {
            return false;
        }
        return true;
    }

    for (size_t i = 0; i + 1 < adjacentFaces.size(); ++i) {
        for (size_t j = i + 1; j < adjacentFaces.size(); ++j) {
            GeomAbs_Shape continuity = BRep_Tool::Continuity(edge, adjacentFaces[i], adjacentFaces[j]);
            if (continuity >= GeomAbs_G1) {
                continue;
            }
            if (isSharpEdgeByAngle(edge, adjacentFaces[i], adjacentFaces[j])) {
                return true;
            }
        }
    }

    return false;
}

struct FaceDisjointSet {
    std::unordered_map<std::string, std::string> parent;

    void add(const std::string& id) {
        parent.emplace(id, id);
    }

    std::string find(const std::string& id) {
        auto it = parent.find(id);
        if (it == parent.end()) {
            return id;
        }
        if (it->second == id) {
            return id;
        }
        it->second = find(it->second);
        return it->second;
    }

    void unite(const std::string& a, const std::string& b) {
        std::string rootA = find(a);
        std::string rootB = find(b);
        if (rootA == rootB) {
            return;
        }
        if (rootA < rootB) {
            parent[rootB] = rootA;
        } else {
            parent[rootA] = rootB;
        }
    }
};

} // namespace

namespace onecad::render {

SceneMeshStore::Mesh TessellationCache::buildMesh(const std::string& bodyId,
                                                  const TopoDS_Shape& shape,
                                                  kernel::elementmap::ElementMap& elementMap) const {
    SceneMeshStore::Mesh mesh;
    mesh.bodyId = bodyId;
    mesh.modelMatrix.setToIdentity();

    if (shape.IsNull()) {
        return mesh;
    }

    // Compute adaptive deflection based on bounding box
    double linearDeflection = settings_.linearDeflection;
    if (settings_.adaptive) {
        Bnd_Box bbox;
        BRepBndLib::Add(shape, bbox);
        if (!bbox.IsVoid()) {
            double xmin, ymin, zmin, xmax, ymax, zmax;
            bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);
            double diagonal = std::sqrt(std::pow(xmax - xmin, 2) +
                                        std::pow(ymax - ymin, 2) +
                                        std::pow(zmax - zmin, 2));
            // Scale deflection: smaller models get finer tessellation
            linearDeflection = std::min(settings_.linearDeflection, diagonal * 0.001);
            // Ensure minimum quality
            linearDeflection = std::max(linearDeflection, 0.001);
        }
    }

    BRepMesh_IncrementalMesh mesher(shape, linearDeflection,
                                    settings_.parallel, settings_.angularDeflection, true);
    mesher.Perform();
    if (!mesher.IsDone()) {
        return mesh;
    }

    // Build edge-to-faces ancestor map to identify visible (sharp) edges
    TopTools_IndexedDataMapOfShapeListOfShape edgeToFacesMap;
    TopExp::MapShapesAndAncestors(shape, TopAbs_EDGE, TopAbs_FACE, edgeToFacesMap);

    // Collect only edges that represent real boundaries (sharp or open edges)
    VisibleEdgeSet visibleEdges;
    for (int i = 1; i <= edgeToFacesMap.Extent(); ++i) {
        const TopoDS_Edge& edge = TopoDS::Edge(edgeToFacesMap.FindKey(i));
        const TopTools_ListOfShape& faces = edgeToFacesMap.FindFromIndex(i);
        if (isVisibleEdge(edge, faces)) {
            visibleEdges.insert(edge);
        }
    }

    std::unordered_map<TopoDS_Face, std::string, TopTools_ShapeMapHasher, TopTools_ShapeMapHasher> faceIdByShape;

    for (TopExp_Explorer faceExp(shape, TopAbs_FACE); faceExp.More(); faceExp.Next()) {
        TopoDS_Face face = TopoDS::Face(faceExp.Current());
        TopLoc_Location location;
        Handle(Poly_Triangulation) triangulation = BRep_Tool::Triangulation(face, location);
        if (triangulation.IsNull()) {
            continue;
        }

        std::string faceId;
        auto ids = elementMap.findIdsByShape(face);
        if (!ids.empty()) {
            faceId = ids.front().value;
        }

        if (faceId.empty()) {
            faceId = bodyId + "/face/unknown_" + std::to_string(mesh.triangles.size());
        }

        faceIdByShape.emplace(face, faceId);

        SceneMeshStore::FaceTopology topology = buildFaceTopology(bodyId, face, elementMap, visibleEdges);
        topology.faceId = faceId;
        mesh.topologyByFace[faceId] = std::move(topology);

        const gp_Trsf& trsf = location.Transformation();
        std::uint32_t nodeOffset = static_cast<std::uint32_t>(mesh.vertices.size());
        int nodeCount = triangulation->NbNodes();
        mesh.vertices.reserve(mesh.vertices.size() + nodeCount);
        for (int i = 1; i <= nodeCount; ++i) {
            gp_Pnt p = triangulation->Node(i).Transformed(trsf);
            mesh.vertices.emplace_back(static_cast<float>(p.X()),
                                       static_cast<float>(p.Y()),
                                       static_cast<float>(p.Z()));
        }

        int triCount = triangulation->NbTriangles();
        mesh.triangles.reserve(mesh.triangles.size() + triCount);
        for (int i = 1; i <= triCount; ++i) {
            int n1 = 0;
            int n2 = 0;
            int n3 = 0;
            triangulation->Triangle(i).Get(n1, n2, n3);
            SceneMeshStore::Triangle tri;
            tri.i0 = nodeOffset + static_cast<std::uint32_t>(n1 - 1);
            tri.i1 = nodeOffset + static_cast<std::uint32_t>(n2 - 1);
            tri.i2 = nodeOffset + static_cast<std::uint32_t>(n3 - 1);
            tri.faceId = faceId;
            mesh.triangles.push_back(tri);
        }
    }

    FaceDisjointSet faceGroups;
    for (const auto& entry : faceIdByShape) {
        faceGroups.add(entry.second);
    }

    for (int i = 1; i <= edgeToFacesMap.Extent(); ++i) {
        const TopoDS_Edge& edge = TopoDS::Edge(edgeToFacesMap.FindKey(i));
        if (visibleEdges.find(edge) != visibleEdges.end()) {
            continue;
        }
        const TopTools_ListOfShape& faces = edgeToFacesMap.FindFromIndex(i);
        std::string firstId;
        for (TopTools_ListIteratorOfListOfShape it(faces); it.More(); it.Next()) {
            TopoDS_Face face = TopoDS::Face(it.Value());
            auto faceIdIt = faceIdByShape.find(face);
            if (faceIdIt == faceIdByShape.end()) {
                continue;
            }
            if (firstId.empty()) {
                firstId = faceIdIt->second;
            } else {
                faceGroups.unite(firstId, faceIdIt->second);
            }
        }
    }

    for (const auto& entry : faceIdByShape) {
        const std::string& faceId = entry.second;
        mesh.faceGroupByFaceId[faceId] = faceGroups.find(faceId);
    }

    return mesh;
}

SceneMeshStore::FaceTopology TessellationCache::buildFaceTopology(
    const std::string& bodyId,
    const TopoDS_Face& face,
    kernel::elementmap::ElementMap& elementMap,
    const VisibleEdgeSet& visibleEdges) const {
    SceneMeshStore::FaceTopology topology;

    std::unordered_set<std::string> seenEdges;
    std::unordered_set<std::string> seenVertices;
    std::unordered_map<TopoDS_Shape, std::string, TopTools_ShapeMapHasher, TopTools_ShapeMapHasher>
        generatedEdgeIds;
    std::unordered_map<TopoDS_Shape, std::string, TopTools_ShapeMapHasher, TopTools_ShapeMapHasher>
        generatedVertexIds;
    int unknownEdgeCount = 0;
    int unknownVertexCount = 0;

    for (TopExp_Explorer wireExp(face, TopAbs_WIRE); wireExp.More(); wireExp.Next()) {
        TopoDS_Wire wire = TopoDS::Wire(wireExp.Current());
        for (BRepTools_WireExplorer edgeExp(wire, face); edgeExp.More(); edgeExp.Next()) {
            TopoDS_Edge edge = edgeExp.Current();

            // Skip edges that are not visible boundaries (tangent/seam edges)
            if (visibleEdges.find(edge) == visibleEdges.end()) {
                continue;
            }

            std::string edgeId;
            auto edgeIds = elementMap.findIdsByShape(edge);
            if (!edgeIds.empty()) {
                edgeId = edgeIds.front().value;
            }
            if (edgeId.empty()) {
                auto it = generatedEdgeIds.find(edge);
                if (it != generatedEdgeIds.end()) {
                    edgeId = it->second;
                } else {
                    edgeId = bodyId + "/edge/unknown_" + std::to_string(unknownEdgeCount++);
                    generatedEdgeIds.emplace(edge, edgeId);
                }
            }

            if (seenEdges.find(edgeId) == seenEdges.end()) {
                SceneMeshStore::EdgePolyline polyline;
                polyline.edgeId = edgeId;

                BRepAdaptor_Curve curve(edge);
                double first = curve.FirstParameter();
                double last = curve.LastParameter();
                double length = 0.0;
                try {
                    length = GCPnts_AbscissaPoint::Length(curve, first, last);
                } catch (const Standard_Failure&) {
                    length = 0.0;
                }
                double step = std::max(settings_.linearDeflection * 2.0, 0.1);
                int segments = std::max(2, static_cast<int>(std::ceil(length / step)));

                GCPnts_UniformAbscissa abscissa(curve, segments);
                if (abscissa.IsDone() && abscissa.NbPoints() > 1) {
                    for (int i = 1; i <= abscissa.NbPoints(); ++i) {
                        double param = abscissa.Parameter(i);
                        gp_Pnt point = curve.Value(param);
                        polyline.points.emplace_back(static_cast<float>(point.X()),
                                                     static_cast<float>(point.Y()),
                                                     static_cast<float>(point.Z()));
                    }
                } else {
                    gp_Pnt p1 = curve.Value(first);
                    gp_Pnt p2 = curve.Value(last);
                    polyline.points.emplace_back(static_cast<float>(p1.X()),
                                                 static_cast<float>(p1.Y()),
                                                 static_cast<float>(p1.Z()));
                    polyline.points.emplace_back(static_cast<float>(p2.X()),
                                                 static_cast<float>(p2.Y()),
                                                 static_cast<float>(p2.Z()));
                }

                if (polyline.points.size() >= 2) {
                    topology.edges.push_back(std::move(polyline));
                    seenEdges.insert(edgeId);
                }
            }

            TopoDS_Vertex v1;
            TopoDS_Vertex v2;
            TopExp::Vertices(edge, v1, v2);
            TopoDS_Vertex vertices[2] = {v1, v2};
            for (const auto& vertex : vertices) {
                if (vertex.IsNull()) {
                    continue;
                }
                std::string vertexId;
                auto vertexIds = elementMap.findIdsByShape(vertex);
                if (!vertexIds.empty()) {
                    vertexId = vertexIds.front().value;
                }
                if (vertexId.empty()) {
                    auto it = generatedVertexIds.find(vertex);
                    if (it != generatedVertexIds.end()) {
                        vertexId = it->second;
                    } else {
                        vertexId = bodyId + "/vertex/unknown_" + std::to_string(unknownVertexCount++);
                        generatedVertexIds.emplace(vertex, vertexId);
                    }
                }
                if (seenVertices.find(vertexId) != seenVertices.end()) {
                    continue;
                }
                gp_Pnt point = BRep_Tool::Pnt(vertex);
                if (!vertex.Location().IsIdentity()) {
                    point.Transform(vertex.Location().Transformation());
                }
                SceneMeshStore::VertexSample sample;
                sample.vertexId = vertexId;
                sample.position = QVector3D(static_cast<float>(point.X()),
                                            static_cast<float>(point.Y()),
                                            static_cast<float>(point.Z()));
                topology.vertices.push_back(sample);
                seenVertices.insert(vertexId);
            }
        }
    }

    return topology;
}

} // namespace onecad::render
