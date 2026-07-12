/**
 * @file ModalOverlay.h
 * @brief Reusable in-app modal dialog rendered inside the main window.
 */
#ifndef ONECAD_UI_COMPONENTS_MODALOVERLAY_H
#define ONECAD_UI_COMPONENTS_MODALOVERLAY_H

#include <QWidget>
#include <QString>
#include <QMetaObject>

class QVBoxLayout;
class QLayout;
class QLabel;
class QDialogButtonBox;
class QKeyEvent;
class QMouseEvent;
class QShowEvent;

namespace onecad {
namespace ui {

/**
 * @brief Modal dialog rendered as an in-app overlay (not a native OS window).
 *
 * Fills its parent with a dim scrim and centers a themed card. Subclasses (or
 * callers) populate the card body via addContentWidget()/addContentLayout() and
 * override onAccept()/onReject() to validate/apply. The overlay self-positions by
 * tracking its parent's geometry, so the owner only needs to create, show, and
 * react to finished(bool).
 *
 * Standard chrome: title label, content area, hidden inline error label, and an
 * Ok/Cancel button box. Esc / outside-click cancel; Enter accepts.
 */
class ModalOverlay : public QWidget {
    Q_OBJECT

public:
    explicit ModalOverlay(QWidget* parent = nullptr);
    ~ModalOverlay() override;

    void setTitle(const QString& title);
    void setCardMinimumWidth(int width);
    void addContentWidget(QWidget* widget);
    void addContentLayout(QLayout* layout);
    void setAcceptText(const QString& text);
    void setRejectText(const QString& text);
    void setError(const QString& message);
    void clearError();

signals:
    void finished(bool accepted);

public slots:
    void accept();
    void reject();

protected:
    /// Validate/apply on OK. Return false to keep the overlay open (e.g. after setError()).
    virtual bool onAccept() { return true; }
    /// Cleanup on Cancel / Esc / outside-click.
    virtual void onReject() {}
    /// Build scrim + card stylesheet from the active theme. Subclasses may extend.
    virtual void applyTheme();

    QWidget* card() const { return panel_; }
    QVBoxLayout* contentLayout() const { return contentLayout_; }

    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void fitToParent();

    QWidget* panel_ = nullptr;
    QLabel* titleLabel_ = nullptr;
    QVBoxLayout* contentLayout_ = nullptr;
    QLabel* errorLabel_ = nullptr;
    QDialogButtonBox* buttonBox_ = nullptr;
    QMetaObject::Connection themeConnection_;
};

} // namespace ui
} // namespace onecad

#endif // ONECAD_UI_COMPONENTS_MODALOVERLAY_H
