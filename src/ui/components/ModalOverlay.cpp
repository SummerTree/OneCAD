/**
 * @file ModalOverlay.cpp
 * @brief Implementation of ModalOverlay.
 */
#include "ModalOverlay.h"
#include "../theme/ThemeManager.h"
#include "../theme/ThemeConfig.h"

#include <QAbstractAnimation>
#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QEasingCurve>
#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QShowEvent>
#include <QStyle>
#include <QVBoxLayout>

namespace onecad::ui {

namespace {
constexpr int kDefaultCardWidth = 340;
constexpr int kFadeMs = 180;
} // namespace

ModalOverlay::ModalOverlay(QWidget* parent)
    : QWidget(parent) {
    setObjectName("ModalOverlay");
    setAutoFillBackground(true);
    setAttribute(Qt::WA_StyledBackground, true);
    setFocusPolicy(Qt::StrongFocus);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(48, 48, 48, 48);
    rootLayout->setSpacing(0);
    rootLayout->addStretch();

    panel_ = new QWidget(this);
    panel_->setObjectName("modalCard");
    panel_->setMinimumWidth(kDefaultCardWidth);
    panel_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

    auto* panelLayout = new QVBoxLayout(panel_);
    panelLayout->setContentsMargins(24, 20, 24, 20);
    panelLayout->setSpacing(14);

    titleLabel_ = new QLabel(panel_);
    titleLabel_->setObjectName("modalTitle");
    titleLabel_->hide();
    panelLayout->addWidget(titleLabel_);

    contentLayout_ = new QVBoxLayout;
    contentLayout_->setContentsMargins(0, 0, 0, 0);
    contentLayout_->setSpacing(8);
    panelLayout->addLayout(contentLayout_);

    errorLabel_ = new QLabel(panel_);
    errorLabel_->setObjectName("modalError");
    errorLabel_->setWordWrap(true);
    errorLabel_->hide();
    panelLayout->addWidget(errorLabel_);

    buttonBox_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, panel_);
    // Differentiate the primary (accept) vs. ghost (cancel) actions via the global
    // QPushButton[primary]/[ghost] theme rules.
    if (auto* okButton = buttonBox_->button(QDialogButtonBox::Ok)) {
        okButton->setProperty("primary", true);
    }
    if (auto* cancelButton = buttonBox_->button(QDialogButtonBox::Cancel)) {
        cancelButton->setProperty("ghost", true);
    }
    connect(buttonBox_, &QDialogButtonBox::accepted, this, &ModalOverlay::accept);
    connect(buttonBox_, &QDialogButtonBox::rejected, this, &ModalOverlay::reject);
    panelLayout->addWidget(buttonBox_);

    rootLayout->addWidget(panel_, 0, Qt::AlignHCenter);
    rootLayout->addStretch();

    auto* panelOpacity = new QGraphicsOpacityEffect(panel_);
    panel_->setGraphicsEffect(panelOpacity);
    panelOpacity->setOpacity(0.0);

    if (parent) {
        parent->installEventFilter(this);
    }

    themeConnection_ = connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
                               this, &ModalOverlay::applyTheme, Qt::UniqueConnection);
    applyTheme();
}

ModalOverlay::~ModalOverlay() {
    QObject::disconnect(themeConnection_);
}

void ModalOverlay::setTitle(const QString& title) {
    titleLabel_->setText(title);
    titleLabel_->setVisible(!title.isEmpty());
}

void ModalOverlay::setCardMinimumWidth(int width) {
    panel_->setMinimumWidth(width);
}

void ModalOverlay::addContentWidget(QWidget* widget) {
    contentLayout_->addWidget(widget);
}

void ModalOverlay::addContentLayout(QLayout* layout) {
    contentLayout_->addLayout(layout);
}

void ModalOverlay::setAcceptText(const QString& text) {
    if (auto* button = buttonBox_->button(QDialogButtonBox::Ok)) {
        button->setText(text);
    }
}

void ModalOverlay::setRejectText(const QString& text) {
    if (auto* button = buttonBox_->button(QDialogButtonBox::Cancel)) {
        button->setText(text);
    }
}

void ModalOverlay::setError(const QString& message) {
    errorLabel_->setText(message);
    errorLabel_->setVisible(!message.isEmpty());
}

void ModalOverlay::clearError() {
    errorLabel_->clear();
    errorLabel_->hide();
}

void ModalOverlay::accept() {
    if (onAccept()) {
        emit finished(true);
    }
}

void ModalOverlay::reject() {
    onReject();
    emit finished(false);
}

void ModalOverlay::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_Escape:
            reject();
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            accept();
            return;
        default:
            break;
    }
    QWidget::keyPressEvent(event);
}

void ModalOverlay::mousePressEvent(QMouseEvent* event) {
    // Click on the scrim (outside the card) cancels.
    if (panel_ && !panel_->geometry().contains(event->pos())) {
        reject();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ModalOverlay::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    fitToParent();
    raise();
    setFocus(Qt::OtherFocusReason);

    if (auto* effect = qobject_cast<QGraphicsOpacityEffect*>(panel_->graphicsEffect())) {
        effect->setOpacity(0.0);
        auto* anim = new QPropertyAnimation(effect, "opacity", panel_);
        anim->setDuration(kFadeMs);
        anim->setStartValue(0.0);
        anim->setEndValue(1.0);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

bool ModalOverlay::eventFilter(QObject* watched, QEvent* event) {
    if (watched == parentWidget() && event->type() == QEvent::Resize) {
        fitToParent();
    }
    return QWidget::eventFilter(watched, event);
}

void ModalOverlay::fitToParent() {
    if (QWidget* parent = parentWidget()) {
        setGeometry(parent->rect());
    }
}

void ModalOverlay::applyTheme() {
    const auto& theme = ThemeManager::instance().currentTheme();
    const auto& ui = theme.ui;

    QColor scrim = ui.windowBackground;
    scrim.setAlphaF(theme.isDark ? 0.60 : 0.45);

    QColor cardBg = ui.inspectorBackground.isValid() ? ui.inspectorBackground : ui.panelBackground;
    QColor cardBorder = ui.panelBorder;
    QColor titleColor = ui.widgetText;
    QColor errorColor = theme.status.dofError.isValid() ? theme.status.dofError : QColor("#e05252");

    QString styleSheet = QStringLiteral(
        "#ModalOverlay { background: %1; }"
        "QWidget#modalCard {"
        "  background: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 14px;"
        "}"
        "QLabel#modalTitle { background: transparent; font-size: 15px; font-weight: 600; color: %4; }"
        "QLabel#modalError { background: transparent; font-size: 12px; color: %5; }")
        .arg(toQssColor(scrim),
             toQssColor(cardBg),
             toQssColor(cardBorder),
             toQssColor(titleColor),
             toQssColor(errorColor));

    setStyleSheet(styleSheet);

    // Re-polish the action buttons so the primary/ghost property selectors from the
    // global stylesheet are re-evaluated after this local stylesheet is (re)applied.
    if (buttonBox_) {
        for (QAbstractButton* button : buttonBox_->buttons()) {
            button->style()->unpolish(button);
            button->style()->polish(button);
        }
    }
}

} // namespace onecad::ui
