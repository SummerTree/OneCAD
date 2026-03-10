#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QTimer>
#include <functional>
#include <vector>

namespace onecad::app {

class Document;

class AutosaveManager : public QObject {
    Q_OBJECT

public:
    explicit AutosaveManager(QObject* parent = nullptr);
    ~AutosaveManager() override;

    using SaveFunction = std::function<bool(const QString& path, const Document* doc)>;
    using VersionCheckFunction = std::function<QString(const QString& path)>;

    void setSaveFunction(SaveFunction fn);
    void setVersionCheckFunction(VersionCheckFunction fn);
    void setDocument(Document* document);
    void setCurrentFilePath(const QString& path);
    void setIntervalMs(int ms);
    int intervalMs() const { return m_intervalMs; }

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    static QString autosaveDirectory();
    static QStringList findRecoveryFiles();

    struct RecoveryInfo {
        QString autosavePath;
        QString originalName;
        QDateTime savedAt;
        int operationCount = 0;
    };
    std::vector<RecoveryInfo> scanRecoveryFiles() const;
    static bool removeRecoveryFile(const QString& path);
    static void cleanupOldAutosaves(int maxAgeDays = 7);

signals:
    void autosaveFailed(const QString& errorMessage);
    void autosaveCompleted(const QString& path);

private slots:
    void onDocumentModified(bool modified);
    void performAutosave();

private:
    QString autosaveFilePath() const;

    Document* m_document = nullptr;
    QString m_currentFilePath;
    QTimer m_timer;
    int m_intervalMs = 120000; // 2 minutes
    bool m_enabled = true;
    bool m_dirty = false;
    SaveFunction m_saveFn;
    VersionCheckFunction m_versionCheckFn;
};

} // namespace onecad::app
