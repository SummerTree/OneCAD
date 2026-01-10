/**
 * @file StepExporter.h
 * @brief STEP file export using OpenCASCADE
 */

#pragma once

#include <QString>
#include <QStringList>
#include <vector>

class TopoDS_Shape;

namespace onecad::app {
class Document;
}

namespace onecad::io {

/**
 * @brief Result of STEP export
 */
struct StepExportResult {
    bool success = false;
    QString errorMessage;
    int bodyCount = 0;
};

/**
 * @brief STEP file export using OCCT's STEPControl_Writer
 */
class StepExporter {
public:
    /**
     * @brief Export all visible bodies to STEP file
     */
    static StepExportResult exportDocument(const QString& filepath,
                                            const app::Document* document);
    
    /**
     * @brief Export specific shapes to STEP file
     */
    static StepExportResult exportShapes(const QString& filepath,
                                          const std::vector<TopoDS_Shape>& shapes);
    
    /**
     * @brief Export single shape to STEP file
     */
    static StepExportResult exportShape(const QString& filepath,
                                         const TopoDS_Shape& shape);
    
private:
    StepExporter() = delete;
};

} // namespace onecad::io
