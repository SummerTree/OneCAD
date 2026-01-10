/**
 * @file ZipPackage.cpp
 * @brief ZIP archive implementation using QuaZip
 */

#include "ZipPackage.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>

#if HAS_QUAZIP

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>
#include <quazip/quazipnewinfo.h>

namespace onecad::io {

// =============================================================================
// Implementation class (pImpl pattern)
// =============================================================================

class ZipPackage::Impl {
public:
    QuaZip zip;
    QString errorString;
    bool isWriteMode = false;
    bool finalized = false;
    
    Impl() = default;
    
    bool openForRead(const QString& path) {
        zip.setZipName(path);
        if (!zip.open(QuaZip::mdUnzip)) {
            errorString = QString("Failed to open ZIP for reading: %1").arg(path);
            return false;
        }
        isWriteMode = false;
        return true;
    }
    
    bool openForWrite(const QString& path) {
        QFileInfo fileInfo(path);
        QDir().mkpath(fileInfo.absolutePath());
        
        zip.setZipName(path);
        if (!zip.open(QuaZip::mdCreate)) {
            errorString = QString("Failed to create ZIP: %1").arg(path);
            return false;
        }
        isWriteMode = true;
        return true;
    }
};

std::unique_ptr<ZipPackage> ZipPackage::openRead(const QString& path) {
    auto pkg = std::unique_ptr<ZipPackage>(new ZipPackage());
    if (!pkg->pImpl_->openForRead(path)) {
        return nullptr;
    }
    return pkg;
}

std::unique_ptr<ZipPackage> ZipPackage::createWrite(const QString& path) {
    auto pkg = std::unique_ptr<ZipPackage>(new ZipPackage());
    if (!pkg->pImpl_->openForWrite(path)) {
        return nullptr;
    }
    return pkg;
}

bool ZipPackage::isSupported() {
    return true;
}

ZipPackage::ZipPackage() : pImpl_(std::make_unique<Impl>()) {}

ZipPackage::~ZipPackage() {
    if (pImpl_ && pImpl_->zip.isOpen()) {
        pImpl_->zip.close();
    }
}

ZipPackage::ZipPackage(ZipPackage&& other) noexcept = default;
ZipPackage& ZipPackage::operator=(ZipPackage&& other) noexcept = default;

QByteArray ZipPackage::readFile(const QString& path) {
    if (!pImpl_) return {};
    if (!pImpl_->zip.isOpen() || pImpl_->isWriteMode) {
        pImpl_->errorString = "ZIP not open for reading";
        return {};
    }
    
    if (!pImpl_->zip.setCurrentFile(path)) {
        pImpl_->errorString = QString("File not found in ZIP: %1").arg(path);
        return {};
    }
    
    QuaZipFile file(&pImpl_->zip);
    if (!file.open(QIODevice::ReadOnly)) {
        pImpl_->errorString = QString("Failed to open file in ZIP: %1").arg(path);
        return {};
    }
    
    QByteArray data = file.readAll();
    file.close();
    return data;
}

bool ZipPackage::fileExists(const QString& path) const {
    if (!pImpl_->zip.isOpen()) {
        return false;
    }
    
    auto& zip = const_cast<QuaZip&>(pImpl_->zip);
    return zip.setCurrentFile(path);
}

QStringList ZipPackage::listFiles(const QString& prefix) const {
    QStringList result;
    
    if (!pImpl_->zip.isOpen()) {
        return result;
    }
    
    auto& zip = const_cast<QuaZip&>(pImpl_->zip);
    
    for (bool more = zip.goToFirstFile(); more; more = zip.goToNextFile()) {
        QString name = zip.getCurrentFileName();
        if (prefix.isEmpty() || name.startsWith(prefix)) {
            result.append(name);
        }
    }
    
    return result;
}

bool ZipPackage::writeFile(const QString& path, const QByteArray& data) {
    if (!pImpl_) return false;
    if (!pImpl_->zip.isOpen() || !pImpl_->isWriteMode) {
        pImpl_->errorString = "ZIP not open for writing";
        return false;
    }
    
    if (pImpl_->finalized) {
        pImpl_->errorString = "Cannot write to finalized ZIP";
        return false;
    }
    
    QuaZipFile file(&pImpl_->zip);
    
    QuaZipNewInfo info(path);
    info.dateTime = QDateTime(QDate(1980, 1, 1), QTime(0, 0, 0));
    
    if (!file.open(QIODevice::WriteOnly, info, nullptr, 0, 0, 0, false)) {
        pImpl_->errorString = QString("Failed to create file in ZIP: %1").arg(path);
        return false;
    }
    
    qint64 written = file.write(data);
    file.close();
    
    if (written != data.size()) {
        pImpl_->errorString = QString("Failed to write all data to: %1").arg(path);
        return false;
    }
    
    return true;
}

bool ZipPackage::finalize() {
    if (!pImpl_->zip.isOpen()) {
        pImpl_->errorString = "ZIP not open";
        return false;
    }
    
    pImpl_->zip.close();
    pImpl_->finalized = true;
    
    if (pImpl_->zip.getZipError() != 0) {
        pImpl_->errorString = QString("Error finalizing ZIP: error code %1")
            .arg(pImpl_->zip.getZipError());
        return false;
    }
    
    return true;
}

QString ZipPackage::errorString() const {
    return pImpl_->errorString;
}

bool ZipPackage::isValid() const {
    return pImpl_ && (pImpl_->zip.isOpen() || pImpl_->finalized);
}

} // namespace onecad::io

#else // !HAS_QUAZIP

// Stub implementation when QuaZip is not available
// System-zip implementation when QuaZip is not available
#include <QTemporaryDir>
#include <QProcess>
#include <QStandardPaths>
#include <QDirIterator>
#include <QFile>

namespace onecad::io {

class ZipPackage::Impl {
public:
    QTemporaryDir tempDir;
    QString zipPath;
    QString errorString;
    bool isWriteMode = false;
    bool finalized = false;
    
    Impl() = default;
    
    // Check if zip/unzip tools are available
    static bool hasZipTools() {
        return !QStandardPaths::findExecutable("zip").isEmpty() && 
               !QStandardPaths::findExecutable("unzip").isEmpty();
    }
    
    bool openForRead(const QString& path) {
        if (!hasZipTools()) {
            errorString = "System 'unzip' utility not found";
            return false;
        }
        
        if (!tempDir.isValid()) {
            errorString = "Failed to create temporary directory";
            return false;
        }
        
        zipPath = path;
        isWriteMode = false;
        
        // Unzip content to temp dir
        QProcess unzip;
        unzip.setProgram("unzip");
        unzip.setArguments({"-q", "-o", path, "-d", tempDir.path()});
        unzip.start();
        unzip.waitForFinished();
        
        if (unzip.exitStatus() != QProcess::NormalExit || unzip.exitCode() != 0) {
            errorString = QString("Failed to unzip file: %1\n%2")
                          .arg(path, QString::fromUtf8(unzip.readAllStandardError()));
            return false;
        }
        
        return true;
    }
    
    bool openForWrite(const QString& path) {
        if (!hasZipTools()) {
            errorString = "System 'zip' utility not found";
            return false;
        }
        
        if (!tempDir.isValid()) {
            errorString = "Failed to create temporary directory";
            return false;
        }
        
        zipPath = path;
        isWriteMode = true;
        return true;
    }
};

std::unique_ptr<ZipPackage> ZipPackage::openRead(const QString& path) {
    auto pkg = std::unique_ptr<ZipPackage>(new ZipPackage());
    if (!pkg->pImpl_->openForRead(path)) {
        return nullptr;
    }
    return pkg;
}

std::unique_ptr<ZipPackage> ZipPackage::createWrite(const QString& path) {
    auto pkg = std::unique_ptr<ZipPackage>(new ZipPackage());
    if (!pkg->pImpl_->openForWrite(path)) {
        return nullptr;
    }
    return pkg;
}

// Return true as we have a fallback implementation
bool ZipPackage::isSupported() {
    return Impl::hasZipTools();
}

ZipPackage::ZipPackage() : pImpl_(std::make_unique<Impl>()) {}
ZipPackage::~ZipPackage() = default;
ZipPackage::ZipPackage(ZipPackage&& other) noexcept = default;
ZipPackage& ZipPackage::operator=(ZipPackage&& other) noexcept = default;

QByteArray ZipPackage::readFile(const QString& path) {
    if (!pImpl_ || !pImpl_->tempDir.isValid()) return {};
    
    QFile file(QDir(pImpl_->tempDir.path()).filePath(path));
    if (!file.open(QIODevice::ReadOnly)) {
        pImpl_->errorString = QString("Failed to open file: %1").arg(path);
        return {};
    }
    return file.readAll();
}

bool ZipPackage::fileExists(const QString& path) const {
    if (!pImpl_ || !pImpl_->tempDir.isValid()) return false;
    return QFile::exists(QDir(pImpl_->tempDir.path()).filePath(path));
}

QStringList ZipPackage::listFiles(const QString& prefix) const {
    if (!pImpl_ || !pImpl_->tempDir.isValid()) return {};
    
    QStringList result;
    QDir dir(pImpl_->tempDir.path());
    
    // Simple recursive search (could be optimized)
    QDirIterator it(dir, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QString relPath = dir.relativeFilePath(it.filePath());
        if (prefix.isEmpty() || relPath.startsWith(prefix)) {
            if (it.fileInfo().isFile()) {
                result.append(relPath);
            }
        }
    }
    return result;
}

bool ZipPackage::writeFile(const QString& path, const QByteArray& data) {
    if (!pImpl_ || !pImpl_->tempDir.isValid()) return false;
    
    QString fullPath = QDir(pImpl_->tempDir.path()).filePath(path);
    
    // Ensure parent dir exists
    QFileInfo fi(fullPath);
    QDir().mkpath(fi.absolutePath());
    
    QFile file(fullPath);
    if (!file.open(QIODevice::WriteOnly)) {
        pImpl_->errorString = QString("Failed to create file: %1").arg(path);
        return false;
    }
    
    if (file.write(data) != data.size()) {
        pImpl_->errorString = QString("Failed to write data: %1").arg(path);
        return false;
    }
    
    return true;
}

bool ZipPackage::finalize() {
    if (!pImpl_->isWriteMode) return true;
    if (pImpl_->finalized) return true;
    
    // Zip the temp dir contents to destination
    QProcess zip;
    zip.setProgram("zip");
    zip.setWorkingDirectory(pImpl_->tempDir.path());
    // -r: recursive, -q: quiet
    zip.setArguments({"-r", "-q", pImpl_->zipPath, "."});
    
    zip.start();
    zip.waitForFinished();
    
    if (zip.exitStatus() != QProcess::NormalExit || zip.exitCode() != 0) {
        pImpl_->errorString = QString("Failed to zip files: %1").arg(QString::fromUtf8(zip.readAllStandardError()));
        return false;
    }
    
    pImpl_->finalized = true;
    return true;
}

QString ZipPackage::errorString() const { return pImpl_->errorString; }
bool ZipPackage::isValid() const { return pImpl_ && pImpl_->tempDir.isValid(); }

} // namespace onecad::io

#endif // HAS_QUAZIP
