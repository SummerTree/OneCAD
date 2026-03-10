/**
 * @file StepImporter.cpp
 * @brief XDE-based STEP import with colors, names, assembly flattening, and shape healing
 */

#include "StepImporter.h"
#include "../../app/document/Document.h"

#include <STEPCAFControl_Reader.hxx>
#include <XCAFApp_Application.hxx>
#include <TDocStd_Document.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <TDataStd_Name.hxx>
#include <TDF_LabelSequence.hxx>
#include <TCollection_ExtendedString.hxx>
#include <TCollection_AsciiString.hxx>
#include <Quantity_Color.hxx>

#include <IFSelect_ReturnStatus.hxx>
#include <Interface_Static.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Face.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <ShapeFix_Shape.hxx>

#include <map>

namespace onecad::io {

namespace {

void healShape(TopoDS_Shape& shape) {
    BRepCheck_Analyzer checker(shape, Standard_True);
    if (!checker.IsValid()) {
        ShapeFix_Shape fixer(shape);
        fixer.Perform();
        TopoDS_Shape fixed = fixer.Shape();
        if (!fixed.IsNull()) {
            shape = fixed;
        }
    }
}

QString labelName(const TDF_Label& label) {
    Handle(TDataStd_Name) nameAttr;
    if (label.FindAttribute(TDataStd_Name::GetID(), nameAttr)) {
        TCollection_AsciiString ascii(nameAttr->Get());
        return QString::fromUtf8(ascii.ToCString());
    }
    return {};
}

struct CollectedSolid {
    TopoDS_Shape shape;
    QString name;
};

void collectSolids(const Handle(XCAFDoc_ShapeTool)& shapeTool,
                   const TDF_Label& label,
                   const QString& parentName,
                   std::vector<CollectedSolid>& solids) {
    QString name = labelName(label);
    if (name.isEmpty()) {
        name = parentName;
    }

    if (shapeTool->IsAssembly(label)) {
        TDF_LabelSequence components;
        shapeTool->GetComponents(label, components);
        for (int i = 1; i <= components.Length(); ++i) {
            TDF_Label comp = components.Value(i);
            TDF_Label ref;
            if (shapeTool->IsReference(comp)) {
                if (shapeTool->GetReferredShape(comp, ref)) {
                    QString refName = labelName(ref);
                    if (refName.isEmpty()) {
                        refName = labelName(comp);
                    }
                    if (refName.isEmpty()) {
                        refName = name;
                    }
                    // Use GetShape(comp) to get the already-transformed shape
                    TopoDS_Shape compShape = shapeTool->GetShape(comp);
                    if (!compShape.IsNull()) {
                        bool foundSolid = false;
                        int solidIdx = 1;
                        for (TopExp_Explorer solidExp(compShape, TopAbs_SOLID); solidExp.More(); solidExp.Next()) {
                            TopoDS_Solid solid = TopoDS::Solid(solidExp.Current());
                            if (!solid.IsNull()) {
                                QString solidName = refName;
                                if (solidName.isEmpty()) {
                                    solidName = QString("Solid %1").arg(solidIdx);
                                }
                                solids.push_back({solid, solidName});
                                solidIdx++;
                                foundSolid = true;
                            }
                        }
                        if (!foundSolid) {
                            for (TopExp_Explorer shellExp(compShape, TopAbs_SHELL); shellExp.More(); shellExp.Next()) {
                                TopoDS_Shell shell = TopoDS::Shell(shellExp.Current());
                                if (!shell.IsNull()) {
                                    solids.push_back({shell, refName.isEmpty() ? QString("Surface") : refName});
                                    foundSolid = true;
                                }
                            }
                        }
                        if (!foundSolid) {
                            solids.push_back({compShape, refName.isEmpty() ? QString("Geometry") : refName});
                        }
                    }
                    continue;
                }
            }
            collectSolids(shapeTool, comp, name, solids);
        }
        return;
    }

    if (shapeTool->IsSimpleShape(label)) {
        TopoDS_Shape shape = shapeTool->GetShape(label);
        if (shape.IsNull()) {
            return;
        }

        // Extract individual solids
        bool foundSolid = false;
        int solidIdx = 1;
        for (TopExp_Explorer solidExp(shape, TopAbs_SOLID); solidExp.More(); solidExp.Next()) {
            TopoDS_Solid solid = TopoDS::Solid(solidExp.Current());
            if (!solid.IsNull()) {
                QString solidName = name;
                if (solidName.isEmpty()) {
                    solidName = QString("Solid %1").arg(solidIdx);
                }
                solids.push_back({solid, solidName});
                solidIdx++;
                foundSolid = true;
            }
        }

        // Fallback: try shells
        if (!foundSolid) {
            for (TopExp_Explorer shellExp(shape, TopAbs_SHELL); shellExp.More(); shellExp.Next()) {
                TopoDS_Shell shell = TopoDS::Shell(shellExp.Current());
                if (!shell.IsNull()) {
                    solids.push_back({shell, name.isEmpty() ? QString("Surface") : name});
                    foundSolid = true;
                }
            }
        }

        // Last resort: use compound directly
        if (!foundSolid && !shape.IsNull()) {
            solids.push_back({shape, name.isEmpty() ? QString("Geometry") : name});
        }
    }
}

std::vector<FaceColorEntry> extractFaceColors(const TopoDS_Shape& solid,
                                               const Handle(XCAFDoc_ColorTool)& colorTool) {
    std::vector<FaceColorEntry> colors;
    if (colorTool.IsNull()) return colors;
    int faceIndex = 0;
    for (TopExp_Explorer exp(solid, TopAbs_FACE); exp.More(); exp.Next(), ++faceIndex) {
        TopoDS_Face face = TopoDS::Face(exp.Current());
        Quantity_Color color;
        bool found = false;

        if (colorTool->GetColor(face, XCAFDoc_ColorSurf, color)) {
            found = true;
        } else if (colorTool->GetColor(face, XCAFDoc_ColorGen, color)) {
            found = true;
        } else if (colorTool->GetColor(face, XCAFDoc_ColorCurv, color)) {
            found = true;
        }

        if (found) {
            FaceColorEntry entry;
            entry.faceIndex = faceIndex;
            entry.rgba[0] = static_cast<float>(color.Red());
            entry.rgba[1] = static_cast<float>(color.Green());
            entry.rgba[2] = static_cast<float>(color.Blue());
            entry.rgba[3] = 1.0f;
            colors.push_back(entry);
        }
    }
    return colors;
}

} // namespace

StepImportResult StepImporter::import(const QString& filepath) {
    StepImportResult result;

    // Create XCAF application and document
    Handle(XCAFApp_Application) app = XCAFApp_Application::GetApplication();
    Handle(TDocStd_Document) doc;
    app->NewDocument("MDTV-XCAF", doc);

    if (doc.IsNull()) {
        result.errorMessage = "Failed to create XDE document";
        return result;
    }

    // Set unit interpretation to mm
    Interface_Static::SetCVal("xstep.cascade.unit", "MM");

    // Create XCAF STEP reader
    STEPCAFControl_Reader reader;
    reader.SetColorMode(Standard_True);
    reader.SetNameMode(Standard_True);

    IFSelect_ReturnStatus readStatus = reader.ReadFile(filepath.toStdString().c_str());
    if (readStatus != IFSelect_RetDone) {
        result.errorMessage = QString("Failed to read STEP file: %1").arg(filepath);
        app->Close(doc);
        return result;
    }

    if (!reader.Transfer(doc)) {
        result.errorMessage = "Failed to transfer STEP data to XDE document";
        app->Close(doc);
        return result;
    }

    Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
    Handle(XCAFDoc_ColorTool) colorTool = XCAFDoc_DocumentTool::ColorTool(doc->Main());

    // Get free (top-level) shapes
    TDF_LabelSequence roots;
    shapeTool->GetFreeShapes(roots);

    if (roots.Length() == 0) {
        result.errorMessage = "STEP file contains no geometry";
        app->Close(doc);
        return result;
    }

    // Collect all solids by flattening assemblies
    std::vector<CollectedSolid> collectedSolids;
    for (int i = 1; i <= roots.Length(); ++i) {
        collectSolids(shapeTool, roots.Value(i), QString(), collectedSolids);
    }

    // Deduplicate names
    std::map<QString, int> nameCount;
    for (auto& cs : collectedSolids) {
        int& count = nameCount[cs.name];
        if (++count > 1) cs.name += QString(" (%1)").arg(count);
    }

    if (collectedSolids.empty()) {
        result.errorMessage = "STEP file contains no usable geometry";
        app->Close(doc);
        return result;
    }

    // Build ImportedBody for each solid
    int bodyIndex = 1;
    for (auto& cs : collectedSolids) {
        healShape(cs.shape);

        ImportedBody body;
        body.shape = cs.shape;
        body.originalEntityName = cs.name;
        body.name = cs.name.isEmpty()
            ? QString("Imported Body %1").arg(bodyIndex)
            : cs.name;
        body.faceColors = extractFaceColors(cs.shape, colorTool);
        result.bodies.push_back(std::move(body));
        bodyIndex++;
    }

    result.success = true;
    app->Close(doc);
    return result;
}

StepImportResult StepImporter::importIntoDocument(const QString& filepath,
                                                   app::Document* document) {
    StepImportResult result = import(filepath);
    if (!result.success) {
        return result;
    }

    for (const auto& body : result.bodies) {
        std::string bodyId = document->addBody(body.shape);
        if (!bodyId.empty()) {
            document->setBodyName(bodyId, body.name.toStdString());
            document->addBaseBodyId(bodyId);
        }
    }

    return result;
}

} // namespace onecad::io
