/**
 * @file DimensionEditor.h
 * @brief Inline editor for dimensional constraints
 */

#ifndef ONECAD_UI_SKETCH_DIMENSIONEDITOR_H
#define ONECAD_UI_SKETCH_DIMENSIONEDITOR_H

#include <QLineEdit>
#include <QMetaObject>
#include <QString>
#include <QPoint>

class QKeyEvent;
class QFocusEvent;

namespace onecad::ui {

/**
 * @brief Inline editor widget for editing dimensional constraint values.
 *
 * Appears when double-clicking on a dimensional constraint (Distance, Angle,
 * Radius, Diameter). Supports basic math expressions (+, -, *, /).
 *
 * Usage:
 * - Double-click constraint → editor appears at constraint position
 * - Enter value or expression → press Enter to confirm
 * - Press Escape to cancel
 */
class DimensionEditor : public QLineEdit {
    Q_OBJECT

public:
    explicit DimensionEditor(QWidget* parent = nullptr);
    ~DimensionEditor() override = default;

    /**
     * @brief Show editor for a specific constraint
     * @param constraintId ID of the constraint to edit
     * @param currentValue Current value of the dimension
     * @param units Display units (mm, °, etc.)
     * @param screenPos Position in screen coordinates
     */
    void showForConstraint(const QString& constraintId, double currentValue,
                           const QString& units, const QPoint& screenPos);

    /**
     * @brief Hide and reset the editor
     */
    void cancel();

    /**
     * @brief Get the constraint ID being edited
     */
    QString constraintId() const { return m_constraintId; }

signals:
    /**
     * @brief Emitted when a new value is confirmed
     * @param constraintId ID of the edited constraint
     * @param newValue The new value
     */
    void valueConfirmed(const QString& constraintId, double newValue);

    /**
     * @brief Emitted when editing is cancelled
     */
    void editCancelled();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    void updateTheme();
    void confirmValue();
    double parseExpression(const QString& text) const;

    QString m_constraintId;
    double m_originalValue = 0.0;
    QString m_units;
    QMetaObject::Connection m_themeConnection;
};

} // namespace onecad::ui

#endif // ONECAD_UI_SKETCH_DIMENSIONEDITOR_H
