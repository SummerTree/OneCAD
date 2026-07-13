#ifndef ONECAD_UI_INSPECTOR_PROPERTYINSPECTOR_H
#define ONECAD_UI_INSPECTOR_PROPERTYINSPECTOR_H

#include <QDockWidget>
#include <QString>
#include <string>

class QLabel;
class QStackedWidget;
class QLineEdit;
class QCheckBox;
class QTimer;

namespace onecad::app {
class Document;
}

namespace onecad {
namespace ui {

class MassPropertiesPanel;

class PropertyInspector : public QDockWidget {
    Q_OBJECT

public:
    explicit PropertyInspector(QWidget* parent = nullptr);

    void setDocument(app::Document* document);

public slots:
    void showEmptyState();
    void showBodyProperties(const QString& bodyId);
    void showOperationProperties(const QString& opId);
    void onSelectionChanged();
    // Re-resolves whatever is currently shown (body or operation) against the
    // document; falls back to the empty state if the id no longer exists. Call
    // after regeneration/rollback so displayed properties never go stale.
    void refresh();

signals:
    // Edits are surfaced as requests so the owner can route them through undoable
    // commands; the inspector never mutates the document directly.
    void bodyRenameRequested(const QString& bodyId, const QString& newName);
    void bodyVisibilityChangeRequested(const QString& bodyId, bool visible);

private:
    void setupUi();
    void createEmptyStateWidget();
    void createBodyWidget();
    void createOperationWidget();
    void computeMassPropsNow();

    app::Document* m_document = nullptr;
    QStackedWidget* m_stackedWidget = nullptr;
    QWidget* m_emptyWidget = nullptr;
    QWidget* m_bodyWidget = nullptr;
    QWidget* m_operationWidget = nullptr;
    MassPropertiesPanel* m_massProps = nullptr;
    QLineEdit* m_bodyNameEdit = nullptr;
    QCheckBox* m_bodyVisibleCheck = nullptr;
    QLabel* m_opTypeLabel = nullptr;
    QLabel* m_opIdLabel = nullptr;
    QLabel* m_opParamsLabel = nullptr;

    QString m_currentBodyId;
    QString m_currentOpId;
    QTimer* m_massPropsTimer = nullptr;
};

} // namespace ui
} // namespace onecad

#endif // ONECAD_UI_INSPECTOR_PROPERTYINSPECTOR_H
