/**
 * @file DirectoryPackage.h
 * @brief Directory-based implementation of Package interface
 */

#pragma once

#include "Package.h"

#include <memory>

namespace onecad::io {

/**
 * @brief Directory-based package implementation
 * 
 * Stores package contents as files in a directory.
 * Intended for Git version control and debugging.
 * Directory typically named with .onecadpkg extension.
 */
class DirectoryPackage : public Package {
public:
    /**
     * @brief Open existing directory for reading
     */
    static std::unique_ptr<DirectoryPackage> openRead(const QString& path);
    
    /**
     * @brief Create new directory for writing
     */
    static std::unique_ptr<DirectoryPackage> createWrite(const QString& path);
    
    ~DirectoryPackage() override;
    
    // Move only
    DirectoryPackage(DirectoryPackage&& other) noexcept;
    DirectoryPackage& operator=(DirectoryPackage&& other) noexcept;
    DirectoryPackage(const DirectoryPackage&) = delete;
    DirectoryPackage& operator=(const DirectoryPackage&) = delete;
    
    // Package interface
    QByteArray readFile(const QString& path) override;
    bool fileExists(const QString& path) const override;
    QStringList listFiles(const QString& prefix) const override;
    bool writeFile(const QString& path, const QByteArray& data) override;
    bool finalize() override;
    QString errorString() const override;
    bool isValid() const override;
    
private:
    DirectoryPackage();
    
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace onecad::io
