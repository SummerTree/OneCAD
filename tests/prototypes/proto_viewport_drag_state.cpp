#include <QApplication>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QMouseEvent>
#include <QPoint>
#include <QSize>
#include <QSurfaceFormat>
#include <QGuiApplication>
#include <QOpenGLContext>
#include <QWidget>
#include <cstdio>
#include <cstdlib>

#include <gp_Pnt2d.hxx>

#include <cmath>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "app/document/Document.h"
#include "core/sketch/Sketch.h"
#include "core/sketch/SketchPoint.h"
#include "ui/viewport/Viewport.h"

namespace {

constexpr int kPointDragThresholdPixels = 4;
constexpr double kSketchTolerance = 1e-6;
constexpr int kViewportInitTimeoutMs = 5000;

void sendMousePress(onecad::ui::Viewport* viewport, const QPoint& pos) {
    QMouseEvent event(QEvent::MouseButtonPress,
                      pos,
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(viewport, &event);
}

void sendMouseMove(onecad::ui::Viewport* viewport, const QPoint& pos) {
    QMouseEvent event(QEvent::MouseMove,
                      pos,
                      Qt::NoButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(viewport, &event);
}

void sendMouseRelease(onecad::ui::Viewport* viewport, const QPoint& pos) {
    QMouseEvent event(QEvent::MouseButtonRelease,
                      pos,
                      Qt::LeftButton,
                      Qt::NoButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(viewport, &event);
}

std::optional<QPoint> findEmptyScreenPoint(
    const onecad::ui::Viewport& viewport,
    const onecad::core::sketch::Vec2d& avoidPosition) {

    const QSize viewportSize = viewport.size();
    const std::vector<QPoint> candidates = {
        {20, 20},
        {viewportSize.width() / 2, 20},
        {viewportSize.width() - 20, 20},
        {20, viewportSize.height() / 2},
        {viewportSize.width() - 20, viewportSize.height() - 20},
        {viewportSize.width() / 2, viewportSize.height() / 2}
    };

    for (const QPoint& candidate : candidates) {
        if (candidate.x() < 0 || candidate.y() < 0 ||
            candidate.x() >= viewportSize.width() ||
            candidate.y() >= viewportSize.height()) {
            continue;
        }
        const auto sketchPos = viewport.screenToSketch(candidate);
        const double distance = std::hypot(sketchPos.x - avoidPosition.x,
                                           sketchPos.y - avoidPosition.y);
        if (distance > 1.0) {
            return candidate;
        }
    }

    return std::nullopt;
}

onecad::core::sketch::Sketch* createSketchWithPoint(
    onecad::app::Document& document,
    double x,
    double y,
    onecad::core::sketch::EntityID& outPointId) {

    auto sketchOwner = std::make_unique<onecad::core::sketch::Sketch>();
    const std::string sketchId = document.addSketch(std::move(sketchOwner));
    if (sketchId.empty()) {
        return nullptr;
    }
    auto* sketch = document.getSketch(sketchId);
    if (!sketch) {
        return nullptr;
    }
    outPointId = sketch->addPoint(x, y);
    return sketch;
}

onecad::core::sketch::Vec2d getPointPosition(
    const onecad::core::sketch::Sketch* sketch,
    const onecad::core::sketch::EntityID& pointId) {

    if (!sketch) {
        return {0.0, 0.0};
    }
    const auto* point = sketch->getEntityAs<onecad::core::sketch::SketchPoint>(pointId);
    if (!point) {
        return {0.0, 0.0};
    }
    const gp_Pnt2d position = point->position();
    return {position.X(), position.Y()};
}

bool runJitterScenario(onecad::ui::Viewport& viewport,
                       onecad::app::Document& document) {

    onecad::core::sketch::EntityID pointId;
    auto* sketch = createSketchWithPoint(document, 0.0, 0.0, pointId);
    if (!sketch) {
        std::cerr << "Failed to create jitter sketch" << std::endl;
        return false;
    }

    viewport.enterSketchMode(sketch);
    QCoreApplication::processEvents();

    const auto initialPosition = getPointPosition(sketch, pointId);
    const auto startPoint = findEmptyScreenPoint(viewport, initialPosition);
    if (!startPoint.has_value()) {
        std::cerr << "Could not find an empty screen location for jitter test" << std::endl;
        viewport.exitSketchMode();
        document.clear();
        return false;
    }

    sendMousePress(&viewport, *startPoint);
    const QPoint jitterTarget = *startPoint + QPoint(kPointDragThresholdPixels - 1, 0);
    sendMouseMove(&viewport, jitterTarget);
    sendMouseMove(&viewport, jitterTarget);
    sendMouseRelease(&viewport, jitterTarget);

    const auto finalPosition = getPointPosition(sketch, pointId);
    const double movement = std::hypot(finalPosition.x - initialPosition.x,
                                       finalPosition.y - initialPosition.y);

    viewport.exitSketchMode();
    document.clear();

    if (movement > kSketchTolerance) {
        std::cerr << "Small jitter unexpectedly translated the sketch point by "
                  << movement << " units" << std::endl;
        return false;
    }

    std::cout << "Jitter scenario passed: no translation for tiny drags." << std::endl;
    return true;
}

bool runSketchMoveBugScenario(onecad::ui::Viewport& viewport,
                              onecad::app::Document& document) {

    onecad::core::sketch::EntityID pointId;
    auto* sketch = createSketchWithPoint(document, 12.0, -3.0, pointId);
    if (!sketch) {
        std::cerr << "Failed to create sketch for move reproduction" << std::endl;
        return false;
    }

    viewport.enterSketchMode(sketch);
    QCoreApplication::processEvents();

    const auto initialPosition = getPointPosition(sketch, pointId);
    const auto startPoint = findEmptyScreenPoint(viewport, initialPosition);
    if (!startPoint.has_value()) {
        std::cerr << "Could not find an empty screen location for move reproduction" << std::endl;
        viewport.exitSketchMode();
        document.clear();
        return false;
    }

    sendMousePress(&viewport, *startPoint);
    const QPoint thresholdPoint = *startPoint + QPoint(kPointDragThresholdPixels + 2, 0);
    sendMouseMove(&viewport, thresholdPoint);

    const QPoint translationPoint = thresholdPoint + QPoint(12, 5);
    sendMouseMove(&viewport, translationPoint);
    sendMouseRelease(&viewport, translationPoint);

    const auto finalPosition = getPointPosition(sketch, pointId);
    const double movement = std::hypot(finalPosition.x - initialPosition.x,
                                       finalPosition.y - initialPosition.y);

    viewport.exitSketchMode();
    document.clear();

    if (movement <= kSketchTolerance) {
        return true;
    }

    std::cerr << "RED: Unexpected sketch translation reproduces the drag-state bug ("
              << "start=(" << initialPosition.x << ", " << initialPosition.y << ") "
              << "end=(" << finalPosition.x << ", " << finalPosition.y << "))" << std::endl;
    return false;
}

}

int main(int argc, char* argv[]) {
    qputenv("QT_OPENGL", "software");
    QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);

    QSurfaceFormat format;
    format.setVersion(4, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(4);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);

    onecad::ui::Viewport viewport;
    viewport.setAttribute(Qt::WA_DontShowOnScreen);
    viewport.resize(960, 640);
    viewport.show();
    viewport.update();

    QElapsedTimer timer;
    timer.start();
    while (!viewport.isValid() && timer.elapsed() < kViewportInitTimeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }

    const QString platformName = QGuiApplication::platformName();
    const QOpenGLContext* viewportContext = viewport.context();
    const QOpenGLContext* currentContext = QOpenGLContext::currentContext();
    std::cerr << "GL init info: platform=" << platformName.toStdString()
              << ", viewportContext=" << viewportContext
              << ", currentContext=" << currentContext
              << ", viewportValid=" << viewport.isValid()
              << std::endl;

    if (!viewport.isValid()) {
        std::cerr << "FAILED: viewport GL init" << std::endl;
        std::fflush(stdout);
        std::fflush(stderr);
        std::_Exit(3);
    }

    auto document = std::make_unique<onecad::app::Document>();
    viewport.setDocument(document.get());

    const bool skipSketchMoveScenario = std::getenv("PROTO_VIEWPORT_DRAG_SKIP_MOVE") != nullptr;

    int exitCode = 0;
    if (!runJitterScenario(viewport, *document)) {
        std::cerr << "RED: jitter scenario translated the sketch" << std::endl;
        exitCode = 2;
    } else if (skipSketchMoveScenario) {
        std::cout << "SKIP: empty-area move scenario disabled by PROTO_VIEWPORT_DRAG_SKIP_MOVE" << std::endl;
        exitCode = 0;
    } else if (!runSketchMoveBugScenario(viewport, *document)) {
        std::cerr << "RED: empty-area drag moved sketch" << std::endl;
        exitCode = 1;
    } else {
        std::cout << "GREEN: empty-area drag did not move the sketch" << std::endl;
        exitCode = 0;
    }

    std::fflush(stdout);
    std::fflush(stderr);
    std::_Exit(exitCode);
}
