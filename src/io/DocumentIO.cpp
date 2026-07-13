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
#include "../app/document/DatumPlane.h"
#include "../app/history/RegenerationEngine.h"
#include "../core/sketch/Sketch.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFileInfo>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <Standard_ErrorHandler.hxx>
#include <Standard_Failure.hxx>
#include <Standard_Stream.hxx>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace onecad::io {

namespace {

QJsonArray vec3ToJson(const core::sketch::Vec3d& v) {
    return QJsonArray{v.x, v.y, v.z};
}

core::sketch::Vec3d vec3FromJson(const QJsonArray& a, const core::sketch::Vec3d& fallback) {
    if (a.size() != 3) return fallback;
    return {a[0].toDouble(), a[1].toDouble(), a[2].toDouble()};
}

QJsonObject planeToJson(const core::sketch::SketchPlane& p) {
    QJsonObject o;
    o["origin"] = vec3ToJson(p.origin);
    o["xAxis"] = vec3ToJson(p.xAxis);
    o["yAxis"] = vec3ToJson(p.yAxis);
    o["normal"] = vec3ToJson(p.normal);
    return o;
}

core::sketch::SketchPlane planeFromJson(const QJsonObject& o) {
    core::sketch::SketchPlane p = core::sketch::SketchPlane::XY();
    p.origin = vec3FromJson(o["origin"].toArray(), p.origin);
    p.xAxis = vec3FromJson(o["xAxis"].toArray(), p.xAxis);
    p.yAxis = vec3FromJson(o["yAxis"].toArray(), p.yAxis);
    p.normal = vec3FromJson(o["normal"].toArray(), p.normal);
    return p;
}

} // namespace


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
    if (!HistoryIO::saveHistory(package, document)) {
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
    
    // 4. Load operation history first (determines if we regenerate or load BREP)
    QString historyError;
    HistoryIO::loadHistory(package, document.get(), historyError);

    // 4a. Legacy compatibility: face/push-pull extrudes (FaceRef input) were
    // removed — the op still parses but would hard-fail regeneration. Suppress
    // it now (preserves dependency links; regeneration skips it); the
    // user-facing failure message is recorded AFTER the load-time regeneration,
    // which clears operation failures.
    std::vector<std::string> legacyFaceRefOps;
    for (const auto& op : document->operations()) {
        if (op.type == app::OperationType::Extrude &&
            std::holds_alternative<app::FaceRef>(op.input)) {
            document->setOperationSuppressed(op.opId, true);
            legacyFaceRefOps.push_back(op.opId);
            qWarning() << "Suppressed legacy FaceRef extrude on load:"
                       << QString::fromStdString(op.opId);
        }
    }

    // 4b. Load ElementMap for stable topology references (if present)
    QString elementMapError;
    ElementMapIO::loadElementMap(package, document->elementMap(), elementMapError);

    struct BodyMeta {
        QString name;
        bool visible = true;
        QString brepPath;
    };

    std::unordered_map<std::string, BodyMeta> bodyMeta;
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
        if (bodyId.isEmpty()) {
            continue;
        }

        BodyMeta meta;
        meta.name = bodyJson["name"].toString();
        meta.visible = bodyJson["visible"].toBool(true);
        meta.brepPath = bodyJson["brepPath"].toString();
        if (meta.brepPath.isEmpty()) {
            meta.brepPath = QString("bodies/%1.brep").arg(bodyId);
        }

        bodyMeta[bodyId.toStdString()] = meta;
    }

    auto loadBodyFromBrep = [&](const std::string& bodyId, const BodyMeta& meta) {
        QByteArray brepData = package->readFile(meta.brepPath);
        if (brepData.isEmpty()) {
            qWarning() << "Missing BREP data for body:" << QString::fromStdString(bodyId);
            return false;
        }

        TopoDS_Shape shape;
        BRep_Builder builder;
        std::string brepString(brepData.constData(), brepData.size());
        std::istringstream stream(brepString);
        // Corrupt/truncated .brep raises OCCT exceptions from BRepTools::Read;
        // a bad body must degrade to a load warning, never crash the open.
        try {
            OCC_CATCH_SIGNALS
            BRepTools::Read(shape, stream, builder);
        } catch (const Standard_Failure& failure) {
            qWarning() << "Corrupt BREP for body:" << QString::fromStdString(bodyId)
                       << (failure.GetMessageString() ? failure.GetMessageString() : "OCCT failure");
            return false;
        } catch (const std::exception& ex) {
            qWarning() << "Corrupt BREP for body:" << QString::fromStdString(bodyId) << ex.what();
            return false;
        } catch (...) {
            qWarning() << "Corrupt BREP for body:" << QString::fromStdString(bodyId);
            return false;
        }
        if (stream.fail() || shape.IsNull()) {
            qWarning() << "Failed to read BREP for body:" << QString::fromStdString(bodyId);
            return false;
        }

        if (!document->addBodyWithId(bodyId, shape, meta.name.toStdString())) {
            return false;
        }
        document->setBodyVisible(bodyId, meta.visible);
        return true;
    };

    // 5. If operations exist, regenerate from history (seed base bodies from BREP)
    if (!document->operations().empty()) {
        std::unordered_set<std::string> createdBodies;
        for (const auto& op : document->operations()) {
            bool createsBody = false;
            if (op.type == app::OperationType::Extrude) {
                if (std::holds_alternative<app::ExtrudeParams>(op.params)) {
                    const auto& params = std::get<app::ExtrudeParams>(op.params);
                    createsBody = (params.booleanMode == app::BooleanMode::NewBody) &&
                                   std::holds_alternative<app::SketchRegionRef>(op.input);
                }
            } else if (op.type == app::OperationType::Revolve) {
                if (std::holds_alternative<app::RevolveParams>(op.params)) {
                    const auto& params = std::get<app::RevolveParams>(op.params);
                    createsBody = (params.booleanMode == app::BooleanMode::NewBody) &&
                                   std::holds_alternative<app::SketchRegionRef>(op.input);
                }
            }

            if (createsBody) {
                for (const auto& bodyId : op.resultBodyIds) {
                    createdBodies.insert(bodyId);
                }
            }
        }

        std::unordered_set<std::string> baseBodies;
        for (const auto& [bodyId, meta] : bodyMeta) {
            if (createdBodies.find(bodyId) != createdBodies.end()) {
                continue;
            }
            if (loadBodyFromBrep(bodyId, meta)) {
                baseBodies.insert(bodyId);
            }
        }
        document->setBaseBodyIds(baseBodies);

        app::history::RegenerationEngine regen(document.get());
        auto result = regen.regenerateToAppliedCount(document->appliedOpCount());

        if (result.status == app::history::RegenStatus::CriticalFailure) {
            // All ops failed - store for UI to show RegenFailureDialog
            QString failedOps;
            for (const auto& f : result.failedOps) {
                if (!failedOps.isEmpty()) failedOps += "; ";
                failedOps += QString::fromStdString(f.opId + ": " + f.errorMessage);
            }
            if (failedOps.isEmpty()) {
                errorMessage = "Regeneration failed: dependency cycle or invalid history";
            } else {
                errorMessage = QString("Regeneration failed: %1").arg(failedOps);
            }
            // Continue anyway - partial document may be usable
        } else if (result.status == app::history::RegenStatus::PartialFailure) {
            // Some ops failed - log warning
            qWarning() << "Some operations failed during regeneration:";
            for (const auto& f : result.failedOps) {
                qWarning() << "  " << QString::fromStdString(f.opId)
                           << ":" << QString::fromStdString(f.errorMessage);
            }
        }

        // Surface the legacy FaceRef suppression through the regen-failure UI
        // (recorded post-regeneration; regeneration clears operation failures).
        for (const auto& opId : legacyFaceRefOps) {
            document->setOperationFailed(
                opId,
                "Legacy face extrude (push/pull) is no longer supported; "
                "recreate it as a sketch-on-face extrude");
        }

        // Apply metadata for regenerated bodies (including base bodies)
        for (const auto& [bodyId, meta] : bodyMeta) {
            if (document->getBodyShape(bodyId)) {
                if (!meta.name.isEmpty()) {
                    document->setBodyName(bodyId, meta.name.toStdString());
                }
                document->setBodyVisible(bodyId, meta.visible);
            }
        }
    } else {
        // 5b. No operations - fallback to BREP cache (backward compat)
        bool loadedBodies = false;
        std::unordered_set<std::string> baseBodies;
        for (const auto& [bodyId, meta] : bodyMeta) {
            if (loadBodyFromBrep(bodyId, meta)) {
                loadedBodies = true;
                baseBodies.insert(bodyId);
            }
        }
        if (loadedBodies) {
            document->setBaseBodyIds(baseBodies);
        }
    }

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

    // Datum (reference) planes — stored inline with their resolved frame so loads
    // need no re-derivation (robust to load ordering vs regeneration).
    QJsonArray datums;
    for (const auto& id : document->getDatumPlaneIds()) {
        const app::DatumPlane* d = document->getDatumPlane(id);
        if (!d) {
            continue;
        }
        QJsonObject dj;
        dj["id"] = QString::fromStdString(d->id);
        dj["name"] = QString::fromStdString(d->name);
        dj["kind"] = QString::fromStdString(app::datumPlaneKindName(d->kind));
        dj["basePlaneId"] = QString::fromStdString(d->basePlaneId);
        dj["baseBodyId"] = QString::fromStdString(d->baseBodyId);
        dj["baseFaceId"] = QString::fromStdString(d->baseFaceId);
        dj["axisEdgeId"] = QString::fromStdString(d->axisEdgeId);
        dj["offset"] = d->offset;
        dj["angleDeg"] = d->angleDeg;
        dj["resolvedValid"] = d->resolvedValid;
        dj["resolvedPlane"] = planeToJson(d->resolvedPlane);
        datums.append(dj);
    }
    json["datumPlanes"] = datums;

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

    // Datum planes are restored verbatim (with their saved resolved frame); they are
    // not recomputed here because referenced faces may only exist after regeneration.
    if (document && json.contains("datumPlanes")) {
        for (const auto& v : json["datumPlanes"].toArray()) {
            const QJsonObject dj = v.toObject();
            app::DatumPlane d;
            d.id = dj["id"].toString().toStdString();
            d.name = dj["name"].toString().toStdString();
            d.kind = app::datumPlaneKindFromName(dj["kind"].toString().toStdString());
            d.basePlaneId = dj["basePlaneId"].toString().toStdString();
            d.baseBodyId = dj["baseBodyId"].toString().toStdString();
            d.baseFaceId = dj["baseFaceId"].toString().toStdString();
            d.axisEdgeId = dj["axisEdgeId"].toString().toStdString();
            d.offset = dj["offset"].toDouble();
            d.angleDeg = dj["angleDeg"].toDouble();
            d.resolvedValid = dj["resolvedValid"].toBool();
            d.resolvedPlane = planeFromJson(dj["resolvedPlane"].toObject());
            document->addDatumPlane(std::move(d), /*recompute=*/false);
        }
    }

    // Document structure is parsed - sketches/bodies loaded separately
    return true;
}

} // namespace onecad::io
