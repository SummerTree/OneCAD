#include "CommandPalette.h"
#include "ActionRegistry.h"
#include "../theme/ThemeManager.h"
#include "../theme/ThemeConfig.h"

#include <QAction>
#include <QApplication>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

namespace onecad {
namespace ui {

namespace {
// Item data roles. kRoleId stays at Qt::UserRole — executeSelected() depends on it.
constexpr int kRoleId = Qt::UserRole;
constexpr int kRoleName = Qt::UserRole + 1;
constexpr int kRoleCategory = Qt::UserRole + 2;
constexpr int kRoleShortcut = Qt::UserRole + 3;
constexpr int kRoleQuery = Qt::UserRole + 4;

// Paints each result row: a category badge, the display name with the matched
// substring bolded, and a right-aligned shortcut hint. Reads theme colors at paint time.
class PaletteItemDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override {
        QSize s = QStyledItemDelegate::sizeHint(option, index);
        s.setHeight(std::max(s.height(), 32));
        return s;
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        const ThemeDefinition& theme = ThemeManager::instance().currentTheme();
        const ThemeMetrics& m = theme.metrics;
        const bool selected = option.state & QStyle::State_Selected;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const QRect row = option.rect;
        const int pad = m.spacingSm;

        if (selected) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(theme.ui.treeSelectedBackground);
            painter->drawRoundedRect(row.adjusted(2, 1, -2, -1), m.radiusSm, m.radiusSm);
        }

        const QString name = index.data(kRoleName).toString();
        const QString category = index.data(kRoleCategory).toString();
        const QString shortcut = index.data(kRoleShortcut).toString();
        const QString query = index.data(kRoleQuery).toString();

        const QColor textColor = selected ? theme.ui.treeSelectedText : theme.ui.widgetText;
        const QColor mutedColor = theme.ui.inspectorHintText;

        const QFont baseFont = option.font;
        int x = row.left() + pad;
        const int cy = row.center().y();

        // Category badge pill.
        if (!category.isEmpty()) {
            QFont badgeFont = baseFont;
            badgeFont.setPixelSize(theme.typography.sizeXs);
            const QFontMetrics bfm(badgeFont);
            const int bw = bfm.horizontalAdvance(category) + 2 * m.spacingSm;
            const int bh = bfm.height() + 2;
            const QRect badge(x, cy - bh / 2, bw, bh);
            painter->setPen(Qt::NoPen);
            painter->setBrush(theme.ui.treeHoverBackground);
            painter->drawRoundedRect(badge, m.radiusSm, m.radiusSm);
            painter->setFont(badgeFont);
            painter->setPen(mutedColor);
            painter->drawText(badge, Qt::AlignCenter, category);
            x += bw + m.spacingSm;
        }

        // Right-aligned shortcut.
        int rightLimit = row.right() - pad;
        if (!shortcut.isEmpty()) {
            const QFontMetrics fm(baseFont);
            const int sw = fm.horizontalAdvance(shortcut);
            const QRect sr(rightLimit - sw, row.top(), sw, row.height());
            painter->setFont(baseFont);
            painter->setPen(selected ? textColor : mutedColor);
            painter->drawText(sr, Qt::AlignVCenter | Qt::AlignRight, shortcut);
            rightLimit -= sw + m.spacingMd;
        }

        // Display name with bolded match.
        const QFontMetrics fm(baseFont);
        const int nameWidth = std::max(0, rightLimit - x);
        const QString elided = fm.elidedText(name, Qt::ElideRight, nameWidth);
        const QRect nameRect(x, row.top(), nameWidth, row.height());

        qsizetype matchStart = -1;
        qsizetype matchLen = 0;
        if (!query.isEmpty()) {
            matchStart = elided.toLower().indexOf(query.toLower());
            if (matchStart >= 0) {
                matchLen = std::min(query.length(), elided.length() - matchStart);
            }
        }

        painter->setPen(textColor);
        if (matchStart < 0) {
            painter->setFont(baseFont);
            painter->drawText(nameRect, Qt::AlignVCenter | Qt::AlignLeft, elided);
        } else {
            QFont boldFont = baseFont;
            boldFont.setBold(true);
            int drawX = x;
            const auto drawSegment = [&](const QString& seg, const QFont& f) {
                if (seg.isEmpty()) return;
                painter->setFont(f);
                const QRect segRect(drawX, row.top(),
                                    QFontMetrics(f).horizontalAdvance(seg), row.height());
                painter->drawText(segRect, Qt::AlignVCenter | Qt::AlignLeft, seg);
                drawX += QFontMetrics(f).horizontalAdvance(seg);
            };
            drawSegment(elided.left(matchStart), baseFont);
            drawSegment(elided.mid(matchStart, matchLen), boldFont);
            drawSegment(elided.mid(matchStart + matchLen), baseFont);
        }

        painter->restore();
    }
};
} // namespace

CommandPalette::CommandPalette(QWidget* parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("CommandPalette"));
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedWidth(460);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    m_searchField = new QLineEdit(this);
    m_searchField->setPlaceholderText(tr("Type a command..."));
    m_searchField->setClearButtonEnabled(true);
    layout->addWidget(m_searchField);

    m_resultsList = new QListWidget(this);
    m_resultsList->setMaximumHeight(360);
    m_resultsList->setItemDelegate(new PaletteItemDelegate(m_resultsList));
    m_resultsList->setUniformItemSizes(false);
    layout->addWidget(m_resultsList);

    connect(m_searchField, &QLineEdit::textChanged,
            this, &CommandPalette::onTextChanged);
    connect(m_resultsList, &QListWidget::itemActivated,
            this, &CommandPalette::onItemActivated);

    m_searchField->installEventFilter(this);
    m_resultsList->installEventFilter(this);

    m_themeConnection = connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
                                this, &CommandPalette::applyTheme, Qt::UniqueConnection);
    applyTheme();
}

CommandPalette::~CommandPalette() {
    QObject::disconnect(m_themeConnection);
}

void CommandPalette::applyTheme() {
    const ThemeDefinition& theme = ThemeManager::instance().currentTheme();
    const ThemeUiColors& ui = theme.ui;
    const ThemeMetrics& m = theme.metrics;
    setStyleSheet(
        QStringLiteral(
            "QWidget#CommandPalette {"
            "  background-color: %1;"
            "  border: 1px solid %2;"
            "  border-radius: %3px;"
            "}"
            "QWidget#CommandPalette QLineEdit {"
            "  background-color: %4;"
            "  color: %5;"
            "  border: 1px solid %2;"
            "  border-radius: %6px;"
            "  padding: %7px %8px;"
            "}"
            "QWidget#CommandPalette QListWidget {"
            "  background-color: transparent;"
            "  border: none;"
            "  outline: 0;"
            "}")
            .arg(toQssColor(ui.panelBackground),
                 toQssColor(ui.panelBorder),
                 QString::number(m.radiusLg),
                 toQssColor(ui.inspectorBackground),
                 toQssColor(ui.widgetText),
                 QString::number(m.radiusSm))
            .arg(m.spacingSm)
            .arg(m.spacingMd));
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
        // Accessible/fallback text; the delegate paints the rich layout.
        auto* item = new QListWidgetItem(
            QStringLiteral("%1  %2").arg(entry.category, entry.displayName));
        item->setData(kRoleId, entry.id);
        item->setData(kRoleName, entry.displayName);
        item->setData(kRoleCategory, entry.category);
        item->setData(kRoleQuery, query);
        if (entry.action) {
            item->setData(kRoleShortcut,
                          entry.action->shortcut().toString(QKeySequence::NativeText));
        }
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
