#ifndef ONECAD_UI_VIEWPORT_COMMON_H
#define ONECAD_UI_VIEWPORT_COMMON_H

#include "../../core/sketch/Sketch.h"
#include "../../core/sketch/SketchRenderer.h"
#include "../theme/ThemeManager.h"

#include <QMatrix4x4>
#include <QPointF>
#include <QRectF>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

#include <array>
#include <cmath>
#include <limits>

namespace onecad::ui::viewport_common {

// Constants
constexpr float kPlaneSelectSize = 120.0f;
constexpr float kPlaneSelectHalf = kPlaneSelectSize * 0.5f;
constexpr float kThumbnailCameraAngle = 45.0f;
constexpr float kGridDepthScaleMin = 1.0f;
constexpr float kGridDepthScaleMax = 8.0f;
constexpr float kGridBoundsMaxScale = 6.0f;

// Structs
struct PlaneAxes {
    QVector3D normal;
    QVector3D xAxis;
    QVector3D yAxis;
};

struct PlaneSelectionVisual {
    core::sketch::SketchPlane plane;
    QString label;
    QColor color;
};

// Inline helpers
inline PlaneAxes buildPlaneAxes(const core::sketch::SketchPlane& plane) {
    PlaneAxes axes;
    axes.normal = QVector3D(plane.normal.x, plane.normal.y, plane.normal.z);
    axes.xAxis = QVector3D(plane.xAxis.x, plane.xAxis.y, plane.xAxis.z);
    axes.yAxis = QVector3D(plane.yAxis.x, plane.yAxis.y, plane.yAxis.z);

    if (axes.normal.lengthSquared() < 1e-8f) {
        axes.normal = QVector3D::crossProduct(axes.xAxis, axes.yAxis);
    }
    if (axes.normal.lengthSquared() < 1e-8f) {
        axes.normal = QVector3D(0.0f, 0.0f, 1.0f);
    }
    axes.normal.normalize();

    if (axes.xAxis.lengthSquared() < 1e-8f) {
        axes.xAxis = QVector3D::crossProduct(axes.yAxis, axes.normal);
    }
    if (axes.xAxis.lengthSquared() < 1e-8f) {
        axes.xAxis = (std::abs(axes.normal.z()) < 0.9f)
            ? QVector3D::crossProduct(axes.normal, QVector3D(0, 0, 1))
            : QVector3D::crossProduct(axes.normal, QVector3D(0, 1, 0));
    }

    axes.xAxis -= axes.normal * QVector3D::dotProduct(axes.normal, axes.xAxis);
    if (axes.xAxis.lengthSquared() < 1e-8f) {
        axes.xAxis = (std::abs(axes.normal.z()) < 0.9f)
            ? QVector3D::crossProduct(axes.normal, QVector3D(0, 0, 1))
            : QVector3D::crossProduct(axes.normal, QVector3D(0, 1, 0));
    }
    axes.xAxis.normalize();

    axes.yAxis = QVector3D::crossProduct(axes.normal, axes.xAxis).normalized();
    return axes;
}

inline core::sketch::Vec3d toVec3d(const QColor& color) {
    return {color.redF(), color.greenF(), color.blueF()};
}

inline void applySketchColors(const ThemeSketchColors& colors, core::sketch::SketchRenderStyle* style) {
    if (!style) {
        return;
    }
    style->colors.normalGeometry = toVec3d(colors.normalGeometry);
    style->colors.constructionGeometry = toVec3d(colors.constructionGeometry);
    style->colors.selectedGeometry = toVec3d(colors.selectedGeometry);
    style->colors.previewGeometry = toVec3d(colors.previewGeometry);
    style->colors.errorGeometry = toVec3d(colors.errorGeometry);
    style->colors.constraintIcon = toVec3d(colors.constraintIcon);
    style->colors.dimensionText = toVec3d(colors.dimensionText);
    style->colors.conflictHighlight = toVec3d(colors.conflictHighlight);
    style->colors.fullyConstrained = toVec3d(colors.fullyConstrained);
    style->colors.underConstrained = toVec3d(colors.underConstrained);
    style->colors.overConstrained = toVec3d(colors.overConstrained);
    style->colors.gridMajor = toVec3d(colors.gridMajor);
    style->colors.gridMinor = toVec3d(colors.gridMinor);
    style->colors.regionFill = toVec3d(colors.regionFill);
}

inline bool projectToScreen(const QMatrix4x4& viewProjection,
                             const QVector3D& worldPos,
                             float width,
                             float height,
                             QPointF* outPos) {
    QVector4D clip = viewProjection * QVector4D(worldPos, 1.0f);
    if (clip.w() <= 1e-6f) {
        return false;
    }
    QVector3D ndc = clip.toVector3D() / clip.w();
    float x = (ndc.x() * 0.5f + 0.5f) * width;
    float y = (1.0f - (ndc.y() * 0.5f + 0.5f)) * height;
    *outPos = QPointF(x, y);
    return true;
}

inline bool intersectRayWithPlane(const QVector3D& origin,
                                   const QVector3D& direction,
                                   const QVector3D& planeOrigin,
                                   const QVector3D& planeNormal,
                                   QVector3D* outPoint,
                                   float* outDistance) {
    constexpr float kEpsilon = 1e-6f;
    float denom = QVector3D::dotProduct(direction, planeNormal);
    if (std::abs(denom) < kEpsilon) {
        return false;
    }
    float t = QVector3D::dotProduct(planeOrigin - origin, planeNormal) / denom;
    if (t < 0.0f) {
        return false;
    }
    if (outPoint) {
        *outPoint = origin + direction * t;
    }
    if (outDistance) {
        *outDistance = t;
    }
    return true;
}

inline QVector2D worldToPlaneCoords(const QVector3D& worldPoint,
                                     const QVector3D& planeOrigin,
                                     const QVector3D& planeXAxis,
                                     const QVector3D& planeYAxis) {
    QVector3D relative = worldPoint - planeOrigin;
    return {
        QVector3D::dotProduct(relative, planeXAxis),
        QVector3D::dotProduct(relative, planeYAxis)
    };
}

inline QMatrix4x4 buildPlaneModelMatrix(const QVector3D& planeOrigin,
                                         const QVector3D& planeXAxis,
                                         const QVector3D& planeYAxis,
                                         const QVector3D& planeNormal) {
    QMatrix4x4 model;
    model.setColumn(0, QVector4D(planeXAxis, 0.0f));
    model.setColumn(1, QVector4D(planeYAxis, 0.0f));
    model.setColumn(2, QVector4D(planeNormal, 0.0f));
    model.setColumn(3, QVector4D(planeOrigin, 1.0f));
    return model;
}

inline bool computePlaneBoundsOnPlane(const QMatrix4x4& viewProjection,
                                       const QVector3D& planeOrigin,
                                       const QVector3D& planeNormal,
                                       const QVector3D& planeXAxis,
                                       const QVector3D& planeYAxis,
                                       QRectF* outBounds) {
    bool invertible = false;
    QMatrix4x4 inverse = viewProjection.inverted(&invertible);
    if (!invertible) {
        return false;
    }

    QVector2D minPoint(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    QVector2D maxPoint(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
    int hitCount = 0;

    constexpr float kEpsilon = 1e-6f;
    const float ndc[2] = { -1.0f, 1.0f };

    for (float x : ndc) {
        for (float y : ndc) {
            QVector4D nearPoint = inverse * QVector4D(x, y, -1.0f, 1.0f);
            QVector4D farPoint = inverse * QVector4D(x, y, 1.0f, 1.0f);

            if (qFuzzyIsNull(nearPoint.w()) || qFuzzyIsNull(farPoint.w())) {
                continue;
            }

            QVector3D p0 = nearPoint.toVector3D() / nearPoint.w();
            QVector3D p1 = farPoint.toVector3D() / farPoint.w();
            QVector3D dir = p1 - p0;

            float denom = QVector3D::dotProduct(dir, planeNormal);
            if (std::abs(denom) < kEpsilon) {
                continue;
            }

            float t = QVector3D::dotProduct(planeOrigin - p0, planeNormal) / denom;
            if (t < 0.0f) {
                continue;
            }

            QVector3D hit = p0 + dir * t;
            if (!std::isfinite(hit.x()) || !std::isfinite(hit.y()) || !std::isfinite(hit.z())) {
                continue;
            }
            QVector2D planePoint = worldToPlaneCoords(hit, planeOrigin, planeXAxis, planeYAxis);
            minPoint.setX(std::min(minPoint.x(), planePoint.x()));
            minPoint.setY(std::min(minPoint.y(), planePoint.y()));
            maxPoint.setX(std::max(maxPoint.x(), planePoint.x()));
            maxPoint.setY(std::max(maxPoint.y(), planePoint.y()));
            ++hitCount;
        }
    }

    if (hitCount < 4) {
        return false;
    }

    *outBounds = QRectF(QPointF(minPoint.x(), minPoint.y()),
                        QPointF(maxPoint.x(), maxPoint.y())).normalized();
    return true;
}

inline std::array<PlaneSelectionVisual, 3> planeSelections(const ThemeViewportPlaneColors& colors) {
    return {{
        {core::sketch::SketchPlane::XY(), QStringLiteral("XY"), colors.xy},
        {core::sketch::SketchPlane::XZ(), QStringLiteral("XZ"), colors.xz},
        {core::sketch::SketchPlane::YZ(), QStringLiteral("YZ"), colors.yz}
    }};
}

} // namespace onecad::ui::viewport_common

#endif // ONECAD_UI_VIEWPORT_COMMON_H
