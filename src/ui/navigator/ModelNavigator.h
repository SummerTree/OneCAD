#ifndef ONECAD_UI_NAVIGATOR_MODELNAVIGATOR_H
#define ONECAD_UI_NAVIGATOR_MODELNAVIGATOR_H

#include <QWidget>
#include <QString>
#include <unordered_map>
#include <string>

class QTreeWidget;
class QTreeWidgetItem;
class QStackedLayout;
class QToolButton;
class QFrame;

namespace onecad {
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

public slots:
    // Document model integration
    void onSketchAdded(const QString& id);
    void onSketchRemoved(const QString& id);
    void onSketchRenamed(const QString& id, const QString& newName);

private slots:
    void onItemClicked(QTreeWidgetItem* item, int column);
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);

private:
    void setupUi();
    void createPlaceholderItems();
    void applyCollapseState();
    void updateOverlayButtonIcon();

    QStackedLayout* m_stack = nullptr;
    QFrame* m_panel = nullptr;
    QToolButton* m_collapseButton = nullptr;
    QToolButton* m_expandButton = nullptr;
    QTreeWidget* m_treeWidget = nullptr;
    QTreeWidgetItem* m_bodiesRoot = nullptr;
    QTreeWidgetItem* m_sketchesRoot = nullptr;
    bool m_collapsed = false;

    // Map sketch IDs to tree items
    std::unordered_map<std::string, QTreeWidgetItem*> m_sketchItems;

    // Counter for unique sketch naming
    unsigned int m_sketchCounter = 0;
};

} // namespace ui
} // namespace onecad

#endif // ONECAD_UI_NAVIGATOR_MODELNAVIGATOR_H
