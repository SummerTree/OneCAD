#ifndef ONECAD_UI_NAVIGATOR_MODELNAVIGATOR_H
#define ONECAD_UI_NAVIGATOR_MODELNAVIGATOR_H

#include <QMetaObject>
#include <QWidget>
#include <QString>
#include <QColor>
#include <vector>
#include <unordered_map>
#include <string>

class QTreeWidget;
class QTreeWidgetItem;
class QFrame;
class QPropertyAnimation;
class QLabel;
class QToolButton;
class QLineEdit;
class QEvent;

namespace onecad {
namespace app { class Document; }
namespace ui {

/**
 * @brief Model navigator showing document structure.
 * 
 * Displays hierarchical tree of:
 * - Bodies
 * - Sketches  
 * - Feature History (when parametric mode)
 */
class ModelNavigator : public QWidget {
    Q_OBJECT

public:
    explicit ModelNavigator(QWidget* parent = nullptr);
    ~ModelNavigator() override = default;
    void setCollapsed(bool collapsed);
    bool isCollapsed() const { return m_collapsed; }

signals:
    void itemSelected(const QString& itemId);
    void itemDoubleClicked(const QString& itemId);
    void editSketchRequested(const QString& sketchId);
    void sketchSelected(const QString& sketchId);
    void bodySelected(const QString& bodyId);
    void collapsedChanged(bool collapsed);
    void renameRequested(const QString& itemId);
    void renameCommitted(const QString& itemId, const QString& newName);
    void isolateRequested(const QString& itemId);
    void deleteRequested(const QString& itemId);
    void visibilityToggled(const QString& itemId, bool visible);

public slots:
    // Document model integration
    void rebuild(const app::Document* doc);
    void onSketchAdded(const QString& id);
    void onSketchRemoved(const QString& id);
    void onSketchRenamed(const QString& id, const QString& newName);
    void onBodyAdded(const QString& id);
    void onBodyRemoved(const QString& id);
    void onBodyRenamed(const QString& id, const QString& newName);

    // Visibility sync from Document
    void onBodyVisibilityChanged(const QString& id, bool visible);
    void onSketchVisibilityChanged(const QString& id, bool visible);

    // Inline editing
    void startInlineEdit(const QString& itemId);

private slots:
    void onItemClicked(QTreeWidgetItem* item, int column);
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    struct ItemCollection {
        std::unordered_map<std::string, QTreeWidgetItem*>& items;
        unsigned int& counter;
        QTreeWidgetItem* root = nullptr;
        QString namePrefix;
        QString placeholderText;
    };

    enum class ItemType { Body, Sketch };

    struct ItemEntry {
        ItemType type;
        QTreeWidgetItem* item;
        QWidget* widget;
        QLabel* iconLabel;
        QLabel* textLabel;
        QToolButton* eyeButton;
        QToolButton* overflowButton;
        bool visible;
    };

    void setupUi();
    void setupRoots();
    void createPlaceholderItems();
    void createPlaceholder(QTreeWidgetItem* root, const QString& text);
    QWidget* createSectionHeader(const QString& text);
    QWidget* createItemWidget(ItemEntry& entry, const QString& text, ItemType type, bool visible);
    void refreshItemWidget(ItemEntry& entry);
    void setupItemConnections(ItemEntry& entry);
    void updateSelectionState();
    ItemEntry* entryForItem(QTreeWidgetItem* item);
    ItemEntry* entryForId(const std::string& id);
    void showContextMenu(const QPoint& pos, QTreeWidgetItem* item);
    void handleVisibilityToggle(ItemEntry& entry);
    void applyCollapseState(bool animate);
    void addItem(ItemCollection& collection, const QString& id);
    void finishInlineEdit();
    void cancelInlineEdit();
    void removeItem(ItemCollection& collection, const QString& id);
    void renameItem(ItemCollection& collection, const QString& id, const QString& newName);
    void updateTheme();
    void updatePlaceholderColors();
    void applyPlaceholderColor(QTreeWidgetItem* item);

    QFrame* m_panel = nullptr;
    QTreeWidget* m_treeWidget = nullptr;
    QTreeWidgetItem* m_bodiesRoot = nullptr;
    QTreeWidgetItem* m_sketchesRoot = nullptr;
    std::vector<ItemEntry> m_entries;
    bool m_collapsed = false;
    QPropertyAnimation* m_widthAnimation = nullptr;
    int m_expandedWidth = 260;
    int m_collapsedWidth = 0;
    QColor m_placeholderColor;
    QMetaObject::Connection m_themeConnection;

    // Map sketch IDs to tree items
    std::unordered_map<std::string, QTreeWidgetItem*> m_sketchItems;
    std::unordered_map<std::string, QTreeWidgetItem*> m_bodyItems;

    // Counter for unique sketch naming
    unsigned int m_sketchCounter = 0;
    unsigned int m_bodyCounter = 0;

    // Inline editing state
    QLineEdit* m_inlineEditor = nullptr;
    ItemEntry* m_editingEntry = nullptr;
    QString m_editingItemId;
};

} // namespace ui
} // namespace onecad

#endif // ONECAD_UI_NAVIGATOR_MODELNAVIGATOR_H
