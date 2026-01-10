#ifndef ONECAD_UI_COMPONENTS_TOGGLESWITCH_H
#define ONECAD_UI_COMPONENTS_TOGGLESWITCH_H

#include <QCheckBox>
#include <QPropertyAnimation>

namespace onecad::ui {

class ToggleSwitch : public QCheckBox {
    Q_OBJECT
    Q_PROPERTY(float indicatorOpacity READ indicatorOpacity WRITE setIndicatorOpacity)

public:
    explicit ToggleSwitch(const QString& text = "", QWidget* parent = nullptr);

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
    float m_indicatorOpacity = 0.0f;
    bool m_isHovered = false;
    QPropertyAnimation* m_animate = nullptr;
};

} // namespace onecad::ui

#endif // ONECAD_UI_COMPONENTS_TOGGLESWITCH_H
