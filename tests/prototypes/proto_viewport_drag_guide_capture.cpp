#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QPoint>
#include <QPointF>
#include <QSize>
#include <QSurfaceFormat>

#include <gp_Pnt2d.hxx>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>

#include "app/document/Document.h"
#include "core/sketch/Sketch.h"
#include "core/sketch/SketchPoint.h"
#include "ui/viewport/Viewport.h"

namespace {

constexpr int kPointDragThresholdPixels = 4;
constexpr int kViewportInitTimeoutMs = 5000;

void sendMousePress(onecad::ui::Viewport* viewport, const QPoint& pos) {
    const QPointF pointF(pos);
    QMouseEvent event(QEvent::MouseButtonPress,
                      pointF,
                      pointF,
                      pointF,
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(viewport, &event);
}

void sendMouseMove(onecad::ui::Viewport* viewport, const QPoint& pos) {
    const QPointF pointF(pos);
    QMouseEvent event(QEvent::MouseMove,
                      pointF,
                      pointF,
                      pointF,
                      Qt::NoButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(viewport, &event);
}

void sendMouseRelease(onecad::ui::Viewport* viewport, const QPoint& pos) {
    const QPointF pointF(pos);
    QMouseEvent event(QEvent::MouseButtonRelease,
                      pointF,
                      pointF,
                      pointF,
                      Qt::LeftButton,
                      Qt::NoButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(viewport, &event);
}

bool waitForViewportValid(onecad::ui::Viewport& viewport) {
    QElapsedTimer timer;
    timer.start();
    while (!viewport.isValid() && timer.elapsed() < kViewportInitTimeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    return viewport.isValid();
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
    const gp_Pnt2d p = point->position();
    return {p.X(), p.Y()};
}

std::optional<QPoint> findScreenPointForSketch(
    const onecad::ui::Viewport& viewport,
    const onecad::core::sketch::Vec2d& target) {

    const QSize size = viewport.size();
    if (size.width() <= 0 || size.height() <= 0) {
        return std::nullopt;
    }

    double bestDist = std::numeric_limits<double>::infinity();
    QPoint bestPoint(size.width() / 2, size.height() / 2);

    constexpr int kCoarseStep = 8;
    for (int y = 0; y < size.height(); y += kCoarseStep) {
        for (int x = 0; x < size.width(); x += kCoarseStep) {
            const QPoint candidate(x, y);
            const auto sketchPos = viewport.screenToSketch(candidate);
            const double dist = std::hypot(sketchPos.x - target.x, sketchPos.y - target.y);
            if (dist < bestDist) {
                bestDist = dist;
                bestPoint = candidate;
            }
        }
    }

    const int minX = std::max(0, bestPoint.x() - 16);
    const int maxX = std::min(size.width() - 1, bestPoint.x() + 16);
    const int minY = std::max(0, bestPoint.y() - 16);
    const int maxY = std::min(size.height() - 1, bestPoint.y() + 16);
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const QPoint candidate(x, y);
            const auto sketchPos = viewport.screenToSketch(candidate);
            const double dist = std::hypot(sketchPos.x - target.x, sketchPos.y - target.y);
            if (dist < bestDist) {
                bestDist = dist;
                bestPoint = candidate;
            }
        }
    }

    if (bestDist > 1.0) {
        return std::nullopt;
    }
    return bestPoint;
}

QString repoEvidencePath(const QString& fileName) {
    QDir binDir = QFileInfo(QCoreApplication::applicationFilePath()).absoluteDir();
    if (!binDir.cdUp() || !binDir.cdUp()) {
        return {};
    }
    const QString evidenceDir = binDir.filePath(QStringLiteral(".sisyphus/evidence"));
    QDir dir;
    if (!dir.mkpath(evidenceDir)) {
        return {};
    }
    return QDir(evidenceDir).filePath(fileName);
}

bool captureAndSave(onecad::ui::Viewport& viewport,
                    const QString& outputPath,
                    const char* label) {
    if (outputPath.isEmpty()) {
        std::cerr << "FAILED: invalid output path for " << label << std::endl;
        return false;
    }

    viewport.update();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    const QImage image = viewport.grabFramebuffer();
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        std::cerr << "FAILED: grabFramebuffer returned null for " << label << std::endl;
        return false;
    }

    if (!image.save(outputPath)) {
        std::cerr << "FAILED: could not save " << label << " to "
                  << outputPath.toStdString() << std::endl;
        return false;
    }

    const QFileInfo fileInfo(outputPath);
    if (!fileInfo.exists() || fileInfo.size() <= 0) {
        std::cerr << "FAILED: saved file empty for " << label << " at "
                  << outputPath.toStdString() << std::endl;
        return false;
    }

    std::cout << "Saved " << label << ": " << outputPath.toStdString()
              << " (" << fileInfo.size() << " bytes)" << std::endl;
    return true;
}

bool runGuideCaptureScenario(onecad::ui::Viewport& viewport,
                             onecad::app::Document& document) {
    auto sketchOwner = std::make_unique<onecad::core::sketch::Sketch>();
    const std::string sketchId = document.addSketch(std::move(sketchOwner));
    if (sketchId.empty()) {
        std::cerr << "FAILED: addSketch returned empty id" << std::endl;
        return false;
    }

    auto* sketch = document.getSketch(sketchId);
    if (!sketch) {
        std::cerr << "FAILED: could not fetch sketch" << std::endl;
        return false;
    }

    const auto pointA = sketch->addPoint(-20.0, 0.0);
    const auto pointB = sketch->addPoint(20.0, 0.0);
    const auto lineId = sketch->addLine(pointA, pointB);
    if (pointA.empty() || pointB.empty() || lineId.empty()) {
        std::cerr << "FAILED: could not build 2-point line sketch" << std::endl;
        return false;
    }

    const auto horizontalId = sketch->addHorizontal(lineId);
    if (horizontalId.empty()) {
        std::cerr << "FAILED: addHorizontal failed" << std::endl;
        return false;
    }

    const auto solveResult = sketch->solve();
    if (!solveResult.success) {
        std::cerr << "FAILED: initial sketch solve failed" << std::endl;
        return false;
    }

    const auto freeDirections = sketch->getPointFreeDirections(pointB);
    if (freeDirections.size() != 1) {
        std::cerr << "FAILED: expected exactly 1 free direction, got "
                  << freeDirections.size() << std::endl;
        return false;
    }

    viewport.enterSketchMode(sketch);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    const auto startSketchPos = getPointPosition(sketch, pointB);
    const auto startScreen = findScreenPointForSketch(viewport, startSketchPos);
    if (!startScreen.has_value()) {
        std::cerr << "FAILED: could not map endpoint to screen" << std::endl;
        viewport.exitSketchMode();
        return false;
    }

    const QPoint thresholdMove = *startScreen + QPoint(kPointDragThresholdPixels + 3, 0);
    const QPoint dragMove = thresholdMove + QPoint(20, 0);

    sendMousePress(&viewport, *startScreen);
    sendMouseMove(&viewport, thresholdMove);
    sendMouseMove(&viewport, dragMove);

    const QString happyPath = repoEvidencePath(QStringLiteral("task-10-guide-happy.png"));
    if (!captureAndSave(viewport, happyPath, "task-10-guide-happy.png")) {
        viewport.exitSketchMode();
        return false;
    }

    sendMouseRelease(&viewport, dragMove);
    viewport.update();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    const QString idlePath = repoEvidencePath(QStringLiteral("task-10-guide-idle.png"));
    if (!captureAndSave(viewport, idlePath, "task-10-guide-idle.png")) {
        viewport.exitSketchMode();
        return false;
    }

    viewport.exitSketchMode();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    return true;
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

    if (!waitForViewportValid(viewport)) {
        std::cerr << "FAILED: viewport GL init" << std::endl;
        std::fflush(stdout);
        std::fflush(stderr);
        std::_Exit(3);
    }

    const QString platformName = QGuiApplication::platformName();
    std::cerr << "GL init info: platform=" << platformName.toStdString()
              << ", viewportContext=" << viewport.context()
              << ", currentContext=" << QOpenGLContext::currentContext()
              << ", viewportValid=" << viewport.isValid()
              << std::endl;

    auto document = std::make_unique<onecad::app::Document>();
    viewport.setDocument(document.get());

    const bool ok = runGuideCaptureScenario(viewport, *document);
    std::fflush(stdout);
    std::fflush(stderr);
    std::_Exit(ok ? 0 : 1);
}
