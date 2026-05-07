#ifndef ONECAD_UI_VIEWPORT_COMMON_H
#define ONECAD_UI_VIEWPORT_COMMON_H

#include "../../core/sketch/Sketch.h"
#include "../../core/sketch/SketchRenderer.h"
#include "../../render/Camera3D.h"
#include "../theme/ThemeManager.h"

#include <QMatrix4x4>
#include <QPointF>
#include <QRectF>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

#include <algorithm>
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

struct SketchPlaneViewportInfo {
    core::sketch::Viewport viewport;
    QRectF bounds;
    QRectF fallbackBounds;
    QRectF frustumBounds;
    bool hasFrustumBounds = false;
    bool usesFrustumBounds = false;
    float depthScale = 1.0f;
};

struct PlaneDepthRangeInfo {
    bool hasIntersection = false;
    float minDistance = std::numeric_limits<float>::max();
    float maxDistance = 0.0f;
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

inline PlaneDepthRangeInfo samplePerspectivePlaneDepthRange(
    const QMatrix4x4& viewProjection,
    const QVector3D& cameraPosition,
    const QVector3D& planeOrigin,
    const QVector3D& planeNormal) {

    PlaneDepthRangeInfo info;
    bool invertible = false;
    const QMatrix4x4 inverse = viewProjection.inverted(&invertible);
    if (!invertible) {
        return info;
    }

    constexpr float kSamples[3] = {-1.0f, 0.0f, 1.0f};
    constexpr float kEpsilon = 1e-6f;
    QVector3D normalizedNormal = planeNormal;
    if (normalizedNormal.lengthSquared() < 1e-8f) {
        return info;
    }
    normalizedNormal.normalize();

    for (float x : kSamples) {
        for (float y : kSamples) {
            const QVector4D nearPoint = inverse * QVector4D(x, y, -1.0f, 1.0f);
            const QVector4D farPoint = inverse * QVector4D(x, y, 1.0f, 1.0f);
            if (std::abs(nearPoint.w()) < kEpsilon || std::abs(farPoint.w()) < kEpsilon) {
                continue;
            }

            const QVector3D nearWorld = nearPoint.toVector3D() / nearPoint.w();
            const QVector3D farWorld = farPoint.toVector3D() / farPoint.w();
            QVector3D rayDirection = farWorld - cameraPosition;
            if (rayDirection.lengthSquared() < 1e-8f) {
                rayDirection = farWorld - nearWorld;
            }
            if (rayDirection.lengthSquared() < 1e-8f) {
                continue;
            }
            rayDirection.normalize();

            const float denom = QVector3D::dotProduct(rayDirection, normalizedNormal);
            if (std::abs(denom) < kEpsilon) {
                continue;
            }

            const float t = QVector3D::dotProduct(planeOrigin - cameraPosition, normalizedNormal) / denom;
            if (!std::isfinite(t) || t <= 0.0f) {
                continue;
            }

            info.hasIntersection = true;
            info.minDistance = std::min(info.minDistance, t);
            info.maxDistance = std::max(info.maxDistance, t);
        }
    }

    return info;
}

inline SketchPlaneViewportInfo buildSketchViewportForPlane(
    const core::sketch::SketchPlane& plane,
    const render::Camera3D& camera,
    const QMatrix4x4& viewProjection,
    int viewportWidth,
    int viewportHeight,
    qreal devicePixelRatio,
    double pixelScale) {

    SketchPlaneViewportInfo info;
    if (viewportWidth <= 0 || viewportHeight <= 0) {
        info.viewport.zoom = 1.0;
        return info;
    }

    const double safePixelScale = (pixelScale > 0.0) ? pixelScale : 1.0;
    const double safeDevicePixelRatio = (devicePixelRatio > 0.0) ? devicePixelRatio : 1.0;
    const double maxDimension = static_cast<double>(qMax(viewportWidth, viewportHeight));
    const double viewExtent = 0.5 * maxDimension * safeDevicePixelRatio * safePixelScale;

    const PlaneAxes axes = buildPlaneAxes(plane);
    const QVector3D planeOrigin(plane.origin.x, plane.origin.y, plane.origin.z);
    const QVector3D cameraPosition = camera.position();
    const QVector3D cameraTarget = camera.target();
    const QVector3D cameraForward = camera.forward();
    const float cameraDistance = camera.distance();

    float planeDistance = cameraDistance;
    QVector3D planeAnchorWorld = planeOrigin;
    const bool hasPlaneHit = intersectRayWithPlane(cameraPosition,
                                                   cameraForward,
                                                   planeOrigin,
                                                   axes.normal,
                                                   &planeAnchorWorld,
                                                   &planeDistance);
    if (!hasPlaneHit) {
        const float targetSignedDistance = QVector3D::dotProduct(cameraTarget - planeOrigin,
                                                                 axes.normal);
        planeAnchorWorld = cameraTarget - axes.normal * targetSignedDistance;
    }

    if (camera.projectionType() == render::Camera3D::ProjectionType::Perspective &&
        cameraDistance > 1e-4f) {
        info.depthScale = planeDistance / cameraDistance;
    }
    info.depthScale = std::clamp(info.depthScale, kGridDepthScaleMin, kGridDepthScaleMax);

    const QVector2D planeAnchor = worldToPlaneCoords(planeAnchorWorld,
                                                     planeOrigin,
                                                     axes.xAxis,
                                                     axes.yAxis);
    const float viewHalf = static_cast<float>(viewExtent) * info.depthScale;
    const core::sketch::Vec2d legacyCenter = plane.toSketch(
        {cameraTarget.x(), cameraTarget.y(), cameraTarget.z()}
    );
    const QRectF legacyBounds(
        QPointF(legacyCenter.x - static_cast<double>(viewportWidth) * safeDevicePixelRatio * safePixelScale * 0.5,
                legacyCenter.y - static_cast<double>(viewportHeight) * safeDevicePixelRatio * safePixelScale * 0.5),
        QPointF(legacyCenter.x + static_cast<double>(viewportWidth) * safeDevicePixelRatio * safePixelScale * 0.5,
                legacyCenter.y + static_cast<double>(viewportHeight) * safeDevicePixelRatio * safePixelScale * 0.5)
    );
    info.fallbackBounds = QRectF(QPointF(planeAnchor.x() - viewHalf, planeAnchor.y() - viewHalf),
                                 QPointF(planeAnchor.x() + viewHalf, planeAnchor.y() + viewHalf))
                              .normalized();
    info.bounds = info.fallbackBounds;

    QRectF frustumBounds;
    info.hasFrustumBounds = computePlaneBoundsOnPlane(viewProjection,
                                                      planeOrigin,
                                                      axes.normal,
                                                      axes.xAxis,
                                                      axes.yAxis,
                                                      &frustumBounds);
    if (info.hasFrustumBounds) {
        info.frustumBounds = frustumBounds.normalized();

        const float maxHalf = 0.5f * static_cast<float>(qMax(info.frustumBounds.width(),
                                                             info.frustumBounds.height()));
        const QVector2D frustumCenter(static_cast<float>(info.frustumBounds.center().x()),
                                      static_cast<float>(info.frustumBounds.center().y()));
        const float centerDistance = (frustumCenter - planeAnchor).length();
        const float maxAllowed = viewHalf * kGridBoundsMaxScale;

        if (maxHalf <= maxAllowed && centerDistance <= maxAllowed) {
            info.usesFrustumBounds = true;
            info.bounds = info.frustumBounds;
        }
    }
    info.bounds = info.bounds.united(legacyBounds.normalized());

    constexpr double kMinViewportSize = 1e-6;
    info.viewport.center = {info.bounds.center().x(), info.bounds.center().y()};
    info.viewport.size = {std::max(info.bounds.width(), kMinViewportSize),
                          std::max(info.bounds.height(), kMinViewportSize)};

    const double unitsPerPixelY =
        info.viewport.size.y / (static_cast<double>(viewportHeight) * safeDevicePixelRatio);
    info.viewport.zoom = unitsPerPixelY > 1e-9 ? 1.0 / unitsPerPixelY : 1.0;
    return info;
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
