/**
 * @file RegenFailureDialog.cpp
 * @brief Implementation of RegenFailureDialog.
 */
#include "RegenFailureDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>

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
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // Header
    auto* headerLabel = new QLabel(tr("Some operations failed during regeneration:"));
    headerLabel->setStyleSheet("font-weight: bold;");
    mainLayout->addWidget(headerLabel);

    // Failure list
    failureList_ = new QListWidget;
    failureList_->setStyleSheet(R"(
        QListWidget {
            background-color: #1e1e1e;
            border: 1px solid #3e3e42;
            color: #cccccc;
        }
        QListWidget::item {
            padding: 8px;
            border-bottom: 1px solid #3e3e42;
        }
    )");

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

    auto* suppressBtn = new QPushButton(tr("Suppress Failed"));
    suppressBtn->setToolTip(tr("Keep in history but mark as suppressed"));
    connect(suppressBtn, &QPushButton::clicked, this, &RegenFailureDialog::onSuppressFailed);

    auto* cancelBtn = new QPushButton(tr("Cancel"));
    cancelBtn->setToolTip(tr("Leave document in partial state"));
    connect(cancelBtn, &QPushButton::clicked, this, &RegenFailureDialog::onCancel);

    buttonLayout->addStretch();
    buttonLayout->addWidget(deleteBtn);
    buttonLayout->addWidget(suppressBtn);
    buttonLayout->addWidget(cancelBtn);

    mainLayout->addLayout(buttonLayout);
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
