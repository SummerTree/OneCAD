/**
 * @file StartOverlay.cpp
 * @brief Implementation of startup overlay.
 */

#include "StartOverlay.h"

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
    recentGrid_ = new QGridLayout(recentContainer_);
    recentGrid_->setContentsMargins(0, 0, 0, 0);
    recentGrid_->setSpacing(12);

    auto* scroll = new QScrollArea(panel_);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(recentContainer_);
    panelLayout->addWidget(scroll);

    rootLayout->addWidget(panel_, 0, Qt::AlignHCenter);
    rootLayout->addStretch();

    auto* panelOpacity = new QGraphicsOpacityEffect(panel_);
    panel_->setGraphicsEffect(panelOpacity);
    panelOpacity->setOpacity(0.0);

    connect(newButton, &QPushButton::clicked, this, &StartOverlay::handleNewProject);
    connect(openButton, &QPushButton::clicked, this, &StartOverlay::handleOpenProject);

    setStyleSheet(
        "#StartOverlay {"
        "  background: qradialgradient(cx:0.2, cy:0.1, radius:1,"
        "    stop:0 #f8f6f0, stop:0.55 #efeae1, stop:1 #e2dbcf);"
        "  font-family: 'Avenir Next', 'Avenir', 'Helvetica Neue', sans-serif;"
        "}"
        "QWidget#panel {"
        "  background: #ffffff;"
        "  border: 1px solid #e0dbd1;"
        "  border-radius: 18px;"
        "}"
        "QLabel#title { font-size: 22px; font-weight: 600; color: #1f1c18; }"
        "QLabel#subtitle { font-size: 13px; color: #6b6256; }"
        "QLabel#sectionTitle { font-size: 13px; font-weight: 600; color: #3e3830; }"
        "QPushButton#primaryTile {"
        "  background: #1b1a17; color: #f5f3ee; border-radius: 14px;"
        "  font-size: 16px; font-weight: 600; }"
        "QPushButton#primaryTile:hover { background: #272522; }"
        "QPushButton#secondaryTile {"
        "  background: #ffffff; color: #1b1a17; border-radius: 14px;"
        "  border: 1px solid #d9d3c7; font-size: 16px; font-weight: 600; }"
        "QPushButton#secondaryTile:hover { background: #f0ece4; }"
        "QPushButton#recentTile {"
        "  background: #ffffff; color: #201d18; border-radius: 10px;"
        "  border: 1px solid #ded8cc; text-align: left; padding: 12px;"
        "  font-size: 13px; }"
        "QPushButton#recentTile:hover { background: #f2ede5; }"
        "QLabel#emptyState { color: #6b6256; font-size: 13px; }"
    );
}

void StartOverlay::setProjects(const QStringList& projects) {
    projects_ = projects;
    rebuildRecentGrid();
}

void StartOverlay::rebuildRecentGrid() {
    while (QLayoutItem* item = recentGrid_->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    if (projects_.isEmpty()) {
        recentEmptyLabel_ = new QLabel(tr("No projects yet."));
        recentEmptyLabel_->setObjectName("emptyState");
        recentGrid_->addWidget(recentEmptyLabel_, 0, 0);
        return;
    }

    const int columns = 2;
    int row = 0;
    int col = 0;

    for (const QString& path : projects_) {
        QFileInfo info(path);
        QString title = projectDisplayName(path);
        QString subtitle = QDir::toNativeSeparators(info.absoluteFilePath());

        auto* tile = new QPushButton(QString("%1\n%2").arg(title, subtitle));
        tile->setObjectName("recentTile");
        tile->setCursor(Qt::PointingHandCursor);
        tile->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        tile->setMinimumHeight(74);
        tile->setToolTip(subtitle);

        connect(tile, &QPushButton::clicked, this, [this, path]() {
            handleRecentClicked(path);
        });

        recentGrid_->addWidget(tile, row, col);
        col++;
        if (col >= columns) {
            col = 0;
            row++;
        }
    }
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
