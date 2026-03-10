/**
 * @file PlatformPaths.h
 * @brief Cross-platform path resolution for application data.
 */
#ifndef ONECAD_APP_PLATFORMPATHS_H
#define ONECAD_APP_PLATFORMPATHS_H

#include <QString>

namespace onecad::app {

class PlatformPaths {
public:
    /// Application data directory (settings, recovery, logs).
    /// macOS: ~/Library/Application Support/OneCAD/
    /// Linux: ~/.local/share/OneCAD/
    static QString appDataDir();

    /// Autosave/recovery directory.
    static QString recoveryDir();

    /// Log file directory.
    static QString logDir();

    /// User documents default directory.
    static QString documentsDir();

    /// Ensure a directory exists, creating it if necessary.
    static bool ensureDir(const QString& path);
};

} // namespace onecad::app

#endif // ONECAD_APP_PLATFORMPATHS_H
