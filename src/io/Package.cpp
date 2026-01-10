/**
 * @file Package.cpp
 * @brief Factory methods for Package interface
 */

#include "Package.h"
#include "ZipPackage.h"
#include "DirectoryPackage.h"

#include <QFileInfo>

namespace onecad::io {

std::unique_ptr<Package> Package::openForRead(const QString& path) {
    QFileInfo info(path);
    
    // If it's a directory, use DirectoryPackage (even if extension is .onecad)
    if (info.isDir()) {
        return DirectoryPackage::openRead(path);
    }
    
    // If it ends with .onecadpkg, use DirectoryPackage
    if (path.endsWith(".onecadpkg", Qt::CaseInsensitive)) {
        return DirectoryPackage::openRead(path);
    }
    
    // Otherwise, try ZIP if supported
    if (ZipPackage::isSupported()) {
        return ZipPackage::openRead(path);
    }
    
    return nullptr;
}

std::unique_ptr<Package> Package::createForWrite(const QString& path) {
    // If it ends with .onecadpkg, use DirectoryPackage
    if (path.endsWith(".onecadpkg", Qt::CaseInsensitive)) {
        return DirectoryPackage::createWrite(path);
    }
    
    // Check if ZIP is supported
    if (ZipPackage::isSupported()) {
        return ZipPackage::createWrite(path);
    }
    
    // Fallback: If ZIP not supported, use DirectoryPackage regardless of extension
    // This allows saving on systems without QuaZip
    return DirectoryPackage::createWrite(path);
}

} // namespace onecad::io
