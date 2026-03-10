#include "ActionRegistry.h"

#include <QAction>
#include <algorithm>

namespace onecad {
namespace ui {

ActionRegistry& ActionRegistry::instance() {
    static ActionRegistry registry;
    return registry;
}

ActionRegistry::ActionRegistry(QObject* parent)
    : QObject(parent) {}

void ActionRegistry::registerAction(const QString& id, const QString& displayName,
                                     const QString& category, QAction* action) {
    for (auto& entry : m_actions) {
        if (entry.id == id) {
            entry.displayName = displayName;
            entry.category = category;
            entry.action = action;
            return;
        }
    }
    m_actions.append({id, displayName, category, action});
}

void ActionRegistry::unregisterAction(const QString& id) {
    m_actions.removeIf([&id](const ActionEntry& e) { return e.id == id; });
}

QList<ActionEntry> ActionRegistry::search(const QString& query) const {
    if (query.isEmpty()) {
        return m_actions;
    }

    QList<ActionEntry> results;
    const QString lower = query.toLower();

    // Score-based fuzzy match: exact substring > word start > any match
    struct Scored {
        ActionEntry entry;
        int score;
    };
    QList<Scored> scored;

    for (const auto& entry : m_actions) {
        const QString name = entry.displayName.toLower();
        const QString cat = entry.category.toLower();

        if (name == lower) {
            scored.append({entry, 100});
        } else if (name.startsWith(lower)) {
            scored.append({entry, 80});
        } else if (name.contains(lower)) {
            scored.append({entry, 60});
        } else if (cat.contains(lower)) {
            scored.append({entry, 40});
        } else {
            // Simple fuzzy: all query chars appear in order
            int qi = 0;
            for (int ni = 0; ni < name.size() && qi < lower.size(); ++ni) {
                if (name[ni] == lower[qi]) {
                    ++qi;
                }
            }
            if (qi == lower.size()) {
                scored.append({entry, 20});
            }
        }
    }

    std::sort(scored.begin(), scored.end(),
              [](const Scored& a, const Scored& b) { return a.score > b.score; });

    for (const auto& s : scored) {
        results.append(s.entry);
    }
    return results;
}

} // namespace ui
} // namespace onecad
