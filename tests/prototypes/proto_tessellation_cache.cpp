#include "app/document/Document.h"

#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
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

    std::cout << "Tessellation cache prototype passed.\n";
    return 0;
}
