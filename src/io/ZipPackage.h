/**
 * @file ZipPackage.h
 * @brief ZIP archive implementation of Package interface
 */

#pragma once

#include "Package.h"
#include <QFile>

// Forward declare QuaZip types to avoid header dependency in public API
class QuaZip;
class QuaZipFile;

namespace onecad::io {

/**
 * @brief ZIP-based package implementation
 * 
 * Uses QuaZip library for ZIP file handling.
 * All files are stored uncompressed for v1.0 (Git-friendly, debuggable).
 */
class ZipPackage : public Package {
public:
    /**
     * @brief Open existing ZIP for reading
     */
    static std::unique_ptr<ZipPackage> openRead(const QString& path);
    
    /**
     * @brief Create new ZIP for writing
     */
    static std::unique_ptr<ZipPackage> createWrite(const QString& path);
    
    /**
     * @brief Check if ZIP support is available (compiled with QuaZip)
     */
    static bool isSupported();
    
    ~ZipPackage() override;
    
    // Move only
    ZipPackage(ZipPackage&& other) noexcept;
    ZipPackage& operator=(ZipPackage&& other) noexcept;
    
    // Package interface
    QByteArray readFile(const QString& path) override;
    bool fileExists(const QString& path) const override;
    QStringList listFiles(const QString& prefix) const override;
    bool writeFile(const QString& path, const QByteArray& data) override;
    bool finalize() override;
    QString errorString() const override;
    bool isValid() const override;
    
private:
    ZipPackage();
    
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace onecad::io
