#pragma once

#include <QObject>
#include <QVector3D>
#include <QPoint>
#include <QString>
#include <optional>
#include <vector>

class TopoDS_Shape;

namespace onecad::app {
class Document;
}

namespace onecad::ui {
class Viewport;
}

namespace onecad::ui::tools {

class MeasureTool : public QObject {
    Q_OBJECT

public:
    enum class Mode { PointToPoint, EdgeLength, FaceArea };

    struct Measurement {
        QVector3D point1;
        QVector3D point2;
        double distance = 0.0;
        double area = 0.0;
        QString label;
        Mode mode = Mode::PointToPoint;
    };

    explicit MeasureTool(Viewport* viewport, QObject* parent = nullptr);

    void setDocument(app::Document* document);
    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    void activate();
    void deactivate();
    bool isActive() const { return m_active; }

    bool handleMousePress(const QPoint& screenPos, Qt::MouseButton button);
    bool handleMouseMove(const QPoint& screenPos);

    const std::vector<Measurement>& measurements() const { return m_measurements; }
    void clearMeasurements();

    static double computeEdgeLength(const TopoDS_Shape& edge);
    static double computeFaceArea(const TopoDS_Shape& face);
    static QVector3D computeFaceCentroid(const TopoDS_Shape& face);

signals:
    void measurementAdded(const Measurement& measurement);
    void measurementsCleared();

private:
    Viewport* m_viewport = nullptr;
    app::Document* m_document = nullptr;
    Mode m_mode = Mode::PointToPoint;
    bool m_active = false;
    std::optional<QVector3D> m_firstPoint;
    std::vector<Measurement> m_measurements;
};

} // namespace onecad::ui::tools
