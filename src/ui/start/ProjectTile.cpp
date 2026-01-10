/**
 * @file ProjectTile.cpp
 * @brief Implementation of project tile widget
 */

#include "ProjectTile.h"
#include "../theme/ThemeManager.h"

#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QMouseEvent>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QStyleOption>
#include <QVBoxLayout>

namespace onecad::ui {

namespace {
constexpr int kThumbnailSize = 120;
constexpr int kTileWidth = 160;
constexpr int kTileHeight = 200;
constexpr int kPlaceholderSize = 64;
} // namespace

ProjectTile::ProjectTile(const QString& path,
                         const QImage& thumbnail,
                         QWidget* parent)
    : QWidget(parent)
    , m_path(path)
{
    setObjectName("projectTile");
    setAttribute(Qt::WA_Hover, true);
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedSize(kTileWidth, kTileHeight);
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    // Thumbnail
    m_thumbnailLabel = new QLabel(this);
    m_thumbnailLabel->setFixedSize(kThumbnailSize, kThumbnailSize);
    m_thumbnailLabel->setAlignment(Qt::AlignCenter);
    m_thumbnailLabel->setScaledContents(false);

    if (!thumbnail.isNull()) {
        QPixmap pix = QPixmap::fromImage(thumbnail.scaled(
            kThumbnailSize, kThumbnailSize,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation));
        m_thumbnailLabel->setPixmap(pix);
    } else {
        // Use placeholder icon
        QPixmap placeholder(":/icons/ic_project_placeholder.svg");
        if (!placeholder.isNull()) {
            m_thumbnailLabel->setPixmap(placeholder.scaled(
                kPlaceholderSize, kPlaceholderSize,
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation));
        }
    }

    layout->addWidget(m_thumbnailLabel, 0, Qt::AlignCenter);

    // Project name
    QFileInfo info(path);
    QString name = info.baseName();
    if (name.isEmpty()) {
        name = info.fileName();
    }

    m_nameLabel = new QLabel(name, this);
    m_nameLabel->setAlignment(Qt::AlignCenter);
    m_nameLabel->setWordWrap(true);
    m_nameLabel->setObjectName("projectName");
    layout->addWidget(m_nameLabel);

    // Path (truncated)
    QString displayPath = QDir::toNativeSeparators(info.absolutePath());
    if (displayPath.length() > 25) {
        displayPath = "..." + displayPath.right(22);
    }
    m_pathLabel = new QLabel(displayPath, this);
    m_pathLabel->setAlignment(Qt::AlignCenter);
    m_pathLabel->setObjectName("projectPath");
    m_pathLabel->setToolTip(info.absoluteFilePath());
    layout->addWidget(m_pathLabel);

    // Date
    QString dateStr = info.lastModified().toString("MMM d, yyyy");
    m_dateLabel = new QLabel(dateStr, this);
    m_dateLabel->setAlignment(Qt::AlignCenter);
    m_dateLabel->setObjectName("projectDate");
    layout->addWidget(m_dateLabel);

    m_themeConnection = connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
                                this, &ProjectTile::applyTheme, Qt::UniqueConnection);
    applyTheme();
}

ProjectTile::~ProjectTile() {
    disconnect(m_themeConnection);
}

void ProjectTile::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit clicked(m_path);
    }
    QWidget::mousePressEvent(event);
}

void ProjectTile::enterEvent(QEnterEvent* event) {
    QWidget::enterEvent(event);
    update();
}

void ProjectTile::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
    update();
}

void ProjectTile::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QStyleOption option;
    option.initFrom(this);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    style()->drawPrimitive(QStyle::PE_Widget, &option, &painter, this);

    // No thumbnail shadow - border is handled by stylesheet.
}

void ProjectTile::contextMenuEvent(QContextMenuEvent* event) {
    if (!event) {
        return;
    }

    QMenu menu(this);
    QAction* deleteAction = menu.addAction(tr("Delete"));
    QAction* chosen = menu.exec(event->globalPos());
    if (chosen == deleteAction) {
        emit deleteRequested(m_path);
    }
}

void ProjectTile::applyTheme() {
    const auto& theme = ThemeManager::instance().currentTheme();

    QColor textColor = theme.ui.widgetText;
    QColor hintColor = theme.ui.inspectorHintText.isValid()
        ? theme.ui.inspectorHintText
        : textColor;
    QColor hoverColor = theme.ui.treeHoverBackground.isValid()
        ? theme.ui.treeHoverBackground
        : QColor(0, 0, 0, 24);
    QColor baseColor(0, 0, 0, 0);
    QColor borderColor = theme.ui.panelBorder.isValid()
        ? theme.ui.panelBorder
        : theme.ui.toolButtonBorder;
    QColor hoverBorder = theme.ui.toolButtonHoverBorder.isValid()
        ? theme.ui.toolButtonHoverBorder
        : borderColor;
    QString style = QString(
        "QWidget#projectTile { background: %1; border-radius: 10px; border: 1px solid %2; }"
        "QWidget#projectTile:hover { background: %3; border: 1px solid %4; }"
        "QLabel#projectName { background: transparent; color: %5; font-weight: 600; font-size: 12px; }"
        "QLabel#projectPath { background: transparent; color: %6; font-size: 10px; }"
        "QLabel#projectDate { background: transparent; color: %6; font-size: 10px; }"
    ).arg(toQssColor(baseColor),
          toQssColor(borderColor),
          toQssColor(hoverColor),
          toQssColor(hoverBorder),
          textColor.name(),
          hintColor.name());

    setStyleSheet(style);
}

} // namespace onecad::ui
