/**
 * @file ElementMapIO.cpp
 * @brief Implementation of ElementMap serialization
 */

#include "ElementMapIO.h"
#include "Package.h"
#include "JSONUtils.h"
#include "../kernel/elementmap/ElementMap.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <cmath>
#include <optional>
#include <gp_Vec.hxx>

namespace onecad::io {

using namespace kernel::elementmap;

namespace {

QString kindToString(ElementKind kind) {
    switch (kind) {
        case ElementKind::Body: return "Body";
        case ElementKind::Face: return "Face";
        case ElementKind::Edge: return "Edge";
        case ElementKind::Vertex: return "Vertex";
        default: return "Unknown";
    }
}

ElementKind stringToKind(const QString& str) {
    if (str == "Body") return ElementKind::Body;
    if (str == "Face") return ElementKind::Face;
    if (str == "Edge") return ElementKind::Edge;
    if (str == "Vertex") return ElementKind::Vertex;
    return ElementKind::Unknown;
}

bool isValidShapeType(int value) {
    return value >= static_cast<int>(TopAbs_COMPOUND) &&
        value <= static_cast<int>(TopAbs_SHAPE);
}

bool isValidSurfaceType(int value) {
    return value >= static_cast<int>(GeomAbs_Plane) &&
        value <= static_cast<int>(GeomAbs_OtherSurface);
}

bool isValidCurveType(int value) {
    return value >= static_cast<int>(GeomAbs_Line) &&
        value <= static_cast<int>(GeomAbs_OtherCurve);
}

bool extractVec3(const QJsonObject& json, const char* key,
                 double& x, double& y, double& z, QString& error) {
    if (!json.contains(key)) {
        error = QString("Missing %1").arg(key);
        return false;
    }
    QJsonValue value = json.value(key);
    if (!value.isArray()) {
        error = QString("%1 must be an array").arg(key);
        return false;
    }
    QJsonArray arr = value.toArray();
    if (arr.size() != 3 || !arr[0].isDouble() || !arr[1].isDouble() || !arr[2].isDouble()) {
        error = QString("%1 must contain three numeric values").arg(key);
        return false;
    }
    x = arr[0].toDouble();
    y = arr[1].toDouble();
    z = arr[2].toDouble();
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
        error = QString("%1 contains non-finite values").arg(key);
        return false;
    }
    return true;
}

bool parseDir(const QJsonObject& json, const char* key, gp_Dir& out, QString& error) {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    if (!extractVec3(json, key, x, y, z, error)) {
        return false;
    }
    gp_Vec vec(x, y, z);
    if (vec.Magnitude() <= 1e-12) {
        error = QString("%1 vector magnitude is too small").arg(key);
        return false;
    }
    out = gp_Dir(vec);
    return true;
}

QJsonObject serializeDescriptor(const ElementDescriptor& desc) {
    QJsonObject json;
    json["shapeType"] = static_cast<int>(desc.shapeType);
    json["surfaceType"] = static_cast<int>(desc.surfaceType);
    json["curveType"] = static_cast<int>(desc.curveType);
    
    // Center point
    QJsonArray center;
    center.append(desc.center.X());
    center.append(desc.center.Y());
    center.append(desc.center.Z());
    json["center"] = center;
    
    json["size"] = desc.size;
    json["magnitude"] = desc.magnitude;
    
    // Normal (if present)
    if (desc.hasNormal) {
        QJsonArray normal;
        normal.append(desc.normal.X());
        normal.append(desc.normal.Y());
        normal.append(desc.normal.Z());
        json["normal"] = normal;
        json["hasNormal"] = true;
    }
    
    // Tangent (if present)
    if (desc.hasTangent) {
        QJsonArray tangent;
        tangent.append(desc.tangent.X());
        tangent.append(desc.tangent.Y());
        tangent.append(desc.tangent.Z());
        json["tangent"] = tangent;
        json["hasTangent"] = true;
    }
    
    json["adjacencyHash"] = QString::number(desc.adjacencyHash, 16);
    
    return json;
}

std::optional<ElementDescriptor> deserializeDescriptor(const QJsonObject& json, QString& error) {
    ElementDescriptor desc;

    if (!json.contains("shapeType") || !json["shapeType"].isDouble() ||
        !json.contains("surfaceType") || !json["surfaceType"].isDouble() ||
        !json.contains("curveType") || !json["curveType"].isDouble()) {
        error = "Descriptor missing or invalid type fields";
        return std::nullopt;
    }

    const int shapeTypeValue = json["shapeType"].toInt();
    const int surfaceTypeValue = json["surfaceType"].toInt();
    const int curveTypeValue = json["curveType"].toInt();

    if (!isValidShapeType(shapeTypeValue) ||
        !isValidSurfaceType(surfaceTypeValue) ||
        !isValidCurveType(curveTypeValue)) {
        error = "Descriptor type fields out of range";
        return std::nullopt;
    }

    desc.shapeType = static_cast<TopAbs_ShapeEnum>(shapeTypeValue);
    desc.surfaceType = static_cast<GeomAbs_SurfaceType>(surfaceTypeValue);
    desc.curveType = static_cast<GeomAbs_CurveType>(curveTypeValue);

    double cx = 0.0;
    double cy = 0.0;
    double cz = 0.0;
    if (!extractVec3(json, "center", cx, cy, cz, error)) {
        return std::nullopt;
    }
    desc.center = gp_Pnt(cx, cy, cz);

    if (!json.contains("size") || !json["size"].isDouble() ||
        !json.contains("magnitude") || !json["magnitude"].isDouble()) {
        error = "Descriptor missing size or magnitude";
        return std::nullopt;
    }

    desc.size = json["size"].toDouble();
    desc.magnitude = json["magnitude"].toDouble();
    if (!std::isfinite(desc.size) || desc.size < 0.0 ||
        !std::isfinite(desc.magnitude) || desc.magnitude < 0.0) {
        error = "Descriptor size or magnitude invalid";
        return std::nullopt;
    }

    if (json.contains("normal")) {
        if (!parseDir(json, "normal", desc.normal, error)) {
            return std::nullopt;
        }
        desc.hasNormal = true;
    }

    if (json.contains("tangent")) {
        if (!parseDir(json, "tangent", desc.tangent, error)) {
            return std::nullopt;
        }
        desc.hasTangent = true;
    }

    if (json.contains("adjacencyHash")) {
        if (!json["adjacencyHash"].isString()) {
            error = "adjacencyHash must be a hex string";
            return std::nullopt;
        }
        bool ok = false;
        desc.adjacencyHash = json["adjacencyHash"].toString().toULongLong(&ok, 16);
        if (!ok) {
            error = "adjacencyHash has invalid format";
            return std::nullopt;
        }
    }

    return desc;
}

} // anonymous namespace

bool ElementMapIO::saveElementMap(Package* package,
                                   const ElementMap& elementMap) {
    QJsonObject json = serializeElementMap(elementMap);
    return package->writeFile("topology/elementmap.json", JSONUtils::toCanonicalJson(json));
}

bool ElementMapIO::loadElementMap(Package* package,
                                   ElementMap& elementMap,
                                   QString& errorMessage) {
    QByteArray data = package->readFile("topology/elementmap.json");
    if (data.isEmpty()) {
        // Not an error - new document may not have ElementMap
        return true;
    }
    
    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        errorMessage = QString("Invalid JSON in elementmap.json: %1").arg(parseError.errorString());
        return false;
    }
    
    return deserializeElementMap(jsonDoc.object(), elementMap, errorMessage);
}

QJsonObject ElementMapIO::serializeElementMap(const ElementMap& elementMap) {
    QJsonObject json;
    
    // Metadata
    json["schemaVersion"] = "1.0.0";
    json["hashAlgorithm"] = "fnv1a64";
    json["quantizationEpsilon"] = 1e-6;
    
    // Serialize all entries
    QJsonArray entries;
    for (const auto& id : elementMap.ids()) {
        const Entry* entry = elementMap.find(id);
        if (!entry) continue;
        
        QJsonObject entryJson;
        entryJson["id"] = QString::fromStdString(id.value);
        entryJson["kind"] = kindToString(entry->kind);
        entryJson["opId"] = QString::fromStdString(entry->opId);
        
        // Sources
        QJsonArray sources;
        for (const auto& sourceId : entry->sources) {
            sources.append(QString::fromStdString(sourceId.value));
        }
        entryJson["sources"] = sources;
        
        // Descriptor
        entryJson["descriptor"] = serializeDescriptor(entry->descriptor);
        
        entries.append(entryJson);
    }
    json["entries"] = entries;
    
    return json;
}

bool ElementMapIO::deserializeElementMap(const QJsonObject& json,
                                          ElementMap& elementMap,
                                          QString& errorMessage) {
    // Check schema version
    QString version = json["schemaVersion"].toString();
    if (!version.startsWith("1.")) {
        errorMessage = QString("Unsupported ElementMap schema version: %1").arg(version);
        return false;
    }

    if (!json.contains("entries") || !json["entries"].isArray()) {
        errorMessage = "ElementMap entries array missing or invalid";
        return false;
    }
    
    // Parse entries
    ElementMap tempMap;
    QJsonArray entries = json["entries"].toArray();
    for (const auto& entryVal : entries) {
        QJsonObject entryJson = entryVal.toObject();

        if (!entryJson.contains("id") || !entryJson["id"].isString() ||
            !entryJson.contains("kind") || !entryJson["kind"].isString() ||
            !entryJson.contains("opId") || !entryJson["opId"].isString() ||
            !entryJson.contains("sources") || !entryJson["sources"].isArray() ||
            !entryJson.contains("descriptor") || !entryJson["descriptor"].isObject()) {
            errorMessage = "ElementMap entry missing required fields";
            return false;
        }
        
        ElementId id = ElementId::From(entryJson["id"].toString().toStdString());
        if (id.value.empty()) {
            errorMessage = "Invalid ElementMap entry id";
            return false;
        }
        ElementKind kind = stringToKind(entryJson["kind"].toString());
        if (kind == ElementKind::Unknown) {
            errorMessage = QString("Unsupported ElementMap kind: %1").arg(entryJson["kind"].toString());
            return false;
        }
        std::string opId = entryJson["opId"].toString().toStdString();
        
        // Parse sources
        std::vector<ElementId> sources;
        QJsonArray sourcesArr = entryJson["sources"].toArray();
        for (const auto& sourceVal : sourcesArr) {
            if (!sourceVal.isString()) {
                errorMessage = "Invalid ElementMap source id";
                return false;
            }
            ElementId sourceId = ElementId::From(sourceVal.toString().toStdString());
            if (sourceId.value.empty()) {
                errorMessage = "ElementMap source id is empty";
                return false;
            }
            sources.push_back(sourceId);
        }
        
        // Parse descriptor
        QString descriptorError;
        auto descriptorOpt = deserializeDescriptor(entryJson["descriptor"].toObject(), descriptorError);
        if (!descriptorOpt) {
            errorMessage = QString("Invalid ElementMap descriptor for %1: %2")
                .arg(QString::fromStdString(id.value))
                .arg(descriptorError);
            return false;
        }
        
        // Register entry (without shape - will be rebuilt from history)
        tempMap.registerEntry(id, kind, *descriptorOpt, opId, sources);
    }
    
    elementMap.clear();
    for (const auto& id : tempMap.ids()) {
        const Entry* entry = tempMap.find(id);
        if (!entry) {
            continue;
        }
        elementMap.registerEntry(entry->id, entry->kind, entry->descriptor, entry->opId, entry->sources);
    }
    return true;
}

} // namespace onecad::io
