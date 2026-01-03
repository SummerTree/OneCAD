#include "ModelNavigator.h"
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QStackedLayout>
#include <QToolButton>
#include <QFrame>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include "../theme/ThemeManager.h"

namespace onecad {
namespace ui {

ModelNavigator::ModelNavigator(QWidget* parent)
    : QWidget(parent) {
    setupUi();
    createPlaceholderItems();
    updateOverlayButtonIcon();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &ModelNavigator::updateOverlayButtonIcon, Qt::UniqueConnection);
    applyCollapseState();
}

void ModelNavigator::setupUi() {
    m_stack = new QStackedLayout(this);
    m_stack->setContentsMargins(0, 0, 0, 0);

    m_panel = new QFrame(this);
    m_panel->setObjectName("NavigatorPanel");
    m_panel->setFrameShape(QFrame::StyledPanel);
    m_panel->setFrameShadow(QFrame::Raised);

    QVBoxLayout* panelLayout = new QVBoxLayout(m_panel);
    panelLayout->setContentsMargins(10, 10, 10, 10);
    panelLayout->setSpacing(8);

    QWidget* header = new QWidget(m_panel);
    QHBoxLayout* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(6);

    QLabel* title = new QLabel(tr("Navigator"), header);
    title->setObjectName("NavigatorTitle");

    m_collapseButton = new QToolButton(header);
    m_collapseButton->setText(tr("Hide"));
    m_collapseButton->setAutoRaise(true);
    m_collapseButton->setToolTip(tr("Collapse navigator"));

    headerLayout->addWidget(title);
    headerLayout->addStretch();
    headerLayout->addWidget(m_collapseButton);

    m_treeWidget = new QTreeWidget(m_panel);
    m_treeWidget->setHeaderHidden(true);
    m_treeWidget->setIndentation(16);
    m_treeWidget->setAnimated(true);
    m_treeWidget->setExpandsOnDoubleClick(false);

    connect(m_treeWidget, &QTreeWidget::itemClicked,
            this, &ModelNavigator::onItemClicked);
    connect(m_treeWidget, &QTreeWidget::itemDoubleClicked,
            this, &ModelNavigator::onItemDoubleClicked);

    panelLayout->addWidget(header);
    panelLayout->addWidget(m_treeWidget, 1);

    m_expandButton = new QToolButton(this);
    m_expandButton->setObjectName("navigatorOverlayButton");
    m_expandButton->setText(QString());
    m_expandButton->setToolTip(tr("Expand navigator"));
    m_expandButton->setAutoRaise(false);
    m_expandButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_expandButton->setFixedSize(36, 36);
    m_expandButton->setIconSize(QSize(18, 18));

    connect(m_collapseButton, &QToolButton::clicked, this, [this]() {
        setCollapsed(true);
    });
    connect(m_expandButton, &QToolButton::clicked, this, [this]() {
        setCollapsed(false);
    });

    m_stack->addWidget(m_panel);
    m_stack->addWidget(m_expandButton);
    m_stack->setCurrentWidget(m_panel);
}

void ModelNavigator::createPlaceholderItems() {
    // Bodies section
    m_bodiesRoot = new QTreeWidgetItem(m_treeWidget);
    m_bodiesRoot->setText(0, tr("📦 Bodies"));
    m_bodiesRoot->setExpanded(true);
    
    // Sketches section
    m_sketchesRoot = new QTreeWidgetItem(m_treeWidget);
    m_sketchesRoot->setText(0, tr("✏️ Sketches"));
    m_sketchesRoot->setExpanded(true);
    
    // Placeholder items (will be populated dynamically later)
    auto* placeholder = new QTreeWidgetItem(m_bodiesRoot);
    placeholder->setText(0, tr("(No bodies)"));
    placeholder->setFlags(placeholder->flags() & ~Qt::ItemIsSelectable);
    placeholder->setForeground(0, QColor(128, 128, 128));
    
    auto* sketchPlaceholder = new QTreeWidgetItem(m_sketchesRoot);
    sketchPlaceholder->setText(0, tr("(No sketches)"));
    sketchPlaceholder->setFlags(sketchPlaceholder->flags() & ~Qt::ItemIsSelectable);
    sketchPlaceholder->setForeground(0, QColor(128, 128, 128));
}

void ModelNavigator::setCollapsed(bool collapsed) {
    if (m_collapsed == collapsed) {
        return;
    }

    m_collapsed = collapsed;
    applyCollapseState();
}

void ModelNavigator::applyCollapseState() {
    if (m_collapsed) {
        m_stack->setCurrentWidget(m_expandButton);
        setFixedSize(m_expandButton->size());
    } else {
        m_stack->setCurrentWidget(m_panel);
        setFixedSize(260, 320);
    }
}

void ModelNavigator::updateOverlayButtonIcon() {
    const bool isDark = ThemeManager::instance().isDark();
    const QColor strokeColor = isDark ? QColor(245, 245, 245) : QColor(34, 34, 34);

    QPixmap pixmap(18, 18);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(strokeColor, 2, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(pen);

    painter.drawLine(QPointF(3, 5), QPointF(15, 5));
    painter.drawLine(QPointF(3, 9), QPointF(15, 9));
    painter.drawLine(QPointF(3, 13), QPointF(15, 13));

    painter.end();

    m_expandButton->setIcon(QIcon(pixmap));
}

void ModelNavigator::onItemClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    if (item && item != m_bodiesRoot && item != m_sketchesRoot) {
        emit itemSelected(item->data(0, Qt::UserRole).toString());
    }
}

void ModelNavigator::onItemDoubleClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    if (item && item != m_bodiesRoot && item != m_sketchesRoot) {
        QString itemId = item->data(0, Qt::UserRole).toString();
        emit itemDoubleClicked(itemId);

        // Direct lookup instead of linear iteration
        std::string stdId = itemId.toStdString();
        auto it = m_sketchItems.find(stdId);
        if (it != m_sketchItems.end() && it->second == item) {
            emit editSketchRequested(itemId);
        }
    }
}

void ModelNavigator::onSketchAdded(const QString& id) {
    std::string stdId = id.toStdString();

    // Remove placeholder if this is the first sketch
    if (m_sketchItems.empty() && m_sketchesRoot->childCount() > 0) {
        QTreeWidgetItem* firstChild = m_sketchesRoot->child(0);
        if (firstChild && !(firstChild->flags() & Qt::ItemIsSelectable)) {
            delete firstChild;
        }
    }

    // Use monotonically increasing counter for unique naming
    ++m_sketchCounter;

    // Create new tree item for this sketch
    auto* item = new QTreeWidgetItem(m_sketchesRoot);
    item->setText(0, QString("Sketch %1").arg(m_sketchCounter));
    item->setData(0, Qt::UserRole, id);
    item->setFlags(item->flags() | Qt::ItemIsSelectable);

    m_sketchItems[stdId] = item;
    m_sketchesRoot->setExpanded(true);
}

void ModelNavigator::onSketchRemoved(const QString& id) {
    std::string stdId = id.toStdString();
    auto it = m_sketchItems.find(stdId);
    if (it != m_sketchItems.end()) {
        delete it->second;
        m_sketchItems.erase(it);
    }

    // Add placeholder back if no sketches left
    if (m_sketchItems.empty()) {
        auto* placeholder = new QTreeWidgetItem(m_sketchesRoot);
        placeholder->setText(0, tr("(No sketches)"));
        placeholder->setFlags(placeholder->flags() & ~Qt::ItemIsSelectable);
        placeholder->setForeground(0, QColor(128, 128, 128));
    }
}

void ModelNavigator::onSketchRenamed(const QString& id, const QString& newName) {
    std::string stdId = id.toStdString();
    auto it = m_sketchItems.find(stdId);
    if (it != m_sketchItems.end()) {
        it->second->setText(0, newName);
    }
}

} // namespace ui
} // namespace onecad
