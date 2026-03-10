#ifndef ONECAD_UI_PALETTE_COMMANDPALETTE_H
#define ONECAD_UI_PALETTE_COMMANDPALETTE_H

#include <QWidget>

class QLineEdit;
class QListWidget;
class QListWidgetItem;

namespace onecad {
namespace ui {

class CommandPalette : public QWidget {
    Q_OBJECT

public:
    explicit CommandPalette(QWidget* parent = nullptr);

    void activate();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onTextChanged(const QString& text);
    void onItemActivated(QListWidgetItem* item);

private:
    void populateResults(const QString& query);
    void executeSelected();
    void dismiss();

    QLineEdit* m_searchField = nullptr;
    QListWidget* m_resultsList = nullptr;
};

} // namespace ui
} // namespace onecad

#endif // ONECAD_UI_PALETTE_COMMANDPALETTE_H
