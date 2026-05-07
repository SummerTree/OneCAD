#include "core/sketch/Sketch.h"
#include "render/Camera3D.h"
#include "ui/viewport/ViewportCommon.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <QtMath>

#include <cassert>
#include <cmath>
#include <iostream>

using onecad::core::sketch::SketchPlane;
using onecad::render::Camera3D;
using onecad::ui::viewport_common::PlaneDepthRangeInfo;
using onecad::ui::viewport_common::SketchPlaneViewportInfo;
using onecad::ui::viewport_common::buildSketchViewportForPlane;
using onecad::ui::viewport_common::samplePerspectivePlaneDepthRange;

namespace {

constexpr int kViewportWidth = 1280;
constexpr int kViewportHeight = 720;
constexpr qreal kDevicePixelRatio = 1.0;
constexpr double kEpsilon = 1e-6;

double computePixelScale(const Camera3D& camera) {
    if (camera.projectionType() == Camera3D::ProjectionType::Orthographic) {
        return static_cast<double>(camera.orthoScale()) /
               (static_cast<double>(kViewportHeight) * kDevicePixelRatio);
    }

    const double halfFov = qDegreesToRadians(camera.fov() * 0.5f);
    const double worldHeight = 2.0 * static_cast<double>(camera.distance()) * std::tan(halfFov);
    return worldHeight / (static_cast<double>(kViewportHeight) * kDevicePixelRatio);
}

QMatrix4x4 computeViewProjection(const Camera3D& camera) {
    const float aspectRatio = static_cast<float>(kViewportWidth) /
                              static_cast<float>(kViewportHeight);
    return camera.projectionMatrix(aspectRatio) * camera.viewMatrix();
}

QRectF viewportRect(const onecad::core::sketch::Viewport& viewport) {
    return QRectF(QPointF(viewport.center.x - viewport.size.x * 0.5,
                          viewport.center.y - viewport.size.y * 0.5),
                  QPointF(viewport.center.x + viewport.size.x * 0.5,
                          viewport.center.y + viewport.size.y * 0.5))
        .normalized();
}

bool rectContainsRect(const QRectF& outer, const QRectF& inner, double epsilon = kEpsilon) {
    return outer.left() <= inner.left() + epsilon &&
           outer.right() + epsilon >= inner.right() &&
           outer.top() <= inner.top() + epsilon &&
           outer.bottom() + epsilon >= inner.bottom();
}

bool isFiniteViewport(const onecad::core::sketch::Viewport& viewport) {
    return std::isfinite(viewport.center.x) &&
           std::isfinite(viewport.center.y) &&
           std::isfinite(viewport.size.x) &&
           std::isfinite(viewport.size.y) &&
           viewport.size.x > 0.0 &&
           viewport.size.y > 0.0 &&
           std::isfinite(viewport.zoom) &&
           viewport.zoom > 0.0;
}

onecad::core::sketch::Viewport buildLegacyViewport(const SketchPlane& plane,
                                                   const Camera3D& camera,
                                                   double pixelScale) {
    const QVector3D target = camera.target();
    const onecad::core::sketch::Vec3d target3d{target.x(), target.y(), target.z()};
    onecad::core::sketch::Viewport viewport;
    viewport.center = plane.toSketch(target3d);
    viewport.size = {
        static_cast<double>(kViewportWidth) * kDevicePixelRatio * pixelScale,
        static_cast<double>(kViewportHeight) * kDevicePixelRatio * pixelScale
    };
    viewport.zoom = pixelScale > 0.0 ? 1.0 / pixelScale : 1.0;
    return viewport;
}

SketchPlane makePlane(const QVector3D& origin,
                      const QVector3D& normal,
                      const QVector3D& xHint) {
    QVector3D zAxis = normal.normalized();
    QVector3D xAxis = xHint - zAxis * QVector3D::dotProduct(xHint, zAxis);
    if (xAxis.lengthSquared() < 1e-8f) {
        xAxis = QVector3D::crossProduct(zAxis, QVector3D(0.0f, 0.0f, 1.0f));
    }
    if (xAxis.lengthSquared() < 1e-8f) {
        xAxis = QVector3D::crossProduct(zAxis, QVector3D(0.0f, 1.0f, 0.0f));
    }
    xAxis.normalize();
    const QVector3D yAxis = QVector3D::crossProduct(zAxis, xAxis).normalized();

    return {
        {origin.x(), origin.y(), origin.z()},
        {xAxis.x(), xAxis.y(), xAxis.z()},
        {yAxis.x(), yAxis.y(), yAxis.z()},
        {zAxis.x(), zAxis.y(), zAxis.z()}
    };
}

void assertViewportNotSmaller(const SketchPlaneViewportInfo& info,
                              const onecad::core::sketch::Viewport& legacyViewport) {
    const QRectF viewportBounds = viewportRect(info.viewport);
    const QRectF legacyBounds = viewportRect(legacyViewport);
    assert(viewportBounds.width() + kEpsilon >= legacyBounds.width());
    assert(viewportBounds.height() + kEpsilon >= legacyBounds.height());
}

void runOrthographicAlignedCase() {
    Camera3D camera;
    camera.setTopView();
    camera.setCameraAngle(0.0f);

    const double pixelScale = computePixelScale(camera);
    const SketchPlane plane = SketchPlane::XY();
    const SketchPlaneViewportInfo info = buildSketchViewportForPlane(
        plane,
        camera,
        computeViewProjection(camera),
        kViewportWidth,
        kViewportHeight,
        kDevicePixelRatio,
        pixelScale
    );

    assert(isFiniteViewport(info.viewport));
    assert(info.hasFrustumBounds);
    assert(info.usesFrustumBounds);
    assert(rectContainsRect(viewportRect(info.viewport), info.frustumBounds));
}

void runPerspectiveXYCase() {
    Camera3D camera;
    camera.setIsometricView();
    camera.setCameraAngle(45.0f);

    const double pixelScale = computePixelScale(camera);
    const SketchPlane plane = SketchPlane::XY();
    const SketchPlaneViewportInfo info = buildSketchViewportForPlane(
        plane,
        camera,
        computeViewProjection(camera),
        kViewportWidth,
        kViewportHeight,
        kDevicePixelRatio,
        pixelScale
    );

    assert(isFiniteViewport(info.viewport));
    assert(info.hasFrustumBounds);
    assert(info.usesFrustumBounds);
    assert(rectContainsRect(viewportRect(info.viewport), info.frustumBounds));
    assertViewportNotSmaller(info, buildLegacyViewport(plane, camera, pixelScale));
}

void runOffsetPlaneCase() {
    Camera3D camera;
    camera.setIsometricView();
    camera.setCameraAngle(45.0f);

    const double pixelScale = computePixelScale(camera);
    SketchPlane plane = SketchPlane::XY();
    plane.origin = {0.0, 0.0, 180.0};

    const SketchPlaneViewportInfo info = buildSketchViewportForPlane(
        plane,
        camera,
        computeViewProjection(camera),
        kViewportWidth,
        kViewportHeight,
        kDevicePixelRatio,
        pixelScale
    );

    assert(isFiniteViewport(info.viewport));
    assert(info.hasFrustumBounds);
    assert(info.usesFrustumBounds);
    assert(rectContainsRect(viewportRect(info.viewport), info.frustumBounds));
    assertViewportNotSmaller(info, buildLegacyViewport(plane, camera, pixelScale));
}

void runNearGrazingFallbackCase() {
    Camera3D camera;
    camera.setIsometricView();
    camera.setCameraAngle(45.0f);

    const QVector3D planeOrigin(0.0f, 0.0f, 0.0f);
    const QVector3D planeNormal = camera.right();
    const QVector3D xHint = camera.forward();
    const SketchPlane plane = makePlane(planeOrigin, planeNormal, xHint);
    const double pixelScale = computePixelScale(camera);

    const SketchPlaneViewportInfo info = buildSketchViewportForPlane(
        plane,
        camera,
        computeViewProjection(camera),
        kViewportWidth,
        kViewportHeight,
        kDevicePixelRatio,
        pixelScale
    );

    assert(isFiniteViewport(info.viewport));
    assert(!info.usesFrustumBounds);
    assertViewportNotSmaller(info, buildLegacyViewport(plane, camera, pixelScale));
}

void runNearClipSamplingCase() {
    Camera3D camera;
    camera.setCameraAngle(45.0f);
    camera.setTarget(QVector3D(0.0f, 1.0f, 0.0f));
    camera.setPosition(QVector3D(0.0f, -0.5f, 0.02f));
    camera.setUp(QVector3D(0.0f, 0.0f, 1.0f));

    const PlaneDepthRangeInfo depthRange = samplePerspectivePlaneDepthRange(
        computeViewProjection(camera),
        camera.position(),
        QVector3D(0.0f, 0.0f, 0.0f),
        QVector3D(0.0f, 0.0f, 1.0f)
    );

    assert(depthRange.hasIntersection);
    assert(depthRange.minDistance < camera.nearPlane());
}

} // namespace

int main() {
    runOrthographicAlignedCase();
    runPerspectiveXYCase();
    runOffsetPlaneCase();
    runNearGrazingFallbackCase();
    runNearClipSamplingCase();

    std::cout << "Sketch plane viewport prototype: OK" << std::endl;
    return 0;
}
