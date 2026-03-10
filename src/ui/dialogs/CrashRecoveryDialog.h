#pragma once

#include <QDialog>
#include <QString>
#include <vector>

#include "../../app/AutosaveManager.h"

class QVBoxLayout;
class QListWidget;

namespace onecad::ui {

class CrashRecoveryDialog : public QDialog {
    Q_OBJECT

public:
    enum Action { Recover, Discard, Skip };

    explicit CrashRecoveryDialog(
        const std::vector<app::AutosaveManager::RecoveryInfo>& recoveryFiles,
        QWidget* parent = nullptr);

    QString selectedFilePath() const { return m_selectedPath; }
    Action selectedAction() const { return m_action; }

private:
    void buildUi(const std::vector<app::AutosaveManager::RecoveryInfo>& files);

    QListWidget* m_list = nullptr;
    std::vector<app::AutosaveManager::RecoveryInfo> m_files;
    QString m_selectedPath;
    Action m_action = Skip;
};

} // namespace onecad::ui
