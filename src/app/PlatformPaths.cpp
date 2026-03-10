/**
 * @file PlatformPaths.cpp
 * @brief Implementation of cross-platform path resolution.
 */
#include "PlatformPaths.h"

#include <QDir>
#include <QStandardPaths>

namespace onecad::app {

QString PlatformPaths::appDataDir() {
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + "/.onecad";
    }
    return base;
}

QString PlatformPaths::recoveryDir() {
    return appDataDir() + "/recovery";
}

QString PlatformPaths::logDir() {
    return appDataDir() + "/logs";
}

QString PlatformPaths::documentsDir() {
    QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (docs.isEmpty()) {
        docs = QDir::homePath();
    }
    return docs;
}

bool PlatformPaths::ensureDir(const QString& path) {
    QDir dir(path);
    if (dir.exists()) {
        return true;
    }
    return dir.mkpath(".");
}

} // namespace onecad::app
