#include "MeshExportDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>

namespace onecad::ui {

MeshExportDialog::MeshExportDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Export Mesh"));
    setMinimumWidth(300);

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;

    m_formatCombo = new QComboBox;
    m_formatCombo->addItem(tr("STL (Binary)"), STL_Binary);
    m_formatCombo->addItem(tr("STL (ASCII)"), STL_ASCII);
    m_formatCombo->addItem(tr("OBJ"), OBJ);
    form->addRow(tr("Format:"), m_formatCombo);

    m_visibleOnlyCheck = new QCheckBox(tr("Visible bodies only"));
    m_visibleOnlyCheck->setChecked(true);
    form->addRow(m_visibleOnlyCheck);

    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

MeshExportDialog::Format MeshExportDialog::selectedFormat() const {
    return static_cast<Format>(m_formatCombo->currentData().toInt());
}

bool MeshExportDialog::visibleOnly() const {
    return m_visibleOnlyCheck->isChecked();
}

QString MeshExportDialog::fileFilter() const {
    switch (selectedFormat()) {
    case STL_Binary:
    case STL_ASCII:
        return tr("STL Files (*.stl)");
    case OBJ:
        return tr("OBJ Files (*.obj)");
    }
    return {};
}

QString MeshExportDialog::defaultExtension() const {
    switch (selectedFormat()) {
    case STL_Binary:
    case STL_ASCII:
        return ".stl";
    case OBJ:
        return ".obj";
    }
    return {};
}

} // namespace onecad::ui
