#include "sketch/Sketch.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>

using namespace onecad::core::sketch;

namespace {

bool approx(double a, double b, double tol = 1e-6) {
    double diff = std::abs(a - b);
    return diff <= tol || diff <= tol * std::max(std::abs(a), std::abs(b));
}

bool approxVec(const Vec2d& lhs, const Vec2d& rhs, double tol = 1e-6) {
    return approx(lhs.x, rhs.x, tol) && approx(lhs.y, rhs.y, tol);
}

Vec2d offset(const Vec2d& base, const Vec2d& delta) {
    return Vec2d{.x = base.x + delta.x, .y = base.y + delta.y};
}

double distance(const Vec2d& lhs, const Vec2d& rhs) {
    double dx = lhs.x - rhs.x;
    double dy = lhs.y - rhs.y;
    return std::sqrt((dx * dx) + (dy * dy));
}

Vec2d currentPosition(const Sketch& sketch, const EntityID& pointId) {
    if (const auto* point = sketch.getEntityAs<SketchPoint>(pointId)) {
        return Vec2d{.x = point->position().X(), .y = point->position().Y()};
    }
    return Vec2d{.x = 0.0, .y = 0.0};
}

std::unordered_map<EntityID, Vec2d> buildTargets(const Sketch& sketch,
                                                const std::unordered_set<EntityID>& points,
                                                const Vec2d& delta) {
    std::unordered_map<EntityID, Vec2d> targets;
    targets.reserve(points.size());
    for (const auto& id : points) {
        Vec2d current = currentPosition(sketch, id);
        targets[id] = offset(current, delta);
    }
    return targets;
}

}

int main() {
    Sketch sketch;
    auto pointA = sketch.addPoint(0.0, 0.0);
    auto pointB = sketch.addPoint(10.0, 0.0);
    if (pointA.empty() || pointB.empty()) {
        std::cout << "RED: failed to create base points" << '\n';
        std::cout << std::flush;
        std::_Exit(1);
    }

    std::unordered_set<EntityID> selection{pointA, pointB};
    Vec2d groupDelta{.x = 3.7, .y = -1.25};
    Vec2d baselineA = currentPosition(sketch, pointA);
    Vec2d baselineB = currentPosition(sketch, pointB);
    double baselineSpacing = distance(baselineA, baselineB);

    int exitCode = 0;
    auto markRed = [&](const std::string& message) {
        std::cout << "RED: " << message << '\n';
        exitCode = 1;
    };

    sketch.beginGroupDrag(selection);
    SolveResult rigidResult = sketch.solveWithGroupDrag(buildTargets(sketch, selection, groupDelta));
    sketch.endGroupDrag();

    if (!rigidResult.success) {
        markRed("group drag not implemented" +
                std::string(rigidResult.errorMessage.empty() ? "" : ": " + rigidResult.errorMessage));
    } else {
        Vec2d afterA = currentPosition(sketch, pointA);
        Vec2d afterB = currentPosition(sketch, pointB);
        if (!approxVec(afterA, offset(baselineA, groupDelta))) {
            markRed("point A did not follow the requested translation");
        }
        if (!approxVec(afterB, offset(baselineB, groupDelta))) {
            markRed("point B did not follow the requested translation");
        }
        double afterSpacing = distance(afterA, afterB);
        if (!approx(afterSpacing, baselineSpacing)) {
            markRed("point spacing changed during group drag");
        }
    }

    Sketch constrained;
    auto constrainedA = constrained.addPoint(0.0, 0.0);
    auto constrainedB = constrained.addPoint(12.0, 0.0);
    auto constrainedLine = constrained.addLine(constrainedA, constrainedB);
    assert(!constrainedLine.empty());
    assert(!constrained.addHorizontal(constrainedLine).empty());

    std::unordered_set<EntityID> constrainedSelection{constrainedA, constrainedB};
    Vec2d constrainedDelta{6.5, 0.0};
    Vec2d constrainedBeforeA = currentPosition(constrained, constrainedA);
    Vec2d constrainedBeforeB = currentPosition(constrained, constrainedB);

    constrained.beginGroupDrag(constrainedSelection);
    SolveResult constrainedResult = constrained.solveWithGroupDrag(
        buildTargets(constrained, constrainedSelection, constrainedDelta));
    constrained.endGroupDrag();

    if (!constrainedResult.success) {
        markRed("constrained group drag failed" +
                std::string(constrainedResult.errorMessage.empty() ? "" : ": " + constrainedResult.errorMessage));
    } else {
        Vec2d constrainedAfterA = currentPosition(constrained, constrainedA);
        Vec2d constrainedAfterB = currentPosition(constrained, constrainedB);
        if (!approxVec(constrainedAfterA, offset(constrainedBeforeA, constrainedDelta))) {
            markRed("constrained point A did not match requested delta");
        }
        if (!approxVec(constrainedAfterB, offset(constrainedBeforeB, constrainedDelta))) {
            markRed("constrained point B did not match requested delta");
        }
        if (!approx(constrainedAfterA.y, constrainedBeforeA.y, 1e-4)) {
            markRed("constrained Y changed unexpectedly for point A");
        }
        if (!approx(constrainedAfterB.y, constrainedBeforeB.y, 1e-4)) {
            markRed("constrained Y changed unexpectedly for point B");
        }
    }

    auto fixed = sketch.addFixed(pointA);
    if (fixed.empty()) {
        markRed("could not apply Fixed constraint to point A");
    }

    Vec2d lockedBaseA = currentPosition(sketch, pointA);
    Vec2d lockedBaseB = currentPosition(sketch, pointB);

    sketch.beginGroupDrag(selection);
    SolveResult atomicResult = sketch.solveWithGroupDrag(buildTargets(sketch, selection, groupDelta));
    sketch.endGroupDrag();

    if (atomicResult.success) {
        markRed("atomic reject did not block the group drag");
    }
    if (atomicResult.errorMessage.empty()) {
        markRed("atomic reject missing error message payload");
    }

    Vec2d lockedAfterA = currentPosition(sketch, pointA);
    Vec2d lockedAfterB = currentPosition(sketch, pointB);

    if (!approxVec(lockedAfterA, lockedBaseA)) {
        markRed("point A moved despite atomic reject");
    }
    if (!approxVec(lockedAfterB, lockedBaseB)) {
        markRed("point B moved despite atomic reject");
    }

    Sketch coincidentSketch;
    auto coincidentA = coincidentSketch.addPoint(0.0, 0.0);
    auto coincidentB = coincidentSketch.addPoint(8.0, 0.0);
    auto coincidentFixed = coincidentSketch.addPoint(8.0, 0.0);
    assert(!coincidentSketch.addFixed(coincidentFixed).empty());
    assert(!coincidentSketch.addCoincident(coincidentB, coincidentFixed).empty());

    std::unordered_set<EntityID> coincidentSelection{coincidentA, coincidentB};
    Vec2d coincidentBaselineA = currentPosition(coincidentSketch, coincidentA);
    Vec2d coincidentBaselineB = currentPosition(coincidentSketch, coincidentB);
    Vec2d coincidentDelta{4.0, 3.5};

    coincidentSketch.beginGroupDrag(coincidentSelection);
    SolveResult coincidentResult = coincidentSketch.solveWithGroupDrag(
        buildTargets(coincidentSketch, coincidentSelection, coincidentDelta));
    coincidentSketch.endGroupDrag();

    if (coincidentResult.success) {
        markRed("coincident-locked member should reject but succeeded");
    }
    if (coincidentResult.errorMessage.empty()) {
        markRed("coincident reject missing error payload");
    }

    Vec2d coincidentAfterA = currentPosition(coincidentSketch, coincidentA);
    Vec2d coincidentAfterB = currentPosition(coincidentSketch, coincidentB);
    if (!approxVec(coincidentAfterA, coincidentBaselineA)) {
        markRed("coincident scenario moved point A despite reject");
    }
    if (!approxVec(coincidentAfterB, coincidentBaselineB)) {
        markRed("coincident scenario moved point B despite reject");
    }

    std::cout << std::flush;
    std::_Exit(exitCode);
}
