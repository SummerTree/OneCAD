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
class QCheckBox;
class QLabel;
class QSpinBox;
class QVBoxLayout;

namespace onecad {
namespace app {
class Document;
struct OperationRecord;
struct ExtrudeParams;
struct LinearPatternParams;
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
 * @brief Dialog for editing operation parameters with live preview.
 *
 * v1: Supports Extrude, Revolve, and Linear Pattern operations.
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
    bool initializePreviewState();
    bool applyChanges();
    void clearPreview();
    void buildExtrudeUi(const app::ExtrudeParams& params);
    void buildRevolveUi(const app::RevolveParams& params);
    void buildLinearPatternUi(const app::LinearPatternParams& params);
    app::ExtrudeParams getExtrudeParams() const;
    app::RevolveParams getRevolveParams() const;
    app::LinearPatternParams getLinearPatternParams() const;

    app::Document* document_ = nullptr;
    Viewport* viewport_ = nullptr;
    app::commands::CommandProcessor* commandProcessor_ = nullptr;
    std::string opId_;
    QTimer* debounceTimer_ = nullptr;
    std::unique_ptr<app::Document> previewDocument_;
    std::unique_ptr<app::history::RegenerationEngine> previewEngine_;

    // Parameter controls
    QVBoxLayout* paramsLayout_ = nullptr;
    QDoubleSpinBox* distanceSpinbox_ = nullptr;   // Extrude
    QDoubleSpinBox* draftAngleSpinbox_ = nullptr; // Extrude
    QDoubleSpinBox* angleSpinbox_ = nullptr;      // Revolve
    QDoubleSpinBox* spacingSpinbox_ = nullptr;    // Linear pattern
    QSpinBox* countSpinbox_ = nullptr;            // Linear pattern
    QCheckBox* fuseCheck_ = nullptr;              // Linear pattern

    enum class Mode {
        Extrude,
        Revolve,
        LinearPattern
    };

    Mode mode_ = Mode::Extrude;
    bool hasChanges_ = false;
    bool previewValid_ = true;
    QString previewError_;
};

} // namespace ui
} // namespace onecad

#endif // ONECAD_UI_HISTORY_EDITPARAMETERDIALOG_H
