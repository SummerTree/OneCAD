#include "SidebarToolButton.h"

#include <QPainter>
#include <QPalette>
#include <QEvent>
#include <QFont>
#include <QSizePolicy>
#include <QSvgRenderer>
#include <QFile>

namespace onecad {
namespace ui {

SidebarToolButton::SidebarToolButton(const QString& symbol,
                                     const QString& tooltip,
                                     QWidget* parent)
    : QToolButton(parent)
    , m_symbol(symbol)
    , m_isFromSvg(false) {
    setProperty("sidebarButton", true);
    setToolTip(tooltip);
    setToolButtonStyle(Qt::ToolButtonIconOnly);
    setAutoRaise(false);
    setIconSize(QSize(24, 24));
    setFixedSize(42, 42);  // Fixed size prevents expansion conflicts
    updateIcon();
}

SidebarToolButton* SidebarToolButton::fromSvgIcon(const QString& svgPath,
                                                   const QString& tooltip,
                                                   QWidget* parent) {
    auto* button = new SidebarToolButton("", tooltip, parent);
    button->m_symbol = svgPath;
    button->m_isFromSvg = true;
    button->setIcon(button->loadSvgIcon(svgPath));
    return button;
}

void SidebarToolButton::setSymbol(const QString& symbol) {
    if (m_symbol == symbol) {
        return;
    }

    m_symbol = symbol;
    updateIcon();
}

void SidebarToolButton::changeEvent(QEvent* event) {
    if (event->type() == QEvent::PaletteChange ||
        event->type() == QEvent::StyleChange) {
        updateIcon();
    }

    QToolButton::changeEvent(event);
}

void SidebarToolButton::updateIcon() {
    if (m_isFromSvg) {
        setIcon(loadSvgIcon(m_symbol));
    } else {
        setIcon(iconFromSymbol(m_symbol));
    }
}

QIcon SidebarToolButton::iconFromSymbol(const QString& symbol) const {
    const int size = 28;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QFont font = painter.font();
    font.setPointSize(18);
    painter.setFont(font);
    painter.setPen(palette().color(QPalette::ButtonText));
    painter.drawText(pixmap.rect(), Qt::AlignCenter, symbol);
    painter.end();

    return QIcon(pixmap);
}

QIcon SidebarToolButton::loadSvgIcon(const QString& svgPath) const {
    // First, try to load the SVG file from Qt resources
    QFile file(svgPath);
    if (!file.exists()) {
        qWarning() << "SVG file does not exist:" << svgPath;
        return QIcon();
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open SVG file:" << svgPath << "Error:" << file.errorString();
        return QIcon();
    }
    
    // Read SVG data
    QByteArray svgData = file.readAll();
    file.close();
    
    // Get the button text color from palette for theme integration
    QColor iconColor = palette().color(QPalette::ButtonText);
    
    // Convert to QString for manipulation
    QString svgString = QString::fromUtf8(svgData);
    
    // Replace currentColor with actual color (in case SVG uses it)
    // This allows the SVG to adapt to the palette
    svgString.replace("currentColor", iconColor.name());
    
    // Also handle white color replacement for legacy SVGs
    svgString.replace("#FFFFFF", iconColor.name());
    svgString.replace("#ffffff", iconColor.name());
    
    // Convert back to QByteArray
    QByteArray modifiedSvg = svgString.toUtf8();
    
    // Create SVG renderer with modified data
    QSvgRenderer renderer(modifiedSvg);
    
    if (!renderer.isValid()) {
        qWarning() << "SVG renderer is invalid for:" << svgPath;
        return QIcon();
    }
    
   // Render to pixmap
    const int size = 28;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    renderer.render(&painter);
    painter.end();
    
    return QIcon(pixmap);
}

} // namespace ui
} // namespace onecad
