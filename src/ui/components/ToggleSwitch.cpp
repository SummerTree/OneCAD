#include "ToggleSwitch.h"
#include <QPainter>
#include <QPropertyAnimation>
#include <QStyleOption>

namespace onecad::ui {

ToggleSwitch::ToggleSwitch(const QString& text, QWidget* parent)
    : QCheckBox(text, parent) {
    // Custom drawing, so no standard indicator
    setStyleSheet("QCheckBox::indicator { width: 0px; height: 0px; }");
    setCursor(Qt::PointingHandCursor);
    
    // Animation for smoothness
    m_animate = new QPropertyAnimation(this, "indicatorOpacity", this);
    m_animate->setDuration(150);
    
    // Crucial for layout to work correctly
    setMinimumHeight(28);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void ToggleSwitch::setIndicatorOpacity(float opacity) {
    m_indicatorOpacity = opacity;
    update();
}

void ToggleSwitch::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Layout: Text on left, Switch on right (iOS style)
    // Adjust logic if RTL layout is enforced by panel, but Manual layout here is safer
    
    QRect rect = contentsRect();
    int switchWidth = 36;
    int switchHeight = 20;
    
    // Position switch at right edge
    int switchX = rect.width() - switchWidth; 
    int switchY = (rect.height() - switchHeight) / 2;
    QRect switchRect(switchX, switchY, switchWidth, switchHeight);

    // Draw Text
    // Leave some padding
    QRect textRect = rect;
    textRect.setRight(switchX - 10);
    
    p.setPen(isEnabled() ? QColor("#E0E0E0") : QColor("#888888")); // Lighter text for dark theme
    QFont font = p.font();
    font.setPixelSize(13); // Match previous design
    p.setFont(font);
    
    // Use QStyle functionality to respect alignment if needed, 
    // but here we force space-between style: Text Left, Switch Right.
    p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text());

    // --- Draw Switch ---
    
    // Interpolate colors based on checked state (or animation value)
    // Actually, simple state check with animated position is better for "knob sliding" feel.
    // For now, let's just animate the knob position using m_indicatorOpacity (0.0 to 1.0)
    
    // If not animating, sync internal state
    if (m_animate->state() != QPropertyAnimation::Running) {
        m_indicatorOpacity = isChecked() ? 1.0f : 0.0f;
    }

    float t = m_indicatorOpacity; // 0.0 (Off) -> 1.0 (On)

    // Track Color
    // Off: #3a3a3a, On: #007AFF (Apple Blue)
    // Interpolate?
    QColor offColor("#3a3a3a");
    QColor onColor("#007AFF");
    
    int r = offColor.red() + (onColor.red() - offColor.red()) * t;
    int g = offColor.green() + (onColor.green() - offColor.green()) * t;
    int b = offColor.blue() + (onColor.blue() - offColor.blue()) * t;
    QColor trackColor(r, g, b);
    
    // Track Border
    // Off: #555, On: #0066DD
    p.setPen(Qt::NoPen);
    p.setBrush(trackColor);
    p.drawRoundedRect(switchRect, switchHeight / 2, switchHeight / 2);
    
    // Knob
    int knobPadding = 2;
    int knobSize = switchHeight - 2 * knobPadding;
    
    // Position: t=0 -> Left, t=1 -> Right
    int startX = switchX + knobPadding;
    int endX = switchX + switchWidth - knobSize - knobPadding;
    int knobX = startX + (endX - startX) * t;
    int knobY = switchY + knobPadding;
    
    p.setBrush(Qt::white);
    p.drawEllipse(knobX, knobY, knobSize, knobSize);
}

void ToggleSwitch::enterEvent(QEnterEvent* event) {
    m_isHovered = true;
    update();
    QCheckBox::enterEvent(event);
}

void ToggleSwitch::leaveEvent(QEvent* event) {
    m_isHovered = false;
    update();
    QCheckBox::leaveEvent(event);
}

void ToggleSwitch::checkStateSet() {
    // Start animation logic
    m_animate->stop();
    m_animate->setStartValue(m_indicatorOpacity);
    m_animate->setEndValue(isChecked() ? 1.0f : 0.0f);
    m_animate->start();
    
    QCheckBox::checkStateSet();
}

void ToggleSwitch::nextCheckState() {
    QCheckBox::nextCheckState();
}

bool ToggleSwitch::hitButton(const QPoint& pos) const {
    return contentsRect().contains(pos);
}

QSize ToggleSwitch::sizeHint() const {
    // Ensure height accommodates the switch
    return QSize(200, 28);
}

} // namespace onecad::ui
