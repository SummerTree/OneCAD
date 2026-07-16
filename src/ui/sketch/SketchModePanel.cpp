#include "SketchModePanel.h"
#include "../../core/sketch/Sketch.h"
#include "../../core/sketch/SketchTypes.h"
#include "../components/IconLoader.h"
#include "../theme/ThemeManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QFrame>
#include <QWheelEvent>
#include <QIcon>
#include <QPalette>

namespace onecad::ui {

namespace {
// Constraint type → two-tone SVG icon resource (see resources/icons/DESIGN.md).
QString constraintIconPath(core::sketch::ConstraintType t) {
    using CT = core::sketch::ConstraintType;
    switch (t) {
        case CT::Horizontal:         return QStringLiteral(":/icons/ic_constraint_horizontal.svg");
        case CT::Vertical:           return QStringLiteral(":/icons/ic_constraint_vertical.svg");
        case CT::Parallel:           return QStringLiteral(":/icons/ic_constraint_parallel.svg");
        case CT::Perpendicular:      return QStringLiteral(":/icons/ic_constraint_perpendicular.svg");
        case CT::Tangent:            return QStringLiteral(":/icons/ic_constraint_tangent.svg");
        case CT::Concentric:         return QStringLiteral(":/icons/ic_constraint_concentric.svg");
        case CT::Coincident:         return QStringLiteral(":/icons/ic_constraint_coincident.svg");
        case CT::Equal:              return QStringLiteral(":/icons/ic_constraint_equal.svg");
        case CT::Midpoint:           return QStringLiteral(":/icons/ic_constraint_midpoint.svg");
        case CT::Symmetric:          return QStringLiteral(":/icons/ic_constraint_symmetric.svg");
        case CT::OnCurve:            return QStringLiteral(":/icons/ic_constraint_point_on_curve.svg");
        case CT::Distance:           return QStringLiteral(":/icons/ic_constraint_distance.svg");
        case CT::HorizontalDistance: return QStringLiteral(":/icons/ic_constraint_distance_h.svg");
        case CT::VerticalDistance:   return QStringLiteral(":/icons/ic_constraint_distance_v.svg");
        case CT::Angle:              return QStringLiteral(":/icons/ic_constraint_angle.svg");
        case CT::Radius:             return QStringLiteral(":/icons/ic_constraint_radius.svg");
        case CT::Diameter:           return QStringLiteral(":/icons/ic_constraint_diameter.svg");
        case CT::Fixed:              return QStringLiteral(":/icons/ic_constraint_fix.svg");
        default:                     return QString();
    }
}
} // namespace

SketchModePanel::SketchModePanel(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::ClickFocus);
    setupUi();
}

void SketchModePanel::mousePressEvent(QMouseEvent* event) {
    event->accept();
}

void SketchModePanel::mouseReleaseEvent(QMouseEvent* event) {
    event->accept();
}

void SketchModePanel::mouseMoveEvent(QMouseEvent* event) {
    event->accept();
}

void SketchModePanel::wheelEvent(QWheelEvent* event) {
    event->accept();
}

void SketchModePanel::setupUi() {
    setObjectName("ConstraintToolsPanel");
    setWindowFlag(Qt::FramelessWindowHint, true);
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedWidth(180);

    // Container styling (background, border, radius) from ThemeManager#ConstraintToolsPanel
    setStyleSheet(R"(
        QLabel#title {
            font-weight: bold;
            font-size: 11px;
            padding: 8px;
            color: palette(text);
        }
        QLabel#section {
            font-size: 10px;
            padding: 4px 8px;
            color: palette(disabled, text);
        }
        QPushButton {
            text-align: left;
            padding: 6px 8px;
            border: none;
            border-radius: 3px;
            font-size: 11px;
        }
        QPushButton:hover {
            background-color: palette(midlight);
        }
        QPushButton:pressed {
            background-color: palette(highlight);
            color: palette(highlighted-text);
        }
        QPushButton:disabled {
            color: palette(disabled, text);
        }
        QFrame#separator {
            background-color: palette(mid);
            max-height: 1px;
            margin: 4px 8px;
        }
    )");

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(8, 8, 8, 8);
    m_layout->setSpacing(2);

    // Title
    m_titleLabel = new QLabel(tr("Constraint tools"), this);
    m_titleLabel->setObjectName("title");
    m_layout->addWidget(m_titleLabel);

    // Geometric constraints section
    QLabel* geoLabel = new QLabel(tr("Geometric"), this);
    geoLabel->setObjectName("section");
    m_layout->addWidget(geoLabel);

    using CT = core::sketch::ConstraintType;

    // Define constraint buttons (type, name, shortcut). Icons resolve from type.
    std::vector<std::tuple<CT, QString, QString>> geometricConstraints = {
        {CT::Horizontal, tr("Horizontal"), "H"},
        {CT::Vertical, tr("Vertical"), "V"},
        {CT::Parallel, tr("Parallel"), "P"},
        {CT::Perpendicular, tr("Perpendicular"), "N"},
        {CT::Tangent, tr("Tangent"), "T"},
        {CT::Concentric, tr("Concentric"), ""},
        {CT::Coincident, tr("Coincident"), "C"},
        {CT::Equal, tr("Equal"), "E"},
        {CT::Midpoint, tr("Midpoint"), "M"},
        {CT::Symmetric, tr("Symmetric"), ""},
        {CT::OnCurve, tr("Point On Curve"), ""},
    };

    for (const auto& [type, name, shortcut] : geometricConstraints) {
        const QString tooltip = shortcut.isEmpty()
            ? tr("Apply %1 constraint").arg(name)
            : tr("Apply %1 constraint [%2]").arg(name, shortcut);
        const QString iconPath = constraintIconPath(type);
        QPushButton* btn = createButton(iconPath, name, shortcut, tooltip);
        m_constraintButtons.push_back({type, iconPath, name, shortcut, tooltip, btn});
        m_layout->addWidget(btn);

        connect(btn, &QPushButton::clicked, this, [this, type]() {
            emit constraintRequested(type);
        });
    }

    // Separator
    QFrame* sep1 = new QFrame(this);
    sep1->setObjectName("separator");
    sep1->setFrameShape(QFrame::HLine);
    m_layout->addWidget(sep1);

    // Dimensional constraints section
    QLabel* dimLabel = new QLabel(tr("Dimensional"), this);
    dimLabel->setObjectName("section");
    m_layout->addWidget(dimLabel);

    std::vector<std::tuple<CT, QString, QString>> dimensionalConstraints = {
        {CT::Distance, tr("Distance"), "D"},
        {CT::HorizontalDistance, tr("H-Distance"), ""},
        {CT::VerticalDistance, tr("V-Distance"), ""},
        {CT::Angle, tr("Angle"), "A"},
        {CT::Radius, tr("Radius"), "R"},
        {CT::Diameter, tr("Diameter"), ""},
    };

    for (const auto& [type, name, shortcut] : dimensionalConstraints) {
        QString tooltip = shortcut.isEmpty()
            ? tr("Apply %1 constraint").arg(name)
            : tr("Apply %1 constraint [%2]").arg(name, shortcut);
        const QString iconPath = constraintIconPath(type);
        QPushButton* btn = createButton(iconPath, name, shortcut, tooltip);
        m_constraintButtons.push_back({type, iconPath, name, shortcut, tooltip, btn});
        m_layout->addWidget(btn);

        connect(btn, &QPushButton::clicked, this, [this, type]() {
            emit constraintRequested(type);
        });
    }

    // Separator
    QFrame* sep2 = new QFrame(this);
    sep2->setObjectName("separator");
    sep2->setFrameShape(QFrame::HLine);
    m_layout->addWidget(sep2);

    // Fixed constraint
    const QString fixPath = constraintIconPath(CT::Fixed);
    QPushButton* fixBtn = createButton(fixPath, tr("Lock/Fix"), "F",
                                       tr("Fix selected point [F]"));
    m_constraintButtons.push_back({CT::Fixed, fixPath,
                                   tr("Lock/Fix"), "F", tr("Fix selected point [F]"), fixBtn});
    m_layout->addWidget(fixBtn);

    connect(fixBtn, &QPushButton::clicked, this, [this]() {
        emit constraintRequested(CT::Fixed);
    });

    m_layout->addStretch();

    // Recolor constraint icons when the theme or icon preset changes.
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &SketchModePanel::updateConstraintIcons, Qt::UniqueConnection);
}

QPushButton* SketchModePanel::createButton(const QString& iconPath, const QString& name,
                                            const QString& shortcut, const QString& tooltip) {
    QString text = name;
    if (!shortcut.isEmpty()) {
        text += QString("  [%1]").arg(shortcut);
    }

    QPushButton* btn = new QPushButton(text, this);
    btn->setToolTip(tooltip);
    btn->setCursor(Qt::PointingHandCursor);
    if (!iconPath.isEmpty()) {
        btn->setIcon(IconLoader::load(iconPath, palette().color(QPalette::ButtonText), 18));
        btn->setIconSize(QSize(18, 18));
    }
    return btn;
}

void SketchModePanel::updateConstraintIcons() {
    const QColor primary = palette().color(QPalette::ButtonText);
    for (auto& cb : m_constraintButtons) {
        if (cb.button && !cb.icon.isEmpty()) {
            cb.button->setIcon(IconLoader::load(cb.icon, primary, 18));
        }
    }
}

void SketchModePanel::setSketch(core::sketch::Sketch* sketch) {
    m_sketch = sketch;
    updateButtonStates();
}

void SketchModePanel::setApplicableConstraints(
    const std::unordered_set<core::sketch::ConstraintType>& applicableConstraints) {
    m_applicableConstraints = applicableConstraints;
    updateButtonStates();
}

void SketchModePanel::updateButtonStates() {
    for (auto& cb : m_constraintButtons) {
        if (cb.button) {
            const bool enabled = (m_sketch != nullptr) &&
                                 (m_applicableConstraints.find(cb.type) != m_applicableConstraints.end());
            cb.button->setEnabled(enabled);
            cb.button->setToolTip(enabled
                ? cb.tooltip
                : tr("Not available for the current selection"));
        }
    }
}

} // namespace onecad::ui
