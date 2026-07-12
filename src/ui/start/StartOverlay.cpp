/**
 * @file StartOverlay.cpp
 * @brief Implementation of startup overlay.
 */

#include "StartOverlay.h"
#include "ProjectTile.h"
#include "../theme/ThemeManager.h"
#include "../../io/OneCADFileIO.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QEasingCurve>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QSizePolicy>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

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

    // Brand + version header band.
    auto* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(8);
    auto* brandLogo = new QLabel(tr("OneCAD"));
    brandLogo->setObjectName("brandLogo");
    auto* versionLabel = new QLabel(tr("v%1").arg(QCoreApplication::applicationVersion()));
    versionLabel->setObjectName("versionLabel");
    headerLayout->addWidget(brandLogo);
    headerLayout->addStretch();
    headerLayout->addWidget(versionLabel);
    panelLayout->addLayout(headerLayout);

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

    // Search + sort controls (view-state over the loaded project list).
    auto* controlsLayout = new QHBoxLayout();
    controlsLayout->setSpacing(8);
    searchEdit_ = new QLineEdit(panel_);
    searchEdit_->setObjectName("searchEdit");
    searchEdit_->setPlaceholderText(tr("Search projects…"));
    searchEdit_->setClearButtonEnabled(true);
    searchEdit_->addAction(QIcon(":/icons/ic_search.svg"), QLineEdit::LeadingPosition);
    sortCombo_ = new QComboBox(panel_);
    sortCombo_->setObjectName("sortCombo");
    sortCombo_->addItem(tr("Date modified"), static_cast<int>(SortMode::DateModified));
    sortCombo_->addItem(tr("Name"), static_cast<int>(SortMode::Name));
    controlsLayout->addWidget(searchEdit_, 1);
    controlsLayout->addWidget(sortCombo_);
    panelLayout->addLayout(controlsLayout);

    recentContainer_ = new QWidget(panel_);
    recentLayout_ = new QGridLayout(recentContainer_);
    recentLayout_->setContentsMargins(0, 0, 0, 0);
    recentLayout_->setSpacing(12);

    scroll_ = new QScrollArea(panel_);
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setMinimumHeight(260);
    scroll_->setWidget(recentContainer_);
    panelLayout->addWidget(scroll_);

    rootLayout->addWidget(panel_, 0, Qt::AlignHCenter);
    rootLayout->addStretch();

    auto* panelOpacity = new QGraphicsOpacityEffect(panel_);
    panel_->setGraphicsEffect(panelOpacity);
    panelOpacity->setOpacity(0.0);

    // Debounce responsive grid rebuilds on resize to avoid layout thrashing.
    resizeDebounce_ = new QTimer(this);
    resizeDebounce_->setSingleShot(true);
    resizeDebounce_->setInterval(100);
    connect(resizeDebounce_, &QTimer::timeout, this, [this]() {
        if (!scroll_) return;
        if (columnCountForWidth(scroll_->viewport()->width()) != lastColumnCount_) {
            rebuildRecentGrid();
        }
    });

    connect(newButton, &QPushButton::clicked, this, &StartOverlay::handleNewProject);
    connect(openButton, &QPushButton::clicked, this, &StartOverlay::handleOpenProject);
    connect(searchEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
        filterText_ = text;
        applyFilterAndSort();
    });
    connect(sortCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        sortMode_ = static_cast<SortMode>(sortCombo_->currentData().toInt());
        applyFilterAndSort();
    });

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

    // Styles for the header, search/sort, and import-tile widgets added on top of
    // the original ramp (kept as a separate block to avoid renumbering placeholders).
    const QString extra = QStringLiteral(
        "QLabel#brandLogo { background: transparent; font-size: 15px; font-weight: 600; color: %1; }"
        "QLabel#versionLabel { background: transparent; font-size: 12px; color: %2; }"
        "QLineEdit#searchEdit { background: %3; color: %1; border: 1px solid %4;"
        "  border-radius: 8px; padding: 5px 8px; }"
        "QComboBox#sortCombo { background: %3; color: %1; border: 1px solid %4;"
        "  border-radius: 8px; padding: 4px 8px; }"
        "QToolButton#importTile { background: %5; color: %1; border: 1px dashed %4;"
        "  border-radius: 10px; font-size: 12px; }"
        "QToolButton#importTile:hover { background: %6; border: 1px solid %7; }")
        .arg(toQssColor(titleColor),
             toQssColor(subtitleColor),
             toQssColor(ui.inspectorBackground.isValid() ? ui.inspectorBackground : ui.widgetBackground),
             toQssColor(ui.panelBorder),
             toQssColor(ui.toolButtonBackground),
             toQssColor(ui.toolButtonHoverBackground),
             toQssColor(theme.button.accent));

    setStyleSheet(styleSheet + extra);
}

void StartOverlay::setProjects(const QStringList& projects) {
    projects_ = projects;
    thumbnailCache_.clear();
    applyFilterAndSort();
}

void StartOverlay::applyFilterAndSort() {
    rebuildRecentGrid();
    if (scroll_ && scroll_->verticalScrollBar()) {
        scroll_->verticalScrollBar()->setValue(0);
    }
}

int StartOverlay::columnCountForWidth(int width) const {
    constexpr int kTileWidth = 160; // mirrors ProjectTile
    const int spacing = recentLayout_ ? recentLayout_->spacing() : 12;
    if (width <= 0) {
        return 3;
    }
    const int cols = width / (kTileWidth + spacing);
    return std::clamp(cols, 2, 6);
}

QStringList StartOverlay::computeVisibleProjects() const {
    QStringList visible;
    visible.reserve(projects_.size());
    for (const QString& path : projects_) {
        if (filterText_.isEmpty() ||
            projectDisplayName(path).contains(filterText_, Qt::CaseInsensitive)) {
            visible.append(path);
        }
    }

    if (sortMode_ == SortMode::Name) {
        std::sort(visible.begin(), visible.end(), [](const QString& a, const QString& b) {
            return projectDisplayName(a).localeAwareCompare(projectDisplayName(b)) < 0;
        });
    } else {
        // Date modified, newest first (matches listProjectsInDefaultDirectory order).
        std::sort(visible.begin(), visible.end(), [](const QString& a, const QString& b) {
            return QFileInfo(a).lastModified() > QFileInfo(b).lastModified();
        });
    }
    return visible;
}

void StartOverlay::rebuildRecentGrid() {
    // Clear existing widgets
    while (QLayoutItem* item = recentLayout_->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    recentEmptyLabel_ = nullptr;

    const int cols = columnCountForWidth(scroll_ ? scroll_->viewport()->width() : 0);
    lastColumnCount_ = cols;

    // The Import STEP quick action always occupies the first cell.
    auto* importTile = new QToolButton(recentContainer_);
    importTile->setObjectName("importTile");
    importTile->setText(tr("Import STEP…"));
    importTile->setIcon(QIcon(":/icons/ic_import.svg"));
    importTile->setIconSize(QSize(40, 40));
    importTile->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    importTile->setFixedSize(160, 200);
    importTile->setCursor(Qt::PointingHandCursor);
    connect(importTile, &QToolButton::clicked, this,
            [this]() { emit importStepProjectRequested(); });
    recentLayout_->addWidget(importTile, 0, 0);

    const QStringList visible = computeVisibleProjects();

    if (visible.isEmpty()) {
        recentEmptyLabel_ = new QLabel(
            filterText_.isEmpty() ? tr("No projects yet.") : tr("No projects match."));
        recentEmptyLabel_->setObjectName("emptyState");
        recentLayout_->addWidget(recentEmptyLabel_, 1, 0, 1, cols);
        recentLayout_->setRowStretch(2, 1);
        return;
    }

    int index = 1; // cell 0 is the import tile
    for (const QString& path : visible) {
        QImage thumbnail;
        if (thumbnailCache_.contains(path)) {
            thumbnail = thumbnailCache_.value(path);
        } else {
            thumbnail = io::OneCADFileIO::readThumbnail(path);
            thumbnailCache_.insert(path, thumbnail);
        }

        auto* tile = new ProjectTile(path, thumbnail, recentContainer_);
        connect(tile, &ProjectTile::clicked,
                this, &StartOverlay::handleRecentClicked);
        connect(tile, &ProjectTile::deleteRequested,
                this, &StartOverlay::handleDeleteClicked);

        recentLayout_->addWidget(tile, index / cols, index % cols);
        ++index;
    }

    recentLayout_->setRowStretch((index - 1) / cols + 1, 1);
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

void StartOverlay::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    // Coalesce rapid resizes; the timer rebuilds only if the column count changes.
    if (resizeDebounce_) {
        resizeDebounce_->start();
    }
}

} // namespace onecad::ui
