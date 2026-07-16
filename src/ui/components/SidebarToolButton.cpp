#include "SidebarToolButton.h"
#include "IconLoader.h"

#include <QPainter>
#include <QPalette>
#include <QEvent>
#include <QFont>
#include <QSizePolicy>

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
    // Primary color comes from the palette (theme-integrated); IconLoader applies
    // the active mono/two-tone preset + accent. Re-runs on PaletteChange/StyleChange.
    return IconLoader::load(svgPath, palette().color(QPalette::ButtonText), 28);
}

} // namespace ui
} // namespace onecad
