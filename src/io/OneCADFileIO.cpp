/**
 * @file OneCADFileIO.cpp
 * @brief Implementation of high-level file I/O
 */

#include "OneCADFileIO.h"
#include "Package.h"
#include "JSONUtils.h"
#include "ManifestIO.h"
#include "DocumentIO.h"
#include "HistoryIO.h"
#include "../app/document/Document.h"

#include <QJsonDocument>
#include <QBuffer>
#include <optional>

namespace onecad::io {

namespace {

std::optional<QJsonObject> readAndValidateManifest(Package* package, QString& errorMessage) {
    if (!package) {
        errorMessage = "Invalid package";
        return std::nullopt;
    }

    QByteArray manifestData = package->readFile("manifest.json");
    if (manifestData.isEmpty()) {
        errorMessage = "Missing manifest.json";
        return std::nullopt;
    }

    QJsonParseError parseError;
    QJsonDocument manifestDoc = QJsonDocument::fromJson(manifestData, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        errorMessage = QString("Invalid manifest.json: %1").arg(parseError.errorString());
        return std::nullopt;
    }

    QString validationError = ManifestIO::validateManifest(manifestDoc.object());
    if (!validationError.isEmpty()) {
        errorMessage = validationError;
        return std::nullopt;
    }

    return manifestDoc.object();
}

} // namespace

FileIOResult OneCADFileIO::save(const QString& filepath,
                                 const app::Document* document,
                                 const QImage& thumbnail) {
    FileIOResult result;
    result.filepath = filepath;

    // 1. Create package for writing
    auto package = Package::createForWrite(filepath);
    if (!package) {
        result.errorMessage = QString("Failed to create file: %1").arg(filepath);
        return result;
    }

    // 2. Compute operations hash for manifest
    QString opsHash = HistoryIO::computeOpsHash(document->operations());

    // 3. Write manifest.json first
    QJsonObject manifest = ManifestIO::createManifest(document, opsHash);
    if (!package->writeFile("manifest.json", JSONUtils::toCanonicalJson(manifest))) {
        result.errorMessage = "Failed to write manifest.json";
        return result;
    }

    // 4. Save all document components
    if (!DocumentIO::saveDocument(package.get(), document)) {
        result.errorMessage = "Failed to save document contents: " + package->errorString();
        return result;
    }

    // 5. Write thumbnail if provided
    if (!thumbnail.isNull()) {
        QByteArray pngData;
        QBuffer buffer(&pngData);
        buffer.open(QIODevice::WriteOnly);
        thumbnail.save(&buffer, "PNG");
        buffer.close();

        if (!package->writeFile("thumbnail.png", pngData)) {
            qWarning() << "Thumbnail write failed:" << package->errorString();
            // Don't fail save - thumbnail is optional
        }
    }

    // 6. Finalize package
    if (!package->finalize()) {
        result.errorMessage = "Failed to finalize file: " + package->errorString();
        return result;
    }

    result.success = true;
    return result;
}

std::unique_ptr<app::Document> OneCADFileIO::load(const QString& filepath,
                                                   QString& errorMessage,
                                                   QObject* parent) {
    // 1. Open package for reading
    auto package = Package::openForRead(filepath);
    if (!package) {
        errorMessage = QString("Failed to open file: %1").arg(filepath);
        return nullptr;
    }
    
    // 2. Read and validate manifest
    if (!readAndValidateManifest(package.get(), errorMessage)) {
        return nullptr;
    }
    
    // 3. Load document
    return DocumentIO::loadDocument(package.get(), parent, errorMessage);
}

FileIOResult OneCADFileIO::validate(const QString& filepath) {
    FileIOResult result;
    result.filepath = filepath;
    
    // Try to open and validate manifest
    auto package = Package::openForRead(filepath);
    if (!package) {
        result.errorMessage = QString("Failed to open file: %1").arg(filepath);
        return result;
    }
    
    if (!readAndValidateManifest(package.get(), result.errorMessage)) {
        return result;
    }
    
    result.success = true;
    return result;
}

QString OneCADFileIO::getFileVersion(const QString& filepath) {
    auto package = Package::openForRead(filepath);
    if (!package) {
        return {};
    }

    QByteArray manifestData = package->readFile("manifest.json");
    if (manifestData.isEmpty()) {
        return {};
    }

    QJsonDocument manifestDoc = QJsonDocument::fromJson(manifestData);
    return ManifestIO::getFormatVersion(manifestDoc.object());
}

QImage OneCADFileIO::readThumbnail(const QString& filepath) {
    auto package = Package::openForRead(filepath);
    if (!package) {
        return {};
    }

    QByteArray data = package->readFile("thumbnail.png");
    if (data.isEmpty()) {
        return {};  // Graceful if missing
    }

    return QImage::fromData(data, "PNG");
}

} // namespace onecad::io
