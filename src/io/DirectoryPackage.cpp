/**
 * @file DirectoryPackage.cpp
 * @brief Directory-based package implementation
 */

#include "DirectoryPackage.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

namespace onecad::io {

// =============================================================================
// Implementation class
// =============================================================================

class DirectoryPackage::Impl {
public:
    QString basePath;
    QString errorString;
    bool isWriteMode = false;
    bool valid = false;
    
    QString fullPath(const QString& relativePath) const {
        // Prevent path traversal
        if (relativePath.contains("..")) {
            return QString();
        }
        
        return QDir(basePath).filePath(relativePath);
    }
};

// =============================================================================
// Factory methods
// =============================================================================

std::unique_ptr<DirectoryPackage> DirectoryPackage::openRead(const QString& path) {
    QDir dir(path);
    if (!dir.exists()) {
        return nullptr;
    }
    
    auto pkg = std::unique_ptr<DirectoryPackage>(new DirectoryPackage());
    pkg->pImpl_->basePath = path;
    pkg->pImpl_->isWriteMode = false;
    pkg->pImpl_->valid = true;
    return pkg;
}

std::unique_ptr<DirectoryPackage> DirectoryPackage::createWrite(const QString& path) {
    QDir dir(path);
    
    // Create directory if it doesn't exist
    if (!dir.exists() && !dir.mkpath(".")) {
        return nullptr;
    }
    
    auto pkg = std::unique_ptr<DirectoryPackage>(new DirectoryPackage());
    pkg->pImpl_->basePath = path;
    pkg->pImpl_->isWriteMode = true;
    pkg->pImpl_->valid = true;
    return pkg;
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

DirectoryPackage::DirectoryPackage() : pImpl_(std::make_unique<Impl>()) {}

DirectoryPackage::~DirectoryPackage() = default;

DirectoryPackage::DirectoryPackage(DirectoryPackage&& other) noexcept = default;
DirectoryPackage& DirectoryPackage::operator=(DirectoryPackage&& other) noexcept = default;

// =============================================================================
// Read operations
// =============================================================================

QByteArray DirectoryPackage::readFile(const QString& path) {
    if (!isValid()) return {};

    QString fullPath = pImpl_->fullPath(path);
    if (fullPath.isEmpty()) {
        pImpl_->errorString = "Invalid path (contains ..)";
        return {};
    }
    
    QFile file(fullPath);
    if (!file.open(QIODevice::ReadOnly)) {
        pImpl_->errorString = QString("Failed to open file: %1").arg(fullPath);
        return {};
    }
    
    return file.readAll();
}

bool DirectoryPackage::fileExists(const QString& path) const {
    return QFile::exists(pImpl_->fullPath(path));
}

QStringList DirectoryPackage::listFiles(const QString& prefix) const {
    QStringList result;
    
    QString searchPath = pImpl_->basePath;
    if (!prefix.isEmpty()) {
        searchPath = pImpl_->fullPath(prefix);
    }
    
    QDir baseDir(pImpl_->basePath);
    QDirIterator it(searchPath, QDir::Files, QDirIterator::Subdirectories);
    
    while (it.hasNext()) {
        it.next();
        // Store relative path from base
        QString relativePath = baseDir.relativeFilePath(it.filePath());
        
        // If prefix was specified, filter matches
        if (prefix.isEmpty() || relativePath.startsWith(prefix)) {
            result.append(relativePath);
        }
    }
    
    return result;
}

// =============================================================================
// Write operations
// =============================================================================

bool DirectoryPackage::writeFile(const QString& path, const QByteArray& data) {
    if (!isValid()) return false;

    QString fullPath = pImpl_->fullPath(path);
    if (fullPath.isEmpty()) {
        pImpl_->errorString = "Invalid path (contains ..)";
        return false;
    }
    
    // Ensure parent directory exists
    QFileInfo fileInfo(fullPath);
    QDir().mkpath(fileInfo.absolutePath());
    
    QFile file(fullPath);
    if (!file.open(QIODevice::WriteOnly)) {
        pImpl_->errorString = QString("Failed to create file: %1").arg(fullPath);
        return false;
    }
    
    qint64 written = file.write(data);
    file.close();
    
    if (written != data.size()) {
        pImpl_->errorString = QString("Failed to write all data to: %1").arg(fullPath);
        return false;
    }
    
    return true;
}

bool DirectoryPackage::finalize() {
    // No-op for directory package - files already written to disk
    return true;
}

// =============================================================================
// Info
// =============================================================================

QString DirectoryPackage::errorString() const {
    return pImpl_->errorString;
}

bool DirectoryPackage::isValid() const {
    return pImpl_ && pImpl_->valid;
}

} // namespace onecad::io
