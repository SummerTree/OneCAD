/**
 * @file OneCADFileIO.h
 * @brief High-level API for saving/loading .onecad files
 */

#pragma once

#include <QString>
#include <QImage>
#include <memory>

namespace onecad::app {
class Document;
}

namespace onecad::io {

/**
 * @brief Result of a file operation
 */
struct FileIOResult {
    bool success = false;
    QString errorMessage;
    QString filepath;
    
    operator bool() const { return success; }
};

/**
 * @brief High-level API for .onecad file operations
 * 
 * Orchestrates saving/loading of complete documents using the Package
 * abstraction and individual serializers for each component.
 */
class OneCADFileIO {
public:
    /**
     * @brief Save document to .onecad file
     * @param filepath Path to save (creates/overwrites)
     * @param document Document to save
     * @param thumbnail Optional viewport thumbnail (stored as thumbnail.png)
     * @return Result with success status and any error message
     */
    static FileIOResult save(const QString& filepath,
                             const app::Document* document,
                             const QImage& thumbnail = QImage());
    
    /**
     * @brief Load document from .onecad file
     * @param filepath Path to load
     * @param parent QObject parent for the new Document
     * @return Loaded document, or nullptr on error
     */
    static std::unique_ptr<app::Document> load(const QString& filepath,
                                                QString& errorMessage,
                                                QObject* parent = nullptr);
    
    /**
     * @brief Validate .onecad file without fully loading
     * @param filepath Path to validate
     * @return Result with validation status
     */
    static FileIOResult validate(const QString& filepath);
    
    /**
     * @brief Get file format version from file
     * @param filepath Path to check
     * @return Version string (e.g., "1.0.0"), or empty on error
     */
    static QString getFileVersion(const QString& filepath);

    /**
     * @brief Read thumbnail from .onecad file
     * @param filepath Path to read
     * @return Thumbnail image, or null QImage if not present
     */
    static QImage readThumbnail(const QString& filepath);
    
private:
    OneCADFileIO() = delete;
};

} // namespace onecad::io
