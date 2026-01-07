#ifndef ONECAD_UI_THEME_THEMECONFIG_H
#define ONECAD_UI_THEME_THEMECONFIG_H

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector3D>
#include <vector>

namespace onecad::ui {

struct ThemeUiColors {
    QColor windowBackground;
    QColor widgetBackground;
    QColor widgetText;
    QColor navigatorBackground;
    QColor panelBackground;
    QColor inspectorBackground;
    QColor panelBorder;
    QColor menuBarBackground;
    QColor menuBarText;
    QColor menuBarBorder;
    QColor menuItemSelectedBackground;
    QColor menuItemSelectedText;
    QColor menuBackground;
    QColor menuText;
    QColor menuBorder;
    QColor menuSeparator;
    QColor statusBarBackground;
    QColor statusBarText;
    QColor statusBarBorder;
    QColor dockTitleBackground;
    QColor dockTitleBorder;
    QColor treeBackground;
    QColor treeText;
    QColor treeHoverBackground;
    QColor treeSelectedBackground;
    QColor treeSelectedText;
    QColor sidebarButtonBackground;
    QColor sidebarButtonText;
    QColor sidebarButtonBorder;
    QColor sidebarButtonHoverBackground;
    QColor sidebarButtonHoverBorder;
    QColor sidebarButtonPressedBackground;
    QColor sidebarButtonPressedBorder;
    QColor toolButtonBackground;
    QColor toolButtonText;
    QColor toolButtonBorder;
    QColor toolButtonHoverBackground;
    QColor toolButtonHoverBorder;
    QColor toolButtonPressedBackground;
    QColor toolButtonPressedBorder;
    QColor inspectorHintText;
    QColor inspectorTipText;
    QColor inspectorEntityIdText;
    QColor inspectorSeparator;
    QColor inspectorPlaceholderText;
    QColor scrollbarTrack;
    QColor scrollbarHandle;
    QColor scrollbarHandleHover;
};

struct ThemeDimensionEditorColors {
    QColor background;
    QColor border;
    QColor borderFocus;
};

struct ThemeConstraintColors {
    QColor unsatisfiedText;
};

struct ThemeStatusColors {
    QColor dofError;
    QColor dofOk;
    QColor dofWarning;
};

struct ThemeNavigatorColors {
    QColor placeholderText;
    QColor headerText;
    QColor headerBackground;
    QColor divider;
    QColor itemText;
    QColor itemIcon;
    QColor itemHoverBackground;
    QColor itemSelectedBackground;
    QColor itemSelectedText;
    QColor inlineButtonBackground;
    QColor inlineButtonHoverBackground;
    QColor inlineButtonBorder;
    QColor inlineButtonHoverBorder;
};

struct ThemeViewCubeColors {
    QColor face;
    QColor faceHover;
    QColor text;
    QColor textHover;
    QColor edgeHover;
    QColor cornerHover;
    QColor faceBorder;
    QColor axisX;
    QColor axisY;
    QColor axisZ;
};

struct ThemeViewportGridColors {
    QColor major;
    QColor minor;
    QColor axisX;
    QColor axisY;
    QColor axisZ;
};

struct ThemeViewportPlaneColors {
    QColor xy;
    QColor xz;
    QColor yz;
    QColor labelText;
};

struct ThemeViewportOverlayColors {
    QColor previewDimensionText;
    QColor previewDimensionBackground;
    QColor toolIndicator;
    QColor toolLabelText;
    QColor toolLabelBackground;
};

struct ThemeViewportSelectionColors {
    QColor faceFillHover;
    QColor faceOutlineHover;
    QColor faceFillSelected;
    QColor faceOutlineSelected;
    QColor edgeHover;
    QColor edgeSelected;
    QColor vertexHover;
    QColor vertexSelected;
};

struct ThemeViewportBodyColors {
    QColor base;
    QColor edge;
    QColor specular;
    QColor rim;
    QColor glow;
    QColor highlight;
    QVector3D keyLightDir{ -0.4f, 0.5f, 0.75f };      // View-space light directions
    QVector3D fillLightDir{ 0.6f, -0.2f, 0.55f };
    float fillLightIntensity = 0.35f;
    float ambientIntensity = 0.25f;
    QVector3D hemiUpDir{ 0.0f, 1.0f, 0.0f };
    QColor hemiSky = QColor(230, 235, 242);
    QColor hemiGround = QColor(77, 71, 64);
    float ambientGradientStrength = 0.08f;
    QVector3D ambientGradientDir{ 0.0f, 1.0f, 0.0f };
};

struct ThemeViewportColors {
    QColor background;
    ThemeViewportGridColors grid;
    ThemeViewportPlaneColors planes;
    ThemeViewportOverlayColors overlay;
    ThemeViewportSelectionColors selection;
    ThemeViewportBodyColors body;
};

struct ThemeSketchColors {
    QColor normalGeometry;
    QColor constructionGeometry;
    QColor selectedGeometry;
    QColor previewGeometry;
    QColor errorGeometry;
    QColor constraintIcon;
    QColor dimensionText;
    QColor conflictHighlight;
    QColor fullyConstrained;
    QColor underConstrained;
    QColor overConstrained;
    QColor gridMajor;
    QColor gridMinor;
    QColor regionFill;
};

struct ThemeDefinition {
    QString id;
    QString displayName;
    bool isDark = false;
    ThemeUiColors ui;
    ThemeDimensionEditorColors dimensionEditor;
    ThemeConstraintColors constraints;
    ThemeStatusColors status;
    ThemeNavigatorColors navigator;
    ThemeViewCubeColors viewCube;
    ThemeViewportColors viewport;
    ThemeSketchColors sketch;
};

struct ThemeCatalog {
    QString systemLightId;
    QString systemDarkId;
    std::vector<ThemeDefinition> themes;
};

const ThemeCatalog& themeCatalog();
const ThemeDefinition* findTheme(const QString& id);
const ThemeDefinition& themeById(const QString& id);
const ThemeDefinition& systemTheme(bool dark);
QStringList themeIds();
QStringList themeDisplayNames();
QString toQssColor(const QColor& color);

} // namespace onecad::ui

#endif // ONECAD_UI_THEME_THEMECONFIG_H
