/**
 * @file HistoryPanel.h
 * @brief Feature history tree panel (Fusion 360 style).
 */
#ifndef ONECAD_UI_HISTORY_HISTORYPANEL_H
#define ONECAD_UI_HISTORY_HISTORYPANEL_H

#include <QWidget>
#include <QString>
#include <vector>
#include <unordered_map>
#include <string>

class QTreeWidget;
class QTreeWidgetItem;
class QFrame;
class QPropertyAnimation;
class QLabel;
class QToolButton;
class QMenu;

namespace onecad {
namespace app {
class Document;
struct OperationRecord;
enum class OperationType;
namespace commands {
class CommandProcessor;
}
}

namespace ui {

class Viewport;

/**
 * @brief Feature history panel showing parametric operation tree.
 *
 * Displays operations in dependency order:
 * - Extrude, Revolve (editable)
 * - Fillet, Chamfer, Shell, Boolean (display-only for v1)
 *
 * States:
 * - Normal: default appearance
 * - Selected: bold
 * - Failed: red background, strikethrough
 * - Suppressed: gray, italic
 */
class HistoryPanel : public QWidget {
    Q_OBJECT

public:
    explicit HistoryPanel(QWidget* parent = nullptr);
    ~HistoryPanel() override = default;

    void setDocument(app::Document* doc);
    void setViewport(Viewport* viewport);
    void setCommandProcessor(app::commands::CommandProcessor* processor);
    void setCollapsed(bool collapsed);
    bool isCollapsed() const { return collapsed_; }

signals:
    void collapsedChanged(bool collapsed);
    void operationSelected(const QString& opId);
    void operationDoubleClicked(const QString& opId);
    void editRequested(const QString& opId);
    void rollbackRequested(const QString& opId);
    void suppressRequested(const QString& opId, bool suppress);
    void deleteRequested(const QString& opId);

public slots:
    void rebuild();
    void onOperationAdded(const QString& opId);
    void onOperationRemoved(const QString& opId);
    void onOperationFailed(const QString& opId, const QString& reason);
    void onOperationSucceeded(const QString& opId);
    void onOperationSuppressed(const QString& opId, bool suppressed);

private slots:
    void onItemClicked(QTreeWidgetItem* item, int column);
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onCustomContextMenu(const QPoint& pos);

private:
    struct ItemEntry {
        std::string opId;
        app::OperationType type{};
        QTreeWidgetItem* item = nullptr;
        QWidget* widget = nullptr;
        QLabel* iconLabel = nullptr;
        QLabel* textLabel = nullptr;
        QToolButton* statusButton = nullptr;
        bool failed = false;
        bool suppressed = false;
        std::string failureReason;
    };

    void setupUi();
    void applyCollapseState(bool animate);
    QWidget* createItemWidget(ItemEntry& entry, const QString& text);
    void updateItemState(ItemEntry& entry);
    QString operationDisplayName(const app::OperationRecord& op) const;
    QString operationIcon(app::OperationType type) const;
    bool isEditableType(app::OperationType type) const;
    ItemEntry* entryForItem(QTreeWidgetItem* item);
    ItemEntry* entryForId(const std::string& opId);
    void showContextMenu(const QPoint& pos, QTreeWidgetItem* item);
    void showEditDialog(const std::string& opId);

    QFrame* panel_ = nullptr;
    QTreeWidget* treeWidget_ = nullptr;
    app::Document* document_ = nullptr;
    Viewport* viewport_ = nullptr;
    app::commands::CommandProcessor* commandProcessor_ = nullptr;
    std::vector<ItemEntry> entries_;
    bool collapsed_ = false;
    QPropertyAnimation* widthAnimation_ = nullptr;
    int expandedWidth_ = 260;
    int collapsedWidth_ = 0;
};

} // namespace ui
} // namespace onecad

#endif // ONECAD_UI_HISTORY_HISTORYPANEL_H
