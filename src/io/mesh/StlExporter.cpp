#include "StlExporter.h"
#include "../../app/document/Document.h"
#include "../../render/scene/SceneMeshStore.h"

#include <QFile>
#include <QDataStream>
#include <QTextStream>
#include <QVector3D>

namespace onecad::io {

namespace {

QVector3D triangleNormal(const QVector3D& v0, const QVector3D& v1, const QVector3D& v2) {
    QVector3D edge1 = v1 - v0;
    QVector3D edge2 = v2 - v0;
    QVector3D n = QVector3D::crossProduct(edge1, edge2);
    float len = n.length();
    if (len < 1e-12f) {
        return {0.0f, 0.0f, 1.0f};
    }
    return n / len;
}

bool writeBinaryStl(const QString& filepath,
                    const std::vector<const render::SceneMeshStore::Mesh*>& meshes,
                    int& totalTriangles) {
    QFile file(filepath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    // Count total triangles
    uint32_t triCount = 0;
    for (const auto* mesh : meshes) {
        triCount += static_cast<uint32_t>(mesh->triangles.size());
    }
    totalTriangles = static_cast<int>(triCount);

    // 80-byte header
    char header[80] = {};
    snprintf(header, sizeof(header), "OneCAD STL Export");
    file.write(header, 80);

    // Triangle count (little-endian)
    file.write(reinterpret_cast<const char*>(&triCount), 4);

    // Triangles
    for (const auto* mesh : meshes) {
        for (const auto& tri : mesh->triangles) {
            const QVector3D& v0 = mesh->vertices[tri.i0];
            const QVector3D& v1 = mesh->vertices[tri.i1];
            const QVector3D& v2 = mesh->vertices[tri.i2];
            QVector3D n = triangleNormal(v0, v1, v2);

            // Normal (3 floats)
            float nf[3] = {n.x(), n.y(), n.z()};
            file.write(reinterpret_cast<const char*>(nf), 12);

            // Vertex 0
            float vf0[3] = {v0.x(), v0.y(), v0.z()};
            file.write(reinterpret_cast<const char*>(vf0), 12);

            // Vertex 1
            float vf1[3] = {v1.x(), v1.y(), v1.z()};
            file.write(reinterpret_cast<const char*>(vf1), 12);

            // Vertex 2
            float vf2[3] = {v2.x(), v2.y(), v2.z()};
            file.write(reinterpret_cast<const char*>(vf2), 12);

            // Attribute byte count
            uint16_t attrib = 0;
            file.write(reinterpret_cast<const char*>(&attrib), 2);
        }
    }
    return true;
}

bool writeAsciiStl(const QString& filepath,
                   const std::vector<const render::SceneMeshStore::Mesh*>& meshes,
                   int& totalTriangles) {
    QFile file(filepath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    out << "solid OneCAD\n";

    int count = 0;
    for (const auto* mesh : meshes) {
        for (const auto& tri : mesh->triangles) {
            const QVector3D& v0 = mesh->vertices[tri.i0];
            const QVector3D& v1 = mesh->vertices[tri.i1];
            const QVector3D& v2 = mesh->vertices[tri.i2];
            QVector3D n = triangleNormal(v0, v1, v2);

            out << "  facet normal " << n.x() << " " << n.y() << " " << n.z() << "\n";
            out << "    outer loop\n";
            out << "      vertex " << v0.x() << " " << v0.y() << " " << v0.z() << "\n";
            out << "      vertex " << v1.x() << " " << v1.y() << " " << v1.z() << "\n";
            out << "      vertex " << v2.x() << " " << v2.y() << " " << v2.z() << "\n";
            out << "    endloop\n";
            out << "  endfacet\n";
            ++count;
        }
    }

    out << "endsolid OneCAD\n";
    totalTriangles = count;
    return true;
}

} // namespace

MeshExportResult StlExporter::exportDocument(
    const QString& filepath,
    const app::Document* document,
    const StlExportOptions& options) {

    MeshExportResult result;
    if (!document) {
        result.errorMessage = "No document";
        return result;
    }

    // Collect visible meshes
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

    bool ok = false;
    if (options.binary) {
        ok = writeBinaryStl(filepath, meshes, result.triangleCount);
    } else {
        ok = writeAsciiStl(filepath, meshes, result.triangleCount);
    }

    if (!ok) {
        result.errorMessage = "Failed to write file: " + filepath;
        return result;
    }

    result.success = true;
    result.bodyCount = static_cast<int>(meshes.size());
    return result;
}

} // namespace onecad::io
