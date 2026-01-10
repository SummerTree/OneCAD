/**
 * @file StepImporter.cpp
 * @brief Implementation of STEP import using OCCT
 */

#include "StepImporter.h"
#include "../../app/document/Document.h"

#include <STEPControl_Reader.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Interface_Static.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Shell.hxx>
#include <XCAFDoc_ShapeTool.hxx>

namespace onecad::io {

StepImportResult StepImporter::import(const QString& filepath) {
    StepImportResult result;
    
    // Create STEP reader
    STEPControl_Reader reader;
    
    // Set unit interpretation
    Interface_Static::SetCVal("xstep.cascade.unit", "MM");
    
    // Read file
    IFSelect_ReturnStatus readStatus = reader.ReadFile(filepath.toStdString().c_str());
    
    if (readStatus != IFSelect_RetDone) {
        result.errorMessage = QString("Failed to read STEP file: %1").arg(filepath);
        return result;
    }
    
    // Check for roots
    int numRoots = reader.NbRootsForTransfer();
    if (numRoots == 0) {
        result.errorMessage = "STEP file contains no geometry roots";
        return result;
    }
    
    // Transfer all roots
    reader.TransferRoots();
    
    // Get the combined shape
    TopoDS_Shape combinedShape = reader.OneShape();
    
    if (combinedShape.IsNull()) {
        result.errorMessage = "Failed to transfer geometry from STEP file";
        return result;
    }
    
    // Extract individual solids (per user decision: each solid becomes separate body)
    int bodyIndex = 1;
    TopExp_Explorer solidExp(combinedShape, TopAbs_SOLID);
    
    for (; solidExp.More(); solidExp.Next()) {
        TopoDS_Solid solid = TopoDS::Solid(solidExp.Current());
        if (!solid.IsNull()) {
            ImportedBody body;
            body.shape = solid;
            body.name = QString("Imported Body %1").arg(bodyIndex++);
            result.bodies.push_back(body);
        }
    }
    
    // If no solids, try shells
    if (result.bodies.empty()) {
        TopExp_Explorer shellExp(combinedShape, TopAbs_SHELL);
        for (; shellExp.More(); shellExp.Next()) {
            TopoDS_Shell shell = TopoDS::Shell(shellExp.Current());
            if (!shell.IsNull()) {
                ImportedBody body;
                body.shape = shell;
                body.name = QString("Imported Surface %1").arg(bodyIndex++);
                result.bodies.push_back(body);
            }
        }
    }
    
    // If still empty and we have a compound, use it directly
    if (result.bodies.empty() && !combinedShape.IsNull()) {
        ImportedBody body;
        body.shape = combinedShape;
        body.name = "Imported Geometry";
        result.bodies.push_back(body);
    }
    
    if (result.bodies.empty()) {
        result.errorMessage = "STEP file contains no usable geometry";
        return result;
    }
    
    result.success = true;
    return result;
}

StepImportResult StepImporter::importIntoDocument(const QString& filepath,
                                                   app::Document* document) {
    // Import shapes
    StepImportResult result = import(filepath);
    
    if (!result.success) {
        return result;
    }
    
    // Add each body to document
    for (const auto& body : result.bodies) {
        document->addBody(body.shape);
        
        // Get the ID of the just-added body and set its name
        auto bodyIds = document->getBodyIds();
        if (!bodyIds.empty()) {
            document->setBodyName(bodyIds.back(), body.name.toStdString());
        }
    }
    
    return result;
}

} // namespace onecad::io
