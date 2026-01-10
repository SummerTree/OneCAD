#include "SnapSettingsPanel.h"
#include "../theme/ThemeManager.h"
#include "../components/ToggleSwitch.h" // Include custom component

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QFrame>
#include <QMouseEvent>
#include <QWheelEvent>

namespace onecad::ui {

SnapSettingsPanel::SnapSettingsPanel(QWidget* parent)
    : QWidget(parent) {
    setFocusPolicy(Qt::ClickFocus);
    setupUi();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &SnapSettingsPanel::updateTheme, Qt::UniqueConnection);
    updateTheme();
}

void SnapSettingsPanel::setupUi() {
    if (layout()) {
        return; // Build UI only once; theme updates should not rebuild widgets
    }

    setObjectName("SnapSettingsPanel");
    setWindowFlag(Qt::FramelessWindowHint, true);
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedWidth(260);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);

    // Section: Snap to
    auto* snapLabel = new QLabel(tr("SNAP TO"), this);
    snapLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    snapLabel->setFixedHeight(22);
    layout->addWidget(snapLabel);
    
    // Group options tightly
    auto* snapLayout = new QVBoxLayout();
    snapLayout->setSpacing(4);
    
    m_snapGrid = createToggle(tr("Grid"));
    snapLayout->addWidget(m_snapGrid);

    m_snapSketchLines = createToggle(tr("Sketch Guide Lines"));
    snapLayout->addWidget(m_snapSketchLines);

    m_snapSketchPoints = createToggle(tr("Sketch Guide Points"));
    snapLayout->addWidget(m_snapSketchPoints);

    m_snap3DPoints = createToggle(tr("3D Guide Points"));
    snapLayout->addWidget(m_snap3DPoints);

    m_snap3DEdges = createToggle(tr("Distant Edges"));
    snapLayout->addWidget(m_snap3DEdges);
    
    layout->addLayout(snapLayout);

    // Separator
    layout->addSpacing(8);
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Plain);
    sep->setFixedHeight(1);
    layout->addWidget(sep);
    layout->addSpacing(8);

    // Section: Show
    auto* showLabel = new QLabel(tr("SHOW"), this);
    showLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    showLabel->setFixedHeight(22);
    layout->addWidget(showLabel);
    
    auto* showLayout = new QVBoxLayout();
    showLayout->setSpacing(4);
    
    m_showGuidePoints = createToggle(tr("Guide Points"));
    showLayout->addWidget(m_showGuidePoints);

    m_showHints = createToggle(tr("Snapping Hints"));
    showLayout->addWidget(m_showHints);
    
    layout->addLayout(showLayout);
    
    // Initialize to default ON state
    m_snapGrid->setChecked(true);
    m_snapSketchLines->setChecked(true);
    m_snapSketchPoints->setChecked(true);
    m_snap3DPoints->setChecked(true);
    m_snap3DEdges->setChecked(true);
    m_showGuidePoints->setChecked(true);
    m_showHints->setChecked(true);
}

class ToggleSwitch* SnapSettingsPanel::createToggle(const QString& text) {
    auto* box = new ToggleSwitch(text, this);
    box->setChecked(true); // Default to ON
    
    // Connect standard toggled signal
    connect(box, &QCheckBox::toggled, this, &SnapSettingsPanel::settingsChanged);
    return box;
}

void SnapSettingsPanel::updateTheme() {
    const ThemeDefinition& theme = ThemeManager::instance().currentTheme();

    auto toHex = [](const QColor& c) {
        return QString("#%1%2%3")
            .arg(c.red(), 2, 16, QChar('0'))
            .arg(c.green(), 2, 16, QChar('0'))
            .arg(c.blue(), 2, 16, QChar('0'));
    };

    const QString bgColor = toHex(theme.ui.sidebarButtonBackground);
    const QString borderColor = toHex(theme.ui.sidebarButtonBorder);
    const QString headerColor = toHex(theme.ui.sidebarButtonText);

    setStyleSheet(QString(R"(
        #SnapSettingsPanel {
            background-color: %1;
            border: 1px solid %2;
            border-radius: 12px;
        }
        #SnapSettingsPanel QLabel {
            font-size: 12px;
            color: %3;
            font-weight: 600;
            letter-spacing: 0.6px;
            margin-bottom: 6px;
            background-color: transparent;
        }
        #SnapSettingsPanel QFrame {
            background-color: #333;
            max-height: 1px;
            border: none;
        }
    )").arg(bgColor, borderColor, headerColor));
}

void SnapSettingsPanel::setSettings(const SnapSettings& settings) {
    QSignalBlocker b1(m_snapGrid);
    QSignalBlocker b2(m_snapSketchLines);
    QSignalBlocker b3(m_snapSketchPoints);
    QSignalBlocker b4(m_snap3DPoints);
    QSignalBlocker b5(m_snap3DEdges);
    QSignalBlocker b6(m_showGuidePoints);
    QSignalBlocker b7(m_showHints);

    m_snapGrid->setChecked(settings.grid);
    m_snapSketchLines->setChecked(settings.sketchGuideLines);
    m_snapSketchPoints->setChecked(settings.sketchGuidePoints);
    m_snap3DPoints->setChecked(settings.activeLayer3DPoints);
    m_snap3DEdges->setChecked(settings.activeLayer3DEdges);
    m_showGuidePoints->setChecked(settings.showGuidePoints);
    m_showHints->setChecked(settings.showSnappingHints);
}

SnapSettingsPanel::SnapSettings SnapSettingsPanel::settings() const {
    SnapSettings s;
    s.grid = m_snapGrid->isChecked();
    s.sketchGuideLines = m_snapSketchLines->isChecked();
    s.sketchGuidePoints = m_snapSketchPoints->isChecked();
    s.activeLayer3DPoints = m_snap3DPoints->isChecked();
    s.activeLayer3DEdges = m_snap3DEdges->isChecked();
    s.showGuidePoints = m_showGuidePoints->isChecked();
    s.showSnappingHints = m_showHints->isChecked();
    return s;
}

void SnapSettingsPanel::mousePressEvent(QMouseEvent* event) {
    event->accept();
}

void SnapSettingsPanel::mouseReleaseEvent(QMouseEvent* event) {
    event->accept();
}

void SnapSettingsPanel::mouseMoveEvent(QMouseEvent* event) {
    event->accept();
}

void SnapSettingsPanel::wheelEvent(QWheelEvent* event) {
    event->accept();
}

} // namespace onecad::ui
