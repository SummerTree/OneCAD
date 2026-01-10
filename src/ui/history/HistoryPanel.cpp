/**
 * @file HistoryPanel.cpp
 * @brief Implementation of feature history tree panel.
 */
#include "HistoryPanel.h"
#include "EditParameterDialog.h"
#include "../../app/document/Document.h"
#include "../../app/document/OperationRecord.h"
#include "../../app/history/DependencyGraph.h"
#include "../viewport/Viewport.h"

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QToolButton>
#include <QMenu>
#include <QPropertyAnimation>
#include <QHeaderView>
#include <QFont>
#include <QSizePolicy>
#include <QEasingCurve>

namespace onecad::ui {

HistoryPanel::HistoryPanel(QWidget* parent)
    : QWidget(parent) {
    setupUi();
    applyCollapseState(false);
}

void HistoryPanel::setupUi() {
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    panel_ = new QFrame(this);
    panel_->setObjectName("historyPanel");
    panel_->setStyleSheet(R"(
        QFrame#historyPanel {
            background-color: #2d2d30;
            border-left: 1px solid #3e3e42;
        }
    )");

    auto* panelLayout = new QVBoxLayout(panel_);
    panelLayout->setContentsMargins(8, 8, 8, 8);
    panelLayout->setSpacing(4);

    // Header
    auto* headerWidget = new QWidget;
    auto* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 4);

    auto* titleLabel = new QLabel("History");
    titleLabel->setStyleSheet("font-weight: bold; color: #cccccc;");
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    panelLayout->addWidget(headerWidget);

    // Tree widget
    treeWidget_ = new QTreeWidget;
    treeWidget_->setHeaderHidden(true);
    treeWidget_->setIndentation(16);
    treeWidget_->setRootIsDecorated(true);
    treeWidget_->setContextMenuPolicy(Qt::CustomContextMenu);
    treeWidget_->setStyleSheet(R"(
        QTreeWidget {
            background-color: #1e1e1e;
            border: 1px solid #3e3e42;
            color: #cccccc;
        }
        QTreeWidget::item {
            height: 28px;
            padding: 2px 4px;
        }
        QTreeWidget::item:selected {
            background-color: #094771;
        }
        QTreeWidget::item:hover:!selected {
            background-color: #2a2d2e;
        }
    )");

    connect(treeWidget_, &QTreeWidget::itemClicked,
            this, &HistoryPanel::onItemClicked);
    connect(treeWidget_, &QTreeWidget::itemDoubleClicked,
            this, &HistoryPanel::onItemDoubleClicked);
    connect(treeWidget_, &QTreeWidget::customContextMenuRequested,
            this, &HistoryPanel::onCustomContextMenu);

    panelLayout->addWidget(treeWidget_);

    mainLayout->addWidget(panel_);

    setMinimumWidth(expandedWidth_);
    setMaximumWidth(expandedWidth_);
}

void HistoryPanel::setDocument(app::Document* doc) {
    document_ = doc;
    rebuild();
}

void HistoryPanel::setViewport(Viewport* viewport) {
    viewport_ = viewport;
}

void HistoryPanel::setCommandProcessor(app::commands::CommandProcessor* processor) {
    commandProcessor_ = processor;
}

void HistoryPanel::rebuild() {
    treeWidget_->clear();
    entries_.clear();

    if (!document_) {
        return;
    }

    const auto& ops = document_->operations();
    if (ops.empty()) {
        auto* placeholder = new QTreeWidgetItem(treeWidget_);
        placeholder->setText(0, "No operations");
        placeholder->setForeground(0, QColor("#666666"));
        placeholder->setFlags(Qt::NoItemFlags);
        return;
    }

    // Build dependency graph for ordering
    app::history::DependencyGraph graph;
    graph.rebuildFromOperations(ops);

    auto sorted = graph.topologicalSort();
    if (sorted.empty() && !ops.empty()) {
        sorted.reserve(ops.size());
        for (const auto& op : ops) {
            sorted.push_back(op.opId);
        }
    }
    std::unordered_map<std::string, const app::OperationRecord*> opById;
    opById.reserve(ops.size());
    for (const auto& op : ops) {
        opById[op.opId] = &op;
    }

    std::unordered_map<std::string, QTreeWidgetItem*> sketchItems;
    std::unordered_map<std::string, QTreeWidgetItem*> opItems;
    std::unordered_map<std::string, std::string> bodyProducers;

    // Create items in topological order
    for (const auto& opId : sorted) {
        const app::OperationRecord* opRecord = nullptr;
        auto opIt = opById.find(opId);
        if (opIt != opById.end()) {
            opRecord = opIt->second;
        }
        if (!opRecord) {
            continue;
        }

        QTreeWidgetItem* parentItem = nullptr;
        if (std::holds_alternative<app::SketchRegionRef>(opRecord->input)) {
            const auto& ref = std::get<app::SketchRegionRef>(opRecord->input);
            auto sketchIt = sketchItems.find(ref.sketchId);
            if (sketchIt == sketchItems.end()) {
                auto* sketchItem = new QTreeWidgetItem(treeWidget_);
                QString sketchName = QString::fromStdString(document_->getSketchName(ref.sketchId));
                sketchItem->setText(0, sketchName);
                sketchItem->setFlags(Qt::ItemIsEnabled);
                QFont font = sketchItem->font(0);
                font.setBold(true);
                sketchItem->setFont(0, font);
                sketchItems[ref.sketchId] = sketchItem;
                parentItem = sketchItem;
            } else {
                parentItem = sketchIt->second;
            }
        } else if (std::holds_alternative<app::FaceRef>(opRecord->input)) {
            const auto& ref = std::get<app::FaceRef>(opRecord->input);
            auto producerIt = bodyProducers.find(ref.bodyId);
            if (producerIt != bodyProducers.end()) {
                auto opItemIt = opItems.find(producerIt->second);
                if (opItemIt != opItems.end()) {
                    parentItem = opItemIt->second;
                }
            }
        } else if (std::holds_alternative<app::BodyRef>(opRecord->input)) {
            const auto& ref = std::get<app::BodyRef>(opRecord->input);
            auto producerIt = bodyProducers.find(ref.bodyId);
            if (producerIt != bodyProducers.end()) {
                auto opItemIt = opItems.find(producerIt->second);
                if (opItemIt != opItems.end()) {
                    parentItem = opItemIt->second;
                }
            }
        }

        if (opRecord->type == app::OperationType::Boolean &&
            std::holds_alternative<app::BooleanParams>(opRecord->params)) {
            const auto& params = std::get<app::BooleanParams>(opRecord->params);
            auto producerIt = bodyProducers.find(params.targetBodyId);
            if (producerIt != bodyProducers.end()) {
                auto opItemIt = opItems.find(producerIt->second);
                if (opItemIt != opItems.end()) {
                    parentItem = opItemIt->second;
                }
            }
        }

        ItemEntry entry;
        entry.opId = opId;
        entry.type = opRecord->type;
        entry.item = parentItem ? new QTreeWidgetItem(parentItem) : new QTreeWidgetItem(treeWidget_);
        entry.failed = document_->isOperationFailed(opId);
        entry.suppressed = document_->isOperationSuppressed(opId);
        if (entry.failed) {
            entry.failureReason = document_->operationFailureReason(opId);
        }

        QString displayName = operationDisplayName(*opRecord);
        entry.widget = createItemWidget(entry, displayName);
        treeWidget_->setItemWidget(entry.item, 0, entry.widget);

        opItems[opId] = entry.item;
        for (const auto& bodyId : opRecord->resultBodyIds) {
            bodyProducers[bodyId] = opId;
        }

        entries_.push_back(std::move(entry));
    }
}

QWidget* HistoryPanel::createItemWidget(ItemEntry& entry, const QString& text) {
    auto* widget = new QWidget;
    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(6);

    // Icon
    entry.iconLabel = new QLabel;
    entry.iconLabel->setFixedSize(16, 16);
    layout->addWidget(entry.iconLabel);

    // Text
    entry.textLabel = new QLabel(text);
    layout->addWidget(entry.textLabel, 1);

    // Status indicator
    entry.statusButton = new QToolButton;
    entry.statusButton->setFixedSize(16, 16);
    entry.statusButton->setAutoRaise(true);
    entry.statusButton->setVisible(false);
    layout->addWidget(entry.statusButton);

    updateItemState(entry);
    return widget;
}

void HistoryPanel::updateItemState(ItemEntry& entry) {
    QString textStyle = "color: #cccccc;";
    QString iconText = operationIcon(entry.type);

    if (entry.failed) {
        textStyle = "color: #f48771; text-decoration: line-through;";
        entry.statusButton->setText("⚠");
        if (!entry.failureReason.empty()) {
            entry.statusButton->setToolTip(QString::fromStdString(entry.failureReason));
        } else {
            entry.statusButton->setToolTip("Operation failed");
        }
        entry.statusButton->setVisible(true);
    } else if (entry.suppressed) {
        textStyle = "color: #666666; font-style: italic;";
        entry.statusButton->setText("○");
        entry.statusButton->setToolTip("Suppressed");
        entry.statusButton->setVisible(true);
    } else {
        entry.statusButton->setVisible(false);
    }

    entry.textLabel->setStyleSheet(textStyle);
    entry.iconLabel->setText(iconText);
    entry.iconLabel->setStyleSheet("color: #888888;");
}

QString HistoryPanel::operationDisplayName(const app::OperationRecord& op) const {
    QString typeName;
    QString params;

    switch (op.type) {
        case app::OperationType::Extrude:
            typeName = "Extrude";
            if (std::holds_alternative<app::ExtrudeParams>(op.params)) {
                const auto& p = std::get<app::ExtrudeParams>(op.params);
                params = QString(" (%1mm)").arg(p.distance, 0, 'f', 1);
            }
            break;
        case app::OperationType::Revolve:
            typeName = "Revolve";
            if (std::holds_alternative<app::RevolveParams>(op.params)) {
                const auto& p = std::get<app::RevolveParams>(op.params);
                params = QString(" (%1°)").arg(p.angleDeg, 0, 'f', 0);
            }
            break;
        case app::OperationType::Fillet:
            typeName = "Fillet";
            if (std::holds_alternative<app::FilletChamferParams>(op.params)) {
                const auto& p = std::get<app::FilletChamferParams>(op.params);
                params = QString(" (R%1)").arg(p.radius, 0, 'f', 1);
            }
            break;
        case app::OperationType::Chamfer:
            typeName = "Chamfer";
            if (std::holds_alternative<app::FilletChamferParams>(op.params)) {
                const auto& p = std::get<app::FilletChamferParams>(op.params);
                params = QString(" (%1)").arg(p.radius, 0, 'f', 1);
            }
            break;
        case app::OperationType::Shell:
            typeName = "Shell";
            if (std::holds_alternative<app::ShellParams>(op.params)) {
                const auto& p = std::get<app::ShellParams>(op.params);
                params = QString(" (%1mm)").arg(p.thickness, 0, 'f', 1);
            }
            break;
        case app::OperationType::Boolean:
            typeName = "Boolean";
            if (std::holds_alternative<app::BooleanParams>(op.params)) {
                const auto& p = std::get<app::BooleanParams>(op.params);
                switch (p.operation) {
                    case app::BooleanParams::Op::Union: params = " (Union)"; break;
                    case app::BooleanParams::Op::Cut: params = " (Cut)"; break;
                    case app::BooleanParams::Op::Intersect: params = " (Intersect)"; break;
                }
            }
            break;
    }

    return typeName + params;
}

QString HistoryPanel::operationIcon(app::OperationType type) const {
    switch (type) {
        case app::OperationType::Extrude: return "↑";
        case app::OperationType::Revolve: return "↻";
        case app::OperationType::Fillet: return "◠";
        case app::OperationType::Chamfer: return "◿";
        case app::OperationType::Shell: return "□";
        case app::OperationType::Boolean: return "⊕";
        default: return "⚙";
    }
}

bool HistoryPanel::isEditableType(app::OperationType type) const {
    // v1: Only Extrude and Revolve are editable
    return type == app::OperationType::Extrude ||
           type == app::OperationType::Revolve;
}

void HistoryPanel::onItemClicked(QTreeWidgetItem* item, int) {
    auto* entry = entryForItem(item);
    if (entry) {
        emit operationSelected(QString::fromStdString(entry->opId));
    }
}

void HistoryPanel::onItemDoubleClicked(QTreeWidgetItem* item, int) {
    auto* entry = entryForItem(item);
    if (!entry || !document_) return;

    // Find the operation to check if editable
    for (const auto& op : document_->operations()) {
        if (op.opId == entry->opId) {
            if (isEditableType(op.type)) {
                showEditDialog(entry->opId);
            }
            break;
        }
    }
}

void HistoryPanel::showEditDialog(const std::string& opId) {
    if (!document_ || !viewport_) return;

    EditParameterDialog dialog(document_, viewport_, commandProcessor_, opId, this);
    if (dialog.exec() == QDialog::Accepted) {
        rebuild();
        viewport_->update();
    }
}

void HistoryPanel::onCustomContextMenu(const QPoint& pos) {
    QTreeWidgetItem* item = treeWidget_->itemAt(pos);
    if (item) {
        showContextMenu(treeWidget_->viewport()->mapToGlobal(pos), item);
    }
}

void HistoryPanel::showContextMenu(const QPoint& pos, QTreeWidgetItem* item) {
    auto* entry = entryForItem(item);
    if (!entry || !document_) return;

    // Find the operation
    const app::OperationRecord* opRecord = nullptr;
    for (const auto& op : document_->operations()) {
        if (op.opId == entry->opId) {
            opRecord = &op;
            break;
        }
    }
    if (!opRecord) return;

    QMenu menu;

    if (isEditableType(opRecord->type)) {
        QAction* editAction = menu.addAction("Edit Parameters...");
        connect(editAction, &QAction::triggered, this, [this, entry]() {
            showEditDialog(entry->opId);
        });
    }

    menu.addSeparator();

    QAction* rollbackAction = menu.addAction("Rollback to Here");
    connect(rollbackAction, &QAction::triggered, this, [this, entry]() {
        emit rollbackRequested(QString::fromStdString(entry->opId));
    });

    QString suppressText = entry->suppressed ? "Unsuppress" : "Suppress";
    QAction* suppressAction = menu.addAction(suppressText);
    connect(suppressAction, &QAction::triggered, this, [this, entry]() {
        emit suppressRequested(QString::fromStdString(entry->opId), !entry->suppressed);
    });

    menu.addSeparator();

    QAction* deleteAction = menu.addAction("Delete");
    deleteAction->setShortcut(QKeySequence::Delete);
    connect(deleteAction, &QAction::triggered, this, [this, entry]() {
        emit deleteRequested(QString::fromStdString(entry->opId));
    });

    menu.exec(pos);
}

HistoryPanel::ItemEntry* HistoryPanel::entryForItem(QTreeWidgetItem* item) {
    for (auto& entry : entries_) {
        if (entry.item == item) {
            return &entry;
        }
    }
    return nullptr;
}

HistoryPanel::ItemEntry* HistoryPanel::entryForId(const std::string& opId) {
    for (auto& entry : entries_) {
        if (entry.opId == opId) {
            return &entry;
        }
    }
    return nullptr;
}

void HistoryPanel::setCollapsed(bool collapsed) {
    if (collapsed_ == collapsed) return;
    collapsed_ = collapsed;
    applyCollapseState(true);
    emit collapsedChanged(collapsed_);
}

void HistoryPanel::applyCollapseState(bool animate) {
    const int targetWidth = collapsed_ ? collapsedWidth_ : expandedWidth_;

    if (!animate) {
        panel_->setVisible(!collapsed_);
        setMinimumWidth(targetWidth);
        setMaximumWidth(targetWidth);
        return;
    }

    if (!collapsed_) {
        panel_->setVisible(true);
    }

    setMinimumWidth(0);

    if (widthAnimation_) {
        widthAnimation_->stop();
        widthAnimation_->deleteLater();
    }

    widthAnimation_ = new QPropertyAnimation(this, "maximumWidth", this);
    widthAnimation_->setDuration(180);
    widthAnimation_->setEasingCurve(QEasingCurve::InOutCubic);
    widthAnimation_->setStartValue(width());
    widthAnimation_->setEndValue(targetWidth);

    connect(widthAnimation_, &QPropertyAnimation::finished, this, [this, targetWidth]() {
        if (collapsed_) {
            panel_->setVisible(false);
        }
        setMaximumWidth(targetWidth);
        setMinimumWidth(targetWidth);
    });

    widthAnimation_->start();
}

void HistoryPanel::onOperationAdded(const QString& opId) {
    rebuild();  // Full rebuild for now
}

void HistoryPanel::onOperationRemoved(const QString& opId) {
    rebuild();
}

void HistoryPanel::onOperationFailed(const QString& opId, const QString& reason) {
    auto* entry = entryForId(opId.toStdString());
    if (entry) {
        entry->failed = true;
        entry->failureReason = reason.toStdString();
        updateItemState(*entry);
    }
}

void HistoryPanel::onOperationSucceeded(const QString& opId) {
    auto* entry = entryForId(opId.toStdString());
    if (entry) {
        entry->failed = false;
        entry->failureReason.clear();
        updateItemState(*entry);
    }
}

void HistoryPanel::onOperationSuppressed(const QString& opId, bool suppressed) {
    auto* entry = entryForId(opId.toStdString());
    if (entry) {
        entry->suppressed = suppressed;
        updateItemState(*entry);
    }
}

} // namespace onecad::ui
