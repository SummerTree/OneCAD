/**
 * @file StepExportDialog.cpp
 * @brief Implementation of STEP export options dialog
 */
#include "StepExportDialog.h"

#include <QComboBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>

namespace onecad::ui {

StepExportDialog::StepExportDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Export STEP Options"));
    setMinimumWidth(300);

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;

    m_schemaCombo = new QComboBox(this);
    m_schemaCombo->addItem("AP214IS (most compatible)", "AP214IS");
    m_schemaCombo->addItem("AP242DIS (modern)", "AP242DIS");
    form->addRow(tr("Schema:"), m_schemaCombo);

    m_visibleOnlyCheck = new QCheckBox(tr("Visible bodies only"), this);
    m_visibleOnlyCheck->setChecked(true);
    form->addRow(m_visibleOnlyCheck);

    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

StepExportDialog::Options StepExportDialog::options() const {
    Options opts;
    opts.schema = m_schemaCombo->currentData().toString();
    opts.visibleOnly = m_visibleOnlyCheck->isChecked();
    return opts;
}

} // namespace onecad::ui
