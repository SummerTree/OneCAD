/**
 * @file StepExportDialog.h
 * @brief Export options dialog for STEP files
 */
#pragma once

#include <QDialog>
#include <QString>

class QComboBox;
class QCheckBox;

namespace onecad::ui {

class StepExportDialog : public QDialog {
    Q_OBJECT

public:
    struct Options {
        QString schema = "AP214IS";
        bool visibleOnly = true;
    };

    explicit StepExportDialog(QWidget* parent = nullptr);

    Options options() const;

private:
    QComboBox* m_schemaCombo = nullptr;
    QCheckBox* m_visibleOnlyCheck = nullptr;
};

} // namespace onecad::ui
