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

    // Above this face count the expensive volume/surface-area integrals are skipped
    // (cheap counts + bounding box are still shown) to keep selection responsive.
    void setHeavyComputeFaceLimit(int limit) { m_heavyComputeFaceLimit = limit; }

private:
    int m_heavyComputeFaceLimit = 40000;
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
