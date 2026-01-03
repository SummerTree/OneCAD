#include "Document.h"
#include "../../core/sketch/Sketch.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>

namespace onecad::app {

Document::Document(QObject* parent)
    : QObject(parent)
{
}

Document::~Document() = default;

std::string Document::addSketch(std::unique_ptr<core::sketch::Sketch> sketch) {
    if (!sketch) {
        return {};
    }

    // Generate unique ID
    std::string id = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();

    // Generate default name
    std::string name = "Sketch " + std::to_string(nextSketchNumber_++);
    sketchNames_[id] = name;

    sketches_[id] = std::move(sketch);
    setModified(true);

    emit sketchAdded(QString::fromStdString(id));
    return id;
}

core::sketch::Sketch* Document::getSketch(const std::string& id) {
    auto it = sketches_.find(id);
    if (it != sketches_.end()) {
        return it->second.get();
    }
    return nullptr;
}

const core::sketch::Sketch* Document::getSketch(const std::string& id) const {
    auto it = sketches_.find(id);
    if (it != sketches_.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<std::string> Document::getSketchIds() const {
    std::vector<std::string> ids;
    ids.reserve(sketches_.size());
    for (const auto& [id, sketch] : sketches_) {
        ids.push_back(id);
    }
    return ids;
}

bool Document::removeSketch(const std::string& id) {
    auto it = sketches_.find(id);
    if (it == sketches_.end()) {
        return false;
    }

    sketches_.erase(it);
    sketchNames_.erase(id);
    setModified(true);

    emit sketchRemoved(QString::fromStdString(id));
    return true;
}

std::string Document::getSketchName(const std::string& id) const {
    auto it = sketchNames_.find(id);
    if (it != sketchNames_.end()) {
        return it->second;
    }
    return "Unnamed Sketch";
}

void Document::setSketchName(const std::string& id, const std::string& name) {
    if (sketches_.find(id) == sketches_.end()) {
        return;
    }

    // Validate name - use fallback for empty names
    std::string finalName = name;
    if (finalName.empty() || finalName.find_first_not_of(" \t\n\r") == std::string::npos) {
        finalName = "Untitled";
    }

    // Only update if name actually changed
    auto it = sketchNames_.find(id);
    if (it != sketchNames_.end() && it->second == finalName) {
        return;
    }

    sketchNames_[id] = finalName;
    setModified(true);
    emit sketchRenamed(QString::fromStdString(id), QString::fromStdString(finalName));
}

void Document::setModified(bool modified) {
    if (modified_ != modified) {
        modified_ = modified;
        emit modifiedChanged(modified);
    }
}

void Document::clear() {
    sketches_.clear();
    sketchNames_.clear();
    nextSketchNumber_ = 1;
    setModified(false);
    emit documentCleared();
}

std::string Document::toJson() const {
    QJsonObject root;

    // Serialize sketches
    QJsonArray sketchArray;
    for (const auto& [id, sketch] : sketches_) {
        QJsonObject sketchObj;
        sketchObj["id"] = QString::fromStdString(id);
        sketchObj["name"] = QString::fromStdString(getSketchName(id));

        // Use Sketch's own serialization with error handling
        QString sketchJson = QString::fromStdString(sketch->toJson());
        QJsonParseError parseError;
        QJsonDocument sketchDoc = QJsonDocument::fromJson(sketchJson.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            // Skip invalid sketch data but continue with others
            continue;
        }
        sketchObj["data"] = sketchDoc.object();

        sketchArray.append(sketchObj);
    }
    root["sketches"] = sketchArray;
    root["nextSketchNumber"] = static_cast<int>(nextSketchNumber_);

    QJsonDocument doc(root);
    return doc.toJson(QJsonDocument::Indented).toStdString();
}

std::unique_ptr<Document> Document::fromJson(const std::string& json, QObject* parent) {
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(json), &parseError);
    if (parseError.error != QJsonParseError::NoError || doc.isNull() || !doc.isObject()) {
        return nullptr;
    }

    auto document = std::make_unique<Document>(parent);
    QJsonObject root = doc.object();

    // Validate nextSketchNumber - must be at least 1
    int parsedNumber = root["nextSketchNumber"].toInt(1);
    document->nextSketchNumber_ = (parsedNumber < 1) ? 1 : static_cast<unsigned int>(parsedNumber);

    QJsonArray sketchArray = root["sketches"].toArray();
    for (const auto& sketchVal : sketchArray) {
        QJsonObject sketchObj = sketchVal.toObject();
        std::string id = sketchObj["id"].toString().toStdString();
        std::string name = sketchObj["name"].toString().toStdString();

        QJsonObject dataObj = sketchObj["data"].toObject();
        QJsonDocument dataDoc(dataObj);
        std::string sketchJson = dataDoc.toJson().toStdString();

        auto sketch = core::sketch::Sketch::fromJson(sketchJson);
        if (sketch) {
            document->sketches_[id] = std::move(sketch);
            document->sketchNames_[id] = name;
        }
    }

    return document;
}

} // namespace onecad::app
