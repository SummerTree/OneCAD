/**
 * @file EditParameterDialog.h
 * @brief Dialog for editing operation parameters with live preview.
 */
#ifndef ONECAD_UI_HISTORY_EDITPARAMETERDIALOG_H
#define ONECAD_UI_HISTORY_EDITPARAMETERDIALOG_H

#include <QDialog>
#include <QString>
#include <QTimer>
#include <memory>
#include <string>

class QDoubleSpinBox;
class QLabel;
class QVBoxLayout;

namespace onecad {
namespace app {
class Document;
struct OperationRecord;
struct ExtrudeParams;
struct RevolveParams;
namespace commands {
class CommandProcessor;
}
namespace history {
class RegenerationEngine;
}
}

namespace ui {

class Viewport;

/**
 * @brief Dialog for editing Extrude/Revolve parameters with live preview.
 *
 * v1: Only supports Extrude and Revolve operations.
 * Uses debounced preview (100ms) on spinbox value changes.
 */
class EditParameterDialog : public QDialog {
    Q_OBJECT

public:
    EditParameterDialog(app::Document* document,
                        Viewport* viewport,
                        app::commands::CommandProcessor* commandProcessor,
                        const std::string& opId,
                        QWidget* parent = nullptr);
    ~EditParameterDialog() override;

signals:
    void previewRequested(const QString& opId);
    void parametersChanged(const QString& opId);

public slots:
    void accept() override;
    void reject() override;

private slots:
    void onValueChanged();
    void updatePreview();

private:
    void setupUi();
    void loadCurrentParams();
    void applyChanges();
    void clearPreview();
    void buildExtrudeUi(const app::ExtrudeParams& params);
    void buildRevolveUi(const app::RevolveParams& params);
    app::ExtrudeParams getExtrudeParams() const;
    app::RevolveParams getRevolveParams() const;

    app::Document* document_ = nullptr;
    Viewport* viewport_ = nullptr;
    app::commands::CommandProcessor* commandProcessor_ = nullptr;
    std::string opId_;
    QTimer* debounceTimer_ = nullptr;

    // Parameter controls
    QVBoxLayout* paramsLayout_ = nullptr;
    QDoubleSpinBox* distanceSpinbox_ = nullptr;   // Extrude
    QDoubleSpinBox* draftAngleSpinbox_ = nullptr; // Extrude
    QDoubleSpinBox* angleSpinbox_ = nullptr;      // Revolve

    bool isExtrude_ = false;
    bool hasChanges_ = false;
};

} // namespace ui
} // namespace onecad

#endif // ONECAD_UI_HISTORY_EDITPARAMETERDIALOG_H
