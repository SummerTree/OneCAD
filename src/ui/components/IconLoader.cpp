#include "IconLoader.h"

#include <QFile>
#include <QPainter>
#include <QSvgRenderer>

namespace onecad {
namespace ui {

namespace {
// Two-tone is the shipping default (validated in the icon contact sheet).
IconLoader::Style g_style = IconLoader::Style::TwoTone;
// Fallback accent = the sentinel authored into the SVGs; ThemeManager overrides
// this with the active theme accent so it adapts to light/dark.
QColor g_accent = QColor(QStringLiteral("#2D7FF9"));

// Accent sentinel token present in every two-tone master SVG.
const QString kAccentToken = QStringLiteral("#2D7FF9");
} // namespace

IconLoader::Style IconLoader::style() { return g_style; }
void IconLoader::setStyle(Style s) { g_style = s; }
QColor IconLoader::accent() { return g_accent; }
void IconLoader::setAccent(const QColor& c) {
    if (c.isValid()) {
        g_accent = c;
    }
}

QIcon IconLoader::load(const QString& path, const QColor& primary, int px) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("IconLoader: cannot open %s", qPrintable(path));
        return QIcon();
    }
    QString svg = QString::fromUtf8(file.readAll());

    const QString primaryName = primary.name();
    const QColor accentColor = (g_style == Style::TwoTone) ? g_accent : primary;

    // Accent sentinel first (specific literal), then the primary tokens.
    svg.replace(kAccentToken, accentColor.name(), Qt::CaseInsensitive);
    svg.replace(QLatin1String("currentColor"), primaryName);
    svg.replace(QLatin1String("#FFFFFF"), primaryName, Qt::CaseInsensitive);

    QSvgRenderer renderer(svg.toUtf8());
    if (!renderer.isValid()) {
        qWarning("IconLoader: invalid SVG %s", qPrintable(path));
        return QIcon();
    }

    QPixmap pixmap(px, px);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    renderer.render(&painter);
    painter.end();
    return QIcon(pixmap);
}

QPixmap IconLoader::tint(const QString& path, const QColor& color, int px) {
    QIcon icon(path);
    QPixmap pixmap = icon.pixmap(QSize(px, px));
    if (pixmap.isNull()) {
        pixmap = QPixmap(px, px);
        pixmap.fill(Qt::transparent);
    }
    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), color);
    return pixmap;
}

} // namespace ui
} // namespace onecad
