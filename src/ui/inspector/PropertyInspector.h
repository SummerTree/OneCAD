#ifndef ONECAD_UI_INSPECTOR_PROPERTYINSPECTOR_H
#define ONECAD_UI_INSPECTOR_PROPERTYINSPECTOR_H

#include <QDockWidget>
#include <string>

class QLabel;
class QStackedWidget;

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

private:
    void setupUi();
    void createEmptyStateWidget();
    void createBodyWidget();
    void createOperationWidget();

    app::Document* m_document = nullptr;
    QStackedWidget* m_stackedWidget = nullptr;
    QWidget* m_emptyWidget = nullptr;
    QWidget* m_bodyWidget = nullptr;
    QWidget* m_operationWidget = nullptr;
    MassPropertiesPanel* m_massProps = nullptr;
    QLabel* m_opTypeLabel = nullptr;
    QLabel* m_opIdLabel = nullptr;
    QLabel* m_opParamsLabel = nullptr;
};

} // namespace ui
} // namespace onecad

#endif // ONECAD_UI_INSPECTOR_PROPERTYINSPECTOR_H
