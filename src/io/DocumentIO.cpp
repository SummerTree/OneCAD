/**
 * @file DocumentIO.cpp
 * @brief Implementation of document serialization
 */

#include "DocumentIO.h"
#include "Package.h"
#include "JSONUtils.h"
#include "SketchIO.h"
#include "ElementMapIO.h"
#include "HistoryIO.h"
#include "../app/document/Document.h"
#include "../core/sketch/Sketch.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QFileInfo>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <Standard_Stream.hxx>
#include <sstream>

namespace onecad::io {

bool DocumentIO::saveDocument(Package* package, const app::Document* document) {
    // 1. Create and write document.json
    QJsonObject docJson = createDocumentJson(document);
    QByteArray docData = JSONUtils::toCanonicalJson(docJson);
    if (!package->writeFile("document.json", docData)) {
        return false;
    }
    
    // 2. Save each sketch to sketches/{uuid}.json
    for (const auto& sketchId : document->getSketchIds()) {
        const auto* sketch = document->getSketch(sketchId);
        if (sketch) {
            if (!SketchIO::saveSketch(package, QString::fromStdString(sketchId), sketch)) {
                return false;
            }
        }
    }
    
    // 3. Save body metadata and BREP cache
    for (const auto& bodyId : document->getBodyIds()) {
        QJsonObject bodyJson;
        bodyJson["bodyId"] = QString::fromStdString(bodyId);
        bodyJson["name"] = QString::fromStdString(document->getBodyName(bodyId));
        bodyJson["visible"] = document->isBodyVisible(bodyId);

        QString brepPath = QString("bodies/%1.brep").arg(QString::fromStdString(bodyId));
        bodyJson["brepPath"] = brepPath;

        QString bodyPath = QString("bodies/%1.json").arg(QString::fromStdString(bodyId));
        if (!package->writeFile(bodyPath, JSONUtils::toCanonicalJson(bodyJson))) {
            return false;
        }

        const TopoDS_Shape* shape = document->getBodyShape(bodyId);
        if (!shape || shape->IsNull()) {
            continue;
        }

        std::ostringstream stream;
        BRepTools::Write(*shape, stream);
        QByteArray brepData = QByteArray::fromStdString(stream.str());
        if (!package->writeFile(brepPath, brepData)) {
            return false;
        }
    }
    
    // 4. Save ElementMap
    if (!ElementMapIO::saveElementMap(package, document->elementMap())) {
        return false;
    }
    
    // 5. Save operation history
    if (!HistoryIO::saveHistory(package, document->operations())) {
        return false;
    }
    
    // 6. Save display metadata
    QJsonObject displayJson;
    // TODO: Add camera position, visibility state, etc.
    displayJson["schemaVersion"] = "1.0.0";
    if (!package->writeFile("metadata/display.json", JSONUtils::toCanonicalJson(displayJson))) {
        return false;
    }
    
    return true;
}

std::unique_ptr<app::Document> DocumentIO::loadDocument(Package* package,
                                                         QObject* parent,
                                                         QString& errorMessage) {
    // 1. Read document.json
    QByteArray docData = package->readFile("document.json");
    if (docData.isEmpty()) {
        errorMessage = "Missing document.json";
        return nullptr;
    }
    
    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(docData, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        errorMessage = QString("Invalid JSON in document.json: %1").arg(parseError.errorString());
        return nullptr;
    }
    
    // 2. Create document and parse structure
    auto document = std::make_unique<app::Document>(parent);
    if (!parseDocumentJson(jsonDoc.object(), document.get(), errorMessage)) {
        return nullptr;
    }
    
    // 3. Load sketches
    QStringList sketchFiles = package->listFiles("sketches/");
    for (const QString& sketchFile : sketchFiles) {
        if (!sketchFile.endsWith(".json")) {
            continue;
        }
        QString sketchId = QFileInfo(sketchFile).baseName();
        auto sketch = SketchIO::loadSketch(package, sketchId, errorMessage);
        if (!sketch) {
            // Log warning but continue - partial recovery
            qWarning() << "Failed to load sketch:" << sketchId << "-" << errorMessage;
            continue;
        }
        document->addSketchWithId(sketchId.toStdString(), std::move(sketch));
    }
    
    // 4. Load body metadata and BREP cache
    bool loadedBodies = false;
    QStringList bodyFiles = package->listFiles("bodies/");
    for (const QString& bodyFile : bodyFiles) {
        if (!bodyFile.endsWith(".json")) {
            continue;
        }
        QByteArray bodyData = package->readFile(bodyFile);
        if (bodyData.isEmpty()) {
            continue;
        }
        QJsonDocument bodyDoc = QJsonDocument::fromJson(bodyData);
        QJsonObject bodyJson = bodyDoc.object();

        QString bodyId = bodyJson["bodyId"].toString();
        if (bodyId.isEmpty()) {
            bodyId = QFileInfo(bodyFile).baseName();
        }
        QString bodyName = bodyJson["name"].toString();
        bool visible = bodyJson["visible"].toBool(true);
        QString brepPath = bodyJson["brepPath"].toString();
        if (brepPath.isEmpty()) {
            brepPath = QString("bodies/%1.brep").arg(bodyId);
        }

        QByteArray brepData = package->readFile(brepPath);
        if (brepData.isEmpty()) {
            qWarning() << "Missing BREP data for body:" << bodyId;
            continue;
        }

        TopoDS_Shape shape;
        BRep_Builder builder;
        std::string brepString(brepData.constData(), brepData.size());
        std::istringstream stream(brepString);
        BRepTools::Read(shape, stream, builder);
        if (stream.fail() || shape.IsNull()) {
            qWarning() << "Failed to read BREP for body:" << bodyId;
            continue;
        }

        if (document->addBodyWithId(bodyId.toStdString(), shape)) {
            loadedBodies = true;
            if (!bodyName.isEmpty()) {
                document->setBodyName(bodyId.toStdString(), bodyName.toStdString());
            }
            document->setBodyVisible(bodyId.toStdString(), visible);
        }
    }
    
    // 5. Load ElementMap (skip if bodies already rebuilt from BREP)
    if (!loadedBodies) {
        ElementMapIO::loadElementMap(package, document->elementMap(), errorMessage);
    }
    
    // 6. Load operation history
    HistoryIO::loadHistory(package, document.get(), errorMessage);
    
    document->setModified(false);
    return document;
}

QJsonObject DocumentIO::createDocumentJson(const app::Document* document) {
    QJsonObject json;
    
    // Document metadata
    json["documentId"] = JSONUtils::generateUuid();
    json["name"] = "Untitled";  // TODO: Get from document
    json["units"] = "mm";
    json["createdAt"] = JSONUtils::currentTimestamp();
    json["modifiedAt"] = JSONUtils::currentTimestamp();
    
    // Sketch references
    QJsonArray sketches;
    for (const auto& sketchId : document->getSketchIds()) {
        sketches.append(QString::fromStdString(sketchId));
    }
    json["sketches"] = sketches;
    
    // Body references
    QJsonArray bodies;
    for (const auto& bodyId : document->getBodyIds()) {
        bodies.append(QString::fromStdString(bodyId));
    }
    json["bodies"] = bodies;
    
    // File paths
    QJsonObject history;
    history["opsPath"] = "history/ops.jsonl";
    history["statePath"] = "history/state.json";
    json["history"] = history;
    
    QJsonObject topology;
    topology["elementMapPath"] = "topology/elementmap.json";
    json["topology"] = topology;
    
    return json;
}

bool DocumentIO::parseDocumentJson(const QJsonObject& json, 
                                    app::Document* document,
                                    QString& errorMessage) {
    // Validate required fields
    if (!json.contains("sketches") || !json.contains("bodies")) {
        errorMessage = "Missing required fields in document.json";
        return false;
    }
    
    // Document structure is parsed - sketches/bodies loaded separately
    return true;
}

} // namespace onecad::io
