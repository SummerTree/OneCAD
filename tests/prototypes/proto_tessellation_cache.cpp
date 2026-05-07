#include "app/document/Document.h"
#include "render/tessellation/TessellationCache.h"

#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <gp_Pnt.hxx>
#include <QCoreApplication>
#include <exception>
#include <iostream>
#include <unordered_set>

namespace {

std::size_t countFaceGroups(const onecad::render::SceneMeshStore::Mesh& mesh) {
    std::unordered_set<std::string> groups;
    if (!mesh.faceGroupByFaceId.empty()) {
        for (const auto& [faceId, groupId] : mesh.faceGroupByFaceId) {
            (void)faceId;
            groups.insert(groupId);
        }
        return groups.size();
    }
    for (const auto& tri : mesh.triangles) {
        groups.insert(tri.faceId);
    }
    return groups.size();
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    onecad::app::Document document;
    TopoDS_Shape shape = BRepPrimAPI_MakeBox(10.0, 10.0, 10.0).Shape();
    std::string bodyId = document.addBody(shape);
    if (bodyId.empty()) {
        std::cerr << "Failed to add body.\n";
        return 1;
    }

    const auto& store = document.meshStore();

    const auto* mesh = store.findMesh(bodyId);
    if (!mesh) {
        std::cerr << "Mesh not found for body.\n";
        return 1;
    }
    if (mesh->triangles.empty()) {
        std::cerr << "No triangles generated.\n";
        return 1;
    }

    for (const auto& tri : mesh->triangles) {
        if (tri.faceId.empty()) {
            std::cerr << "Triangle missing faceId.\n";
            return 1;
        }
        try {
            onecad::kernel::elementmap::ElementId id =
                onecad::kernel::elementmap::ElementId::From(tri.faceId);
            if (!document.elementMap().contains(id)) {
                std::cerr << "FaceId not found in ElementMap.\n";
                return 1;
            }
        } catch (const std::exception& ex) {
            std::cerr << "Invalid faceId: " << tri.faceId << " (" << ex.what() << ")\n";
            return 1;
        }
    }

    TopoDS_Shape cylinderShape = BRepPrimAPI_MakeCylinder(5.0, 10.0).Shape();
    std::string cylinderId = document.addBody(cylinderShape);
    if (cylinderId.empty()) {
        std::cerr << "Failed to add cylinder body.\n";
        return 1;
    }

    const auto* cylinderMesh = store.findMesh(cylinderId);
    if (!cylinderMesh) {
        std::cerr << "Mesh not found for cylinder body.\n";
        return 1;
    }

    std::size_t groupCount = countFaceGroups(*cylinderMesh);
    if (groupCount != 3) {
        std::cerr << "Expected 3 face groups for cylinder (top, bottom, side), got "
                  << groupCount << ".\n";
        return 1;
    }

    onecad::render::TessellationCache tessellator;
    onecad::render::SceneMeshStore::Mesh badMesh;
    onecad::kernel::elementmap::ElementMap badMap;
    std::string error;
    TopoDS_Shape vertexShape = BRepBuilderAPI_MakeVertex(gp_Pnt(0.0, 0.0, 0.0)).Shape();
    if (tessellator.tryBuildMesh("bad", vertexShape, badMap, badMesh, nullptr, &error)) {
        std::cerr << "Vertex tessellation unexpectedly succeeded.\n";
        return 1;
    }
    if (error.empty()) {
        std::cerr << "Tessellation failure did not report an error.\n";
        return 1;
    }

    const auto* originalMesh = store.findMesh(bodyId);
    if (!originalMesh) {
        std::cerr << "Original mesh missing before rollback test.\n";
        return 1;
    }
    const std::size_t originalTriangles = originalMesh->triangles.size();
    if (document.updateBodyShape(bodyId, vertexShape)) {
        std::cerr << "Document accepted non-tessellatable body update.\n";
        return 1;
    }
    const auto* restoredMesh = store.findMesh(bodyId);
    if (!restoredMesh || restoredMesh->triangles.size() != originalTriangles) {
        std::cerr << "Failed tessellation update did not preserve old mesh.\n";
        return 1;
    }
    const TopoDS_Shape* restoredShape = document.getBodyShape(bodyId);
    if (!restoredShape || restoredShape->IsNull()) {
        std::cerr << "Failed tessellation update did not preserve old shape.\n";
        return 1;
    }
    TopExp_Explorer restoredFaceExp(*restoredShape, TopAbs_FACE);
    if (!restoredFaceExp.More()) {
        std::cerr << "Restored shape has no face for ElementMap rollback check.\n";
        return 1;
    }
    const auto restoredFaceIds = document.elementMap().findIdsByShape(restoredFaceExp.Current());
    if (restoredFaceIds.empty()) {
        std::cerr << "Failed tessellation update did not restore ElementMap face bindings.\n";
        return 1;
    }
    const auto* restoredFaceEntry = document.elementMap().find(restoredFaceIds.front());
    if (!restoredFaceEntry || restoredFaceEntry->shape.IsNull()) {
        std::cerr << "Restored ElementMap face entry has no bound shape.\n";
        return 1;
    }
    if (!document.addBody(vertexShape).empty()) {
        std::cerr << "Document accepted non-tessellatable body add.\n";
        return 1;
    }

    std::cout << "Tessellation cache prototype passed.\n";
    return 0;
}
