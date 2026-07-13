/**
 * @file ManifestIO.h
 * @brief Serialization for manifest.json
 */

#pragma once

#include <QJsonObject>
#include <QString>

namespace onecad::app {
class Document;
}

namespace onecad::io {

/**
 * @brief Manifest file constants
 */
struct ManifestConstants {
    static constexpr const char* MAGIC = "ONECAD";
    // 1.1.0: extrude two-direction params + datum planes (additive; 1.0.0
    // files migrate via MigrationRegistry).
    static constexpr const char* FORMAT_VERSION = "1.1.0";
    static constexpr const char* SCHEMA_VERSION = "1.0.0";
};

/**
 * @brief Serialization for manifest.json
 * 
 * Per FILE_FORMAT.md §5:
 * The manifest is the entry point for file validation and version detection.
 */
class ManifestIO {
public:
    /**
     * @brief Create manifest JSON for document
     */
    static QJsonObject createManifest(const app::Document* document,
                                       const QString& opsHash = {});
    
    /**
     * @brief Validate manifest JSON
     * @return Empty string if valid, error message otherwise
     */
    static QString validateManifest(const QJsonObject& manifest);
    
    /**
     * @brief Get format version from manifest
     */
    static QString getFormatVersion(const QJsonObject& manifest);
    
    /**
     * @brief Check if format version is compatible
     */
    static bool isVersionCompatible(const QString& version);
    
private:
    ManifestIO() = delete;
};

} // namespace onecad::io
