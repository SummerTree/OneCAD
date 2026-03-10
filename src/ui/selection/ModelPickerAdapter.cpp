#include "ModelPickerAdapter.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace onecad::ui::selection {

namespace {
constexpr int kVertexPriority = 0;
constexpr int kEdgePriority = 1;
constexpr int kFacePriority = 2;
constexpr int kBodyPriority = 3;
constexpr double kRayParallelEpsilon = 1e-10;
constexpr double kBarycentricSlack = 1e-8;
constexpr double kFaceDepthEpsilon = 1e-4;
constexpr double kOcclusionDepthEpsilon = 1e-2;

struct DVec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

DVec3 toDVec3(const QVector3D& value) {
    return {static_cast<double>(value.x()),
            static_cast<double>(value.y()),
            static_cast<double>(value.z())};
}

QVector3D toQVector3D(const DVec3& value) {
    return QVector3D(static_cast<float>(value.x),
                     static_cast<float>(value.y),
                     static_cast<float>(value.z));
}

DVec3 operator-(const DVec3& a, const DVec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

DVec3 operator+(const DVec3& a, const DVec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

DVec3 operator*(const DVec3& value, double scalar) {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

double dot(const DVec3& a, const DVec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

DVec3 cross(const DVec3& a, const DVec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

double length(const DVec3& value) {
    return std::sqrt(dot(value, value));
}

DVec3 normalize(const DVec3& value) {
    const double len = length(value);
    if (len <= 1e-20) {
        return {};
    }
    return value * (1.0 / len);
}

std::string vertexIdForIndex(std::uint32_t index) {
    return "v" + std::to_string(index);
}

std::string edgeIdForIndices(std::uint32_t a, std::uint32_t b) {
    if (a > b) {
        std::swap(a, b);
    }
    return "e" + std::to_string(a) + "_" + std::to_string(b);
}

bool rayTriangleIntersect(const QVector3D& origin,
                          const QVector3D& direction,
                          const QVector3D& v0,
                          const QVector3D& v1,
                          const QVector3D& v2,
                          double* outT,
                          QVector3D* outNormal) {
    const DVec3 originD = toDVec3(origin);
    const DVec3 directionD = toDVec3(direction);
    const DVec3 v0D = toDVec3(v0);
    const DVec3 v1D = toDVec3(v1);
    const DVec3 v2D = toDVec3(v2);

    const DVec3 edge1 = v1D - v0D;
    const DVec3 edge2 = v2D - v0D;
    const DVec3 pvec = cross(directionD, edge2);
    const double det = dot(edge1, pvec);
    if (std::abs(det) < kRayParallelEpsilon) {
        return false;
    }
    const double invDet = 1.0 / det;
    const DVec3 tvec = originD - v0D;
    const double u = dot(tvec, pvec) * invDet;
    if (u < -kBarycentricSlack || u > 1.0 + kBarycentricSlack) {
        return false;
    }
    const DVec3 qvec = cross(tvec, edge1);
    const double v = dot(directionD, qvec) * invDet;
    if (v < -kBarycentricSlack || (u + v) > 1.0 + kBarycentricSlack) {
        return false;
    }
    const double t = dot(edge2, qvec) * invDet;
    if (t <= 0.0) {
        return false;
    }
    if (outT) {
        *outT = t;
    }
    if (outNormal) {
        *outNormal = toQVector3D(normalize(cross(edge1, edge2)));
    }
    return true;
}

double distancePointToSegment(const QPointF& p, const QPointF& a, const QPointF& b) {
    QPointF ab = b - a;
    double lenSq = ab.x() * ab.x() + ab.y() * ab.y();
    if (lenSq < 1e-6) {
        QPointF diff = p - a;
        return std::sqrt(diff.x() * diff.x() + diff.y() * diff.y());
    }
    double t = ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / lenSq;
    t = std::clamp(t, 0.0, 1.0);
    QPointF proj(a.x() + ab.x() * t, a.y() + ab.y() * t);
    QPointF diff = p - proj;
    return std::sqrt(diff.x() * diff.x() + diff.y() * diff.y());
}

double depthAlongRay(const ModelPickerAdapter::Ray& ray, const QVector3D& worldPos) {
    if (!ray.valid) {
        return std::numeric_limits<double>::infinity();
    }
    const QVector3D delta = worldPos - ray.origin;
    return std::max(0.0, static_cast<double>(QVector3D::dotProduct(delta, ray.direction)));
}

double interpolationFactor(const QPointF& p, const QPointF& a, const QPointF& b) {
    const QPointF ab = b - a;
    const double lenSq = ab.x() * ab.x() + ab.y() * ab.y();
    if (lenSq < 1e-6) {
        return 0.0;
    }
    double t = ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / lenSq;
    return std::clamp(t, 0.0, 1.0);
}

bool isCandidateOccluded(double candidateDepth, const std::optional<double>& frontDepth) {
    if (!frontDepth.has_value()) {
        return false;
    }
    return candidateDepth > *frontDepth + std::max(kOcclusionDepthEpsilon, *frontDepth * 1e-3);
}

} // namespace

std::string ModelPickerAdapter::promotedFaceId(const MeshCache& mesh, const std::string& faceId) {
    auto groupIt = mesh.faceGroupLeaderByFaceId.find(faceId);
    return groupIt != mesh.faceGroupLeaderByFaceId.end() ? groupIt->second : faceId;
}

std::string ModelPickerAdapter::promotedEdgeId(const MeshCache& mesh, const std::string& edgeId) {
    auto groupIt = mesh.edgeGroupLeaderByEdgeId.find(edgeId);
    return groupIt != mesh.edgeGroupLeaderByEdgeId.end() ? groupIt->second : edgeId;
}

ModelPickerAdapter::MeshCache::QuantizedPosition
ModelPickerAdapter::quantizePosition(const QVector3D& position) {
    auto quantize = [](float value) -> std::int64_t {
        return static_cast<std::int64_t>(std::llround(static_cast<double>(value) * 1e5));
    };
    return {quantize(position.x()), quantize(position.y()), quantize(position.z())};
}

void ModelPickerAdapter::setMeshes(std::vector<Mesh>&& meshes) {
    meshes_.clear();
    meshes_.reserve(meshes.size());

    for (auto& mesh : meshes) {
        MeshCache cache;
        cache.bodyId = mesh.bodyId;
        cache.vertices = std::move(mesh.vertices);
        cache.triangles = std::move(mesh.triangles);

        for (const auto& tri : cache.triangles) {
            if (tri.i0 >= cache.vertices.size() ||
                tri.i1 >= cache.vertices.size() ||
                tri.i2 >= cache.vertices.size()) {
                continue;
            }
            std::array<QVector3D, 3> triVerts = {
                cache.vertices[tri.i0],
                cache.vertices[tri.i1],
                cache.vertices[tri.i2]
            };
            cache.faceMap[tri.faceId].push_back(triVerts);
        }

        if (!mesh.topologyByFace.empty()) {
            for (const auto& [faceId, topo] : mesh.topologyByFace) {
                MeshCache::FaceTopologyCache faceCache;

                for (const auto& edge : topo.edges) {
                    if (edge.points.size() < 2) {
                        continue;
                    }
                    if (cache.edgePolylines.find(edge.edgeId) == cache.edgePolylines.end()) {
                        cache.edgePolylines[edge.edgeId] = edge.points;
                    }
                    faceCache.edgeIds.push_back(edge.edgeId);
                }

                for (const auto& vertex : topo.vertices) {
                    if (cache.vertexMap.find(vertex.vertexId) == cache.vertexMap.end()) {
                        cache.vertexMap[vertex.vertexId] = vertex.position;
                    }
                    cache.pickableVertices.insert(vertex.vertexId);
                    faceCache.vertexIds.push_back(vertex.vertexId);
                }

                cache.faceTopology[faceId] = std::move(faceCache);
            }
        } else {
            std::unordered_map<std::string, std::unordered_map<std::string, int>> edgeCountsByFace;
            for (const auto& tri : cache.triangles) {
                if (tri.i0 >= cache.vertices.size() ||
                    tri.i1 >= cache.vertices.size() ||
                    tri.i2 >= cache.vertices.size()) {
                    continue;
                }
                std::array<std::pair<std::uint32_t, std::uint32_t>, 3> edges = {{
                    {tri.i0, tri.i1},
                    {tri.i1, tri.i2},
                    {tri.i2, tri.i0}
                }};
                for (const auto& edge : edges) {
                    std::string edgeId = edgeIdForIndices(edge.first, edge.second);
                    edgeCountsByFace[tri.faceId][edgeId]++;
                }
            }

            for (const auto& [faceId, edges] : edgeCountsByFace) {
                MeshCache::FaceTopologyCache faceCache;
                std::unordered_set<std::string> addedVertices;
                for (const auto& [edgeId, count] : edges) {
                    if (count != 1) {
                        continue;
                    }
                    size_t underscore = edgeId.find('_');
                    if (underscore == std::string::npos) {
                        continue;
                    }
                    std::uint32_t a = static_cast<std::uint32_t>(
                        std::stoul(edgeId.substr(1, underscore - 1)));
                    std::uint32_t b = static_cast<std::uint32_t>(
                        std::stoul(edgeId.substr(underscore + 1)));
                    if (static_cast<size_t>(a) >= cache.vertices.size() ||
                        static_cast<size_t>(b) >= cache.vertices.size()) {
                        continue;
                    }
                    std::vector<QVector3D> polyline = {cache.vertices[a], cache.vertices[b]};
                    if (cache.edgePolylines.find(edgeId) == cache.edgePolylines.end()) {
                        cache.edgePolylines[edgeId] = polyline;
                    }
                    faceCache.edgeIds.push_back(edgeId);

                    std::string vA = vertexIdForIndex(a);
                    std::string vB = vertexIdForIndex(b);
                    cache.vertexMap[vA] = cache.vertices[a];
                    cache.vertexMap[vB] = cache.vertices[b];
                    cache.pickableVertices.insert(vA);
                    cache.pickableVertices.insert(vB);
                    if (addedVertices.insert(vA).second) {
                        faceCache.vertexIds.push_back(vA);
                    }
                    if (addedVertices.insert(vB).second) {
                        faceCache.vertexIds.push_back(vB);
                    }
                }
                cache.faceTopology[faceId] = std::move(faceCache);
            }
        }

        cache.faceGroupLeaderByFaceId = std::move(mesh.faceGroupByFaceId);
        if (cache.faceGroupLeaderByFaceId.empty()) {
            for (const auto& [faceId, tris] : cache.faceMap) {
                (void)tris;
                cache.faceGroupLeaderByFaceId[faceId] = faceId;
            }
        } else {
            for (const auto& [faceId, tris] : cache.faceMap) {
                (void)tris;
                if (cache.faceGroupLeaderByFaceId.find(faceId) == cache.faceGroupLeaderByFaceId.end()) {
                    cache.faceGroupLeaderByFaceId[faceId] = faceId;
                }
            }
        }
        for (const auto& [faceId, leaderId] : cache.faceGroupLeaderByFaceId) {
            cache.faceGroupMembers[leaderId].push_back(faceId);
        }
        for (auto& [leaderId, members] : cache.faceGroupMembers) {
            std::sort(members.begin(), members.end());
            members.erase(std::unique(members.begin(), members.end()), members.end());
            (void)leaderId;
        }

        cache.edgeGroupLeaderByEdgeId = std::move(mesh.edgeGroupByEdgeId);
        if (cache.edgeGroupLeaderByEdgeId.empty()) {
            for (const auto& [edgeId, polyline] : cache.edgePolylines) {
                (void)polyline;
                cache.edgeGroupLeaderByEdgeId[edgeId] = edgeId;
            }
        } else {
            for (const auto& [edgeId, polyline] : cache.edgePolylines) {
                (void)polyline;
                if (cache.edgeGroupLeaderByEdgeId.find(edgeId) == cache.edgeGroupLeaderByEdgeId.end()) {
                    cache.edgeGroupLeaderByEdgeId[edgeId] = edgeId;
                }
            }
        }
        for (const auto& [edgeId, leaderId] : cache.edgeGroupLeaderByEdgeId) {
            cache.edgeGroupMembers[leaderId].push_back(edgeId);
        }
        for (auto& [leaderId, members] : cache.edgeGroupMembers) {
            std::sort(members.begin(), members.end());
            members.erase(std::unique(members.begin(), members.end()), members.end());
            (void)leaderId;
        }

        for (const auto& suppressedVertexId : mesh.suppressedVertexIds) {
            cache.pickableVertices.erase(suppressedVertexId);
            auto vertexIt = cache.vertexMap.find(suppressedVertexId);
            if (vertexIt != cache.vertexMap.end()) {
                cache.suppressedVertexPositions.insert(quantizePosition(vertexIt->second));
            }
        }

        meshes_.push_back(std::move(cache));
    }
}

app::selection::PickResult ModelPickerAdapter::pick(const QPoint& screenPos,
                                                    double tolerancePixels,
                                                    const QMatrix4x4& viewProjection,
                                                    const QSize& viewportSize) const {
    app::selection::PickResult result;
    if (meshes_.empty()) {
        return result;
    }

    Ray ray = buildRay(screenPos, viewProjection, viewportSize);
    if (!ray.valid) {
        return result;
    }
    struct FaceHit {
        const MeshCache* mesh = nullptr;
        std::string faceId;
        QVector3D normal;
        QVector3D point;
        double t = 0.0;
    };

    std::vector<FaceHit> faceHits;
    faceHits.reserve(16);
    std::unordered_map<std::string, size_t> faceIndex;

    for (const auto& mesh : meshes_) {
        for (const auto& tri : mesh.triangles) {
            if (tri.i0 >= mesh.vertices.size() ||
                tri.i1 >= mesh.vertices.size() ||
                tri.i2 >= mesh.vertices.size()) {
                continue;
            }
            const QVector3D& v0 = mesh.vertices[tri.i0];
            const QVector3D& v1 = mesh.vertices[tri.i1];
            const QVector3D& v2 = mesh.vertices[tri.i2];
            double t = 0.0;
            QVector3D normal;
            if (!rayTriangleIntersect(ray.origin, ray.direction, v0, v1, v2, &t, &normal)) {
                continue;
            }
            const std::string faceId = promotedFaceId(mesh, tri.faceId);
            const std::string key = mesh.bodyId + ":" + faceId;
            auto it = faceIndex.find(key);
            if (it == faceIndex.end()) {
                FaceHit hit;
                hit.mesh = &mesh;
                hit.faceId = faceId;
                hit.normal = normal;
                hit.point = ray.origin + (ray.direction * static_cast<float>(t));
                hit.t = t;
                faceIndex[key] = faceHits.size();
                faceHits.push_back(hit);
            } else {
                FaceHit& hit = faceHits[it->second];
                if (t < hit.t) {
                    hit.faceId = faceId;
                    hit.normal = normal;
                    hit.point = ray.origin + (ray.direction * static_cast<float>(t));
                    hit.t = t;
                }
            }
        }
    }

    std::sort(faceHits.begin(), faceHits.end(), [](const FaceHit& a, const FaceHit& b) {
        return a.t < b.t;
    });

    std::optional<double> frontDepth;
    if (!faceHits.empty()) {
        frontDepth = faceHits.front().t;
    }

    std::vector<FaceHit> visibleHits;
    visibleHits.reserve(faceHits.size());
    if (frontDepth.has_value()) {
        for (const auto& hit : faceHits) {
            if (hit.t <= *frontDepth + std::max(kFaceDepthEpsilon, *frontDepth * 1e-5)) {
                visibleHits.push_back(hit);
            }
        }
    }

    const QPointF clickPoint(screenPos);
    const double vertexTolerancePixels = std::max(4.0, tolerancePixels * 0.75);
    const double edgeTolerancePixels = std::max(6.0, tolerancePixels);

    struct VertexCandidate {
        const MeshCache* mesh = nullptr;
        std::string vertexId;
        QVector3D position;
        double distance = std::numeric_limits<double>::max();
        double depth = std::numeric_limits<double>::infinity();
    };

    struct EdgeCandidate {
        const MeshCache* mesh = nullptr;
        std::string edgeId;
        QVector3D point;
        double distance = std::numeric_limits<double>::max();
        double depth = std::numeric_limits<double>::infinity();
    };

    VertexCandidate bestVertex;
    EdgeCandidate bestEdge;

    auto shouldReplaceCandidate = [](double distance,
                                     double depth,
                                     double bestDistance,
                                     double bestDepth) {
        if (distance + 1e-6 < bestDistance) {
            return true;
        }
        if (std::abs(distance - bestDistance) > 1e-6) {
            return false;
        }
        return depth < bestDepth;
    };

    for (const auto& mesh : meshes_) {
        for (const auto& [vertexId, vertexPos] : mesh.vertexMap) {
            if (!mesh.pickableVertices.empty() &&
                mesh.pickableVertices.find(vertexId) == mesh.pickableVertices.end()) {
                continue;
            }
            if (mesh.suppressedVertexPositions.find(quantizePosition(vertexPos)) !=
                mesh.suppressedVertexPositions.end()) {
                continue;
            }
            QPointF projectedPos;
            if (!projectToScreen(viewProjection, vertexPos, viewportSize, &projectedPos)) {
                continue;
            }
            const double distance = std::hypot(clickPoint.x() - projectedPos.x(),
                                               clickPoint.y() - projectedPos.y());
            if (distance > vertexTolerancePixels) {
                continue;
            }
            const double depth = depthAlongRay(ray, vertexPos);
            if (isCandidateOccluded(depth, frontDepth)) {
                continue;
            }
            if (!bestVertex.mesh ||
                shouldReplaceCandidate(distance, depth, bestVertex.distance, bestVertex.depth)) {
                bestVertex.mesh = &mesh;
                bestVertex.vertexId = vertexId;
                bestVertex.position = vertexPos;
                bestVertex.distance = distance;
                bestVertex.depth = depth;
            }
        }

        for (const auto& [edgeId, polyline] : mesh.edgePolylines) {
            if (polyline.size() < 2) {
                continue;
            }
            const std::string promotedId = promotedEdgeId(mesh, edgeId);
            for (size_t i = 0; i + 1 < polyline.size(); ++i) {
                QPointF a;
                QPointF b;
                if (!projectToScreen(viewProjection, polyline[i], viewportSize, &a) ||
                    !projectToScreen(viewProjection, polyline[i + 1], viewportSize, &b)) {
                    continue;
                }
                const double distance = distancePointToSegment(clickPoint, a, b);
                if (distance > edgeTolerancePixels) {
                    continue;
                }
                const double t = interpolationFactor(clickPoint, a, b);
                const QVector3D worldPoint = polyline[i] + ((polyline[i + 1] - polyline[i]) * static_cast<float>(t));
                const double depth = depthAlongRay(ray, worldPoint);
                if (isCandidateOccluded(depth, frontDepth)) {
                    continue;
                }
                if (!bestEdge.mesh ||
                    shouldReplaceCandidate(distance, depth, bestEdge.distance, bestEdge.depth)) {
                    bestEdge.mesh = &mesh;
                    bestEdge.edgeId = promotedId;
                    bestEdge.point = worldPoint;
                    bestEdge.distance = distance;
                    bestEdge.depth = depth;
                }
            }
        }
    }

    if (bestVertex.mesh) {
        app::selection::SelectionItem item;
        item.kind = app::selection::SelectionKind::Vertex;
        item.id = {bestVertex.mesh->bodyId, bestVertex.vertexId};
        item.priority = kVertexPriority;
        item.screenDistance = bestVertex.distance;
        item.depth = bestVertex.depth;
        item.worldPos = {bestVertex.position.x(), bestVertex.position.y(), bestVertex.position.z()};
        result.hits.push_back(item);
    }

    if (!bestEdge.edgeId.empty()) {
        app::selection::SelectionItem item;
        item.kind = app::selection::SelectionKind::Edge;
        item.id = {bestEdge.mesh->bodyId, bestEdge.edgeId};
        item.priority = kEdgePriority;
        item.screenDistance = bestEdge.distance;
        item.depth = bestEdge.depth;
        item.worldPos = {bestEdge.point.x(), bestEdge.point.y(), bestEdge.point.z()};
        result.hits.push_back(item);
    }

    std::unordered_map<std::string, double> bodyDepths;
    std::unordered_map<std::string, QVector3D> bodyPoints;
    std::unordered_map<std::string, QVector3D> bodyNormals;

    for (const auto& hit : visibleHits) {
        app::selection::SelectionItem faceItem;
        faceItem.kind = app::selection::SelectionKind::Face;
        faceItem.id = {hit.mesh->bodyId, hit.faceId};
        faceItem.priority = kFacePriority;
        faceItem.screenDistance = 0.0;
        faceItem.depth = hit.t;
        faceItem.worldPos = {hit.point.x(), hit.point.y(), hit.point.z()};
        faceItem.normal = {hit.normal.x(), hit.normal.y(), hit.normal.z()};
        result.hits.push_back(faceItem);

        auto bodyIt = bodyDepths.find(hit.mesh->bodyId);
        if (bodyIt == bodyDepths.end() || hit.t < bodyIt->second) {
            bodyDepths[hit.mesh->bodyId] = hit.t;
            bodyPoints[hit.mesh->bodyId] = hit.point;
            bodyNormals[hit.mesh->bodyId] = hit.normal;
        }
    }

    for (const auto& [bodyId, depth] : bodyDepths) {
        app::selection::SelectionItem bodyItem;
        bodyItem.kind = app::selection::SelectionKind::Body;
        bodyItem.id = {bodyId, bodyId};
        bodyItem.priority = kBodyPriority;
        bodyItem.screenDistance = 0.0;
        bodyItem.depth = static_cast<double>(depth);
        const QVector3D& point = bodyPoints[bodyId];
        const QVector3D& normal = bodyNormals[bodyId];
        bodyItem.worldPos = {point.x(), point.y(), point.z()};
        bodyItem.normal = {normal.x(), normal.y(), normal.z()};
        result.hits.push_back(bodyItem);
    }

    return result;
}

bool ModelPickerAdapter::getFaceTriangles(const std::string& bodyId,
                                          const std::string& faceId,
                                          std::vector<std::array<QVector3D, 3>>& outTriangles) const {
    for (const auto& mesh : meshes_) {
        if (mesh.bodyId != bodyId) {
            continue;
        }
        const std::string groupId = promotedFaceId(mesh, faceId);
        outTriangles.clear();
        auto membersIt = mesh.faceGroupMembers.find(groupId);
        if (membersIt != mesh.faceGroupMembers.end()) {
            for (const auto& memberId : membersIt->second) {
                auto it = mesh.faceMap.find(memberId);
                if (it != mesh.faceMap.end()) {
                    outTriangles.insert(outTriangles.end(), it->second.begin(), it->second.end());
                }
            }
            return !outTriangles.empty();
        }
        auto it = mesh.faceMap.find(faceId);
        if (it != mesh.faceMap.end()) {
            outTriangles = it->second;
            return true;
        }
        return false;
    }
    return false;
}

bool ModelPickerAdapter::getBodyTriangles(const std::string& bodyId,
                                          std::vector<std::array<QVector3D, 3>>& outTriangles) const {
    for (const auto& mesh : meshes_) {
        if (mesh.bodyId != bodyId) {
            continue;
        }
        outTriangles.clear();
        for (const auto& [faceId, tris] : mesh.faceMap) {
            (void)faceId;
            outTriangles.insert(outTriangles.end(), tris.begin(), tris.end());
        }
        return !outTriangles.empty();
    }
    return false;
}

bool ModelPickerAdapter::getEdgeSegment(const std::string& bodyId,
                                        const std::string& edgeId,
                                        std::array<QVector3D, 2>& outSegment) const {
    std::vector<std::vector<QVector3D>> polylines;
    if (!getEdgePolylines(bodyId, edgeId, polylines) || polylines.empty() || polylines.front().size() < 2) {
        return false;
    }
    outSegment = {polylines.front().front(), polylines.front().back()};
    return true;
}

bool ModelPickerAdapter::getEdgePolylines(const std::string& bodyId,
                                          const std::string& edgeId,
                                          std::vector<std::vector<QVector3D>>& outPolylines) const {
    for (const auto& mesh : meshes_) {
        if (mesh.bodyId != bodyId) {
            continue;
        }
        const std::string groupId = promotedEdgeId(mesh, edgeId);
        outPolylines.clear();
        auto membersIt = mesh.edgeGroupMembers.find(groupId);
        if (membersIt != mesh.edgeGroupMembers.end()) {
            for (const auto& memberId : membersIt->second) {
                auto it = mesh.edgePolylines.find(memberId);
                if (it != mesh.edgePolylines.end() && it->second.size() >= 2) {
                    outPolylines.push_back(it->second);
                }
            }
            return !outPolylines.empty();
        }
        auto it = mesh.edgePolylines.find(edgeId);
        if (it == mesh.edgePolylines.end() || it->second.size() < 2) {
            return false;
        }
        outPolylines.push_back(it->second);
        return true;
    }
    return false;
}

bool ModelPickerAdapter::getVertexPosition(const std::string& bodyId,
                                           const std::string& vertexId,
                                           QVector3D& outVertex) const {
    for (const auto& mesh : meshes_) {
        if (mesh.bodyId != bodyId) {
            continue;
        }
        auto it = mesh.vertexMap.find(vertexId);
        if (it == mesh.vertexMap.end()) {
            return false;
        }
        outVertex = it->second;
        return true;
    }
    return false;
}

bool ModelPickerAdapter::getFaceBoundaryEdges(const std::string& bodyId,
                                               const std::string& faceId,
                                               std::vector<std::vector<QVector3D>>& outEdges) const {
    for (const auto& mesh : meshes_) {
        if (mesh.bodyId != bodyId) {
            continue;
        }
        const std::string groupId = promotedFaceId(mesh, faceId);
        outEdges.clear();
        std::unordered_set<std::string> seenEdgeGroups;
        auto membersIt = mesh.faceGroupMembers.find(groupId);
        if (membersIt != mesh.faceGroupMembers.end()) {
            for (const auto& memberId : membersIt->second) {
                auto topoIt = mesh.faceTopology.find(memberId);
                if (topoIt == mesh.faceTopology.end()) {
                    continue;
                }
                for (const auto& edgeId : topoIt->second.edgeIds) {
                    const std::string promotedEdge = promotedEdgeId(mesh, edgeId);
                    if (!seenEdgeGroups.insert(promotedEdge).second) {
                        continue;
                    }
                    auto edgeMembersIt = mesh.edgeGroupMembers.find(promotedEdge);
                    if (edgeMembersIt != mesh.edgeGroupMembers.end()) {
                        for (const auto& memberEdgeId : edgeMembersIt->second) {
                            auto polyIt = mesh.edgePolylines.find(memberEdgeId);
                            if (polyIt != mesh.edgePolylines.end() && polyIt->second.size() >= 2) {
                                outEdges.push_back(polyIt->second);
                            }
                        }
                        continue;
                    }
                    auto polyIt = mesh.edgePolylines.find(edgeId);
                    if (polyIt != mesh.edgePolylines.end() && polyIt->second.size() >= 2) {
                        outEdges.push_back(polyIt->second);
                    }
                }
            }
            return !outEdges.empty();
        }
        auto topoIt = mesh.faceTopology.find(faceId);
        if (topoIt == mesh.faceTopology.end()) {
            return false;
        }
        seenEdgeGroups.clear();
        for (const auto& edgeId : topoIt->second.edgeIds) {
            const std::string promotedEdge = promotedEdgeId(mesh, edgeId);
            if (!seenEdgeGroups.insert(promotedEdge).second) {
                continue;
            }
            auto edgeMembersIt = mesh.edgeGroupMembers.find(promotedEdge);
            if (edgeMembersIt != mesh.edgeGroupMembers.end()) {
                for (const auto& memberEdgeId : edgeMembersIt->second) {
                    auto polyIt = mesh.edgePolylines.find(memberEdgeId);
                    if (polyIt != mesh.edgePolylines.end() && polyIt->second.size() >= 2) {
                        outEdges.push_back(polyIt->second);
                    }
                }
                continue;
            }
            auto polyIt = mesh.edgePolylines.find(edgeId);
            if (polyIt != mesh.edgePolylines.end() && polyIt->second.size() >= 2) {
                outEdges.push_back(polyIt->second);
            }
        }
        return !outEdges.empty();
    }
    return false;
}

ModelPickerAdapter::Ray ModelPickerAdapter::buildRay(const QPoint& screenPos,
                                                     const QMatrix4x4& viewProjection,
                                                     const QSize& viewportSize) const {
    Ray ray;
    if (viewportSize.width() <= 0 || viewportSize.height() <= 0) {
        ray.origin = QVector3D();
        ray.direction = QVector3D();
        ray.valid = false;
        return ray;
    }

    bool invertible = false;
    QMatrix4x4 invViewProj = viewProjection.inverted(&invertible);
    if (!invertible) {
        ray.origin = QVector3D();
        ray.direction = QVector3D();
        ray.valid = false;
        return ray;
    }

    float ndcX = (2.0f * screenPos.x() / viewportSize.width()) - 1.0f;
    float ndcY = 1.0f - (2.0f * screenPos.y() / viewportSize.height());

    QVector4D nearPoint = invViewProj * QVector4D(ndcX, ndcY, -1.0f, 1.0f);
    QVector4D farPoint = invViewProj * QVector4D(ndcX, ndcY, 1.0f, 1.0f);

    if (std::abs(nearPoint.w()) < 1e-6f || std::abs(farPoint.w()) < 1e-6f) {
        ray.origin = QVector3D();
        ray.direction = QVector3D();
        ray.valid = false;
        return ray;
    }

    QVector3D origin = nearPoint.toVector3D() / nearPoint.w();
    QVector3D farPos = farPoint.toVector3D() / farPoint.w();
    ray.origin = origin;
    ray.direction = (farPos - origin).normalized();
    ray.valid = true;
    return ray;
}

bool ModelPickerAdapter::projectToScreen(const QMatrix4x4& viewProjection,
                                         const QVector3D& worldPos,
                                         const QSize& viewportSize,
                                         QPointF* outPos) const {
    QVector4D clip = viewProjection * QVector4D(worldPos, 1.0f);
    if (clip.w() <= 1e-6f) {
        return false;
    }
    QVector3D ndc = clip.toVector3D() / clip.w();
    float x = (ndc.x() * 0.5f + 0.5f) * viewportSize.width();
    float y = (1.0f - (ndc.y() * 0.5f + 0.5f)) * viewportSize.height();
    *outPos = QPointF(x, y);
    return true;
}

} // namespace onecad::ui::selection
