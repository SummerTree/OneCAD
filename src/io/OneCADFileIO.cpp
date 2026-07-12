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

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QBuffer>
#include <filesystem>
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

// Atomic-save plumbing (docs/FILE_FORMAT.md §16): all content goes to a hidden
// temp sibling of the destination and is swapped in only after finalize()
// succeeds, so no failure or crash mid-save can destroy the previous file.

Package::Format formatForFinalPath(const QString& finalPath) {
    if (finalPath.endsWith(".onecadpkg", Qt::CaseInsensitive)) {
        return Package::Format::Directory;
    }
    return Package::Format::Auto;  // ZIP when supported, directory fallback
}

QString tempSavePath(const QString& finalPath) {
    const QFileInfo info(finalPath);
    return info.absolutePath() + "/." + info.fileName() + ".saving." +
           QString::number(QCoreApplication::applicationPid());
}

void removeSaveArtifact(const QString& path) {
    std::error_code ec;
    std::filesystem::remove_all(std::filesystem::path(path.toStdString()), ec);
}

bool commitTempToFinal(const QString& tempPath, const QString& finalPath,
                       QString& errorMessage) {
    namespace fs = std::filesystem;
    const fs::path temp(tempPath.toStdString());
    const fs::path dest(finalPath.toStdString());
    std::error_code ec;

    const bool tempIsDir = fs::is_directory(temp, ec);
    const bool destExists = fs::exists(dest, ec);
    const bool destIsDir = destExists && fs::is_directory(dest, ec);

    // Plain file over plain file (or nothing): a single atomic rename.
    if (!tempIsDir && !destIsDir) {
        fs::rename(temp, dest, ec);
        if (ec) {
            errorMessage = QString("Failed to move saved file into place: %1")
                               .arg(QString::fromStdString(ec.message()));
            removeSaveArtifact(tempPath);
            return false;
        }
        return true;
    }

    // Directory package (or container-type change): two-rename swap with rollback.
    // The previous data is only removed after the new data is in place.
    fs::path oldPath = dest;
    oldPath += ".old." + std::to_string(QCoreApplication::applicationPid());
    if (destExists) {
        fs::rename(dest, oldPath, ec);
        if (ec) {
            errorMessage = QString("Failed to stage previous file for replacement: %1")
                               .arg(QString::fromStdString(ec.message()));
            removeSaveArtifact(tempPath);
            return false;
        }
    }
    fs::rename(temp, dest, ec);
    if (ec) {
        if (destExists) {
            std::error_code rollback;
            fs::rename(oldPath, dest, rollback);  // best-effort restore
        }
        errorMessage = QString("Failed to move saved file into place: %1")
                           .arg(QString::fromStdString(ec.message()));
        removeSaveArtifact(tempPath);
        return false;
    }
    if (destExists) {
        fs::remove_all(oldPath, ec);
    }
    return true;
}

} // namespace

FileIOResult OneCADFileIO::save(const QString& filepath,
                                 const app::Document* document,
                                 const QImage& thumbnail) {
    FileIOResult result;
    result.filepath = filepath;

    // 1. Create package for writing — against a TEMP path, so the destination is
    // never truncated before the new content is complete (atomic save).
    const QString tempPath = tempSavePath(filepath);
    removeSaveArtifact(tempPath);  // stale leftover from a crashed save
    auto package = Package::createForWrite(tempPath, formatForFinalPath(filepath));
    if (!package) {
        result.errorMessage = QString("Failed to create file: %1").arg(filepath);
        return result;
    }
    const auto fail = [&](const QString& message) {
        result.errorMessage = message;
        package.reset();  // release handles before deleting the temp artifact
        removeSaveArtifact(tempPath);
        return result;
    };

    // 2. Compute operations hash for manifest
    QString opsHash = HistoryIO::computeOpsHash(document->operations());

    // 3. Write manifest.json first
    QJsonObject manifest = ManifestIO::createManifest(document, opsHash);
    if (!package->writeFile("manifest.json", JSONUtils::toCanonicalJson(manifest))) {
        return fail("Failed to write manifest.json");
    }

    // 4. Save all document components
    if (!DocumentIO::saveDocument(package.get(), document)) {
        return fail("Failed to save document contents: " + package->errorString());
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
        return fail("Failed to finalize file: " + package->errorString());
    }
    package.reset();  // close all handles before the swap

    // 7. Swap the finished temp artifact into place.
    QString commitError;
    if (!commitTempToFinal(tempPath, filepath, commitError)) {
        result.errorMessage = commitError;
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
