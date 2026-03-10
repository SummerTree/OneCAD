#ifndef ONECAD_UI_PALETTE_ACTIONREGISTRY_H
#define ONECAD_UI_PALETTE_ACTIONREGISTRY_H

#include <QObject>
#include <QString>
#include <QList>
#include <functional>

class QAction;

namespace onecad {
namespace ui {

struct ActionEntry {
    QString id;
    QString displayName;
    QString category;
    QAction* action = nullptr;
};

class ActionRegistry : public QObject {
    Q_OBJECT

public:
    static ActionRegistry& instance();

    void registerAction(const QString& id, const QString& displayName,
                        const QString& category, QAction* action);
    void unregisterAction(const QString& id);

    const QList<ActionEntry>& actions() const { return m_actions; }

    QList<ActionEntry> search(const QString& query) const;

private:
    explicit ActionRegistry(QObject* parent = nullptr);
    QList<ActionEntry> m_actions;
};

} // namespace ui
} // namespace onecad

#endif // ONECAD_UI_PALETTE_ACTIONREGISTRY_H
