#include "PropertyInspector.h"
#include "MassPropertiesPanel.h"
#include "../../app/document/Document.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace onecad {
namespace ui {

namespace {
// Defer the heavy mass-properties compute so rapid selection changes coalesce.
constexpr int kMassPropsDebounceMs = 120;
} // namespace

PropertyInspector::PropertyInspector(QWidget* parent)
    : QDockWidget(tr("Inspector"), parent) {
    m_massPropsTimer = new QTimer(this);
    m_massPropsTimer->setSingleShot(true);
    m_massPropsTimer->setInterval(kMassPropsDebounceMs);
    connect(m_massPropsTimer, &QTimer::timeout, this, [this]() { computeMassPropsNow(); });

    setupUi();
    showEmptyState();
    setMinimumWidth(250);
    setMaximumWidth(400);
}

void PropertyInspector::setDocument(app::Document* document) {
    m_document = document;
}

void PropertyInspector::setupUi() {
    auto* container = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    m_stackedWidget = new QStackedWidget(container);

    createEmptyStateWidget();
    createBodyWidget();
    createOperationWidget();

    m_stackedWidget->addWidget(m_emptyWidget);
    m_stackedWidget->addWidget(m_bodyWidget);
    m_stackedWidget->addWidget(m_operationWidget);

    mainLayout->addWidget(m_stackedWidget);
    setWidget(container);
}

void PropertyInspector::createEmptyStateWidget() {
    m_emptyWidget = new QWidget;
    auto* layout = new QVBoxLayout(m_emptyWidget);

    auto* titleLabel = new QLabel(tr("No Selection"));
    titleLabel->setAlignment(Qt::AlignCenter);

    auto* hintLabel = new QLabel(tr("Select a body or operation\nto view its properties"));
    hintLabel->setAlignment(Qt::AlignCenter);

    layout->addStretch();
    layout->addWidget(titleLabel);
    layout->addWidget(hintLabel);
    layout->addStretch();
}

void PropertyInspector::createBodyWidget() {
    m_bodyWidget = new QWidget;
    auto* layout = new QVBoxLayout(m_bodyWidget);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // Editable body properties (routed through the owner as undoable commands).
    auto* form = new QFormLayout;
    m_bodyNameEdit = new QLineEdit;
    m_bodyNameEdit->setPlaceholderText(tr("Body name"));
    connect(m_bodyNameEdit, &QLineEdit::editingFinished, this, [this]() {
        if (!m_currentBodyId.isEmpty()) {
            const QString newName = m_bodyNameEdit->text().trimmed();
            if (!newName.isEmpty()) {
                emit bodyRenameRequested(m_currentBodyId, newName);
            }
        }
    });
    form->addRow(tr("Name:"), m_bodyNameEdit);

    m_bodyVisibleCheck = new QCheckBox(tr("Visible"));
    connect(m_bodyVisibleCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (!m_currentBodyId.isEmpty()) {
            emit bodyVisibilityChangeRequested(m_currentBodyId, checked);
        }
    });
    form->addRow(QString(), m_bodyVisibleCheck);
    layout->addLayout(form);

    m_massProps = new MassPropertiesPanel;
    layout->addWidget(m_massProps);

    layout->addStretch();
}

void PropertyInspector::createOperationWidget() {
    m_operationWidget = new QWidget;
    auto* layout = new QVBoxLayout(m_operationWidget);
    layout->setContentsMargins(8, 8, 8, 8);

    m_opTypeLabel = new QLabel;
    m_opIdLabel = new QLabel;
    m_opParamsLabel = new QLabel;
    m_opParamsLabel->setWordWrap(true);

    layout->addWidget(m_opTypeLabel);
    layout->addWidget(m_opIdLabel);
    layout->addSpacing(8);
    layout->addWidget(m_opParamsLabel);
    layout->addStretch();
}

void PropertyInspector::showEmptyState() {
    m_currentBodyId.clear();
    m_currentOpId.clear();
    m_massPropsTimer->stop();
    m_stackedWidget->setCurrentWidget(m_emptyWidget);
}

void PropertyInspector::showBodyProperties(const QString& bodyId) {
    if (!m_document) {
        showEmptyState();
        return;
    }

    std::string id = bodyId.toStdString();
    const auto* shape = m_document->getBodyShape(id);
    if (!shape) {
        showEmptyState();
        return;
    }

    m_currentBodyId = bodyId;
    m_currentOpId.clear();

    QString name = QString::fromStdString(m_document->getBodyName(id));
    if (name.isEmpty()) {
        name = bodyId;
    }
    // Populate editors immediately without re-emitting change signals.
    if (m_bodyNameEdit) {
        QSignalBlocker block(m_bodyNameEdit);
        m_bodyNameEdit->setText(name);
    }
    if (m_bodyVisibleCheck) {
        QSignalBlocker block(m_bodyVisibleCheck);
        m_bodyVisibleCheck->setChecked(m_document->isBodyVisible(id));
    }

    // Switch view immediately for responsiveness; defer the heavy compute.
    m_stackedWidget->setCurrentWidget(m_bodyWidget);
    m_massProps->clear();
    m_massPropsTimer->start();
}

void PropertyInspector::computeMassPropsNow() {
    if (!m_document || m_currentBodyId.isEmpty() || !m_massProps) {
        return;
    }
    const auto* shape = m_document->getBodyShape(m_currentBodyId.toStdString());
    if (!shape) {
        showEmptyState();
        return;
    }
    QString name = QString::fromStdString(m_document->getBodyName(m_currentBodyId.toStdString()));
    if (name.isEmpty()) {
        name = m_currentBodyId;
    }
    m_massProps->updateForShape(*shape, name);
}

void PropertyInspector::refresh() {
    if (!m_currentBodyId.isEmpty()) {
        showBodyProperties(m_currentBodyId);
    } else if (!m_currentOpId.isEmpty()) {
        showOperationProperties(m_currentOpId);
    }
}

void PropertyInspector::showOperationProperties(const QString& opId) {
    if (!m_document) {
        showEmptyState();
        return;
    }

    const auto* op = m_document->findOperation(opId.toStdString());
    if (!op) {
        showEmptyState();
        return;
    }

    m_currentOpId = opId;
    m_currentBodyId.clear();
    m_massPropsTimer->stop();

    m_opTypeLabel->setText(
        tr("Operation: %1").arg(QString::fromLatin1(app::operationTypeName(op->type))));
    m_opIdLabel->setText(tr("ID: %1").arg(opId));

    // Build params summary
    QString params;
    if (std::holds_alternative<app::ExtrudeParams>(op->params)) {
        const auto& p = std::get<app::ExtrudeParams>(op->params);
        params = tr("Distance: %1 mm\nDraft: %2\nMode: %3")
                     .arg(p.distance, 0, 'f', 3)
                     .arg(p.draftAngleDeg, 0, 'f', 1)
                     .arg(QString::fromLatin1(app::booleanModeName(p.booleanMode)));
    } else if (std::holds_alternative<app::RevolveParams>(op->params)) {
        const auto& p = std::get<app::RevolveParams>(op->params);
        params = tr("Angle: %1\nMode: %2")
                     .arg(p.angleDeg, 0, 'f', 1)
                     .arg(QString::fromLatin1(app::booleanModeName(p.booleanMode)));
    } else if (std::holds_alternative<app::FilletChamferParams>(op->params)) {
        const auto& p = std::get<app::FilletChamferParams>(op->params);
        params = tr("%1: %2 mm\nEdges: %3")
                     .arg(p.mode == app::FilletChamferParams::Mode::Fillet ? tr("Fillet") : tr("Chamfer"))
                     .arg(p.radius, 0, 'f', 3)
                     .arg(p.edgeIds.size());
    } else if (std::holds_alternative<app::ShellParams>(op->params)) {
        const auto& p = std::get<app::ShellParams>(op->params);
        params = tr("Thickness: %1 mm\nOpen faces: %2")
                     .arg(p.thickness, 0, 'f', 3)
                     .arg(p.openFaceIds.size());
    } else if (std::holds_alternative<app::LinearPatternParams>(op->params)) {
        const auto& p = std::get<app::LinearPatternParams>(op->params);
        params = tr("Count: %1\nSpacing: %2 mm\nDirection: (%3, %4, %5)")
                     .arg(p.count)
                     .arg(p.spacing, 0, 'f', 3)
                     .arg(p.dirX, 0, 'f', 2)
                     .arg(p.dirY, 0, 'f', 2)
                     .arg(p.dirZ, 0, 'f', 2);
    } else if (std::holds_alternative<app::CircularPatternParams>(op->params)) {
        const auto& p = std::get<app::CircularPatternParams>(op->params);
        params = tr("Count: %1\nAngle: %2\nAxis: (%3, %4, %5)")
                     .arg(p.count)
                     .arg(p.angleDeg, 0, 'f', 1)
                     .arg(p.axisDirX, 0, 'f', 2)
                     .arg(p.axisDirY, 0, 'f', 2)
                     .arg(p.axisDirZ, 0, 'f', 2);
    } else if (std::holds_alternative<app::LoftParams>(op->params)) {
        const auto& p = std::get<app::LoftParams>(op->params);
        params = tr("Sections: %1\nSolid: %2\nRuled: %3")
                     .arg(p.profileSketchIds.size())
                     .arg(p.isSolid ? tr("Yes") : tr("No"))
                     .arg(p.isRuled ? tr("Yes") : tr("No"));
    } else if (std::holds_alternative<app::SweepParams>(op->params)) {
        params = tr("Profile + Path sweep");
    } else if (std::holds_alternative<app::MirrorBodyParams>(op->params)) {
        const auto& p = std::get<app::MirrorBodyParams>(op->params);
        params = tr("Normal: (%1, %2, %3)\nFuse: %4")
                     .arg(p.planeNormalX, 0, 'f', 2)
                     .arg(p.planeNormalY, 0, 'f', 2)
                     .arg(p.planeNormalZ, 0, 'f', 2)
                     .arg(p.fuseWithOriginal ? tr("Yes") : tr("No"));
    }
    m_opParamsLabel->setText(params);
    m_stackedWidget->setCurrentWidget(m_operationWidget);
}

void PropertyInspector::onSelectionChanged() {
    showEmptyState();
}

} // namespace ui
} // namespace onecad
