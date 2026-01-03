/**
 * @file Document.h
 * @brief Document model for storing sketches and bodies
 */

#ifndef ONECAD_APP_DOCUMENT_DOCUMENT_H
#define ONECAD_APP_DOCUMENT_DOCUMENT_H

#include <QObject>
#include <QString>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace onecad::core::sketch {
class Sketch;
}

namespace onecad::app {

/**
 * @brief Central document model owning all sketches and bodies
 *
 * Provides persistent storage for sketch data, emits signals when
 * content changes for UI updates (navigator, viewport, etc.)
 */
class Document : public QObject {
    Q_OBJECT

public:
    explicit Document(QObject* parent = nullptr);
    ~Document() override;

    // Sketch management
    /**
     * @brief Add a new sketch to the document
     * @param sketch Ownership transferred to document
     * @return ID of the added sketch
     */
    std::string addSketch(std::unique_ptr<core::sketch::Sketch> sketch);

    /**
     * @brief Get sketch by ID
     * @return Pointer to sketch or nullptr if not found
     */
    core::sketch::Sketch* getSketch(const std::string& id);
    const core::sketch::Sketch* getSketch(const std::string& id) const;

    /**
     * @brief Get all sketch IDs
     */
    std::vector<std::string> getSketchIds() const;

    /**
     * @brief Get number of sketches
     */
    size_t sketchCount() const { return sketches_.size(); }

    /**
     * @brief Remove sketch by ID
     * @return true if removed, false if not found
     */
    bool removeSketch(const std::string& id);

    /**
     * @brief Get sketch name (for navigator display)
     */
    std::string getSketchName(const std::string& id) const;

    /**
     * @brief Set sketch name
     */
    void setSketchName(const std::string& id, const std::string& name);

    // Document state
    bool isModified() const { return modified_; }
    void setModified(bool modified);
    void clear();

    // Serialization
    std::string toJson() const;
    static std::unique_ptr<Document> fromJson(const std::string& json, QObject* parent = nullptr);

signals:
    void sketchAdded(const QString& id);
    void sketchRemoved(const QString& id);
    void sketchRenamed(const QString& id, const QString& newName);
    void modifiedChanged(bool modified);
    void documentCleared();

private:
    std::unordered_map<std::string, std::unique_ptr<core::sketch::Sketch>> sketches_;
    std::unordered_map<std::string, std::string> sketchNames_;  // id -> display name
    bool modified_ = false;
    unsigned int nextSketchNumber_ = 1;
};

} // namespace onecad::app

#endif // ONECAD_APP_DOCUMENT_DOCUMENT_H
