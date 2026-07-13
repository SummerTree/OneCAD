#include "../../src/app/commands/CommandProcessor.h"
#include "../../src/app/commands/SketchDragGestureCommand.h"
#include "../../src/app/document/Document.h"
#include "../../src/core/sketch/Sketch.h"

#include <cmath>
#include <iostream>
#include <memory>
#include <string>

namespace {

bool approx(double a, double b, double tol = 1e-6) {
    return std::abs(a - b) <= tol;
}

bool pointEquals(const onecad::core::sketch::Sketch* sketch,
                 const std::string& pointId,
                 double x,
                 double y) {
    if (!sketch) {
        return false;
    }
    const auto* point = sketch->getEntityAs<onecad::core::sketch::SketchPoint>(pointId);
    if (!point) {
        return false;
    }
    return approx(point->position().X(), x) && approx(point->position().Y(), y);
}

int stackDepthByRoundtrip(onecad::app::commands::CommandProcessor& processor) {
    int undoCount = 0;
    while (processor.canUndo()) {
        processor.undo();
        ++undoCount;
    }
    int redoCount = 0;
    while (processor.canRedo()) {
        processor.redo();
        ++redoCount;
    }
    if (redoCount != undoCount) {
        return -1;
    }
    return undoCount;
}

bool runMultiSampleGestureTest() {
    onecad::app::Document document;
    onecad::app::commands::CommandProcessor processor;

    auto sketch = std::make_unique<onecad::core::sketch::Sketch>();
    const std::string pointId = sketch->addPoint(1.0, 2.0);
    if (pointId.empty()) {
        std::cerr << "Failed to create sketch point\n";
        return false;
    }

    const std::string sketchId = document.addSketch(std::move(sketch));
    if (sketchId.empty()) {
        std::cerr << "Failed to add sketch to document\n";
        return false;
    }

    const int depthBefore = stackDepthByRoundtrip(processor);
    if (depthBefore != 0) {
        std::cerr << "Expected empty undo stack before gesture\n";
        return false;
    }

    auto gesture = std::make_unique<onecad::app::commands::SketchDragGestureCommand>(&document, sketchId);
    if (!gesture->beginGesture()) {
        std::cerr << "Failed to begin drag gesture capture\n";
        return false;
    }

    for (int i = 0; i < 4; ++i) {
        auto* activeSketch = document.getSketch(sketchId);
        if (!activeSketch) {
            std::cerr << "Sketch missing during drag simulation\n";
            return false;
        }
        activeSketch->translateSketch(1.0, -0.5);
    }

    const auto* draggedSketch = document.getSketch(sketchId);
    if (!pointEquals(draggedSketch, pointId, 5.0, 0.0)) {
        std::cerr << "Drag simulation did not produce expected post state\n";
        return false;
    }

    if (!gesture->finalizeGesture()) {
        std::cerr << "Failed to finalize drag gesture capture\n";
        return false;
    }
    if (!gesture->hasCapturedChange()) {
        std::cerr << "Gesture should capture geometry change\n";
        return false;
    }

    if (!processor.execute(std::move(gesture))) {
        std::cerr << "Failed to execute drag gesture command\n";
        return false;
    }

    const int depthAfter = stackDepthByRoundtrip(processor);
    if (depthAfter != depthBefore + 1) {
        std::cerr << "Expected exactly one undo item for gesture\n";
        return false;
    }

    processor.undo();
    const auto* undoneSketch = document.getSketch(sketchId);
    if (!pointEquals(undoneSketch, pointId, 1.0, 2.0)) {
        std::cerr << "Undo did not restore pre-gesture geometry\n";
        return false;
    }

    processor.redo();
    const auto* redoneSketch = document.getSketch(sketchId);
    if (!pointEquals(redoneSketch, pointId, 5.0, 0.0)) {
        std::cerr << "Redo did not restore post-gesture geometry\n";
        return false;
    }

    return true;
}

bool runNoOpGestureTest() {
    onecad::app::Document document;
    onecad::app::commands::CommandProcessor processor;

    auto sketch = std::make_unique<onecad::core::sketch::Sketch>();
    const std::string pointId = sketch->addPoint(3.0, 4.0);
    if (pointId.empty()) {
        return false;
    }
    const std::string sketchId = document.addSketch(std::move(sketch));
    if (sketchId.empty()) {
        return false;
    }

    const int depthBefore = stackDepthByRoundtrip(processor);
    auto gesture = std::make_unique<onecad::app::commands::SketchDragGestureCommand>(&document, sketchId);
    if (!gesture->beginGesture()) {
        return false;
    }
    if (!gesture->finalizeGesture()) {
        return false;
    }
    if (gesture->hasCapturedChange()) {
        std::cerr << "No-op gesture should not report captured change\n";
        return false;
    }

    const int depthAfter = stackDepthByRoundtrip(processor);
    if (depthAfter != depthBefore) {
        std::cerr << "No-op gesture changed undo stack depth\n";
        return false;
    }
    return true;
}

bool runCancelledGestureTest() {
    onecad::app::Document document;
    onecad::app::commands::CommandProcessor processor;

    auto sketch = std::make_unique<onecad::core::sketch::Sketch>();
    const std::string pointId = sketch->addPoint(10.0, 10.0);
    if (pointId.empty()) {
        return false;
    }
    const std::string sketchId = document.addSketch(std::move(sketch));
    if (sketchId.empty()) {
        return false;
    }

    const int depthBefore = stackDepthByRoundtrip(processor);

    auto gesture = std::make_unique<onecad::app::commands::SketchDragGestureCommand>(&document, sketchId);
    if (!gesture->beginGesture()) {
        return false;
    }

    auto* activeSketch = document.getSketch(sketchId);
    if (!activeSketch) {
        return false;
    }
    activeSketch->translateSketch(-2.0, 1.0);
    gesture->cancelGesture();

    const int depthAfter = stackDepthByRoundtrip(processor);
    if (depthAfter != depthBefore) {
        std::cerr << "Cancelled gesture changed undo stack depth\n";
        return false;
    }
    return true;
}

bool runCreationGestureUndoRedoTest() {
    // A creation gesture (tool adds geometry between begin and finalize) must
    // become ONE undo step: undo removes the geometry, redo restores it.
    onecad::app::Document document;
    onecad::app::commands::CommandProcessor processor;

    auto sketch = std::make_unique<onecad::core::sketch::Sketch>();
    const std::string sketchId = document.addSketch(std::move(sketch));

    auto gesture = std::make_unique<onecad::app::commands::SketchDragGestureCommand>(
        &document, sketchId, "Sketch Geometry");
    if (!gesture->beginGesture()) {
        return false;
    }
    auto* active = document.getSketch(sketchId);
    const auto p1 = active->addPoint(0.0, 0.0);
    const auto p2 = active->addPoint(10.0, 0.0);
    active->addLine(p1, p2);
    if (!gesture->finalizeGesture() || !gesture->hasCapturedChange()) {
        std::cerr << "Creation gesture did not capture the change\n";
        return false;
    }
    if (gesture->label() != "Sketch Geometry") {
        std::cerr << "Gesture label not propagated\n";
        return false;
    }
    if (!processor.execute(std::move(gesture))) {
        return false;
    }

    const std::size_t createdCount = document.getSketch(sketchId)->getAllEntities().size();
    processor.undo();
    if (document.getSketch(sketchId)->getAllEntities().size() >= createdCount) {
        std::cerr << "Undo did not remove the created geometry\n";
        return false;
    }
    processor.redo();
    if (document.getSketch(sketchId)->getAllEntities().size() != createdCount) {
        std::cerr << "Redo did not restore the created geometry\n";
        return false;
    }
    return true;
}

bool runConstraintGestureUndoTest() {
    // Constraint add/remove funnels wrap the mutation in one gesture: undo
    // restores both the constraint count and the solved positions.
    onecad::app::Document document;
    onecad::app::commands::CommandProcessor processor;

    auto sketch = std::make_unique<onecad::core::sketch::Sketch>();
    const auto p1 = sketch->addPoint(0.0, 0.0);
    const auto p2 = sketch->addPoint(10.0, 3.0);
    const auto line = sketch->addLine(p1, p2);
    const std::string sketchId = document.addSketch(std::move(sketch));
    auto* active = document.getSketch(sketchId);
    const std::size_t constraintsBefore = active->getAllConstraints().size();

    auto gesture = std::make_unique<onecad::app::commands::SketchDragGestureCommand>(
        &document, sketchId, "Add Constraint");
    if (!gesture->beginGesture()) {
        return false;
    }
    if (active->addHorizontal(line).empty()) {
        return false;
    }
    active->solve();
    if (!gesture->finalizeGesture() || !gesture->hasCapturedChange()) {
        return false;
    }
    if (!processor.execute(std::move(gesture))) {
        return false;
    }

    processor.undo();
    active = document.getSketch(sketchId);
    if (active->getAllConstraints().size() != constraintsBefore) {
        std::cerr << "Undo did not remove the constraint\n";
        return false;
    }
    if (!pointEquals(active, p2, 10.0, 3.0)) {
        std::cerr << "Undo did not restore the pre-solve position\n";
        return false;
    }
    return true;
}

bool runRestoreBeginStateTest() {
    // Cancelling a tool gesture mid-way (e.g. a line tool's first click) must
    // roll the sketch back to the begin snapshot.
    onecad::app::Document document;
    onecad::app::commands::CommandProcessor processor;

    auto sketch = std::make_unique<onecad::core::sketch::Sketch>();
    const std::string sketchId = document.addSketch(std::move(sketch));

    auto gesture = std::make_unique<onecad::app::commands::SketchDragGestureCommand>(
        &document, sketchId, "Sketch Geometry");
    if (!gesture->beginGesture()) {
        return false;
    }
    document.getSketch(sketchId)->addPoint(4.0, 4.0);  // orphan first click
    if (!gesture->restoreBeginState()) {
        std::cerr << "restoreBeginState failed\n";
        return false;
    }
    gesture->cancelGesture();

    if (!document.getSketch(sketchId)->getAllEntities().empty()) {
        std::cerr << "Cancelled gesture left an orphan point behind\n";
        return false;
    }
    if (stackDepthByRoundtrip(processor) != 0) {
        std::cerr << "Cancelled gesture reached the undo stack\n";
        return false;
    }
    return true;
}

}

int main() {
    if (!runMultiSampleGestureTest()) {
        return 1;
    }
    if (!runNoOpGestureTest()) {
        return 1;
    }
    if (!runCancelledGestureTest()) {
        return 1;
    }
    if (!runCreationGestureUndoRedoTest()) {
        return 1;
    }
    if (!runConstraintGestureUndoTest()) {
        return 1;
    }
    if (!runRestoreBeginStateTest()) {
        return 1;
    }

    std::cout << "Sketch drag undo prototype passed\n";
    return 0;
}
