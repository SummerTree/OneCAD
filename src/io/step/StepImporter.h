/**
 * @file StepImporter.h
 * @brief STEP file import using OpenCASCADE
 */

#pragma once

#include <QString>
#include <vector>

#include <TopoDS_Shape.hxx>

namespace onecad::app {
class Document;
}

namespace onecad::io {

/**
 * @brief Information about an imported body
 */
struct ImportedBody {
    TopoDS_Shape shape;
    QString name;
    QString originalEntityName;  // From STEP file if available
};

/**
 * @brief Result of STEP import
 */
struct StepImportResult {
    bool success = false;
    QString errorMessage;
    std::vector<ImportedBody> bodies;
};

/**
 * @brief STEP file import using OCCT's STEPControl_Reader
 * 
 * Per user decision: Imported shapes become new, separate bodies
 * (no parametric history from import operation).
 */
class StepImporter {
public:
    /**
     * @brief Import STEP file and return shapes
     */
    static StepImportResult import(const QString& filepath);
    
    /**
     * @brief Import STEP file directly into document
     * @param document Document to add bodies to
     * @return Import result with body info
     */
    static StepImportResult importIntoDocument(const QString& filepath,
                                                app::Document* document);
    
private:
    StepImporter() = delete;
};

} // namespace onecad::io
