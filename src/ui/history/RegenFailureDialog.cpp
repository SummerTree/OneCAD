/**
 * @file RegenFailureDialog.cpp
 * @brief Implementation of RegenFailureDialog.
 */
#include "RegenFailureDialog.h"

#include "../theme/ThemeManager.h"
#include "../theme/ThemeConfig.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace onecad::ui {

RegenFailureDialog::RegenFailureDialog(const std::vector<FailedOp>& failedOps,
                                       QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Regeneration Failed"));
    setModal(true);
    setMinimumWidth(400);
    setMinimumHeight(300);

    setupUi(failedOps);
}

void RegenFailureDialog::setupUi(const std::vector<FailedOp>& failedOps) {
    const ThemeDefinition& theme = ThemeManager::instance().currentTheme();

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(theme.metrics.spacingMd);

    // Header
    auto* headerLabel = new QLabel(tr("Some operations failed during regeneration:"));
    QFont headerFont = headerLabel->font();
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);
    mainLayout->addWidget(headerLabel);

    // Failure list (themed from tokens so it follows the active theme).
    failureList_ = new QListWidget;
    failureList_->setStyleSheet(
        QStringLiteral("QListWidget { background-color: %1; border: 1px solid %2; color: %3; }"
                       "QListWidget::item { padding: %4px; border-bottom: 1px solid %2; }")
            .arg(toQssColor(theme.ui.treeBackground),
                 toQssColor(theme.ui.panelBorder),
                 toQssColor(theme.ui.treeText))
            .arg(theme.metrics.spacingSm));

    for (const auto& op : failedOps) {
        QString text = QString("%1: %2").arg(op.description, op.errorMessage);
        failureList_->addItem(text);
    }

    mainLayout->addWidget(failureList_, 1);

    // Info label
    auto* infoLabel = new QLabel(tr("Choose how to handle the failed operations:"));
    mainLayout->addWidget(infoLabel);

    // Buttons
    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->setSpacing(8);

    auto* deleteBtn = new QPushButton(tr("Delete Failed"));
    deleteBtn->setToolTip(tr("Remove failed operations from history"));
    connect(deleteBtn, &QPushButton::clicked, this, &RegenFailureDialog::onDeleteFailed);

    // Suppress is the recommended (non-destructive) action -> primary.
    auto* suppressBtn = new QPushButton(tr("Suppress Failed"));
    suppressBtn->setToolTip(tr("Keep in history but mark as suppressed"));
    suppressBtn->setProperty("primary", true);
    connect(suppressBtn, &QPushButton::clicked, this, &RegenFailureDialog::onSuppressFailed);

    auto* cancelBtn = new QPushButton(tr("Cancel"));
    cancelBtn->setToolTip(tr("Leave document in partial state"));
    cancelBtn->setProperty("ghost", true);
    connect(cancelBtn, &QPushButton::clicked, this, &RegenFailureDialog::onCancel);

    buttonLayout->addStretch();
    buttonLayout->addWidget(deleteBtn);
    buttonLayout->addWidget(suppressBtn);
    buttonLayout->addWidget(cancelBtn);

    mainLayout->addLayout(buttonLayout);

    // Re-polish so the primary/ghost property selectors are applied.
    for (QPushButton* button : {deleteBtn, suppressBtn, cancelBtn}) {
        button->style()->unpolish(button);
        button->style()->polish(button);
    }
}

void RegenFailureDialog::onDeleteFailed() {
    selectedAction_ = Result::DeleteFailed;
    accept();
}

void RegenFailureDialog::onSuppressFailed() {
    selectedAction_ = Result::SuppressFailed;
    accept();
}

void RegenFailureDialog::onCancel() {
    selectedAction_ = Result::Cancel;
    reject();
}

} // namespace onecad::ui
