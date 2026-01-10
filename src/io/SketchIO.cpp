/**
 * @file SketchIO.cpp
 * @brief Implementation of sketch serialization
 */

#include "SketchIO.h"
#include "Package.h"
#include "JSONUtils.h"
#include "core/sketch/Sketch.h"

#include <QJsonDocument>
#include <QJsonArray>

namespace onecad::io {

using namespace core::sketch;

// =============================================================================
// Entity Serialization
// =============================================================================

namespace {

bool looksLikeLegacyEntitySchema(const QJsonObject& entityJson) {
    return entityJson.contains("startPointId") || entityJson.contains("endPointId") ||
           entityJson.contains("centerPointId");
}

QJsonObject migrateLegacyEntity(const QJsonObject& legacy) {
    QJsonObject migrated = legacy;
    if (legacy.contains("startPointId")) {
        migrated["start"] = legacy.value("startPointId");
        migrated.remove("startPointId");
    }
    if (legacy.contains("endPointId")) {
        migrated["end"] = legacy.value("endPointId");
        migrated.remove("endPointId");
    }
    if (legacy.contains("centerPointId")) {
        migrated["center"] = legacy.value("centerPointId");
        migrated.remove("centerPointId");
    }
    return migrated;
}

void ensurePlaneNormal(QJsonObject& plane) {
    if (plane.contains("normal")) {
        return;
    }
    if (!plane.contains("xAxis") || !plane.contains("yAxis")) {
        return;
    }
    QJsonArray x = plane.value("xAxis").toArray();
    QJsonArray y = plane.value("yAxis").toArray();
    if (x.size() != 3 || y.size() != 3) {
        return;
    }
    const double x0 = x.at(0).toDouble();
    const double x1 = x.at(1).toDouble();
    const double x2 = x.at(2).toDouble();
    const double y0 = y.at(0).toDouble();
    const double y1 = y.at(1).toDouble();
    const double y2 = y.at(2).toDouble();
    QJsonArray normal;
    normal.append(x1 * y2 - x2 * y1);
    normal.append(x2 * y0 - x0 * y2);
    normal.append(x0 * y1 - x1 * y0);
    plane["normal"] = normal;
}

} // anonymous namespace

// =============================================================================
// Public API
// =============================================================================

bool SketchIO::saveSketch(Package* package, 
                          const QString& sketchId,
                          const Sketch* sketch) {
    QJsonObject json = serializeSketch(sketchId, sketch);
    QString path = QString("sketches/%1.json").arg(sketchId);
    return package->writeFile(path, JSONUtils::toCanonicalJson(json));
}

std::unique_ptr<Sketch> SketchIO::loadSketch(Package* package,
                                             const QString& sketchId,
                                             QString& errorMessage) {
    QString path = QString("sketches/%1.json").arg(sketchId);
    QByteArray data = package->readFile(path);
    
    if (data.isEmpty()) {
        errorMessage = QString("Sketch file not found: %1").arg(path);
        return nullptr;
    }
    
    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        errorMessage = QString("Invalid JSON in %1: %2").arg(path, parseError.errorString());
        return nullptr;
    }
    
    auto sketch = deserializeSketch(jsonDoc.object(), errorMessage);
    return sketch;
}

QJsonObject SketchIO::serializeSketch(const QString& sketchId,
                                       const Sketch* sketch) {
    QJsonDocument doc = QJsonDocument::fromJson(
        QByteArray::fromStdString(sketch->toJson()));
    QJsonObject json = doc.object();
    json["sketchId"] = sketchId;
    json["schemaVersion"] = "1.0.0";
    return json;
}

std::unique_ptr<Sketch> SketchIO::deserializeSketch(const QJsonObject& json,
                                                     QString& errorMessage) {
    QJsonObject normalized = json;
    bool migrated = false;
    if (normalized.contains("entities") && normalized["entities"].isArray()) {
        QJsonArray entities = normalized["entities"].toArray();
        QJsonArray migratedEntities;
        for (const auto& entityVal : entities) {
            QJsonObject entityJson = entityVal.toObject();
            if (looksLikeLegacyEntitySchema(entityJson)) {
                migrated = true;
                migratedEntities.append(migrateLegacyEntity(entityJson));
            } else {
                migratedEntities.append(entityJson);
            }
        }
        if (migrated) {
            normalized["entities"] = migratedEntities;
        }
    }
    if (normalized.contains("plane") && normalized["plane"].isObject()) {
        QJsonObject plane = normalized["plane"].toObject();
        ensurePlaneNormal(plane);
        normalized["plane"] = plane;
    }

    QJsonDocument doc(normalized);
    auto sketch = Sketch::fromJson(doc.toJson(QJsonDocument::Compact).toStdString());
    if (!sketch) {
        errorMessage = "Failed to deserialize sketch data";
    }
    return sketch;
}

} // namespace onecad::io
