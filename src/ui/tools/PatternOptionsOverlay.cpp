#include "PatternOptionsOverlay.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

namespace onecad::ui::tools {

PatternOptionsOverlay::PatternOptionsOverlay(QWidget* parent)
    : QFrame(parent) {
    setObjectName("PatternOptionsOverlay");
    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Raised);
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(true);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    auto* title = new QLabel(tr("Linear Pattern"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    mainLayout->addWidget(title);

    auto* form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(6);

    sourceValue_ = new QLabel(this);
    sourceValue_->setWordWrap(true);
    form->addRow(tr("Source"), sourceValue_);

    directionValue_ = new QLabel(this);
    directionValue_->setWordWrap(true);
    form->addRow(tr("Direction"), directionValue_);

    spacingSpin_ = new QDoubleSpinBox(this);
    spacingSpin_->setRange(-100000.0, 100000.0);
    spacingSpin_->setDecimals(3);
    spacingSpin_->setSingleStep(1.0);
    spacingSpin_->setSuffix(tr(" mm"));
    form->addRow(tr("Spacing"), spacingSpin_);

    countSpin_ = new QSpinBox(this);
    countSpin_->setRange(2, 64);
    form->addRow(tr("Count"), countSpin_);

    fuseCheck_ = new QCheckBox(tr("Fuse result"), this);
    form->addRow(QString(), fuseCheck_);

    mainLayout->addLayout(form);

    hintLabel_ = new QLabel(this);
    hintLabel_->setWordWrap(true);
    hintLabel_->setText(tr("Click an edge or a planar face on the source body to set direction."));
    mainLayout->addWidget(hintLabel_);

    connect(spacingSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PatternOptionsOverlay::spacingChanged);
    connect(countSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PatternOptionsOverlay::countChanged);
    connect(fuseCheck_, &QCheckBox::toggled, this, &PatternOptionsOverlay::fuseChanged);
}

void PatternOptionsOverlay::setSourceLabel(const QString& text) {
    if (sourceValue_) {
        sourceValue_->setText(text);
    }
}

void PatternOptionsOverlay::setDirectionLabel(const QString& text) {
    if (directionValue_) {
        directionValue_->setText(text);
    }
}

void PatternOptionsOverlay::setSpacingValue(double spacing) {
    if (!spacingSpin_) {
        return;
    }
    const QSignalBlocker blocker(spacingSpin_);
    spacingSpin_->setValue(spacing);
}

void PatternOptionsOverlay::setCountValue(int count) {
    if (!countSpin_) {
        return;
    }
    const QSignalBlocker blocker(countSpin_);
    countSpin_->setValue(count);
}

void PatternOptionsOverlay::setFuseResult(bool fuse) {
    if (!fuseCheck_) {
        return;
    }
    const QSignalBlocker blocker(fuseCheck_);
    fuseCheck_->setChecked(fuse);
}

void PatternOptionsOverlay::setDirectionReady(bool ready) {
    if (spacingSpin_) {
        spacingSpin_->setEnabled(ready);
    }
    if (countSpin_) {
        countSpin_->setEnabled(ready);
    }
    if (fuseCheck_) {
        fuseCheck_->setEnabled(ready);
    }
    if (hintLabel_) {
        hintLabel_->setVisible(!ready);
    }
}

} // namespace onecad::ui::tools
