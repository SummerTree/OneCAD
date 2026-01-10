#ifndef ONECAD_UI_VIEWPORT_SNAPSETTINGSPANEL_H
#define ONECAD_UI_VIEWPORT_SNAPSETTINGSPANEL_H

#include <QWidget>

class QCheckBox;
class QLabel;

namespace onecad::ui {

class SnapSettingsPanel : public QWidget {
    Q_OBJECT

public:
    struct SnapSettings {
        bool grid = true;
        bool sketchGuideLines = true;
        bool sketchGuidePoints = true;
        bool activeLayer3DPoints = true;
        bool activeLayer3DEdges = true;
        bool showGuidePoints = true;
        bool showSnappingHints = true;
    };

    explicit SnapSettingsPanel(QWidget* parent = nullptr);
    ~SnapSettingsPanel() override = default;

    void setSettings(const SnapSettings& settings);
    SnapSettings settings() const;

signals:
    void settingsChanged();

private:
    void setupUi();
    void updateTheme();
    class ToggleSwitch* createToggle(const QString& text);

    // Block mouse/touch from reaching the viewport behind the panel
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

    QLabel* m_titleLabel = nullptr;
    
    // Snap to
    class ToggleSwitch* m_snapGrid = nullptr;
    class ToggleSwitch* m_snapSketchLines = nullptr;
    class ToggleSwitch* m_snapSketchPoints = nullptr;
    class ToggleSwitch* m_snap3DPoints = nullptr;
    class ToggleSwitch* m_snap3DEdges = nullptr;
    
    // Show
    class ToggleSwitch* m_showGuidePoints = nullptr;
    class ToggleSwitch* m_showHints = nullptr;
};

} // namespace onecad::ui

#endif // ONECAD_UI_VIEWPORT_SNAPSETTINGSPANEL_H
