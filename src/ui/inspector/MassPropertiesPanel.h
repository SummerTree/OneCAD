#pragma once

#include <QWidget>
#include <QString>

class QLabel;
class TopoDS_Shape;

namespace onecad::ui {

class MassPropertiesPanel : public QWidget {
    Q_OBJECT

public:
    explicit MassPropertiesPanel(QWidget* parent = nullptr);

    void updateForShape(const TopoDS_Shape& shape, const QString& bodyName);
    void clear();

private:
    QLabel* m_nameLabel = nullptr;
    QLabel* m_volumeLabel = nullptr;
    QLabel* m_surfaceAreaLabel = nullptr;
    QLabel* m_centerOfMassLabel = nullptr;
    QLabel* m_boundingBoxLabel = nullptr;
    QLabel* m_faceCountLabel = nullptr;
    QLabel* m_edgeCountLabel = nullptr;
    QLabel* m_vertexCountLabel = nullptr;
};

} // namespace onecad::ui
