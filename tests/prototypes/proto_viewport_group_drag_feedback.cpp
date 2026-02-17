#include <QApplication>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QMouseEvent>
#include <QPoint>
#include <QPointF>
#include <QSize>
#include <QSurfaceFormat>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "app/document/Document.h"
#include "app/selection/SelectionManager.h"
#include "app/selection/SelectionTypes.h"
#include "core/loop/RegionUtils.h"
#include "core/sketch/Sketch.h"
#include "core/sketch/SketchPoint.h"
#include "core/sketch/SketchRenderer.h"
#define private public
#include "ui/viewport/Viewport.h"
#undef private

namespace {

constexpr int kPointDragThresholdPixels = 4;
constexpr int kViewportInitTimeoutMs = 5000;

struct SketchWithPoints {
    onecad::core::sketch::Sketch* sketch = nullptr;
    std::string sketchId;
    std::vector<onecad::core::sketch::EntityID> pointIds;
};

void sendMousePress(onecad::ui::Viewport* viewport,
                    const QPoint& pos,
                    Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    const QPointF pointF(pos);
    QMouseEvent event(QEvent::MouseButtonPress,
                      pointF,
                      pointF,
                      pointF,
                      Qt::LeftButton,
                      Qt::LeftButton,
                      modifiers);
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

void sendMouseDoubleClick(onecad::ui::Viewport* viewport, const QPoint& pos) {
    const QPointF pointF(pos);
    QMouseEvent event(QEvent::MouseButtonDblClick,
                      pointF,
                      pointF,
                      pointF,
                      Qt::LeftButton,
                      Qt::LeftButton,
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

double distance(const onecad::core::sketch::Vec2d& lhs,
                const onecad::core::sketch::Vec2d& rhs) {
    const double dx = lhs.x - rhs.x;
    const double dy = lhs.y - rhs.y;
    return std::hypot(dx, dy);
}

std::optional<QPoint> findScreenPointForSketch(
    const onecad::ui::Viewport& viewport,
    const onecad::core::sketch::Vec2d& target,
    double tolerance = 5.0,
    const std::vector<onecad::core::sketch::Vec2d>& avoidPoints = {},
    double avoidRadius = 1.0) {
    const QSize size = viewport.size();
    if (size.isEmpty()) {
        return std::nullopt;
    }

    constexpr int kStep = 4;
    double bestDist = std::numeric_limits<double>::infinity();
    QPoint bestPoint(-1, -1);
    for (int y = kStep / 2; y < size.height(); y += kStep) {
        for (int x = kStep / 2; x < size.width(); x += kStep) {
            const QPoint pos(x, y);
            const auto sketchPos = viewport.screenToSketch(pos);
            const double d = distance(sketchPos, target);
            if (d > tolerance) {
                continue;
            }
            bool tooClose = false;
            for (const auto& avoid : avoidPoints) {
                if (distance(sketchPos, avoid) < avoidRadius) {
                    tooClose = true;
                    break;
                }
            }
            if (tooClose) {
                continue;
            }
            if (d < bestDist) {
                bestDist = d;
                bestPoint = pos;
            }
        }
    }

    if (bestPoint.x() >= 0 && bestPoint.y() >= 0) {
        return bestPoint;
    }
    return std::nullopt;
}

SketchWithPoints createSketchWithPoints(onecad::app::Document& document,
                                       const std::vector<onecad::core::sketch::Vec2d>& positions) {
    SketchWithPoints result;
    auto sketchOwner = std::make_unique<onecad::core::sketch::Sketch>();
    result.sketchId = document.addSketch(std::move(sketchOwner));
    if (result.sketchId.empty()) {
        return result;
    }
    result.sketch = document.getSketch(result.sketchId);
    if (!result.sketch) {
        result.sketchId.clear();
        return result;
    }
    result.pointIds.reserve(positions.size());
    for (const auto& pt : positions) {
        result.pointIds.push_back(result.sketch->addPoint(pt.x, pt.y));
    }
    return result;
}

onecad::core::sketch::Vec2d getPointPosition(const onecad::core::sketch::Sketch* sketch,
                                             const onecad::core::sketch::EntityID& id) {
    if (!sketch) {
        return {};
    }
    const auto* point = sketch->getEntityAs<onecad::core::sketch::SketchPoint>(id);
    if (!point) {
        return {};
    }
    const auto pos = point->position();
    return {pos.X(), pos.Y()};
}

bool performGroupDragTest(onecad::ui::Viewport& viewport,
                          onecad::core::sketch::Sketch* sketch,
                          const std::string& sketchId,
                          const std::vector<onecad::core::sketch::EntityID>& pointIds,
                          bool expectSuccess,
                          bool fixPrimaryPoint,
                          QString* statusMessage) {
    if (!sketch || pointIds.size() < 2) {
        return false;
    }
    auto* selectionManager = viewport.findChild<onecad::app::selection::SelectionManager*>();
    if (!selectionManager) {
        return false;
    }

    viewport.enterSketchMode(sketch);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    std::vector<onecad::app::selection::SelectionItem> selection;
    for (size_t i = 0; i < 2; ++i) {
        onecad::app::selection::SelectionItem item;
        item.kind = onecad::app::selection::SelectionKind::SketchPoint;
        item.id = {sketchId, pointIds[i]};
        item.screenDistance = 0.0;
        item.priority = 0;
        selection.push_back(item);
    }
    selectionManager->replaceSelection(selection);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    if (fixPrimaryPoint) {
        sketch->addFixed(pointIds.front());
        sketch->solve();
    }

    const auto initialPos0 = getPointPosition(sketch, pointIds[0]);
    const auto initialPos1 = getPointPosition(sketch, pointIds[1]);
    auto start = findScreenPointForSketch(viewport, initialPos0);
    if (!start.has_value()) {
        viewport.exitSketchMode();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        std::cerr << "TRACE: no screen point near " << initialPos0.x << "," << initialPos0.y << std::endl;
        return false;
    }

    const QPoint target = *start + QPoint(42, 12);
    sendMousePress(&viewport, *start, Qt::ShiftModifier);
    if (selectionManager->selection().size() < 2) {
        std::cerr << "TRACE: selection size after press " << selectionManager->selection().size() << std::endl;
    }
    sendMouseMove(&viewport, *start + QPoint(8, 2));
    sendMouseMove(&viewport, target);
    sendMouseRelease(&viewport, target);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    const auto finalPos0 = getPointPosition(sketch, pointIds[0]);
    const auto finalPos1 = getPointPosition(sketch, pointIds[1]);
    viewport.exitSketchMode();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    const double move0 = distance(initialPos0, finalPos0);
    const double move1 = distance(initialPos1, finalPos1);
    const double sharedDelta = distance({finalPos0.x - initialPos0.x, finalPos0.y - initialPos0.y},
                                        {finalPos1.x - initialPos1.x, finalPos1.y - initialPos1.y});

    if (expectSuccess) {
        if (move0 < 1e-4 || move1 < 1e-4) {
            std::cerr << "TRACE: small move " << move0 << "," << move1 << std::endl;
            return false;
        }
        if (sharedDelta > 1e-3) {
            std::cerr << "TRACE: mismatched delta " << sharedDelta << std::endl;
            return false;
        }
        return true;
    }

    if (move0 > 1e-4 || move1 > 1e-4) {
        std::cerr << "TRACE: moved despite block " << move0 << "," << move1 << std::endl;
        return false;
    }
    if (statusMessage && statusMessage->isEmpty()) {
        return false;
    }
    return true;
}

bool runGroupDragSuccessScenario(onecad::ui::Viewport& viewport,
                                 onecad::app::Document& document) {
    const std::vector<onecad::core::sketch::Vec2d> positions = {{0.0, 0.0}, {10.0, 0.0}};
    const auto sketchData = createSketchWithPoints(document, positions);
    if (!sketchData.sketch) {
        return false;
    }
    bool ok = performGroupDragTest(viewport,
                                   sketchData.sketch,
                                   sketchData.sketchId,
                                   sketchData.pointIds,
                                   true,
                                   false,
                                   nullptr);
    if (ok) {
        std::cout << "PASS: Scenario A – group drag moves selected points together." << std::endl;
    }
    return ok;
}

bool runGroupDragRejectScenario(onecad::ui::Viewport& viewport,
                                onecad::app::Document& document,
                                QString* capturedStatus) {
    const std::vector<onecad::core::sketch::Vec2d> positions = {{0.0, 0.0}, {12.0, 0.0}};
    const auto sketchData = createSketchWithPoints(document, positions);
    if (!sketchData.sketch) {
        return false;
    }
    if (capturedStatus) {
        capturedStatus->clear();
    }
    const bool ok = performGroupDragTest(viewport,
                                         sketchData.sketch,
                                         sketchData.sketchId,
                                         sketchData.pointIds,
                                         false,
                                         true,
                                         capturedStatus);
    if (ok) {
        std::cout << "PASS: Scenario B – blocked group drag emits status feedback." << std::endl;
    }
    return ok;
}

bool runRegionSelectionDragScenario(onecad::ui::Viewport& viewport,
                                    onecad::app::Document& document) {
    const std::vector<onecad::core::sketch::Vec2d> positions = {{-2.0, -1.0}, {2.0, -1.0}, {0.0, 3.0}};
    auto sketchData = createSketchWithPoints(document, positions);
    if (!sketchData.sketch || sketchData.pointIds.size() < 3) {
        return false;
    }

    sketchData.sketch->addLine(sketchData.pointIds[0], sketchData.pointIds[1]);
    sketchData.sketch->addLine(sketchData.pointIds[1], sketchData.pointIds[2]);
    sketchData.sketch->addLine(sketchData.pointIds[2], sketchData.pointIds[0]);

    auto* selectionManager = viewport.findChild<onecad::app::selection::SelectionManager*>();
    if (!selectionManager) {
        return false;
    }

    const auto regionIdOpt = onecad::core::loop::getRegionIdContainingEntity(
        *sketchData.sketch, sketchData.pointIds[0]);
    if (!regionIdOpt.has_value()) {
        return false;
    }

    viewport.enterSketchMode(sketchData.sketch);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    onecad::app::selection::SelectionItem regionItem;
    regionItem.kind = onecad::app::selection::SelectionKind::SketchRegion;
    regionItem.id = {sketchData.sketchId, *regionIdOpt};
    regionItem.screenDistance = 0.0;
    regionItem.priority = -10;
    selectionManager->replaceSelection({regionItem});
    if (viewport.sketchRenderer()) {
        viewport.sketchRenderer()->clearRegionSelection();
        viewport.sketchRenderer()->toggleRegionSelection(*regionIdOpt);
        viewport.sketchRenderer()->updateGeometry();
    }
    viewport.m_selectedRegionId = *regionIdOpt;
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    const auto initial0 = getPointPosition(sketchData.sketch, sketchData.pointIds[0]);
    const auto initial1 = getPointPosition(sketchData.sketch, sketchData.pointIds[1]);
    const auto initial2 = getPointPosition(sketchData.sketch, sketchData.pointIds[2]);
    const onecad::core::sketch::Vec2d centroid{
        (positions[0].x + positions[1].x + positions[2].x) / 3.0,
        (positions[0].y + positions[1].y + positions[2].y) / 3.0
    };
    auto start = findScreenPointForSketch(viewport,
                                          centroid,
                                          5.0,
                                          {initial0, initial1, initial2},
                                          1.5);
    if (!start.has_value()) {
        viewport.exitSketchMode();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        return false;
    }

    const QPoint dragTarget = *start + QPoint(36, 28);
    viewport.m_lastMousePos = *start;
    sendMouseMove(&viewport, dragTarget);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    const auto after0 = getPointPosition(sketchData.sketch, sketchData.pointIds[0]);
    const auto after1 = getPointPosition(sketchData.sketch, sketchData.pointIds[1]);
    const auto after2 = getPointPosition(sketchData.sketch, sketchData.pointIds[2]);
    viewport.exitSketchMode();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    if (distance(initial0, after0) > 1e-4 ||
        distance(initial1, after1) > 1e-4 ||
        distance(initial2, after2) > 1e-4) {
        return false;
    }

    std::cout << "PASS: Scenario C – region selection drag leaves geometry in place." << std::endl;
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
        std::cerr << "FAILED: viewport GL never initialized." << std::endl;
        return 2;
    }

    auto document = std::make_unique<onecad::app::Document>();
    viewport.setDocument(document.get());

    QString statusMessage;
    QObject::connect(&viewport,
                     &onecad::ui::Viewport::statusMessageRequested,
                     &viewport,
                     [&statusMessage](const QString& message) {
                         if (!message.isEmpty()) {
                             statusMessage = message;
                         }
                     });

    if (!runGroupDragSuccessScenario(viewport, *document)) {
        std::cerr << "FAILED: scenario A (group drag)" << std::endl;
        return 1;
    }

    statusMessage.clear();
    if (!runGroupDragRejectScenario(viewport, *document, &statusMessage)) {
        std::cerr << "FAILED: scenario B (blocked group drag)" << std::endl;
        return 1;
    }
    if (statusMessage.isEmpty()) {
        std::cerr << "FAILED: expected status message for blocked group drag" << std::endl;
        return 1;
    }

    if (!runRegionSelectionDragScenario(viewport, *document)) {
        std::cerr << "FAILED: scenario C (region selection drag)" << std::endl;
        return 1;
    }

    std::cout << "GREEN: viewport group drag feedback prototype passed." << std::endl;
    return 0;
}
