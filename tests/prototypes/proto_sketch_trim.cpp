#include "sketch/Sketch.h"
#include "sketch/SketchArc.h"
#include "sketch/SketchCircle.h"
#include "sketch/SketchLine.h"
#include "sketch/SketchPoint.h"
#include "sketch/tools/TrimTool.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <numbers>

using namespace onecad::core::sketch;
using namespace onecad::core::sketch::tools;

namespace {

int countType(const Sketch& sketch, EntityType type) {
    int count = 0;
    for (const auto& entity : sketch.getAllEntities()) {
        if (entity && entity->type() == type) {
            ++count;
        }
    }
    return count;
}

bool lineHasMidpointRightOfOrigin(const Sketch& sketch, const SketchLine& line) {
    auto* start = sketch.getEntityAs<SketchPoint>(line.startPointId());
    auto* end = sketch.getEntityAs<SketchPoint>(line.endPointId());
    if (!start || !end) {
        return false;
    }
    return (start->x() + end->x()) * 0.5 > 0.0;
}

void testLineTrimSpan() {
    Sketch sketch;
    auto horizontal = sketch.addLine(-10.0, 0.0, 10.0, 0.0);
    auto vertical = sketch.addLine(0.0, -5.0, 0.0, 5.0);
    assert(!horizontal.empty() && !vertical.empty());

    TrimTool tool;
    tool.setSketch(&sketch);
    tool.onMousePress(Vec2d{-5.0, 0.0}, Qt::LeftButton);

    assert(!sketch.getEntity(horizontal));
    assert(sketch.getEntity(vertical));
    assert(countType(sketch, EntityType::Line) == 2);

    bool foundRightSegment = false;
    for (const auto& entity : sketch.getAllEntities()) {
        auto* line = dynamic_cast<const SketchLine*>(entity.get());
        if (line && line->id() != vertical && lineHasMidpointRightOfOrigin(sketch, *line)) {
            foundRightSegment = true;
        }
    }
    assert(foundRightSegment);
}

void testArcTrimSpan() {
    Sketch sketch;
    auto center = sketch.addPoint(0.0, 0.0);
    auto arc = sketch.addArc(center, 5.0, 0.0, std::numbers::pi_v<double>);
    auto cutter = sketch.addLine(0.0, -1.0, 0.0, 6.0);
    assert(!arc.empty() && !cutter.empty());

    TrimTool tool;
    tool.setSketch(&sketch);
    tool.onMousePress(Vec2d{4.0, 3.0}, Qt::LeftButton);

    assert(!sketch.getEntity(arc));
    assert(sketch.getEntity(cutter));
    assert(countType(sketch, EntityType::Arc) == 1);
}

void testCircleTrimSpan() {
    Sketch sketch;
    auto center = sketch.addPoint(0.0, 0.0);
    auto circle = sketch.addCircle(center, 5.0);
    auto cutter = sketch.addLine(0.0, -6.0, 0.0, 6.0);
    assert(!circle.empty() && !cutter.empty());

    TrimTool tool;
    tool.setSketch(&sketch);
    tool.onMousePress(Vec2d{5.0, 0.0}, Qt::LeftButton);

    assert(!sketch.getEntity(circle));
    assert(sketch.getEntity(cutter));
    assert(countType(sketch, EntityType::Circle) == 0);
    assert(countType(sketch, EntityType::Arc) == 1);
}

} // namespace

int main() {
    testLineTrimSpan();
    testArcTrimSpan();
    testCircleTrimSpan();

    std::cout << "Sketch trim prototype: OK" << std::endl;
    return 0;
}
