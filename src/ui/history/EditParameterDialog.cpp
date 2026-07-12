/**
 * @file EditParameterDialog.cpp
 * @brief Implementation of EditParameterDialog.
 */
#include "EditParameterDialog.h"
#include "../../app/commands/CommandProcessor.h"
#include "../../app/commands/UpdateOperationParamsCommand.h"
#include "../../app/document/Document.h"
#include "../../app/document/OperationRecord.h"
#include "../../app/history/RegenerationEngine.h"
#include "../../core/sketch/Sketch.h"
#include "../viewport/Viewport.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QCheckBox>
#include <QLoggingCategory>
#include <QSpinBox>

namespace onecad::ui {

Q_LOGGING_CATEGORY(logEditParamsDialog, "onecad.ui.history.editparams")

namespace {
constexpr int kDebounceMs = 100;
constexpr double kMinDistance = -10000.0;
constexpr double kMaxDistance = 10000.0;
constexpr double kMinAngle = -360.0;
constexpr double kMaxAngle = 360.0;
constexpr double kMinDraft = -89.0;
constexpr double kMaxDraft = 89.0;

std::unique_ptr<app::Document> makePreviewDocument(const app::Document& source) {
    auto preview = std::make_unique<app::Document>();

    preview->elementMap().fromString(source.elementMap().toString());

    for (const auto& sketchId : source.getSketchIds()) {
        const auto* sketch = source.getSketch(sketchId);
        if (!sketch) {
            continue;
        }
        auto clone = core::sketch::Sketch::fromJson(sketch->toJson());
        if (!clone) {
            continue;
        }
        preview->addSketchWithId(sketchId, std::move(clone), source.getSketchName(sketchId));
    }

    for (const auto& bodyId : source.getBodyIds()) {
        const TopoDS_Shape* shape = source.getBodyShape(bodyId);
        if (!shape || shape->IsNull()) {
            continue;
        }
        preview->addBodyWithId(bodyId, *shape, source.getBodyName(bodyId));
    }

    for (const auto& op : source.operations()) {
        preview->addOperation(op);
    }

    preview->setOperationSuppressionState(source.operationSuppressionState());
    preview->setBaseBodyIds(source.baseBodyIds());
    return preview;
}
} // namespace

EditParameterDialog::EditParameterDialog(app::Document* document,
                                         Viewport* viewport,
                                         app::commands::CommandProcessor* commandProcessor,
                                         const std::string& opId,
                                         QWidget* parent)
    : ModalOverlay(parent)
    , document_(document)
    , viewport_(viewport)
    , commandProcessor_(commandProcessor)
    , opId_(opId) {
    setTitle(tr("Edit Operation Parameters"));
    setCardMinimumWidth(340);

    // Setup debounce timer
    debounceTimer_ = new QTimer(this);
    debounceTimer_->setSingleShot(true);
    debounceTimer_->setInterval(kDebounceMs);
    connect(debounceTimer_, &QTimer::timeout, this, &EditParameterDialog::updatePreview);

    setupUi();
    loadCurrentParams();
}

EditParameterDialog::~EditParameterDialog() {
    clearPreview();
}

void EditParameterDialog::setupUi() {
    // Title and Ok/Cancel buttons are provided by ModalOverlay; only the
    // parameter form is added to the card body here.
    paramsLayout_ = new QVBoxLayout;
    addContentLayout(paramsLayout_);
}

void EditParameterDialog::loadCurrentParams() {
    if (!document_) return;

    // Find the operation
    for (const auto& op : document_->operations()) {
        if (op.opId == opId_) {
            if (op.type == app::OperationType::Extrude) {
                mode_ = Mode::Extrude;
                if (std::holds_alternative<app::ExtrudeParams>(op.params)) {
                    buildExtrudeUi(std::get<app::ExtrudeParams>(op.params));
                }
            } else if (op.type == app::OperationType::Revolve) {
                mode_ = Mode::Revolve;
                if (std::holds_alternative<app::RevolveParams>(op.params)) {
                    buildRevolveUi(std::get<app::RevolveParams>(op.params));
                }
            } else if (op.type == app::OperationType::LinearPattern) {
                mode_ = Mode::LinearPattern;
                if (std::holds_alternative<app::LinearPatternParams>(op.params)) {
                    buildLinearPatternUi(std::get<app::LinearPatternParams>(op.params));
                }
            }
            break;
        }
    }

    initializePreviewState();
}

bool EditParameterDialog::initializePreviewState() {
    if (!document_) {
        return false;
    }
    if (previewDocument_ && previewEngine_) {
        return true;
    }

    previewDocument_ = makePreviewDocument(*document_);
    previewEngine_ = std::make_unique<app::history::RegenerationEngine>(previewDocument_.get());
    return previewDocument_ != nullptr && previewEngine_ != nullptr;
}

void EditParameterDialog::buildExtrudeUi(const app::ExtrudeParams& params) {
    auto* formLayout = new QFormLayout;

    // Distance spinbox
    distanceSpinbox_ = new QDoubleSpinBox;
    distanceSpinbox_->setRange(kMinDistance, kMaxDistance);
    distanceSpinbox_->setValue(params.distance);
    distanceSpinbox_->setSuffix(" mm");
    distanceSpinbox_->setDecimals(2);
    distanceSpinbox_->setSingleStep(1.0);
    connect(distanceSpinbox_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &EditParameterDialog::onValueChanged);
    formLayout->addRow(tr("Distance:"), distanceSpinbox_);

    // Draft angle spinbox
    draftAngleSpinbox_ = new QDoubleSpinBox;
    draftAngleSpinbox_->setRange(kMinDraft, kMaxDraft);
    draftAngleSpinbox_->setValue(params.draftAngleDeg);
    draftAngleSpinbox_->setSuffix("°");
    draftAngleSpinbox_->setDecimals(1);
    draftAngleSpinbox_->setSingleStep(1.0);
    connect(draftAngleSpinbox_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &EditParameterDialog::onValueChanged);
    formLayout->addRow(tr("Draft Angle:"), draftAngleSpinbox_);

    // End condition (direction 1). Order must match indexToExtrudeMode below.
    extrudeModeCombo_ = new QComboBox;
    extrudeModeCombo_->addItem(tr("Blind"));
    extrudeModeCombo_->addItem(tr("Through All"));
    extrudeModeCombo_->addItem(tr("Symmetric"));
    extrudeModeCombo_->addItem(tr("To Next"));
    extrudeModeCombo_->addItem(tr("To Face"));
    extrudeModeCombo_->setCurrentIndex(static_cast<int>(params.extrudeMode));
    connect(extrudeModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { onValueChanged(); });
    formLayout->addRow(tr("End Condition:"), extrudeModeCombo_);

    paramsLayout_->addLayout(formLayout);
}

void EditParameterDialog::buildRevolveUi(const app::RevolveParams& params) {
    auto* formLayout = new QFormLayout;

    // Angle spinbox
    angleSpinbox_ = new QDoubleSpinBox;
    angleSpinbox_->setRange(kMinAngle, kMaxAngle);
    angleSpinbox_->setValue(params.angleDeg);
    angleSpinbox_->setSuffix("°");
    angleSpinbox_->setDecimals(1);
    angleSpinbox_->setSingleStep(15.0);
    connect(angleSpinbox_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &EditParameterDialog::onValueChanged);
    formLayout->addRow(tr("Angle:"), angleSpinbox_);

    paramsLayout_->addLayout(formLayout);
}

void EditParameterDialog::buildLinearPatternUi(const app::LinearPatternParams& params) {
    auto* formLayout = new QFormLayout;

    spacingSpinbox_ = new QDoubleSpinBox;
    spacingSpinbox_->setRange(kMinDistance, kMaxDistance);
    spacingSpinbox_->setValue(params.spacing);
    spacingSpinbox_->setSuffix(" mm");
    spacingSpinbox_->setDecimals(3);
    spacingSpinbox_->setSingleStep(1.0);
    connect(spacingSpinbox_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &EditParameterDialog::onValueChanged);
    formLayout->addRow(tr("Spacing:"), spacingSpinbox_);

    countSpinbox_ = new QSpinBox;
    countSpinbox_->setRange(2, 64);
    countSpinbox_->setValue(params.count);
    connect(countSpinbox_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &EditParameterDialog::onValueChanged);
    formLayout->addRow(tr("Count:"), countSpinbox_);

    fuseCheck_ = new QCheckBox(tr("Fuse result"));
    fuseCheck_->setChecked(params.fuseResult);
    connect(fuseCheck_, &QCheckBox::toggled, this, [this](bool) { onValueChanged(); });
    formLayout->addRow(QString(), fuseCheck_);

    auto* sourceLabel = new QLabel(QString::fromStdString(params.sourceBodyId));
    sourceLabel->setWordWrap(true);
    formLayout->addRow(tr("Source Body:"), sourceLabel);

    auto* directionLabel = new QLabel(
        tr("(%1, %2, %3)")
            .arg(params.dirX, 0, 'f', 2)
            .arg(params.dirY, 0, 'f', 2)
            .arg(params.dirZ, 0, 'f', 2));
    directionLabel->setWordWrap(true);
    formLayout->addRow(tr("Direction:"), directionLabel);

    paramsLayout_->addLayout(formLayout);
}

app::ExtrudeParams EditParameterDialog::getExtrudeParams() const {
    // Start from the original params so non-edited fields (boolean mode, target body/face,
    // two-direction settings) are preserved, then override the editable controls.
    app::ExtrudeParams params;
    if (document_) {
        for (const auto& op : document_->operations()) {
            if (op.opId == opId_ && std::holds_alternative<app::ExtrudeParams>(op.params)) {
                params = std::get<app::ExtrudeParams>(op.params);
                break;
            }
        }
    }
    if (distanceSpinbox_) {
        params.distance = distanceSpinbox_->value();
    }
    if (draftAngleSpinbox_) {
        params.draftAngleDeg = draftAngleSpinbox_->value();
    }
    if (extrudeModeCombo_) {
        params.extrudeMode = static_cast<app::ExtrudeMode>(extrudeModeCombo_->currentIndex());
    }
    return params;
}

app::RevolveParams EditParameterDialog::getRevolveParams() const {
    app::RevolveParams params;
    params.angleDeg = angleSpinbox_ ? angleSpinbox_->value() : 360.0;
    params.booleanMode = app::BooleanMode::NewBody;
    params.targetBodyId.clear();

    // Find original to preserve boolean mode, axis, and target body
    if (document_) {
        for (const auto& op : document_->operations()) {
            if (op.opId == opId_ && std::holds_alternative<app::RevolveParams>(op.params)) {
                const auto& orig = std::get<app::RevolveParams>(op.params);
                params.booleanMode = orig.booleanMode;
                params.axis = orig.axis;
                params.targetBodyId = orig.targetBodyId;
                qCDebug(logEditParamsDialog) << "getRevolveParams:preserved-target"
                                             << QString::fromStdString(params.targetBodyId);
                break;
            }
        }
    }
    return params;
}

app::LinearPatternParams EditParameterDialog::getLinearPatternParams() const {
    app::LinearPatternParams params;
    params.spacing = spacingSpinbox_ ? spacingSpinbox_->value() : 10.0;
    params.count = countSpinbox_ ? countSpinbox_->value() : 2;
    params.fuseResult = fuseCheck_ ? fuseCheck_->isChecked() : true;

    if (document_) {
        for (const auto& op : document_->operations()) {
            if (op.opId == opId_ && std::holds_alternative<app::LinearPatternParams>(op.params)) {
                const auto& orig = std::get<app::LinearPatternParams>(op.params);
                params.sourceBodyId = orig.sourceBodyId;
                params.dirX = orig.dirX;
                params.dirY = orig.dirY;
                params.dirZ = orig.dirZ;
                break;
            }
        }
    }
    return params;
}

void EditParameterDialog::onValueChanged() {
    hasChanges_ = true;
    previewValid_ = true;
    previewError_.clear();
    clearError();
    debounceTimer_->start();
}

void EditParameterDialog::updatePreview() {
    if (!document_ || !viewport_) return;
    qCDebug(logEditParamsDialog) << "updatePreview:start"
                                 << "opId=" << QString::fromStdString(opId_);

    // Create temporary params variant
    app::OperationParams newParams;
    switch (mode_) {
        case Mode::Extrude:
            newParams = getExtrudeParams();
            break;
        case Mode::Revolve:
            newParams = getRevolveParams();
            break;
        case Mode::LinearPattern:
            newParams = getLinearPatternParams();
            break;
    }

    if (!initializePreviewState()) {
        previewValid_ = false;
        previewError_ = tr("Could not initialize preview.");
        clearPreview();
        return;
    }

    auto* op = previewDocument_->findOperation(opId_);
    if (!op) {
        qCWarning(logEditParamsDialog) << "updatePreview:operation-not-found"
                                        << QString::fromStdString(opId_);
        previewValid_ = false;
        previewError_ = tr("Operation not found.");
        clearPreview();
        return;
    }
    op->params = newParams;

    auto result = previewEngine_->regenerateAll();
    if (result.status != app::history::RegenStatus::Success) {
        previewValid_ = false;
        if (!result.failedOps.empty()) {
            previewError_ = QString::fromStdString(result.failedOps.front().errorMessage);
        } else {
            previewError_ = tr("Regeneration failed.");
        }
        qCWarning(logEditParamsDialog) << "updatePreview:regeneration-failure"
                                        << "opId=" << QString::fromStdString(opId_);
        clearPreview();
        return;
    }

    const std::vector<render::SceneMeshStore::Mesh> meshes = previewDocument_->meshStore().meshes();
    viewport_->setModelPreviewMeshes(meshes);
    previewValid_ = true;
    previewError_.clear();
    qCDebug(logEditParamsDialog) << "updatePreview:done"
                                 << "opId=" << QString::fromStdString(opId_)
                                 << "meshCount=" << meshes.size();

    emit previewRequested(QString::fromStdString(opId_));
}

void EditParameterDialog::clearPreview() {
    if (viewport_) {
        viewport_->clearModelPreviewMeshes();
    }
}

bool EditParameterDialog::onAccept() {
    // Guard against the operation being deleted while the overlay was open.
    if (document_ && !document_->findOperation(opId_)) {
        setError(tr("Operation no longer exists."));
        return false;
    }
    if (!previewValid_) {
        setError(previewError_.isEmpty() ? tr("Preview failed.") : previewError_);
        return false;
    }
    if (hasChanges_) {
        if (!applyChanges()) {
            setError(previewError_.isEmpty() ? tr("Regeneration failed.") : previewError_);
            return false;
        }
    }
    clearPreview();
    return true;
}

void EditParameterDialog::onReject() {
    clearPreview();
}

bool EditParameterDialog::applyChanges() {
    if (!document_) return false;

    // Update operation params
    app::OperationParams newParams;
    switch (mode_) {
        case Mode::Extrude:
            newParams = getExtrudeParams();
            break;
        case Mode::Revolve:
            newParams = getRevolveParams();
            break;
        case Mode::LinearPattern:
            newParams = getLinearPatternParams();
            break;
    }

    auto command = std::make_unique<app::commands::UpdateOperationParamsCommand>(
        document_, opId_, newParams);
    bool success = false;
    if (commandProcessor_) {
        success = commandProcessor_->execute(std::move(command));
    } else {
        success = command->execute();
    }

    if (!success) {
        qCWarning(logEditParamsDialog) << "applyChanges:failed"
                                       << "opId=" << QString::fromStdString(opId_);
        return false;
    }

    hasChanges_ = false;
    emit parametersChanged(QString::fromStdString(opId_));
    return true;
}

} // namespace onecad::ui
