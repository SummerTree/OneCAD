/**
 * @file StepImporter.h
 * @brief STEP file import using OpenCASCADE XDE path
 */

#pragma once

#include <QString>
#include <array>
#include <vector>

#include <TopoDS_Shape.hxx>

namespace onecad::app {
class Document;
}

namespace onecad::io {

struct FaceColorEntry {
    int faceIndex = 0;
    std::array<float, 4> rgba{0, 0, 0, 0};  // linear RGBA; alpha=0 = use default
};

struct ImportedBody {
    TopoDS_Shape shape;
    QString name;
    QString originalEntityName;
    std::vector<FaceColorEntry> faceColors;
};

struct StepImportResult {
    bool success = false;
    QString errorMessage;
    std::vector<ImportedBody> bodies;
};

/**
 * @brief STEP file import using OCCT's STEPCAFControl_Reader (XDE path)
 *
 * Imported shapes become new, separate bodies (no parametric history).
 * Extracts names, per-face colors, and flattens assemblies.
 */
class StepImporter {
public:
    static StepImportResult import(const QString& filepath);

    static StepImportResult importIntoDocument(const QString& filepath,
                                                app::Document* document);

private:
    StepImporter() = delete;
};

} // namespace onecad::io
