/**
 * @file HistoryIO.cpp
 * @brief Implementation of operation history serialization
 */

#include "HistoryIO.h"
#include "Package.h"
#include "JSONUtils.h"
#include "../app/document/Document.h"
#include "../app/document/OperationRecord.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QCryptographicHash>

namespace onecad::io {

using namespace app;

namespace {

QString operationTypeToString(OperationType type) {
    switch (type) {
        case OperationType::Extrude: return "Extrude";
        case OperationType::Revolve: return "Revolve";
        default: return "Unknown";
    }
}

OperationType stringToOperationType(const QString& str) {
    if (str == "Extrude") return OperationType::Extrude;
    if (str == "Revolve") return OperationType::Revolve;
    return OperationType::Extrude;  // Default
}

QString booleanModeToString(BooleanMode mode) {
    switch (mode) {
        case BooleanMode::NewBody: return "NewBody";
        case BooleanMode::Add: return "Add";
        case BooleanMode::Cut: return "Cut";
        case BooleanMode::Intersect: return "Intersect";
        default: return "NewBody";
    }
}

BooleanMode stringToBooleanMode(const QString& str) {
    if (str == "NewBody") return BooleanMode::NewBody;
    if (str == "Add") return BooleanMode::Add;
    if (str == "Cut") return BooleanMode::Cut;
    if (str == "Intersect") return BooleanMode::Intersect;
    return BooleanMode::NewBody;
}

} // anonymous namespace

bool HistoryIO::saveHistory(Package* package,
                            const std::vector<OperationRecord>& operations) {
    // Write ops.jsonl - one JSON object per line
    QByteArray opsData;
    for (const auto& op : operations) {
        QJsonObject opJson = serializeOperation(op);
        QJsonDocument doc(opJson);
        opsData.append(doc.toJson(QJsonDocument::Compact));
        opsData.append('\n');
    }
    
    if (!package->writeFile("history/ops.jsonl", opsData)) {
        return false;
    }
    
    // Write state.json - undo/redo cursor
    QJsonObject stateJson;
    QJsonObject cursor;
    cursor["appliedOpCount"] = static_cast<int>(operations.size());
    if (!operations.empty()) {
        cursor["lastAppliedOpId"] = QString::fromStdString(operations.back().opId);
    }
    stateJson["cursor"] = cursor;
    stateJson["suppressedOps"] = QJsonArray();
    
    return package->writeFile("history/state.json", JSONUtils::toCanonicalJson(stateJson));
}

bool HistoryIO::loadHistory(Package* package,
                            Document* document,
                            QString& errorMessage) {
    // Read ops.jsonl
    QByteArray opsData = package->readFile("history/ops.jsonl");
    if (opsData.isEmpty()) {
        // Not an error - new document may not have history
        return true;
    }
    
    // Parse JSONL (one JSON object per line)
    QList<QByteArray> lines = opsData.split('\n');
    for (const QByteArray& line : lines) {
        if (line.trimmed().isEmpty()) continue;
        
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            errorMessage = QString("Invalid JSON in ops.jsonl: %1").arg(parseError.errorString());
            return false;
        }
        
        OperationRecord op = deserializeOperation(doc.object());
        document->addOperation(op);
    }
    
    return true;
}

QJsonObject HistoryIO::serializeOperation(const OperationRecord& op) {
    QJsonObject json;
    
    json["opId"] = QString::fromStdString(op.opId);
    json["type"] = operationTypeToString(op.type);
    
    // Serialize input
    QJsonObject inputs;
    if (std::holds_alternative<SketchRegionRef>(op.input)) {
        const auto& ref = std::get<SketchRegionRef>(op.input);
        QJsonObject sketch;
        sketch["sketchId"] = QString::fromStdString(ref.sketchId);
        sketch["regionId"] = QString::fromStdString(ref.regionId);
        inputs["sketch"] = sketch;
    }
    else if (std::holds_alternative<FaceRef>(op.input)) {
        const auto& ref = std::get<FaceRef>(op.input);
        QJsonObject face;
        face["bodyId"] = QString::fromStdString(ref.bodyId);
        face["faceId"] = QString::fromStdString(ref.faceId);
        inputs["face"] = face;
    }
    json["inputs"] = inputs;
    
    // Serialize parameters
    QJsonObject params;
    if (std::holds_alternative<ExtrudeParams>(op.params)) {
        const auto& p = std::get<ExtrudeParams>(op.params);
        params["distance"] = p.distance;
        params["draftAngleDeg"] = p.draftAngleDeg;
        params["booleanMode"] = booleanModeToString(p.booleanMode);
    }
    else if (std::holds_alternative<RevolveParams>(op.params)) {
        const auto& p = std::get<RevolveParams>(op.params);
        params["angleDeg"] = p.angleDeg;
        params["booleanMode"] = booleanModeToString(p.booleanMode);
        
        // Serialize axis reference
        if (std::holds_alternative<SketchLineRef>(p.axis)) {
            const auto& axis = std::get<SketchLineRef>(p.axis);
            QJsonObject axisJson;
            axisJson["sketchId"] = QString::fromStdString(axis.sketchId);
            axisJson["lineId"] = QString::fromStdString(axis.lineId);
            params["axisSketchLine"] = axisJson;
        }
        else if (std::holds_alternative<EdgeRef>(p.axis)) {
            const auto& axis = std::get<EdgeRef>(p.axis);
            QJsonObject axisJson;
            axisJson["bodyId"] = QString::fromStdString(axis.bodyId);
            axisJson["edgeId"] = QString::fromStdString(axis.edgeId);
            params["axisEdge"] = axisJson;
        }
    }
    json["params"] = params;
    
    // Serialize outputs
    QJsonArray resultBodies;
    for (const auto& bodyId : op.resultBodyIds) {
        resultBodies.append(QString::fromStdString(bodyId));
    }
    json["resultBodyIds"] = resultBodies;
    
    return json;
}

OperationRecord HistoryIO::deserializeOperation(const QJsonObject& json) {
    OperationRecord op;
    
    op.opId = json["opId"].toString().toStdString();
    op.type = stringToOperationType(json["type"].toString());
    
    // Parse inputs
    QJsonObject inputs = json["inputs"].toObject();
    if (inputs.contains("sketch")) {
        QJsonObject sketch = inputs["sketch"].toObject();
        SketchRegionRef ref;
        ref.sketchId = sketch["sketchId"].toString().toStdString();
        ref.regionId = sketch["regionId"].toString().toStdString();
        op.input = ref;
    }
    else if (inputs.contains("face")) {
        QJsonObject face = inputs["face"].toObject();
        FaceRef ref;
        ref.bodyId = face["bodyId"].toString().toStdString();
        ref.faceId = face["faceId"].toString().toStdString();
        op.input = ref;
    }
    
    // Parse parameters
    QJsonObject params = json["params"].toObject();
    
    if (op.type == OperationType::Extrude) {
        ExtrudeParams p;
        p.distance = params["distance"].toDouble();
        p.draftAngleDeg = params["draftAngleDeg"].toDouble();
        p.booleanMode = stringToBooleanMode(params["booleanMode"].toString());
        op.params = p;
    }
    else if (op.type == OperationType::Revolve) {
        RevolveParams p;
        p.angleDeg = params["angleDeg"].toDouble();
        p.booleanMode = stringToBooleanMode(params["booleanMode"].toString());
        
        if (params.contains("axisSketchLine")) {
            QJsonObject axisJson = params["axisSketchLine"].toObject();
            SketchLineRef axis;
            axis.sketchId = axisJson["sketchId"].toString().toStdString();
            axis.lineId = axisJson["lineId"].toString().toStdString();
            p.axis = axis;
        }
        else if (params.contains("axisEdge")) {
            QJsonObject axisJson = params["axisEdge"].toObject();
            EdgeRef axis;
            axis.bodyId = axisJson["bodyId"].toString().toStdString();
            axis.edgeId = axisJson["edgeId"].toString().toStdString();
            p.axis = axis;
        }
        
        op.params = p;
    }
    
    // Parse result bodies
    QJsonArray resultBodies = json["resultBodyIds"].toArray();
    for (const auto& bodyVal : resultBodies) {
        op.resultBodyIds.push_back(bodyVal.toString().toStdString());
    }
    
    return op;
}

QString HistoryIO::computeOpsHash(const std::vector<OperationRecord>& operations) {
    QCryptographicHash hash(QCryptographicHash::Sha256);
    
    for (const auto& op : operations) {
        QJsonObject opJson = serializeOperation(op);
        QJsonDocument doc(opJson);
        hash.addData(doc.toJson(QJsonDocument::Compact));
    }
    
    return QString::fromLatin1(hash.result().toHex());
}

} // namespace onecad::io
