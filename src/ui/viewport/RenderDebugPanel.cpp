#include "RenderDebugPanel.h"

#include "../theme/ThemeManager.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace onecad::ui {

RenderDebugPanel::RenderDebugPanel(QWidget* parent)
    : QWidget(parent) {
    setupUi();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &RenderDebugPanel::updateTheme, Qt::UniqueConnection);
    updateTheme();
}

void RenderDebugPanel::setupUi() {
    setObjectName("RenderDebugPanel");
    setWindowFlag(Qt::FramelessWindowHint, true);
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedWidth(280);

    setStyleSheet(R"(
        RenderDebugPanel {
            background-color: palette(window);
            border: 1px solid palette(mid);
            border-radius: 4px;
        }
        QLabel#title {
            font-weight: bold;
            font-size: 11px;
            padding: 8px;
            color: palette(text);
        }
        QGroupBox {
            font-size: 10px;
            font-weight: bold;
            margin-top: 6px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 2px 6px;
        }
        QCheckBox, QDoubleSpinBox, QPushButton, QLabel {
            font-size: 10px;
        }
    )");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    m_titleLabel = new QLabel(tr("RENDER DEBUG"), this);
    m_titleLabel->setObjectName("title");
    layout->addWidget(m_titleLabel);

    auto* debugGroup = new QGroupBox(tr("Debug Views"), this);
    auto* debugLayout = new QGridLayout(debugGroup);
    debugLayout->setContentsMargins(6, 8, 6, 6);
    debugLayout->setHorizontalSpacing(6);
    debugLayout->setVerticalSpacing(4);

    m_debugNormals = new QCheckBox(tr("Normals (F1)"), debugGroup);
    m_debugDepth = new QCheckBox(tr("Depth (F2)"), debugGroup);
    m_wireframeOnly = new QCheckBox(tr("Wireframe (F3)"), debugGroup);
    m_disableGamma = new QCheckBox(tr("Gamma Off (F4)"), debugGroup);
    m_useMatcap = new QCheckBox(tr("MatCap (F5)"), debugGroup);

    debugLayout->addWidget(m_debugNormals, 0, 0);
    debugLayout->addWidget(m_debugDepth, 0, 1);
    debugLayout->addWidget(m_wireframeOnly, 1, 0);
    debugLayout->addWidget(m_disableGamma, 1, 1);
    debugLayout->addWidget(m_useMatcap, 2, 0);
    layout->addWidget(debugGroup);

    auto* lightingGroup = new QGroupBox(tr("Lighting"), this);
    auto* lightingLayout = new QGridLayout(lightingGroup);
    lightingLayout->setContentsMargins(6, 8, 6, 6);
    lightingLayout->setHorizontalSpacing(6);
    lightingLayout->setVerticalSpacing(4);

    int row = 0;
    lightingLayout->addWidget(new QLabel(tr("Key Dir"), lightingGroup), row, 0);
    m_keyDirX = createDirectionSpinBox();
    m_keyDirY = createDirectionSpinBox();
    m_keyDirZ = createDirectionSpinBox();
    lightingLayout->addWidget(m_keyDirX, row, 1);
    lightingLayout->addWidget(m_keyDirY, row, 2);
    lightingLayout->addWidget(m_keyDirZ, row, 3);
    row++;

    lightingLayout->addWidget(new QLabel(tr("Fill Dir"), lightingGroup), row, 0);
    m_fillDirX = createDirectionSpinBox();
    m_fillDirY = createDirectionSpinBox();
    m_fillDirZ = createDirectionSpinBox();
    lightingLayout->addWidget(m_fillDirX, row, 1);
    lightingLayout->addWidget(m_fillDirY, row, 2);
    lightingLayout->addWidget(m_fillDirZ, row, 3);
    row++;

    lightingLayout->addWidget(new QLabel(tr("Fill Int"), lightingGroup), row, 0);
    m_fillIntensity = createIntensitySpinBox(0.0, 1.0, 0.05);
    lightingLayout->addWidget(m_fillIntensity, row, 1, 1, 2);
    row++;

    lightingLayout->addWidget(new QLabel(tr("Ambient Int"), lightingGroup), row, 0);
    m_ambientIntensity = createIntensitySpinBox(0.0, 1.0, 0.05);
    lightingLayout->addWidget(m_ambientIntensity, row, 1, 1, 2);
    row++;

    lightingLayout->addWidget(new QLabel(tr("Hemi Up"), lightingGroup), row, 0);
    m_hemiUpX = createDirectionSpinBox();
    m_hemiUpY = createDirectionSpinBox();
    m_hemiUpZ = createDirectionSpinBox();
    lightingLayout->addWidget(m_hemiUpX, row, 1);
    lightingLayout->addWidget(m_hemiUpY, row, 2);
    lightingLayout->addWidget(m_hemiUpZ, row, 3);
    layout->addWidget(lightingGroup);

    auto* gradientGroup = new QGroupBox(tr("Ambient Gradient"), this);
    auto* gradientLayout = new QGridLayout(gradientGroup);
    gradientLayout->setContentsMargins(6, 8, 6, 6);
    gradientLayout->setHorizontalSpacing(6);
    gradientLayout->setVerticalSpacing(4);

    gradientLayout->addWidget(new QLabel(tr("Strength"), gradientGroup), 0, 0);
    m_gradientStrength = createIntensitySpinBox(0.0, 0.5, 0.02);
    gradientLayout->addWidget(m_gradientStrength, 0, 1, 1, 2);

    gradientLayout->addWidget(new QLabel(tr("Direction"), gradientGroup), 1, 0);
    m_gradientDirX = createDirectionSpinBox();
    m_gradientDirY = createDirectionSpinBox();
    m_gradientDirZ = createDirectionSpinBox();
    gradientLayout->addWidget(m_gradientDirX, 1, 1);
    gradientLayout->addWidget(m_gradientDirY, 1, 2);
    gradientLayout->addWidget(m_gradientDirZ, 1, 3);
    layout->addWidget(gradientGroup);

    m_resetButton = new QPushButton(tr("Reset To Theme"), this);
    m_resetButton->setFixedHeight(24);
    layout->addWidget(m_resetButton);

    connect(m_resetButton, &QPushButton::clicked, this, &RenderDebugPanel::resetToThemeRequested);

    auto emitDebugChanged = [this]() {
        emit debugTogglesChanged();
    };
    connect(m_wireframeOnly, &QCheckBox::toggled, this, emitDebugChanged);
    connect(m_disableGamma, &QCheckBox::toggled, this, emitDebugChanged);
    connect(m_useMatcap, &QCheckBox::toggled, this, emitDebugChanged);
    connect(m_debugNormals, &QCheckBox::toggled, this, [this](bool on) {
        if (on) {
            QSignalBlocker block(m_debugDepth);
            m_debugDepth->setChecked(false);
        }
        emit debugTogglesChanged();
    });
    connect(m_debugDepth, &QCheckBox::toggled, this, [this](bool on) {
        if (on) {
            QSignalBlocker block(m_debugNormals);
            m_debugNormals->setChecked(false);
        }
        emit debugTogglesChanged();
    });

    auto emitLightChanged = [this]() {
        emit lightRigChanged();
    };
    for (auto* box : {
             m_keyDirX, m_keyDirY, m_keyDirZ,
             m_fillDirX, m_fillDirY, m_fillDirZ,
             m_hemiUpX, m_hemiUpY, m_hemiUpZ,
             m_gradientDirX, m_gradientDirY, m_gradientDirZ,
             m_fillIntensity, m_ambientIntensity, m_gradientStrength
         }) {
        connect(box, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, emitLightChanged);
    }
}

void RenderDebugPanel::updateTheme() {
    // No-op: styling uses palette, and values are user-tuned.
}

QDoubleSpinBox* RenderDebugPanel::createDirectionSpinBox() {
    auto* box = new QDoubleSpinBox(this);
    box->setRange(-1.0, 1.0);
    box->setSingleStep(0.05);
    box->setDecimals(2);
    box->setFixedWidth(58);
    return box;
}

QDoubleSpinBox* RenderDebugPanel::createIntensitySpinBox(double min, double max, double step) {
    auto* box = new QDoubleSpinBox(this);
    box->setRange(min, max);
    box->setSingleStep(step);
    box->setDecimals(2);
    box->setFixedWidth(80);
    return box;
}

QVector3D RenderDebugPanel::readVector(QDoubleSpinBox* x, QDoubleSpinBox* y, QDoubleSpinBox* z) const {
    return QVector3D(static_cast<float>(x->value()),
                     static_cast<float>(y->value()),
                     static_cast<float>(z->value()));
}

void RenderDebugPanel::setVector(QDoubleSpinBox* x, QDoubleSpinBox* y, QDoubleSpinBox* z,
                                 const QVector3D& v) {
    x->setValue(v.x());
    y->setValue(v.y());
    z->setValue(v.z());
}

RenderDebugPanel::DebugToggles RenderDebugPanel::debugToggles() const {
    DebugToggles toggles;
    toggles.normals = m_debugNormals->isChecked();
    toggles.depth = m_debugDepth->isChecked();
    toggles.wireframe = m_wireframeOnly->isChecked();
    toggles.disableGamma = m_disableGamma->isChecked();
    toggles.matcap = m_useMatcap->isChecked();
    return toggles;
}

void RenderDebugPanel::setDebugToggles(const DebugToggles& toggles) {
    QSignalBlocker b1(m_debugNormals);
    QSignalBlocker b2(m_debugDepth);
    QSignalBlocker b3(m_wireframeOnly);
    QSignalBlocker b4(m_disableGamma);
    QSignalBlocker b5(m_useMatcap);

    m_debugNormals->setChecked(toggles.normals);
    m_debugDepth->setChecked(toggles.depth);
    m_wireframeOnly->setChecked(toggles.wireframe);
    m_disableGamma->setChecked(toggles.disableGamma);
    m_useMatcap->setChecked(toggles.matcap);
}

RenderDebugPanel::LightRig RenderDebugPanel::lightRig() const {
    LightRig rig;
    rig.keyDir = readVector(m_keyDirX, m_keyDirY, m_keyDirZ);
    rig.fillDir = readVector(m_fillDirX, m_fillDirY, m_fillDirZ);
    rig.fillIntensity = static_cast<float>(m_fillIntensity->value());
    rig.ambientIntensity = static_cast<float>(m_ambientIntensity->value());
    rig.hemiUpDir = readVector(m_hemiUpX, m_hemiUpY, m_hemiUpZ);
    rig.gradientDir = readVector(m_gradientDirX, m_gradientDirY, m_gradientDirZ);
    rig.gradientStrength = static_cast<float>(m_gradientStrength->value());
    return rig;
}

void RenderDebugPanel::setLightRig(const LightRig& rig) {
    QSignalBlocker b1(m_keyDirX);
    QSignalBlocker b2(m_keyDirY);
    QSignalBlocker b3(m_keyDirZ);
    QSignalBlocker b4(m_fillDirX);
    QSignalBlocker b5(m_fillDirY);
    QSignalBlocker b6(m_fillDirZ);
    QSignalBlocker b7(m_fillIntensity);
    QSignalBlocker b8(m_ambientIntensity);
    QSignalBlocker b9(m_hemiUpX);
    QSignalBlocker b10(m_hemiUpY);
    QSignalBlocker b11(m_hemiUpZ);
    QSignalBlocker b12(m_gradientDirX);
    QSignalBlocker b13(m_gradientDirY);
    QSignalBlocker b14(m_gradientDirZ);
    QSignalBlocker b15(m_gradientStrength);

    setVector(m_keyDirX, m_keyDirY, m_keyDirZ, rig.keyDir);
    setVector(m_fillDirX, m_fillDirY, m_fillDirZ, rig.fillDir);
    m_fillIntensity->setValue(rig.fillIntensity);
    m_ambientIntensity->setValue(rig.ambientIntensity);
    setVector(m_hemiUpX, m_hemiUpY, m_hemiUpZ, rig.hemiUpDir);
    setVector(m_gradientDirX, m_gradientDirY, m_gradientDirZ, rig.gradientDir);
    m_gradientStrength->setValue(rig.gradientStrength);
}

} // namespace onecad::ui
