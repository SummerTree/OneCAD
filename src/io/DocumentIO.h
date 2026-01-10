/**
 * @file DocumentIO.h
 * @brief Serialization for document.json
 */

#pragma once

#include <QJsonObject>
#include <QString>
#include <memory>

namespace onecad::app {
class Document;
}

namespace onecad::io {

class Package;

/**
 * @brief Serialization for document.json
 * 
 * Per FILE_FORMAT.md §6:
 * document.json is the central hub linking all other components.
 */
class DocumentIO {
public:
    /**
     * @brief Save document structure to package
     * @param package Package to write to
     * @param document Document to serialize
     * @return true on success
     */
    static bool saveDocument(Package* package, const app::Document* document);
    
    /**
     * @brief Load document structure from package
     * @param package Package to read from
     * @param parent QObject parent for new Document
     * @param errorMessage Output error message on failure
     * @return Loaded document, or nullptr on error
     */
    static std::unique_ptr<app::Document> loadDocument(Package* package,
                                                        QObject* parent,
                                                        QString& errorMessage);
    
    /**
     * @brief Create document.json content
     */
    static QJsonObject createDocumentJson(const app::Document* document);
    
    /**
     * @brief Parse document.json and populate document
     */
    static bool parseDocumentJson(const QJsonObject& json, 
                                   app::Document* document,
                                   QString& errorMessage);
    
private:
    DocumentIO() = delete;
};

} // namespace onecad::io
