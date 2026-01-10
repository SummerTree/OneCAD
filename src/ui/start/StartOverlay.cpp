/**
 * @file StartOverlay.cpp
 * @brief Implementation of startup overlay.
 */

#include "StartOverlay.h"
#include "ProjectTile.h"
#include "../theme/ThemeManager.h"
#include "../../io/OneCADFileIO.h"

#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QEasingCurve>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace onecad::ui {

namespace {

QString projectDisplayName(const QString& path) {
    QFileInfo info(path);
    QString name = info.baseName();
    if (name.isEmpty()) {
        name = info.fileName();
    }
    return name;
}

} // namespace

StartOverlay::StartOverlay(QWidget* parent)
    : QWidget(parent) {
    setObjectName("StartOverlay");
    setAutoFillBackground(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_StyledBackground, true);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(48, 48, 48, 48);
    rootLayout->setSpacing(0);
    rootLayout->addStretch();

    panel_ = new QWidget(this);
    panel_->setObjectName("panel");
    panel_->setFixedWidth(720);

    auto* panelLayout = new QVBoxLayout(panel_);
    panelLayout->setContentsMargins(32, 28, 32, 28);
    panelLayout->setSpacing(16);

    auto* title = new QLabel(tr("Start"));
    title->setObjectName("title");
    panelLayout->addWidget(title);

    auto* subtitle = new QLabel(tr("Pick up where you left off or start fresh."));
    subtitle->setObjectName("subtitle");
    panelLayout->addWidget(subtitle);

    auto* actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(12);

    auto* newButton = new QPushButton(tr("New Project"));
    newButton->setObjectName("primaryTile");
    newButton->setCursor(Qt::PointingHandCursor);
    newButton->setMinimumHeight(70);

    auto* openButton = new QPushButton(tr("Open Existing"));
    openButton->setObjectName("secondaryTile");
    openButton->setCursor(Qt::PointingHandCursor);
    openButton->setMinimumHeight(70);

    actionLayout->addWidget(newButton);
    actionLayout->addWidget(openButton);
    panelLayout->addLayout(actionLayout);

    auto* recentLabel = new QLabel(tr("Projects"));
    recentLabel->setObjectName("sectionTitle");
    panelLayout->addWidget(recentLabel);

    recentContainer_ = new QWidget(panel_);
    recentLayout_ = new QGridLayout(recentContainer_);
    recentLayout_->setContentsMargins(0, 0, 0, 0);
    recentLayout_->setSpacing(12);

    auto* scroll = new QScrollArea(panel_);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setMinimumHeight(260);
    scroll->setWidget(recentContainer_);
    panelLayout->addWidget(scroll);

    rootLayout->addWidget(panel_, 0, Qt::AlignHCenter);
    rootLayout->addStretch();

    auto* panelOpacity = new QGraphicsOpacityEffect(panel_);
    panel_->setGraphicsEffect(panelOpacity);
    panelOpacity->setOpacity(0.0);

    connect(newButton, &QPushButton::clicked, this, &StartOverlay::handleNewProject);
    connect(openButton, &QPushButton::clicked, this, &StartOverlay::handleOpenProject);

    themeConnection_ = connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
                               this, &StartOverlay::applyTheme, Qt::UniqueConnection);
    applyTheme();
}

void StartOverlay::applyTheme() {
    const auto& theme = ThemeManager::instance().currentTheme();
    const auto& ui = theme.ui;

    QColor base = ui.windowBackground;
    QColor glow = base.lighter(theme.isDark ? 120 : 108);
    QColor edge = base.darker(theme.isDark ? 130 : 105);

    QColor panelBg = ui.inspectorBackground.isValid() ? ui.inspectorBackground : ui.panelBackground;
    QColor panelBorder = ui.panelBorder;

    QColor titleColor = ui.widgetText;
    QColor subtitleColor = ui.inspectorHintText.isValid() ? ui.inspectorHintText : ui.widgetText;

    QColor primaryBg = ui.toolButtonPressedBackground;
    QColor primaryHover = ui.menuItemSelectedBackground.isValid()
        ? ui.menuItemSelectedBackground
        : primaryBg.lighter(110);
    QColor primaryPressed = ui.toolButtonPressedBorder.isValid()
        ? ui.toolButtonPressedBorder
        : primaryBg.darker(110);
    QColor primaryText = ui.menuItemSelectedText.isValid() ? ui.menuItemSelectedText : ui.widgetText;

    QColor secondaryBg = ui.toolButtonBackground;
    QColor secondaryBorder = ui.toolButtonBorder;
    QColor secondaryHover = ui.toolButtonHoverBackground;
    QColor secondaryText = ui.toolButtonText;

    QColor recentBg = Qt::transparent;
    QColor recentBorder = Qt::transparent;
    QColor recentHover = ui.treeHoverBackground.isValid() ? ui.treeHoverBackground : secondaryHover;
    QColor recentText = ui.widgetText;

    QString styleSheet = QStringLiteral(
        "#StartOverlay {"
        "  background: qradialgradient(cx:0.2, cy:0.1, radius:1,"
        "    stop:0 %1, stop:0.55 %2, stop:1 %3);"
        "  font-family: 'Avenir Next', 'Avenir', 'Helvetica Neue', sans-serif;"
        "}"
        "QWidget#panel {"
        "  background: %4;"
        "  border: 1px solid %5;"
        "  border-radius: 18px;"
        "}"
        "QLabel#title { background: transparent; font-size: 22px; font-weight: 600; color: %6; }"
        "QLabel#subtitle { background: transparent; font-size: 13px; color: %7; }"
        "QLabel#sectionTitle { background: transparent; font-size: 13px; font-weight: 600; color: %6; }"
        "QPushButton#primaryTile {"
        "  background: %8; color: %9; border-radius: 14px;"
        "  font-size: 16px; font-weight: 600; }"
        "QPushButton#primaryTile:hover { background: %10; }"
        "QPushButton#primaryTile:pressed { background: %11; }"
        "QPushButton#secondaryTile {"
        "  background: %12; color: %13; border-radius: 14px;"
        "  border: 1px solid %14; font-size: 16px; font-weight: 600; }"
        "QPushButton#secondaryTile:hover { background: %15; }"
        "QPushButton#recentTile {"
        "  background: %16; color: %17; border-radius: 10px;"
        "  border: 1px solid %18; text-align: left; padding: 12px;"
        "  font-size: 13px; }"
        "QPushButton#recentTile:hover { background: %19; }"
        "QLabel#emptyState { background: transparent; color: %7; font-size: 13px; }"
        "QScrollArea { background: transparent; }"
        "QScrollArea > QWidget { background: transparent; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
    )
        .arg(toQssColor(glow),
             toQssColor(base),
             toQssColor(edge),
             toQssColor(panelBg),
             toQssColor(panelBorder),
             toQssColor(titleColor),
             toQssColor(subtitleColor),
             toQssColor(primaryBg),
             toQssColor(primaryText),
             toQssColor(primaryHover),
             toQssColor(primaryPressed),
             toQssColor(secondaryBg),
             toQssColor(secondaryText),
             toQssColor(secondaryBorder),
             toQssColor(secondaryHover),
             toQssColor(recentBg),
             toQssColor(recentText),
             toQssColor(recentBorder),
             toQssColor(recentHover));

    setStyleSheet(styleSheet);
}

void StartOverlay::setProjects(const QStringList& projects) {
    projects_ = projects;
    rebuildRecentGrid();
}

void StartOverlay::rebuildRecentGrid() {
    // Clear existing widgets
    while (QLayoutItem* item = recentLayout_->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    if (projects_.isEmpty()) {
        recentEmptyLabel_ = new QLabel(tr("No projects yet."));
        recentEmptyLabel_->setObjectName("emptyState");
        recentLayout_->addWidget(recentEmptyLabel_, 0, 0);
        return;
    }

    constexpr int kColumns = 4;
    int row = 0;
    int col = 0;

    for (const QString& path : projects_) {
        // Load thumbnail (lightweight - only reads thumbnail.png from package)
        QImage thumbnail = io::OneCADFileIO::readThumbnail(path);

        auto* tile = new ProjectTile(path, thumbnail, recentContainer_);
        connect(tile, &ProjectTile::clicked,
                this, &StartOverlay::handleRecentClicked);
        connect(tile, &ProjectTile::deleteRequested,
                this, &StartOverlay::handleDeleteClicked);

        recentLayout_->addWidget(tile, row, col);

        col++;
        if (col >= kColumns) {
            col = 0;
            row++;
        }
    }

    // Add stretch at bottom
    recentLayout_->setRowStretch(row + 1, 1);
}

void StartOverlay::handleNewProject() {
    emit newProjectRequested();
}

void StartOverlay::handleOpenProject() {
    emit openProjectRequested();
}

void StartOverlay::handleRecentClicked(const QString& path) {
    emit recentProjectRequested(path);
}

void StartOverlay::handleDeleteClicked(const QString& path) {
    emit deleteProjectRequested(path);
}

void StartOverlay::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!panel_) {
        return;
    }
    auto* effect = qobject_cast<QGraphicsOpacityEffect*>(panel_->graphicsEffect());
    if (!effect) {
        return;
    }
    effect->setOpacity(0.0);
    auto* anim = new QPropertyAnimation(effect, "opacity", panel_);
    anim->setDuration(180);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

} // namespace onecad::ui
