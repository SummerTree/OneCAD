/**
 * @file StepExporter.cpp
 * @brief XDE-based STEP export with colors and names
 */

#include "StepExporter.h"
#include "../../app/document/Document.h"

#include <STEPControl_Writer.hxx>
#include <STEPControl_StepModelType.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <XCAFApp_Application.hxx>
#include <TDocStd_Document.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <TDataStd_Name.hxx>
#include <TCollection_ExtendedString.hxx>
#include <Quantity_Color.hxx>

#include <Interface_Static.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Compound.hxx>
#include <BRep_Builder.hxx>

namespace onecad::io {

StepExportResult StepExporter::exportDocument(const QString& filepath,
                                               const app::Document* document) {
    return exportDocument(filepath, document, ExportOptions{});
}

StepExportResult StepExporter::exportDocument(const QString& filepath,
                                               const app::Document* document,
                                               const ExportOptions& options) {
    StepExportResult result;
    if (!document) {
        result.errorMessage = "Document is null";
        return result;
    }

    // Create XDE document for export
    Handle(XCAFApp_Application) app = XCAFApp_Application::GetApplication();
    Handle(TDocStd_Document) doc;
    app->NewDocument("MDTV-XCAF", doc);

    if (doc.IsNull()) {
        result.errorMessage = "Failed to create XDE document for export";
        return result;
    }

    Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
    Handle(XCAFDoc_ColorTool) colorTool = XCAFDoc_DocumentTool::ColorTool(doc->Main());

    for (const auto& bodyId : document->getBodyIds()) {
        if (options.visibleOnly && !document->isBodyVisible(bodyId)) {
            continue;
        }

        const TopoDS_Shape* shape = document->getBodyShape(bodyId);
        if (!shape || shape->IsNull()) {
            continue;
        }

        // Add shape to XDE document
        TDF_Label label = shapeTool->AddShape(*shape);

        // Set body name
        std::string name = document->getBodyName(bodyId);
        if (!name.empty()) {
            TCollection_ExtendedString extName(name.c_str(), Standard_True);
            TDataStd_Name::Set(label, extName);
        }

        // Set per-face colors if available
        const auto* faceColors = document->getBodyFaceColors(bodyId);
        if (faceColors) {
            for (TopExp_Explorer exp(*shape, TopAbs_FACE); exp.More(); exp.Next()) {
                TopoDS_Face face = TopoDS::Face(exp.Current());
                auto ids = document->elementMap().findIdsByShape(face);
                if (ids.empty()) {
                    continue;
                }
                auto cit = faceColors->find(ids.front().value);
                if (cit != faceColors->end()) {
                    const auto& rgba = cit->second;
                    Quantity_Color color(
                        static_cast<double>(rgba[0]),
                        static_cast<double>(rgba[1]),
                        static_cast<double>(rgba[2]),
                        Quantity_TOC_RGB);
                    colorTool->SetColor(face, color, XCAFDoc_ColorSurf);
                }
            }
        }

        result.bodyCount++;
    }

    if (result.bodyCount == 0) {
        result.errorMessage = "No bodies to export";
        app->Close(doc);
        return result;
    }

    // Configure writer
    STEPCAFControl_Writer writer;
    writer.SetColorMode(Standard_True);
    writer.SetNameMode(Standard_True);

    Interface_Static::SetCVal("write.step.schema", options.schema.toStdString().c_str());
    Interface_Static::SetCVal("write.step.unit", "MM");

    if (!writer.Transfer(doc)) {
        result.errorMessage = "Failed to transfer XDE document to STEP writer";
        app->Close(doc);
        return result;
    }

    IFSelect_ReturnStatus writeStatus = writer.Write(filepath.toStdString().c_str());
    if (writeStatus != IFSelect_RetDone) {
        result.errorMessage = QString("Failed to write STEP file: %1").arg(filepath);
        app->Close(doc);
        return result;
    }

    result.success = true;
    app->Close(doc);
    return result;
}

StepExportResult StepExporter::exportShapes(const QString& filepath,
                                             const std::vector<TopoDS_Shape>& shapes) {
    StepExportResult result;
    if (shapes.empty()) {
        result.errorMessage = "No shapes to export";
        return result;
    }

    STEPControl_Writer writer;
    Interface_Static::SetCVal("write.step.unit", "MM");
    Interface_Static::SetCVal("write.step.schema", "AP214IS");

    for (const auto& shape : shapes) {
        if (shape.IsNull()) {
            continue;
        }
        IFSelect_ReturnStatus status = writer.Transfer(shape, STEPControl_AsIs);
        if (status != IFSelect_RetDone) {
            result.errorMessage = QString("Failed to transfer shape %1 to STEP")
                .arg(result.bodyCount + 1);
            return result;
        }
        result.bodyCount++;
    }

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
