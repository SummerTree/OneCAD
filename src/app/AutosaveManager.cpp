#include "AutosaveManager.h"
#include "document/Document.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace onecad::app {

AutosaveManager::AutosaveManager(QObject* parent)
    : QObject(parent) {
    m_timer.setSingleShot(false);
    m_timer.setInterval(m_intervalMs);
    connect(&m_timer, &QTimer::timeout, this, &AutosaveManager::performAutosave);
}

AutosaveManager::~AutosaveManager() {
    m_timer.stop();
}

void AutosaveManager::setSaveFunction(SaveFunction fn) {
    m_saveFn = std::move(fn);
}

void AutosaveManager::setVersionCheckFunction(VersionCheckFunction fn) {
    m_versionCheckFn = std::move(fn);
}

void AutosaveManager::setDocument(Document* document) {
    if (m_document) {
        disconnect(m_document, nullptr, this, nullptr);
    }
    m_document = document;
    m_dirty = false;

    if (m_document) {
        connect(m_document, &Document::modifiedChanged,
                this, &AutosaveManager::onDocumentModified);
        m_dirty = m_document->isModified();
    }

    if (m_enabled && m_document) {
        m_timer.start();
    } else {
        m_timer.stop();
    }
}

void AutosaveManager::setCurrentFilePath(const QString& path) {
    m_currentFilePath = path;
}

void AutosaveManager::setIntervalMs(int ms) {
    m_intervalMs = qBound(30000, ms, 1800000);
    m_timer.setInterval(m_intervalMs);
}

void AutosaveManager::setEnabled(bool enabled) {
    m_enabled = enabled;
    if (m_enabled && m_document) {
        m_timer.start();
    } else {
        m_timer.stop();
    }
}

QString AutosaveManager::autosaveDirectory() {
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString dir = appData + "/Autosave";
    QDir().mkpath(dir);
    return dir;
}

QStringList AutosaveManager::findRecoveryFiles() {
    QDir dir(autosaveDirectory());
    return dir.entryList(QStringList() << "*.autosave", QDir::Files, QDir::Time);
}

std::vector<AutosaveManager::RecoveryInfo> AutosaveManager::scanRecoveryFiles() const {
    std::vector<RecoveryInfo> results;
    QDir dir(autosaveDirectory());
    const auto files = dir.entryInfoList(
        QStringList() << "*.autosave", QDir::Files, QDir::Time);

    for (const auto& fi : files) {
        RecoveryInfo info;
        info.autosavePath = fi.absoluteFilePath();
        info.savedAt = fi.lastModified();

        // Validate file if version check available
        if (m_versionCheckFn) {
            QString version = m_versionCheckFn(info.autosavePath);
            if (version.isEmpty()) {
                continue; // Corrupt file
            }
        }

        // Extract original name from filename: "OriginalName-autosave.autosave"
        QString baseName = fi.completeBaseName();
        int dashIdx = baseName.lastIndexOf('-');
        if (dashIdx > 0) {
            info.originalName = baseName.left(dashIdx);
        } else {
            info.originalName = baseName;
        }

        results.push_back(std::move(info));
    }
    return results;
}

bool AutosaveManager::removeRecoveryFile(const QString& path) {
    return QFile::remove(path);
}

void AutosaveManager::cleanupOldAutosaves(int maxAgeDays) {
    QDir dir(autosaveDirectory());
    const auto files = dir.entryInfoList(
        QStringList() << "*.autosave", QDir::Files);
    const QDateTime cutoff = QDateTime::currentDateTime().addDays(-maxAgeDays);

    for (const auto& fi : files) {
        if (fi.lastModified() < cutoff) {
            QFile::remove(fi.absoluteFilePath());
        }
    }
}

void AutosaveManager::onDocumentModified(bool modified) {
    m_dirty = modified;
    if (!modified) {
        QString path = autosaveFilePath();
        if (QFile::exists(path)) {
            QFile::remove(path);
        }
    }
}

void AutosaveManager::performAutosave() {
    if (!m_document || !m_dirty || !m_enabled || !m_saveFn) {
        return;
    }

    QString path = autosaveFilePath();
    if (m_saveFn(path, m_document)) {
        emit autosaveCompleted(path);
    } else {
        emit autosaveFailed(QStringLiteral("Autosave failed"));
    }
}

QString AutosaveManager::autosaveFilePath() const {
    QString name;
    if (!m_currentFilePath.isEmpty()) {
        name = QFileInfo(m_currentFilePath).completeBaseName();
    } else {
        name = QStringLiteral("Untitled");
    }
    return autosaveDirectory() + "/" + name + "-autosave.autosave";
}

} // namespace onecad::app
