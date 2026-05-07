#include "Viewport.h"
#include "../../render/Camera3D.h"
#include "../../core/sketch/Sketch.h"

#include <QLoggingCategory>
#include <QWheelEvent>
#include <QtMath>
#include <cmath>

namespace onecad {
namespace ui {

Q_DECLARE_LOGGING_CATEGORY(logUiInput)

namespace sketch = core::sketch;

namespace {
constexpr float kOrbitSensitivity = 0.3f;
constexpr float kNativeZoomMinFactor = 0.7f;
constexpr float kNativeZoomMaxFactor = 1.3f;
constexpr qreal kNativeZoomScaleValueMin = 0.5;
constexpr qreal kNativeZoomScaleValueMax = 1.5;
constexpr qint64 kNativeZoomActiveTimeoutMs = 250;
} // namespace

void Viewport::handlePan(float dx, float dy) {
    m_isNavigating = true;
    m_navigationTimer->start(150);
    m_camera->pan(dx, dy);
    update();
    emit cameraChanged();
}

void Viewport::handleOrbit(float dx, float dy) {
    m_isNavigating = true;
    m_navigationTimer->start(150);
    m_camera->orbit(-dx * kOrbitSensitivity, dy * kOrbitSensitivity);
    update();
    emit cameraChanged();
}

void Viewport::handleZoom(float delta) {
    applyZoomFactor(1.0f - delta * 0.001f);
}

void Viewport::applyZoomFactor(float factor) {
    if (!m_camera || !std::isfinite(factor) || factor <= 0.0f) {
        return;
    }
    m_isNavigating = true;
    m_navigationTimer->start(150);
    m_camera->zoomByFactor(factor);
    update();
    emit cameraChanged();
}

void Viewport::applyAnchoredZoomFactor(float factor, const QPointF& screenPos) {
    if (!m_camera || !std::isfinite(factor) || factor <= 0.0f) {
        return;
    }

    m_isNavigating = true;
    m_navigationTimer->start(150);
    QVector3D preZoomPoint;
    bool preZoomValid = false;

    if (!m_zoomAnchor.valid) {
        initializeZoomAnchor(screenPos);
    }
    if (m_zoomAnchor.valid) {
        preZoomValid = intersectScreenPointWithZoomPlane(screenPos, &preZoomPoint);
    }

    m_camera->zoomByFactor(factor);

    if (preZoomValid) {
        QVector3D postZoomPoint;
        if (intersectScreenPointWithZoomPlane(screenPos, &postZoomPoint)) {
            const QVector3D correction = preZoomPoint - postZoomPoint;
            if (std::isfinite(correction.x()) &&
                std::isfinite(correction.y()) &&
                std::isfinite(correction.z())) {
                m_camera->setPosition(m_camera->position() + correction);
                m_camera->setTarget(m_camera->target() + correction);
                qCDebug(logUiInput) << "zoom_anchor_correction"
                                    << "magnitude=" << correction.length();
            }
        }
    }

    update();
    emit cameraChanged();
}

bool Viewport::isTrackpadScrollEvent(const QWheelEvent* event) const {
    if (!event) {
        return false;
    }
    const bool hasPixelDelta = !event->pixelDelta().isNull();
    const bool hasAngleDelta = !event->angleDelta().isNull();
    return (event->phase() != Qt::NoScrollPhase) || (hasPixelDelta && !hasAngleDelta);
}

bool Viewport::isNativeZoomActive() const {
    if (!m_nativeGestureTimer.isValid()) {
        return false;
    }

    const qint64 now = m_nativeGestureTimer.elapsed();
    const bool active = m_nativeZoomActive &&
        m_lastNativeZoomEventMs >= 0 &&
        (now - m_lastNativeZoomEventMs) < kNativeZoomActiveTimeoutMs;
    const bool suppressing = m_nativeZoomSuppressUntilMs >= 0 &&
        now < m_nativeZoomSuppressUntilMs;

    return active || suppressing;
}

bool Viewport::normalizeNativeZoomFactor(qreal rawValue, float* outFactor) const {
    if (!outFactor || !std::isfinite(rawValue)) {
        return false;
    }

    qreal normalized = rawValue;
    if (rawValue > kNativeZoomScaleValueMin && rawValue < kNativeZoomScaleValueMax) {
        normalized = rawValue - 1.0;
    }

    float factor = static_cast<float>(1.0 - normalized);
    factor = std::clamp(factor, kNativeZoomMinFactor, kNativeZoomMaxFactor);
    *outFactor = factor;
    return true;
}

bool Viewport::initializeZoomAnchor(const QPointF& screenPos) {
    if (!m_camera) {
        clearZoomAnchor();
        return false;
    }

    QVector3D planePoint;
    QVector3D planeNormal;

    if (m_inSketchMode && m_activeSketch) {
        const auto& plane = m_activeSketch->getPlane();
        planePoint = QVector3D(plane.origin.x, plane.origin.y, plane.origin.z);
        planeNormal = QVector3D(plane.normal.x, plane.normal.y, plane.normal.z);
    } else {
        planePoint = m_camera->target();
        planeNormal = m_camera->forward();
    }

    if (planeNormal.lengthSquared() < 1e-8f) {
        clearZoomAnchor();
        return false;
    }
    planeNormal.normalize();

    QVector3D intersection;
    if (!intersectScreenPointWithPlane(screenPos, planePoint, planeNormal, &intersection)) {
        clearZoomAnchor();
        return false;
    }

    m_zoomAnchor.valid = true;
    m_zoomAnchor.planePoint = intersection;
    m_zoomAnchor.planeNormal = planeNormal;
    return true;
}

bool Viewport::intersectScreenPointWithZoomPlane(const QPointF& screenPos, QVector3D* outPoint) const {
    if (!m_zoomAnchor.valid) {
        return false;
    }
    return intersectScreenPointWithPlane(screenPos,
                                         m_zoomAnchor.planePoint,
                                         m_zoomAnchor.planeNormal,
                                         outPoint);
}

bool Viewport::intersectScreenPointWithPlane(const QPointF& screenPos,
                                             const QVector3D& planePoint,
                                             const QVector3D& planeNormal,
                                             QVector3D* outPoint) const {
    if (!m_camera || !outPoint || m_width <= 0 || m_height <= 0) {
        return false;
    }

    QVector3D normalizedNormal = planeNormal;
    if (normalizedNormal.lengthSquared() < 1e-8f) {
        return false;
    }
    normalizedNormal.normalize();

    const float aspectRatio = static_cast<float>(m_width) / static_cast<float>(m_height);
    const QMatrix4x4 view = m_camera->viewMatrix();
    const QMatrix4x4 projection = buildProjectionMatrix(aspectRatio);
    const QMatrix4x4 viewProjection = projection * view;

    bool invertible = false;
    const QMatrix4x4 inverse = viewProjection.inverted(&invertible);
    if (!invertible) {
        return false;
    }

    const float ndcX = (2.0f * static_cast<float>(screenPos.x()) / static_cast<float>(m_width)) - 1.0f;
    const float ndcY = 1.0f - (2.0f * static_cast<float>(screenPos.y()) / static_cast<float>(m_height));
    const QVector4D nearPoint = inverse * QVector4D(ndcX, ndcY, -1.0f, 1.0f);
    const QVector4D farPoint = inverse * QVector4D(ndcX, ndcY, 1.0f, 1.0f);
    if (std::abs(nearPoint.w()) < 1e-8f || std::abs(farPoint.w()) < 1e-8f) {
        return false;
    }

    const QVector3D rayOrigin = nearPoint.toVector3D() / nearPoint.w();
    const QVector3D rayEnd = farPoint.toVector3D() / farPoint.w();
    const QVector3D rayDir = (rayEnd - rayOrigin).normalized();
    const float denom = QVector3D::dotProduct(rayDir, normalizedNormal);
    // Near-parallel guard: reject grazing angles to avoid unstable far intersections
    if (std::abs(denom) < 1e-4f) {
        return false;
    }

    const float t = QVector3D::dotProduct(planePoint - rayOrigin, normalizedNormal) / denom;
    if (!std::isfinite(t) || t < 0.0f) {
        return false;
    }

    // Distance cap: reject intersections unreasonably far from the plane point
    const float planeDist = (planePoint - rayOrigin).length();
    const float maxT = std::max(planeDist * 100.0f, 1e5f);
    if (t > maxT) {
        return false;
    }

    const QVector3D hit = rayOrigin + rayDir * t;
    if (!std::isfinite(hit.x()) || !std::isfinite(hit.y()) || !std::isfinite(hit.z())) {
        return false;
    }

    *outPoint = hit;
    return true;
}

void Viewport::clearZoomAnchor() {
    m_zoomAnchor = {};
}

void Viewport::animateCamera(const CameraState& targetState) {
    if (!m_camera || !m_cameraAnimation) return;

    // Capture start state
    CameraState startState;
    startState.position = m_camera->position();
    startState.target = m_camera->target();
    startState.up = m_camera->up();
    startState.angle = m_camera->cameraAngle();
    startState.orthoScale = m_camera->orthoScale();

    float startVisualScale = startState.orthoScale;
    if (startState.angle >= 0.01f) {
        const float startFov = qMax(startState.angle, 5.0f);
        const float startHalfFovRad = qDegreesToRadians(startFov * 0.5f);
        const float startDistance = (startState.position - startState.target).length();
        startVisualScale = 2.0f * startDistance * qTan(startHalfFovRad);
    }

    float targetVisualScale = targetState.orthoScale;
    if (targetState.angle >= 0.01f) {
        const float targetFov = qMax(targetState.angle, 5.0f);
        const float targetHalfFovRad = qDegreesToRadians(targetFov * 0.5f);
        const float targetDistance = (targetState.position - targetState.target).length();
        targetVisualScale = 2.0f * targetDistance * qTan(targetHalfFovRad);
    }

    m_cameraAnimation->stop();
    m_cameraAnimation->disconnect(this);

    connect(m_cameraAnimation, &QVariantAnimation::valueChanged, this, [this, startState, targetState, startVisualScale, targetVisualScale](const QVariant& value) {
        float t = value.toFloat();

        QVector3D newPos = startState.position * (1.0f - t) + targetState.position * t;
        QVector3D newTarget = startState.target * (1.0f - t) + targetState.target * t;
        QVector3D newUp = (startState.up * (1.0f - t) + targetState.up * t).normalized();
        float newAngle = startState.angle * (1.0f - t) + targetState.angle * t;
        float newVisualScale = startVisualScale * (1.0f - t) + targetVisualScale * t;
        QVector3D viewDir = newPos - newTarget;
        if (viewDir.lengthSquared() < 1e-9f) {
            viewDir = startState.position - startState.target;
        }
        if (viewDir.lengthSquared() < 1e-9f) {
            viewDir = QVector3D(0.0f, 1.0f, 0.0f);
        }
        viewDir.normalize();

        if (newAngle < 0.01f) {
            m_camera->setCameraAngle(0.0f);
            m_camera->setOrthoScale(newVisualScale);
            m_camera->setPosition(newPos);
            m_camera->setTarget(newTarget);
            m_camera->setUp(newUp);
        } else {
            const float effectiveFov = qMax(newAngle, 5.0f);
            const float halfFovRad = qDegreesToRadians(effectiveFov * 0.5f);
            float desiredDistance = newVisualScale * 0.5f / qTan(halfFovRad);
            desiredDistance = qBound(1.0f, desiredDistance, 50000.0f);
            const QVector3D adjustedPos = newTarget + viewDir * desiredDistance;

            m_camera->setCameraAngle(newAngle);
            m_camera->setOrthoScale(newVisualScale);
            m_camera->setPosition(adjustedPos);
            m_camera->setTarget(newTarget);
            m_camera->setUp(newUp);
        }

        update();
        emit cameraChanged();
    });

    connect(m_cameraAnimation, &QVariantAnimation::finished, this, [this, targetState]() {
        m_camera->setCameraAngle(targetState.angle);
        m_camera->setOrthoScale(targetState.orthoScale);
        m_camera->setPosition(targetState.position);
        m_camera->setTarget(targetState.target);
        m_camera->setUp(targetState.up);

        float finalDist = m_camera->distance();
        float finalScale = (m_camera->projectionType() == render::Camera3D::ProjectionType::Perspective)
            ? 2.0f * finalDist * qTan(qDegreesToRadians(m_camera->fov() * 0.5f))
            : m_camera->orthoScale();
        qDebug() << "[AnimationFinished] angle=" << m_camera->cameraAngle()
                 << "distance=" << finalDist << "visualScale=" << finalScale
                 << "expected=" << targetState.orthoScale;

        update();
        emit cameraChanged();
    });

    m_cameraAnimation->setStartValue(0.0f);
    m_cameraAnimation->setEndValue(1.0f);
    m_cameraAnimation->start();
}

void Viewport::setFrontView() {
    m_camera->setFrontView();
    update();
    emit cameraChanged();
}

void Viewport::setBackView() {
    m_camera->setBackView();
    update();
    emit cameraChanged();
}

void Viewport::setLeftView() {
    m_camera->setLeftView();
    update();
    emit cameraChanged();
}

void Viewport::setRightView() {
    m_camera->setRightView();
    update();
    emit cameraChanged();
}

void Viewport::setTopView() {
    m_camera->setTopView();
    update();
    emit cameraChanged();
}

void Viewport::setBottomView() {
    m_camera->setBottomView();
    update();
    emit cameraChanged();
}

void Viewport::setIsometricView() {
    m_camera->setIsometricView();
    update();
    emit cameraChanged();
}

void Viewport::resetView() {
    m_camera->reset();
    update();
    emit cameraChanged();
}

void Viewport::setCameraAngle(float degrees) {
    m_camera->setCameraAngle(degrees);
    update();
    emit cameraChanged();
}

} // namespace ui
} // namespace onecad
