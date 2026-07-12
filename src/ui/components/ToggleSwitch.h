#ifndef ONECAD_UI_COMPONENTS_TOGGLESWITCH_H
#define ONECAD_UI_COMPONENTS_TOGGLESWITCH_H

#include <QCheckBox>
#include <QColor>
#include <QMetaObject>
#include <QPropertyAnimation>

namespace onecad::ui {

class ToggleSwitch : public QCheckBox {
    Q_OBJECT
    Q_PROPERTY(float indicatorOpacity READ indicatorOpacity WRITE setIndicatorOpacity)

public:
    explicit ToggleSwitch(const QString& text = "", QWidget* parent = nullptr);
    ~ToggleSwitch() override;

    float indicatorOpacity() const { return m_indicatorOpacity; }
    void setIndicatorOpacity(float opacity);

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void checkStateSet() override;
    void nextCheckState() override;
    bool hitButton(const QPoint& pos) const override;
    QSize sizeHint() const override;

private:
    // This widget is custom-painted, so it cannot be themed via the global stylesheet;
    // it caches theme colors and repaints on ThemeManager::themeChanged instead.
    void applyTheme();

    float m_indicatorOpacity = 0.0f;
    bool m_isHovered = false;
    QPropertyAnimation* m_animate = nullptr;

    QColor m_textColor;
    QColor m_textDisabledColor;
    QColor m_trackOffColor;
    QColor m_trackOnColor;
    QColor m_knobColor;
    int m_fontPixelSize = 13;
    QMetaObject::Connection m_themeConnection;
};

} // namespace onecad::ui

#endif // ONECAD_UI_COMPONENTS_TOGGLESWITCH_H
