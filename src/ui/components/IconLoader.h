#ifndef ONECAD_UI_COMPONENTS_ICONLOADER_H
#define ONECAD_UI_COMPONENTS_ICONLOADER_H

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

namespace onecad {
namespace ui {

/**
 * @brief Central SVG icon loader implementing the two-preset color model.
 *
 * Master SVGs (see resources/icons/DESIGN.md) use two color tokens:
 *   - `currentColor`  → PRIMARY (geometry / structure)
 *   - `#2D7FF9`       → ACCENT  (the operation verb: arrow, axis, cut, relation)
 *
 * Monochrome preset collapses the accent onto the primary color; Two-tone keeps
 * the accent as the theme accent. The preset + accent are process-global and set
 * by ThemeManager; primary is supplied per call (usually the widget's ButtonText).
 */
class IconLoader {
public:
    enum class Style { Monochrome, TwoTone };

    static Style style();
    static void setStyle(Style s);
    static QColor accent();
    static void setAccent(const QColor& c);

    // Load a resource SVG, recolor per the current preset, render to `px`.
    static QIcon load(const QString& path, const QColor& primary, int px = 28);

    // Silhouette tint (alpha SourceIn) to a single flat color — for contexts
    // that only need a monochrome glyph (tree/status inline buttons).
    static QPixmap tint(const QString& path, const QColor& color, int px = 18);
};

} // namespace ui
} // namespace onecad

#endif // ONECAD_UI_COMPONENTS_ICONLOADER_H
