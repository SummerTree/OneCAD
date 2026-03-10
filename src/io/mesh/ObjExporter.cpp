#include "ObjExporter.h"
#include "StlExporter.h" // for MeshExportResult
#include "../../app/document/Document.h"
#include "../../render/scene/SceneMeshStore.h"

#include <QFile>
#include <QTextStream>

namespace onecad::io {

MeshExportResult ObjExporter::exportDocument(
    const QString& filepath,
    const app::Document* document,
    const ObjExportOptions& options) {

    MeshExportResult result;
    if (!document) {
        result.errorMessage = "No document";
        return result;
    }

    // Collect meshes
    std::vector<const render::SceneMeshStore::Mesh*> meshes;
    const auto& store = document->meshStore();
    store.forEachMesh([&](const render::SceneMeshStore::Mesh& mesh) {
        if (options.visibleOnly && !document->isBodyVisible(mesh.bodyId)) {
            return;
        }
        meshes.push_back(&mesh);
    });

    if (meshes.empty()) {
        result.errorMessage = "No bodies to export";
        return result;
    }

    QFile file(filepath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        result.errorMessage = "Failed to open file: " + filepath;
        return result;
    }

    QTextStream out(&file);
    out << "# OneCAD OBJ Export\n";
    out << "# Bodies: " << meshes.size() << "\n\n";

    uint32_t vertexOffset = 1; // OBJ indices are 1-based
    int totalTriangles = 0;

    for (const auto* mesh : meshes) {
        if (options.groupByBody) {
            QString bodyName = QString::fromStdString(
                document->getBodyName(mesh->bodyId));
            if (bodyName.isEmpty()) {
                bodyName = QString::fromStdString(mesh->bodyId);
            }
            out << "g " << bodyName << "\n";
        }

        // Write vertices
        for (const auto& v : mesh->vertices) {
            out << "v " << v.x() << " " << v.y() << " " << v.z() << "\n";
        }

        // Write normals
        if (options.writeNormals) {
            for (const auto& n : mesh->normals) {
                out << "vn " << n.x() << " " << n.y() << " " << n.z() << "\n";
            }
        }

        // Write faces
        for (const auto& tri : mesh->triangles) {
            uint32_t i0 = tri.i0 + vertexOffset;
            uint32_t i1 = tri.i1 + vertexOffset;
            uint32_t i2 = tri.i2 + vertexOffset;

            if (options.writeNormals) {
                out << "f " << i0 << "//" << i0
                    << " " << i1 << "//" << i1
                    << " " << i2 << "//" << i2 << "\n";
            } else {
                out << "f " << i0 << " " << i1 << " " << i2 << "\n";
            }
            ++totalTriangles;
        }

        vertexOffset += static_cast<uint32_t>(mesh->vertices.size());
        out << "\n";
    }

    result.success = true;
    result.bodyCount = static_cast<int>(meshes.size());
    result.triangleCount = totalTriangles;
    return result;
}

} // namespace onecad::io
