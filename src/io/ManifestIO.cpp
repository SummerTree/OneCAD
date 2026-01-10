/**
 * @file ManifestIO.cpp
 * @brief Implementation of manifest serialization
 */

#include "ManifestIO.h"
#include "JSONUtils.h"
#include "../app/document/Document.h"

#include <QCoreApplication>

namespace onecad::io {

QJsonObject ManifestIO::createManifest(const app::Document* document,
                                        const QString& opsHash) {
    QJsonObject manifest;
    
    // Magic and version
    manifest["magic"] = ManifestConstants::MAGIC;
    manifest["formatVersion"] = ManifestConstants::FORMAT_VERSION;
    
    // Application info
    QJsonObject app;
    app["name"] = QCoreApplication::applicationName().isEmpty() 
                  ? "OneCAD" : QCoreApplication::applicationName();
    app["version"] = QCoreApplication::applicationVersion().isEmpty()
                     ? "1.0.0" : QCoreApplication::applicationVersion();
    manifest["app"] = app;
    
    // Timestamps
    manifest["createdAt"] = JSONUtils::currentTimestamp();
    manifest["savedAt"] = JSONUtils::currentTimestamp();
    
    // Document info
    QJsonObject docInfo;
    docInfo["documentId"] = JSONUtils::generateUuid();
    docInfo["units"] = "mm";  // TODO: Get from document preferences
    docInfo["schemaVersion"] = ManifestConstants::SCHEMA_VERSION;
    manifest["document"] = docInfo;
    
    // Content summary
    QJsonObject contents;
    contents["sketchCount"] = static_cast<int>(document->sketchCount());
    contents["bodyCount"] = static_cast<int>(document->bodyCount());
    contents["operationCount"] = static_cast<int>(document->operations().size());
    manifest["contents"] = contents;
    
    // Hashes for integrity checking
    if (!opsHash.isEmpty()) {
        QJsonObject hashes;
        hashes["opsHash"] = opsHash;
        manifest["hashes"] = hashes;
    }
    
    return manifest;
}

QString ManifestIO::validateManifest(const QJsonObject& manifest) {
    // Check magic
    if (!manifest.contains("magic") || 
        manifest["magic"].toString() != ManifestConstants::MAGIC) {
        return "Invalid or missing magic number - not a valid .onecad file";
    }
    
    // Check format version
    if (!manifest.contains("formatVersion")) {
        return "Missing format version";
    }
    
    QString version = manifest["formatVersion"].toString();
    if (!isVersionCompatible(version)) {
        return QString("Incompatible format version: %1 (expected 1.x)").arg(version);
    }
    
    // Check document section
    if (!manifest.contains("document")) {
        return "Missing document section in manifest";
    }
    
    return {};  // Valid
}

QString ManifestIO::getFormatVersion(const QJsonObject& manifest) {
    return manifest["formatVersion"].toString();
}

bool ManifestIO::isVersionCompatible(const QString& version) {
    // v1.x is compatible with our reader
    if (version.startsWith("1.")) {
        return true;
    }
    
    // Future major versions are not compatible
    return false;
}

} // namespace onecad::io
