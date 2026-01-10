/**
 * @file StepExporter.cpp
 * @brief Implementation of STEP export using OCCT
 */

#include "StepExporter.h"
#include "../../app/document/Document.h"

#include <STEPControl_Writer.hxx>
#include <STEPControl_StepModelType.hxx>
#include <Interface_Static.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <TopoDS_Compound.hxx>
#include <BRep_Builder.hxx>

namespace onecad::io {

StepExportResult StepExporter::exportDocument(const QString& filepath,
                                               const app::Document* document) {
    StepExportResult result;
    if (!document) {
        result.errorMessage = "Document is null";
        return result;
    }
    std::vector<TopoDS_Shape> shapes;
    
    // Collect all visible bodies
    for (const auto& bodyId : document->getBodyIds()) {
        if (document->isBodyVisible(bodyId)) {
            const TopoDS_Shape* shape = document->getBodyShape(bodyId);
            if (shape && !shape->IsNull()) {
                shapes.push_back(*shape);
            }
        }
    }
    
    if (shapes.empty()) {
        result.errorMessage = "No visible bodies to export";
        return result;
    }
    
    return exportShapes(filepath, shapes);
}

StepExportResult StepExporter::exportShapes(const QString& filepath,
                                             const std::vector<TopoDS_Shape>& shapes) {
    StepExportResult result;
    
    if (shapes.empty()) {
        result.errorMessage = "No shapes to export";
        return result;
    }
    
    // Configure STEP writer
    STEPControl_Writer writer;
    
    // Set unit to mm (standard for CAD)
    Interface_Static::SetCVal("write.step.unit", "MM");
    
    // Set schema to AP214 (most compatible)
    Interface_Static::SetCVal("write.step.schema", "AP214");
    
    // Transfer each shape
    for (const auto& shape : shapes) {
        if (shape.IsNull()) continue;
        
        IFSelect_ReturnStatus status = writer.Transfer(shape, STEPControl_AsIs);
        if (status != IFSelect_RetDone) {
            result.errorMessage = QString("Failed to transfer shape %1 to STEP")
                .arg(result.bodyCount + 1);
            return result;
        }
        result.bodyCount++;
    }
    
    // Write file
    IFSelect_ReturnStatus writeStatus = writer.Write(filepath.toStdString().c_str());
    
    if (writeStatus != IFSelect_RetDone) {
        result.errorMessage = QString("Failed to write STEP file: %1").arg(filepath);
        return result;
    }
    
    result.success = true;
    return result;
}

StepExportResult StepExporter::exportShape(const QString& filepath,
                                            const TopoDS_Shape& shape) {
    return exportShapes(filepath, {shape});
}

} // namespace onecad::io
