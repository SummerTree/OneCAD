#include "ThemeConfig.h"

namespace onecad::ui {

QString toQssColor(const QColor& color) {
    if (!color.isValid()) {
        return {};
    }
    if (color.alpha() < 255) {
        return color.name(QColor::HexArgb);
    }
    return color.name();
}

namespace {

// Primary accent (Electric Blue)
const QColor kAccent(0, 148, 198);
const QColor kAccentHover = kAccent.lighter(115);
const QColor kAccentActive = kAccent.darker(110);
const QColor kAccentTextLight(255, 255, 255);
const QColor kAccentTextDark(230, 245, 255);

ThemeDefinition makeLightTheme() {
    ThemeDefinition theme;
    theme.id = QStringLiteral("Light");
    theme.displayName = QStringLiteral("Light");
    theme.isDark = false;

    theme.ui.windowBackground = QColor(243, 243, 243);
    theme.ui.widgetBackground = QColor(243, 243, 243);
    theme.ui.widgetText = QColor(51, 51, 51);
    theme.ui.navigatorBackground = QColor(246, 246, 246);
    theme.ui.panelBackground = QColor(246, 246, 246);
    theme.ui.inspectorBackground = QColor(255, 255, 255);
    theme.ui.panelBorder = QColor(204, 204, 204);
    theme.ui.menuBarBackground = QColor(238, 238, 238);
    theme.ui.menuBarText = QColor(51, 51, 51);
    theme.ui.menuBarBorder = QColor(204, 204, 204);
    theme.ui.menuItemSelectedBackground = kAccentHover;
    theme.ui.menuItemSelectedText = kAccentTextLight;
    theme.ui.menuBackground = QColor(255, 255, 255);
    theme.ui.menuText = QColor(51, 51, 51);
    theme.ui.menuBorder = QColor(204, 204, 204);
    theme.ui.menuSeparator = QColor(221, 221, 221);
    theme.ui.statusBarBackground = QColor(243, 243, 243);
    theme.ui.statusBarText = QColor(17, 17, 17);
    theme.ui.statusBarBorder = QColor(204, 204, 204);
    theme.ui.dockTitleBackground = QColor(238, 238, 238);
    theme.ui.dockTitleBorder = QColor(204, 204, 204);
    theme.ui.treeBackground = QColor(255, 255, 255);
    theme.ui.treeText = QColor(51, 51, 51);
    theme.ui.treeHoverBackground = QColor(240, 240, 240);
    theme.ui.treeSelectedBackground = kAccentHover;
    theme.ui.treeSelectedText = kAccentTextLight;
    theme.ui.sidebarButtonBackground = QColor(246, 246, 246);
    theme.ui.sidebarButtonText = QColor(34, 34, 34);
    theme.ui.sidebarButtonBorder = QColor(204, 204, 204);
    theme.ui.sidebarButtonHoverBackground = QColor(255, 255, 255);
    theme.ui.sidebarButtonHoverBorder = kAccent;
    theme.ui.sidebarButtonPressedBackground = kAccent;
    theme.ui.sidebarButtonPressedBorder = kAccentActive;
    theme.ui.toolButtonBackground = QColor(246, 246, 246);
    theme.ui.toolButtonText = QColor(51, 51, 51);
    theme.ui.toolButtonBorder = QColor(204, 204, 204);
    theme.ui.toolButtonHoverBackground = QColor(255, 255, 255);
    theme.ui.toolButtonHoverBorder = kAccentHover;
    theme.ui.toolButtonPressedBackground = kAccent;
    theme.ui.toolButtonPressedBorder = kAccentActive;
    theme.ui.inspectorHintText = QColor(102, 102, 102);
    theme.ui.inspectorTipText = QColor(58, 143, 58);
    theme.ui.inspectorEntityIdText = QColor(102, 102, 102);
    theme.ui.inspectorSeparator = QColor(204, 204, 204);
    theme.ui.inspectorPlaceholderText = QColor(136, 136, 136);
    theme.ui.scrollbarTrack = QColor(243, 243, 243);
    theme.ui.scrollbarHandle = QColor(193, 193, 193);
    theme.ui.scrollbarHandleHover = QColor(168, 168, 168);

    theme.dimensionEditor.background = QColor(255, 255, 255);
    theme.dimensionEditor.border = kAccent;
    theme.dimensionEditor.borderFocus = kAccentActive;

    theme.constraints.unsatisfiedText = QColor(255, 100, 100);

    theme.status.dofError = QColor(255, 0, 0);
    theme.status.dofOk = QColor(0, 128, 0);
    theme.status.dofWarning = QColor(255, 165, 0);

    theme.navigator.placeholderText = QColor(128, 128, 128);
    theme.navigator.headerText = QColor(38, 44, 52);
    theme.navigator.headerBackground = QColor(232, 237, 242);
    theme.navigator.divider = QColor(210, 214, 220);
    theme.navigator.itemText = theme.ui.treeText;
    theme.navigator.itemIcon = QColor(65, 74, 86);
    theme.navigator.itemHoverBackground = QColor(228, 239, 246);
    theme.navigator.itemSelectedBackground = kAccentHover;
    theme.navigator.itemSelectedText = kAccentTextLight;
    theme.navigator.inlineButtonBackground = QColor(240, 244, 248);
    theme.navigator.inlineButtonHoverBackground = QColor(223, 234, 241);
    theme.navigator.inlineButtonBorder = QColor(210, 214, 220);
    theme.navigator.inlineButtonHoverBorder = kAccent;

    theme.viewCube.face = QColor(214, 214, 214, 191);
    theme.viewCube.faceHover = QColor(kAccent.red(), kAccent.green(), kAccent.blue(), 191);
    theme.viewCube.text = QColor(20, 20, 20);
    theme.viewCube.textHover = kAccentTextLight;
    theme.viewCube.edgeHover = QColor(kAccentHover.red(), kAccentHover.green(), kAccentHover.blue(), 191);
    theme.viewCube.cornerHover = QColor(kAccentHover.red(), kAccentHover.green(), kAccentHover.blue(), 191);
    theme.viewCube.faceBorder = QColor(150, 150, 150, 191);
    theme.viewCube.axisX = QColor(220, 80, 80, 191);
    theme.viewCube.axisY = QColor(80, 200, 120, 191);
    theme.viewCube.axisZ = QColor(80, 120, 220, 191);

    theme.viewport.background = QColor(236, 240, 244);
    theme.viewport.grid.major = QColor(170, 180, 190, 140);
    theme.viewport.grid.minor = QColor(180, 190, 200, 90);
    theme.viewport.grid.axisX = QColor(255, 100, 100);
    theme.viewport.grid.axisY = QColor(100, 255, 100);
    theme.viewport.grid.axisZ = QColor(100, 100, 255);
    theme.viewport.planes.xy = QColor(0, 148, 198, 70);
    theme.viewport.planes.xz = QColor(110, 190, 140, 65);
    theme.viewport.planes.yz = QColor(240, 170, 90, 65);
    theme.viewport.planes.labelText = QColor(30, 32, 36);
    theme.viewport.overlay.previewDimensionText = QColor(0, 0, 0);
    theme.viewport.overlay.previewDimensionBackground = QColor(255, 255, 255, 200);
    theme.viewport.overlay.toolIndicator = QColor(kAccent.red(), kAccent.green(), kAccent.blue(), 235);
    theme.viewport.overlay.toolLabelText = QColor(20, 24, 28);
    theme.viewport.overlay.toolLabelBackground = QColor(255, 255, 255, 210);
    theme.viewport.selection.faceFillHover = QColor(kAccent.red(), kAccent.green(), kAccent.blue(), 70);
    theme.viewport.selection.faceOutlineHover = QColor(kAccentHover.red(), kAccentHover.green(), kAccentHover.blue(), 160);
    theme.viewport.selection.faceFillSelected = QColor(kAccent.red(), kAccent.green(), kAccent.blue(), 110);
    theme.viewport.selection.faceOutlineSelected = QColor(kAccent.red(), kAccent.green(), kAccent.blue(), 220);
    theme.viewport.selection.edgeHover = theme.viewport.selection.faceOutlineHover;
    theme.viewport.selection.edgeSelected = theme.viewport.selection.faceOutlineSelected;
    theme.viewport.selection.vertexHover = theme.viewport.selection.faceOutlineHover;
    theme.viewport.selection.vertexSelected = theme.viewport.selection.faceOutlineSelected;
    theme.viewport.body.base = QColor(176, 184, 194);
    theme.viewport.body.edge = QColor(70, 78, 88);
    theme.viewport.body.specular = QColor(255, 255, 255);
    theme.viewport.body.rim = QColor(210, 220, 232);
    theme.viewport.body.glow = QColor(kAccent.red(), kAccent.green(), kAccent.blue());
    theme.viewport.body.highlight = QColor(210, 220, 232);
    theme.viewport.body.keyLightDir = QVector3D(-0.4f, 0.5f, 0.75f);
    theme.viewport.body.fillLightDir = QVector3D(0.6f, -0.2f, 0.55f);
    theme.viewport.body.fillLightIntensity = 0.35f;
    theme.viewport.body.ambientIntensity = 0.25f;
    theme.viewport.body.hemiUpDir = QVector3D(0.0f, 1.0f, 0.0f);
    theme.viewport.body.hemiSky = QColor(230, 235, 242);
    theme.viewport.body.hemiGround = QColor(77, 71, 64);
    theme.viewport.body.ambientGradientStrength = 0.08f;
    theme.viewport.body.ambientGradientDir = QVector3D(0.0f, 1.0f, 0.0f);

    theme.sketch.normalGeometry = QColor(255, 255, 255);
    theme.sketch.constructionGeometry = QColor(130, 150, 190);
    theme.sketch.selectedGeometry = kAccent;
    theme.sketch.previewGeometry = QColor(180, 184, 188);
    theme.sketch.errorGeometry = QColor(255, 77, 77);
    theme.sketch.constraintIcon = QColor(230, 179, 51);
    theme.sketch.dimensionText = kAccent;
    theme.sketch.conflictHighlight = QColor(255, 0, 0);
    theme.sketch.fullyConstrained = kAccent;
    theme.sketch.underConstrained = QColor(255, 145, 0);
    theme.sketch.overConstrained = QColor(255, 60, 60);
    theme.sketch.gridMajor = QColor(70, 70, 80);
    theme.sketch.gridMinor = QColor(45, 45, 55);
    theme.sketch.regionFill = QColor(kAccent.red(), kAccent.green(), kAccent.blue());

    return theme;
}

ThemeDefinition makeDarkTheme() {
    ThemeDefinition theme;
    theme.id = QStringLiteral("Dark");
    theme.displayName = QStringLiteral("Dark");
    theme.isDark = true;

    theme.ui.windowBackground = QColor(30, 30, 30);
    theme.ui.widgetBackground = QColor(30, 30, 30);
    theme.ui.widgetText = QColor(204, 204, 204);
    theme.ui.navigatorBackground = QColor(45, 45, 48);
    theme.ui.panelBackground = QColor(45, 45, 48);
    theme.ui.inspectorBackground = QColor(45, 45, 48);
    theme.ui.panelBorder = QColor(62, 62, 66);
    theme.ui.menuBarBackground = QColor(45, 45, 48);
    theme.ui.menuBarText = QColor(204, 204, 204);
    theme.ui.menuBarBorder = QColor(62, 62, 66);
    theme.ui.menuItemSelectedBackground = kAccentActive;
    theme.ui.menuItemSelectedText = kAccentTextLight;
    theme.ui.menuBackground = QColor(45, 45, 48);
    theme.ui.menuText = QColor(204, 204, 204);
    theme.ui.menuBorder = QColor(62, 62, 66);
    theme.ui.menuSeparator = QColor(62, 62, 66);
    theme.ui.statusBarBackground = QColor(31, 31, 31);
    theme.ui.statusBarText = QColor(255, 255, 255);
    theme.ui.statusBarBorder = QColor(62, 62, 66);
    theme.ui.dockTitleBackground = QColor(45, 45, 48);
    theme.ui.dockTitleBorder = QColor(62, 62, 66);
    theme.ui.treeBackground = QColor(45, 45, 48);
    theme.ui.treeText = QColor(204, 204, 204);
    theme.ui.treeHoverBackground = QColor(60, 65, 72);
    theme.ui.treeSelectedBackground = kAccentActive;
    theme.ui.treeSelectedText = kAccentTextLight;
    theme.ui.sidebarButtonBackground = QColor(45, 48, 55);
    theme.ui.sidebarButtonText = QColor(245, 245, 245);
    theme.ui.sidebarButtonBorder = QColor(62, 62, 66);
    theme.ui.sidebarButtonHoverBackground = QColor(56, 62, 70);
    theme.ui.sidebarButtonHoverBorder = kAccentHover;
    theme.ui.sidebarButtonPressedBackground = kAccent;
    theme.ui.sidebarButtonPressedBorder = kAccentActive;
    theme.ui.toolButtonBackground = QColor(45, 48, 55);
    theme.ui.toolButtonText = QColor(220, 220, 225);
    theme.ui.toolButtonBorder = QColor(62, 62, 66);
    theme.ui.toolButtonHoverBackground = QColor(56, 62, 70);
    theme.ui.toolButtonHoverBorder = kAccentHover;
    theme.ui.toolButtonPressedBackground = kAccent;
    theme.ui.toolButtonPressedBorder = kAccentActive;
    theme.ui.inspectorHintText = QColor(136, 136, 136);
    theme.ui.inspectorTipText = QColor(107, 159, 107);
    theme.ui.inspectorEntityIdText = QColor(136, 136, 136);
    theme.ui.inspectorSeparator = QColor(62, 62, 66);
    theme.ui.inspectorPlaceholderText = QColor(102, 102, 102);
    theme.ui.scrollbarTrack = QColor(30, 30, 30);
    theme.ui.scrollbarHandle = QColor(66, 66, 66);
    theme.ui.scrollbarHandleHover = QColor(104, 104, 104);

    theme.dimensionEditor.background = QColor(255, 255, 255);
    theme.dimensionEditor.border = kAccent;
    theme.dimensionEditor.borderFocus = kAccentActive;

    theme.constraints.unsatisfiedText = QColor(255, 100, 100);

    theme.status.dofError = QColor(255, 0, 0);
    theme.status.dofOk = QColor(0, 128, 0);
    theme.status.dofWarning = QColor(255, 165, 0);

    theme.navigator.placeholderText = QColor(128, 128, 128);
    theme.navigator.headerText = QColor(230, 235, 242);
    theme.navigator.headerBackground = QColor(52, 56, 62);
    theme.navigator.divider = QColor(62, 62, 66);
    theme.navigator.itemText = theme.ui.treeText;
    theme.navigator.itemIcon = QColor(198, 206, 215);
    theme.navigator.itemHoverBackground = QColor(56, 62, 70);
    theme.navigator.itemSelectedBackground = kAccentActive;
    theme.navigator.itemSelectedText = kAccentTextLight;
    theme.navigator.inlineButtonBackground = QColor(52, 56, 62);
    theme.navigator.inlineButtonHoverBackground = QColor(62, 68, 76);
    theme.navigator.inlineButtonBorder = QColor(62, 62, 66);
    theme.navigator.inlineButtonHoverBorder = kAccentHover;

    theme.viewCube.face = QColor(210, 210, 210, 191);
    theme.viewCube.faceHover = QColor(kAccent.red(), kAccent.green(), kAccent.blue(), 191);
    theme.viewCube.text = QColor(18, 18, 18);
    theme.viewCube.textHover = kAccentTextLight;
    theme.viewCube.edgeHover = QColor(kAccentHover.red(), kAccentHover.green(), kAccentHover.blue(), 191);
    theme.viewCube.cornerHover = QColor(kAccentHover.red(), kAccentHover.green(), kAccentHover.blue(), 191);
    theme.viewCube.faceBorder = QColor(150, 150, 150, 191);
    theme.viewCube.axisX = QColor(220, 80, 80, 191);
    theme.viewCube.axisY = QColor(80, 200, 120, 191);
    theme.viewCube.axisZ = QColor(80, 120, 220, 191);

    theme.viewport.background = QColor(28, 32, 38);
    theme.viewport.grid.major = QColor(90, 98, 110, 120);
    theme.viewport.grid.minor = QColor(60, 68, 80, 80);
    theme.viewport.grid.axisX = QColor(255, 100, 100);
    theme.viewport.grid.axisY = QColor(100, 255, 100);
    theme.viewport.grid.axisZ = QColor(100, 100, 255);
    theme.viewport.planes.xy = QColor(0, 148, 198, 65);
    theme.viewport.planes.xz = QColor(110, 190, 140, 60);
    theme.viewport.planes.yz = QColor(240, 170, 90, 60);
    theme.viewport.planes.labelText = QColor(235, 235, 240);
    theme.viewport.overlay.previewDimensionText = QColor(240, 244, 248);
    theme.viewport.overlay.previewDimensionBackground = QColor(0, 0, 0, 190);
    theme.viewport.overlay.toolIndicator = QColor(kAccent.red(), kAccent.green(), kAccent.blue(), 235);
    theme.viewport.overlay.toolLabelText = QColor(235, 240, 245);
    theme.viewport.overlay.toolLabelBackground = QColor(0, 0, 0, 190);
    theme.viewport.selection.faceFillHover = QColor(kAccent.red(), kAccent.green(), kAccent.blue(), 75);
    theme.viewport.selection.faceOutlineHover = QColor(kAccentHover.red(), kAccentHover.green(), kAccentHover.blue(), 170);
    theme.viewport.selection.faceFillSelected = QColor(kAccent.red(), kAccent.green(), kAccent.blue(), 120);
    theme.viewport.selection.faceOutlineSelected = QColor(kAccent.red(), kAccent.green(), kAccent.blue(), 230);
    theme.viewport.selection.edgeHover = theme.viewport.selection.faceOutlineHover;
    theme.viewport.selection.edgeSelected = theme.viewport.selection.faceOutlineSelected;
    theme.viewport.selection.vertexHover = theme.viewport.selection.faceOutlineHover;
    theme.viewport.selection.vertexSelected = theme.viewport.selection.faceOutlineSelected;
    theme.viewport.body.base = QColor(150, 158, 168);
    theme.viewport.body.edge = QColor(210, 214, 220);
    theme.viewport.body.specular = QColor(255, 255, 255);
    theme.viewport.body.rim = QColor(196, 206, 218);
    theme.viewport.body.glow = QColor(kAccent.red(), kAccent.green(), kAccent.blue());
    theme.viewport.body.highlight = QColor(196, 206, 218);
    theme.viewport.body.keyLightDir = QVector3D(-0.4f, 0.5f, 0.75f);
    theme.viewport.body.fillLightDir = QVector3D(0.6f, -0.2f, 0.55f);
    theme.viewport.body.fillLightIntensity = 0.35f;
    theme.viewport.body.ambientIntensity = 0.25f;
    theme.viewport.body.hemiUpDir = QVector3D(0.0f, 1.0f, 0.0f);
    theme.viewport.body.hemiSky = QColor(160, 168, 182);
    theme.viewport.body.hemiGround = QColor(60, 55, 52);
    theme.viewport.body.ambientGradientStrength = 0.1f;
    theme.viewport.body.ambientGradientDir = QVector3D(0.0f, 1.0f, 0.0f);

    theme.sketch.normalGeometry = QColor(245, 245, 245);
    theme.sketch.constructionGeometry = QColor(120, 138, 170);
    theme.sketch.selectedGeometry = kAccent;
    theme.sketch.previewGeometry = QColor(140, 146, 152);
    theme.sketch.errorGeometry = QColor(255, 90, 90);
    theme.sketch.constraintIcon = QColor(230, 179, 51);
    theme.sketch.dimensionText = kAccent;
    theme.sketch.conflictHighlight = QColor(255, 64, 64);
    theme.sketch.fullyConstrained = kAccent;
    theme.sketch.underConstrained = QColor(255, 145, 0);
    theme.sketch.overConstrained = QColor(255, 60, 60);
    theme.sketch.gridMajor = QColor(90, 96, 110);
    theme.sketch.gridMinor = QColor(55, 60, 72);
    theme.sketch.regionFill = QColor(kAccent.red(), kAccent.green(), kAccent.blue());

    return theme;
}

ThemeCatalog buildCatalog() {
    ThemeCatalog catalog;
    catalog.systemLightId = QStringLiteral("Light");
    catalog.systemDarkId = QStringLiteral("Dark");
    catalog.themes = {makeLightTheme(), makeDarkTheme()};
    return catalog;
}

} // namespace

const ThemeCatalog& themeCatalog() {
    static const ThemeCatalog catalog = buildCatalog();
    return catalog;
}

const ThemeDefinition* findTheme(const QString& id) {
    if (id.isEmpty()) {
        return nullptr;
    }
    const auto& catalog = themeCatalog();
    for (const auto& theme : catalog.themes) {
        if (theme.id == id) {
            return &theme;
        }
    }
    return nullptr;
}

const ThemeDefinition& themeById(const QString& id) {
    if (const ThemeDefinition* theme = findTheme(id)) {
        return *theme;
    }
    const auto& catalog = themeCatalog();
    if (!catalog.themes.empty()) {
        return catalog.themes.front();
    }
    static ThemeDefinition fallback;
    return fallback;
}

const ThemeDefinition& systemTheme(bool dark) {
    const auto& catalog = themeCatalog();
    const QString id = dark ? catalog.systemDarkId : catalog.systemLightId;
    return themeById(id);
}

QStringList themeIds() {
    QStringList ids;
    const auto& catalog = themeCatalog();
    ids.reserve(static_cast<int>(catalog.themes.size()));
    for (const auto& theme : catalog.themes) {
        ids.append(theme.id);
    }
    return ids;
}

QStringList themeDisplayNames() {
    QStringList names;
    const auto& catalog = themeCatalog();
    names.reserve(static_cast<int>(catalog.themes.size()));
    for (const auto& theme : catalog.themes) {
        names.append(theme.displayName);
    }
    return names;
}

} // namespace onecad::ui
