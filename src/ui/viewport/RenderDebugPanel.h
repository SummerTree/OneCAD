#ifndef ONECAD_UI_VIEWPORT_RENDERDEBUGPANEL_H
#define ONECAD_UI_VIEWPORT_RENDERDEBUGPANEL_H

#include <QVector3D>
#include <QWidget>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace onecad::ui {

class RenderDebugPanel : public QWidget {
    Q_OBJECT

public:
    struct DebugToggles {
        bool normals = false;
        bool depth = false;
        bool wireframe = false;
        bool disableGamma = false;
        bool matcap = false;
    };

    struct LightRig {
        QVector3D keyDir{ -0.4f, 0.5f, 0.75f };
        QVector3D fillDir{ 0.6f, -0.2f, 0.55f };
        float fillIntensity = 0.35f;
        float ambientIntensity = 0.25f;
        QVector3D hemiUpDir{ 0.0f, 1.0f, 0.0f };
        QVector3D gradientDir{ 0.0f, 1.0f, 0.0f };
        float gradientStrength = 0.08f;
    };

    explicit RenderDebugPanel(QWidget* parent = nullptr);
    ~RenderDebugPanel() override = default;

    void setDebugToggles(const DebugToggles& toggles);
    DebugToggles debugToggles() const;

    void setLightRig(const LightRig& rig);
    LightRig lightRig() const;

signals:
    void debugTogglesChanged();
    void lightRigChanged();
    void resetToThemeRequested();

private:
    void setupUi();
    void updateTheme();
    QDoubleSpinBox* createDirectionSpinBox();
    QDoubleSpinBox* createIntensitySpinBox(double min, double max, double step);
    QVector3D readVector(QDoubleSpinBox* x, QDoubleSpinBox* y, QDoubleSpinBox* z) const;
    void setVector(QDoubleSpinBox* x, QDoubleSpinBox* y, QDoubleSpinBox* z, const QVector3D& v);

    QLabel* m_titleLabel = nullptr;
    QCheckBox* m_debugNormals = nullptr;
    QCheckBox* m_debugDepth = nullptr;
    QCheckBox* m_wireframeOnly = nullptr;
    QCheckBox* m_disableGamma = nullptr;
    QCheckBox* m_useMatcap = nullptr;

    QDoubleSpinBox* m_keyDirX = nullptr;
    QDoubleSpinBox* m_keyDirY = nullptr;
    QDoubleSpinBox* m_keyDirZ = nullptr;
    QDoubleSpinBox* m_fillDirX = nullptr;
    QDoubleSpinBox* m_fillDirY = nullptr;
    QDoubleSpinBox* m_fillDirZ = nullptr;
    QDoubleSpinBox* m_fillIntensity = nullptr;
    QDoubleSpinBox* m_ambientIntensity = nullptr;
    QDoubleSpinBox* m_hemiUpX = nullptr;
    QDoubleSpinBox* m_hemiUpY = nullptr;
    QDoubleSpinBox* m_hemiUpZ = nullptr;
    QDoubleSpinBox* m_gradientDirX = nullptr;
    QDoubleSpinBox* m_gradientDirY = nullptr;
    QDoubleSpinBox* m_gradientDirZ = nullptr;
    QDoubleSpinBox* m_gradientStrength = nullptr;
    QPushButton* m_resetButton = nullptr;
};

} // namespace onecad::ui

#endif // ONECAD_UI_VIEWPORT_RENDERDEBUGPANEL_H
