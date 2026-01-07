#ifndef ONECAD_UI_MAINWINDOW_H
#define ONECAD_UI_MAINWINDOW_H

#include <QMainWindow>
#include <memory>
#include <string>

class QLabel;
class QSlider;
class QEvent;
class QAction;
namespace onecad {
namespace ui {
class SidebarToolButton;
}
}

namespace onecad {
namespace app {
    class Document;
    namespace commands {
        class CommandProcessor;
    }
}
namespace core::sketch {
    class Sketch;
    enum class ConstraintType;
}
namespace ui {

class Viewport;
class ModelNavigator;
class ContextToolbar;
class ConstraintPanel;
class SketchModePanel;
class RenderDebugPanel;

/**
 * @brief Main application window for OneCAD.
 * 
 * Layout per specification:
 * - Top: Menu bar
 * - Left: Docked navigator with floating action toolbar on the viewport top
 * - Center: 3D Viewport
 * - Right: Property inspector (collapsible dock)
 * - Bottom: Status bar
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onNewSketch();
    void onExitSketch();
    void onSketchModeChanged(bool inSketchMode);
    void onSketchPlanePicked(int planeIndex);
    void onPlaneSelectionCancelled();
    void onImport();
    void onMousePositionChanged(double x, double y, double z);
    void onSketchUpdated();
    void onConstraintRequested(core::sketch::ConstraintType constraintType);

    // Navigator item actions
    void onDeleteItem(const QString& itemId);
    void onRenameItem(const QString& itemId);
    void onVisibilityToggled(const QString& itemId, bool visible);
    void onIsolateItem(const QString& itemId);

private:
    void setupMenuBar();
    void setupToolBar();
    void setupViewport();
    void setupStatusBar();
    void applyTheme();
    void updateDofStatus(core::sketch::Sketch* sketch);
    void applyDofStatusStyle();
    void positionToolbarOverlay();
    void setupNavigatorOverlayButton();
    void positionNavigatorOverlayButton();
    void setupRenderDebugOverlay();
    void positionRenderDebugButton();
    void positionRenderDebugPanel();
    void applyRenderDebugDefaults();
    void positionConstraintPanel();
    void positionSketchModePanel();

    bool eventFilter(QObject* obj, QEvent* event) override;

    // UI Components
    Viewport* m_viewport = nullptr;
    ModelNavigator* m_navigator = nullptr;
    ContextToolbar* m_toolbar = nullptr;
    SidebarToolButton* m_navigatorOverlayButton = nullptr;
    SidebarToolButton* m_renderDebugButton = nullptr;
    ConstraintPanel* m_constraintPanel = nullptr;
    SketchModePanel* m_sketchModePanel = nullptr;
    RenderDebugPanel* m_renderDebugPanel = nullptr;

    // Document model (owns all sketches)
    std::unique_ptr<app::Document> m_document;
    std::unique_ptr<app::commands::CommandProcessor> m_commandProcessor;

    // Active editing state
    std::string m_activeSketchId;  // Currently editing sketch ID (empty if not in sketch mode)
    
    // Status bar labels
    QLabel* m_toolStatus = nullptr;
    QLabel* m_dofStatus = nullptr;
    QLabel* m_coordStatus = nullptr;
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;
    bool m_hasCachedDof = false;
    int m_cachedDof = 0;
    bool m_cachedOverConstrained = false;

    // Camera angle control
    QLabel* m_cameraAngleLabel = nullptr;
    QSlider* m_cameraAngleSlider = nullptr;

    void loadSettings();
    void saveSettings();
};

} // namespace ui
} // namespace onecad

// For backward compatibility with existing main.cpp
using MainWindow = onecad::ui::MainWindow;

#endif // ONECAD_UI_MAINWINDOW_H
