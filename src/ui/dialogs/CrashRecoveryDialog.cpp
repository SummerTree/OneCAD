#include "CrashRecoveryDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace onecad::ui {

CrashRecoveryDialog::CrashRecoveryDialog(
    const std::vector<app::AutosaveManager::RecoveryInfo>& recoveryFiles,
    QWidget* parent)
    : QDialog(parent)
    , m_files(recoveryFiles) {
    setWindowTitle(tr("Recover Unsaved Work"));
    setMinimumWidth(400);
    buildUi(recoveryFiles);
}

void CrashRecoveryDialog::buildUi(
    const std::vector<app::AutosaveManager::RecoveryInfo>& files) {
    auto* layout = new QVBoxLayout(this);

    auto* label = new QLabel(
        tr("OneCAD found unsaved work from a previous session.\n"
           "Would you like to recover it?"));
    layout->addWidget(label);

    m_list = new QListWidget;
    for (const auto& info : files) {
        QString text = info.originalName;
        if (info.savedAt.isValid()) {
            text += tr("  —  ") + info.savedAt.toString("yyyy-MM-dd hh:mm:ss");
        }
        m_list->addItem(text);
    }
    if (m_list->count() > 0) {
        m_list->setCurrentRow(0);
    }
    layout->addWidget(m_list);

    auto* buttons = new QDialogButtonBox;
    auto* recoverBtn = buttons->addButton(tr("Recover"), QDialogButtonBox::AcceptRole);
    auto* discardBtn = buttons->addButton(tr("Discard All"), QDialogButtonBox::DestructiveRole);
    auto* skipBtn = buttons->addButton(tr("Skip"), QDialogButtonBox::RejectRole);
    layout->addWidget(buttons);

    connect(recoverBtn, &QPushButton::clicked, this, [this]() {
        int row = m_list->currentRow();
        if (row >= 0 && row < static_cast<int>(m_files.size())) {
            m_selectedPath = m_files[row].autosavePath;
            m_action = Recover;
        }
        accept();
    });

    connect(discardBtn, &QPushButton::clicked, this, [this]() {
        m_action = Discard;
        accept();
    });

    connect(skipBtn, &QPushButton::clicked, this, [this]() {
        m_action = Skip;
        reject();
    });
}

} // namespace onecad::ui
