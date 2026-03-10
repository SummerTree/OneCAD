#include "CommandPalette.h"
#include "ActionRegistry.h"

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

namespace onecad {
namespace ui {

CommandPalette::CommandPalette(QWidget* parent)
    : QWidget(parent) {
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setFixedWidth(420);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);

    m_searchField = new QLineEdit(this);
    m_searchField->setPlaceholderText(tr("Type a command..."));
    m_searchField->setClearButtonEnabled(true);
    layout->addWidget(m_searchField);

    m_resultsList = new QListWidget(this);
    m_resultsList->setMaximumHeight(320);
    layout->addWidget(m_resultsList);

    connect(m_searchField, &QLineEdit::textChanged,
            this, &CommandPalette::onTextChanged);
    connect(m_resultsList, &QListWidget::itemActivated,
            this, &CommandPalette::onItemActivated);

    m_searchField->installEventFilter(this);
    m_resultsList->installEventFilter(this);
}

void CommandPalette::activate() {
    m_searchField->clear();
    populateResults({});

    // Position centered above parent
    if (parentWidget()) {
        QPoint center = parentWidget()->mapToGlobal(
            QPoint(parentWidget()->width() / 2, parentWidget()->height() / 4));
        move(center.x() - width() / 2, center.y());
    }

    show();
    raise();
    m_searchField->setFocus();
}

bool CommandPalette::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);

        if (ke->key() == Qt::Key_Escape) {
            dismiss();
            return true;
        }
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            executeSelected();
            return true;
        }
        if (ke->key() == Qt::Key_Down && obj == m_searchField) {
            m_resultsList->setFocus();
            if (m_resultsList->count() > 0 && m_resultsList->currentRow() < 0) {
                m_resultsList->setCurrentRow(0);
            }
            return true;
        }
        if (ke->key() == Qt::Key_Up && obj == m_resultsList &&
            m_resultsList->currentRow() == 0) {
            m_searchField->setFocus();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void CommandPalette::onTextChanged(const QString& text) {
    populateResults(text);
}

void CommandPalette::onItemActivated(QListWidgetItem* /*item*/) {
    executeSelected();
}

void CommandPalette::populateResults(const QString& query) {
    m_resultsList->clear();
    const auto entries = ActionRegistry::instance().search(query);

    for (const auto& entry : entries) {
        auto* item = new QListWidgetItem(
            QStringLiteral("[%1] %2").arg(entry.category, entry.displayName));
        item->setData(Qt::UserRole, entry.id);
        m_resultsList->addItem(item);
    }

    if (m_resultsList->count() > 0) {
        m_resultsList->setCurrentRow(0);
    }
}

void CommandPalette::executeSelected() {
    auto* item = m_resultsList->currentItem();
    if (!item) return;

    const QString id = item->data(Qt::UserRole).toString();
    const auto& actions = ActionRegistry::instance().actions();
    for (const auto& entry : actions) {
        if (entry.id == id && entry.action) {
            dismiss();
            entry.action->trigger();
            return;
        }
    }
    dismiss();
}

void CommandPalette::dismiss() {
    hide();
}

} // namespace ui
} // namespace onecad
