/**
 * @file RegenFailureDialog.h
 * @brief Dialog for handling regeneration failures.
 */
#ifndef ONECAD_UI_HISTORY_REGENFAILUREDIALOG_H
#define ONECAD_UI_HISTORY_REGENFAILUREDIALOG_H

#include <QDialog>
#include <QString>
#include <QStringList>
#include <vector>

class QListWidget;

namespace onecad {
namespace ui {

/**
 * @brief Dialog shown when operations fail during regeneration.
 *
 * Options:
 * - Delete Failed: Remove failed operations from history
 * - Suppress Failed: Keep in history but mark as suppressed
 * - Cancel: Abort and leave document in partial state
 */
class RegenFailureDialog : public QDialog {
    Q_OBJECT

public:
    enum class Result {
        DeleteFailed,
        SuppressFailed,
        Cancel
    };

    struct FailedOp {
        QString opId;
        QString description;
        QString errorMessage;
    };

    explicit RegenFailureDialog(const std::vector<FailedOp>& failedOps,
                                QWidget* parent = nullptr);

    Result selectedAction() const { return selectedAction_; }

private slots:
    void onDeleteFailed();
    void onSuppressFailed();
    void onCancel();

private:
    void setupUi(const std::vector<FailedOp>& failedOps);

    Result selectedAction_ = Result::Cancel;
    QListWidget* failureList_ = nullptr;
};

} // namespace ui
} // namespace onecad

#endif // ONECAD_UI_HISTORY_REGENFAILUREDIALOG_H
