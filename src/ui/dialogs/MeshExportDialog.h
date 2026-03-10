#pragma once

#include <QDialog>
#include <QString>

class QComboBox;
class QCheckBox;

namespace onecad::ui {

class MeshExportDialog : public QDialog {
    Q_OBJECT

public:
    enum Format { STL_Binary, STL_ASCII, OBJ };

    explicit MeshExportDialog(QWidget* parent = nullptr);

    Format selectedFormat() const;
    bool visibleOnly() const;
    QString fileFilter() const;
    QString defaultExtension() const;

private:
    QComboBox* m_formatCombo = nullptr;
    QCheckBox* m_visibleOnlyCheck = nullptr;
};

} // namespace onecad::ui
