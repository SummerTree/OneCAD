/**
 * @file Package.h
 * @brief Abstract interface for reading/writing .onecad packages
 * 
 * Supports two backends:
 * - ZIP package (.onecad file) for user distribution
 * - Directory package (.onecadpkg/) for Git and debugging
 */

#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <memory>

namespace onecad::io {

/**
 * @brief Abstract interface for package I/O
 * 
 * A "package" is the container format for .onecad files.
 * Can be backed by a ZIP archive or a directory on disk.
 */
class Package {
public:
    virtual ~Package() = default;
    
    // Prevent copying
    Package(const Package&) = delete;
    Package& operator=(const Package&) = delete;
    
    // Allow moving
    Package(Package&&) = default;
    Package& operator=(Package&&) = default;
    
    // =========================================================================
    // Read Operations
    // =========================================================================
    
    /**
     * @brief Read file contents from package
     * @param path Internal path (e.g., "manifest.json", "sketches/abc.json")
     * @return File contents, or empty QByteArray if not found
     */
    virtual QByteArray readFile(const QString& path) = 0;
    
    /**
     * @brief Check if file exists in package
     */
    virtual bool fileExists(const QString& path) const = 0;
    
    /**
     * @brief List files matching prefix
     * @param prefix Path prefix (e.g., "sketches/" to list all sketches)
     * @return List of matching paths
     */
    virtual QStringList listFiles(const QString& prefix = {}) const = 0;
    
    // =========================================================================
    // Write Operations
    // =========================================================================
    
    /**
     * @brief Write file to package
     * @param path Internal path
     * @param data File contents
     * @return true on success
     */
    virtual bool writeFile(const QString& path, const QByteArray& data) = 0;
    
    /**
     * @brief Finalize writing and close package
     * @return true on success
     * 
     * For ZIP: writes central directory and closes file
     * For Directory: no-op (already written to disk)
     */
    virtual bool finalize() = 0;
    
    // =========================================================================
    // Package Info
    // =========================================================================
    
    /**
     * @brief Get error message from last failed operation
     */
    virtual QString errorString() const = 0;
    
    /**
     * @brief Check if package is valid and open
     */
    virtual bool isValid() const = 0;
    
    // =========================================================================
    // Factory Methods
    // =========================================================================
    
    /**
     * @brief Open existing package for reading
     * @param path Path to .onecad file or .onecadpkg directory
     * @return Package instance, or nullptr on error
     */
    static std::unique_ptr<Package> openForRead(const QString& path);
    
    /**
     * @brief Create new package for writing
     * @param path Path to .onecad file (ZIP) or .onecadpkg directory
     * @return Package instance, or nullptr on error
     * 
     * If path ends with ".onecad", creates ZIP package.
     * If path ends with ".onecadpkg" or is a directory, creates directory package.
     */
    static std::unique_ptr<Package> createForWrite(const QString& path);
    
protected:
    Package() = default;
};

} // namespace onecad::io
