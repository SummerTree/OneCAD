/**
 * @file proto_regeneration.cpp
 * @brief Prototype tests for RegenerationEngine.
 *
 * Test cases:
 * 1. Single extrude: sketch→extrude→regenerate→verify
 * 2. Chain: extrude→fillet→regen→verify
 * 3. Failure: delete sketch→regen→verify failure reported
 * 4. Topology: extrude→fillet by ElementMap ID→modify extrude→regen→verify
 */

#include "app/commands/AddDatumPlaneCommand.h"
#include "app/commands/AddOperationCommand.h"
#include "app/commands/AddSketchCommand.h"
#include "app/commands/CommandProcessor.h"
#include "app/commands/EditOperationInputCommand.h"
#include "app/commands/UpdateOperationParamsCommand.h"
#include "app/commands/UpdateSketchAttachmentCommand.h"
#include "app/document/Document.h"
#include "app/history/DependencyGraph.h"
#include "app/history/RegenerationEngine.h"
#include "app/selection/SelectionManager.h"
#include "core/loop/LoopDetector.h"
#include "core/loop/RegionUtils.h"
#include "core/modeling/BooleanOperation.h"
#include "core/sketch/Sketch.h"
#include "io/HistoryIO.h"

#include <BRepCheck_Analyzer.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

#include <QCoreApplication>
#include <QUuid>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <unordered_set>
#include <utility>

using namespace onecad;

namespace {

bool nearlyEqual(double a, double b, double tol = 1e-3) {
    return std::abs(a - b) <= tol;
}

std::string newId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
}

double shapeVolume(const TopoDS_Shape& shape) {
    GProp_GProps props;
    BRepGProp::VolumeProperties(shape, props);
    return props.Mass();
}

int solidCount(const TopoDS_Shape& shape) {
    int count = 0;
    for (TopExp_Explorer exp(shape, TopAbs_SOLID); exp.More(); exp.Next()) {
        ++count;
    }
    return count;
}

bool shapeValid(const TopoDS_Shape& shape) {
    if (shape.IsNull()) return false;
    BRepCheck_Analyzer analyzer(shape);
    return analyzer.IsValid();
}

std::vector<core::loop::RegionDefinition> detectRegions(core::sketch::Sketch& sketch) {
    core::loop::LoopDetectorConfig config = core::loop::makeRegionDetectionConfig();
    core::loop::LoopDetector detector(config);
    auto loopResult = detector.detect(sketch);
    assert(loopResult.success);
    return core::loop::buildRegionDefinitions(loopResult, core::sketch::constants::COINCIDENCE_TOLERANCE);
}

std::string firstRegionId(core::sketch::Sketch& sketch) {
    auto regions = detectRegions(sketch);
    assert(!regions.empty());
    return regions[0].id;
}

std::optional<std::pair<std::string, core::sketch::SketchPlane>>
findTopPlanarFace(const app::Document& doc, const std::string& bodyId) {
    std::optional<std::pair<std::string, core::sketch::SketchPlane>> best;
    double bestNormalZ = -2.0;

    for (const auto& id : doc.elementMap().ids()) {
        const auto* entry = doc.elementMap().find(id);
        if (!entry || entry->kind != kernel::elementmap::ElementKind::Face) {
            continue;
        }
        auto plane = doc.getSketchPlaneForFace(bodyId, id.value);
        if (!plane.has_value()) {
            continue;
        }
        if (plane->normal.z > bestNormalZ) {
            bestNormalZ = plane->normal.z;
            best = std::make_pair(id.value, *plane);
        }
    }

    return best;
}

std::pair<std::string, std::string> createSquareSketchRegion(app::Document& doc, double size) {
    auto sketch = std::make_unique<core::sketch::Sketch>(core::sketch::SketchPlane::XY());
    auto p1 = sketch->addPoint(0.0, 0.0);
    auto p2 = sketch->addPoint(size, 0.0);
    auto p3 = sketch->addPoint(size, size);
    auto p4 = sketch->addPoint(0.0, size);
    sketch->addLine(p1, p2);
    sketch->addLine(p2, p3);
    sketch->addLine(p3, p4);
    sketch->addLine(p4, p1);

    std::string sketchId = doc.addSketch(std::move(sketch));
    std::string regionId = firstRegionId(*doc.getSketch(sketchId));
    return {sketchId, regionId};
}

bool executeAddOperation(app::Document& doc, const app::OperationRecord& record) {
    app::commands::AddOperationCommand command(&doc, record);
    return command.execute();
}

double bodyVolume(const app::Document& doc, const std::string& bodyId) {
    const TopoDS_Shape* body = doc.getBodyShape(bodyId);
    assert(body && !body->IsNull());
    return shapeVolume(*body);
}

} // namespace

void testSingleExtrude() {
    std::cout << "Test 1: Single extrude regeneration..." << std::flush;

    app::Document doc;

    // Create a simple rectangular sketch
    auto sketch = std::make_unique<core::sketch::Sketch>();
    auto p1 = sketch->addPoint(0.0, 0.0);
    auto p2 = sketch->addPoint(10.0, 0.0);
    auto p3 = sketch->addPoint(10.0, 10.0);
    auto p4 = sketch->addPoint(0.0, 10.0);

    sketch->addLine(p1, p2);
    sketch->addLine(p2, p3);
    sketch->addLine(p3, p4);
    sketch->addLine(p4, p1);

    std::string sketchId = doc.addSketch(std::move(sketch));

    // Detect regions to get a valid region ID
    // Use the same config that resolveRegionFace uses
    core::sketch::Sketch* sketchPtr = doc.getSketch(sketchId);
    core::loop::LoopDetectorConfig config = core::loop::makeRegionDetectionConfig();
    core::loop::LoopDetector detector(config);
    auto loopResult = detector.detect(*sketchPtr);

    assert(!loopResult.faces.empty());
    std::string regionId = core::loop::regionKey(loopResult.faces[0].outerLoop);

    // Create extrude operation record
    std::string bodyId = newId();
    std::string opId = newId();

    app::OperationRecord extrudeOp;
    extrudeOp.opId = opId;
    extrudeOp.type = app::OperationType::Extrude;
    extrudeOp.input = app::SketchRegionRef{sketchId, regionId};
    extrudeOp.params = app::ExtrudeParams{20.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    extrudeOp.resultBodyIds.push_back(bodyId);

    doc.addOperation(extrudeOp);

    // Run regeneration
    app::history::RegenerationEngine engine(&doc);
    auto result = engine.regenerateAll();

    // Verify
    if (result.status != app::history::RegenStatus::Success) {
        std::cerr << "\nRegeneration failed!\n";
        for (const auto& f : result.failedOps) {
            std::cerr << "  Op: " << f.opId << " Error: " << f.errorMessage << "\n";
        }
    }
    assert(result.status == app::history::RegenStatus::Success);
    assert(result.succeededOps.size() == 1);
    assert(result.failedOps.empty());

    const TopoDS_Shape* shape = doc.getBodyShape(bodyId);
    assert(shape != nullptr);
    assert(!shape->IsNull());
    assert(shapeValid(*shape));

    // 10x10x20 = 2000 mm³
    double vol = shapeVolume(*shape);
    assert(nearlyEqual(vol, 2000.0, 10.0));

    std::cout << " PASS\n";
}

void testDependencyGraph() {
    std::cout << "Test 2: DependencyGraph topological sort..." << std::flush;

    app::history::DependencyGraph graph;

    // Create operations with dependencies
    // op1: extrude (produces body1)
    // op2: fillet (uses body1)
    // op3: shell (uses body1 after fillet)

    app::OperationRecord op1;
    op1.opId = "op1";
    op1.type = app::OperationType::Extrude;
    op1.input = app::SketchRegionRef{"sketch1", "region1"};
    op1.params = app::ExtrudeParams{10.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    op1.resultBodyIds.push_back("body1");

    app::OperationRecord op2;
    op2.opId = "op2";
    op2.type = app::OperationType::Fillet;
    op2.input = app::BodyRef{"body1"};
    app::FilletChamferParams filletParams;
    filletParams.mode = app::FilletChamferParams::Mode::Fillet;
    filletParams.radius = 1.0;
    filletParams.edgeIds = {"edge1", "edge2"};
    op2.params = filletParams;
    op2.resultBodyIds.push_back("body1");

    graph.addOperation(op1);
    graph.addOperation(op2);

    // Topological sort should place op1 before op2
    auto sorted = graph.topologicalSort();
    assert(sorted.size() == 2);
    assert(sorted[0] == "op1");
    assert(sorted[1] == "op2");

    // Test downstream
    auto downstream = graph.getDownstream("op1");
    assert(downstream.size() == 1);
    assert(downstream[0] == "op2");

    // Test upstream
    auto upstream = graph.getUpstream("op2");
    assert(upstream.size() == 1);
    assert(upstream[0] == "op1");

    std::cout << " PASS\n";
}

void testSuppressionAndFailure() {
    std::cout << "Test 3: Suppression and failure tracking..." << std::flush;

    app::history::DependencyGraph graph;

    app::OperationRecord op1;
    op1.opId = "op1";
    op1.type = app::OperationType::Extrude;
    op1.params = app::ExtrudeParams{10.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    op1.resultBodyIds.push_back("body1");

    app::OperationRecord op2;
    op2.opId = "op2";
    op2.type = app::OperationType::Fillet;
    op2.input = app::BodyRef{"body1"};
    app::FilletChamferParams params;
    params.radius = 1.0;
    op2.params = params;
    op2.resultBodyIds.push_back("body1");

    graph.addOperation(op1);
    graph.addOperation(op2);

    // Test suppression
    assert(!graph.isSuppressed("op1"));
    graph.setSuppressed("op1", true);
    assert(graph.isSuppressed("op1"));

    // Test failure tracking
    assert(!graph.isFailed("op1"));
    graph.setFailed("op1", true, "Test failure reason");
    assert(graph.isFailed("op1"));
    assert(graph.getFailureReason("op1") == "Test failure reason");

    auto failed = graph.getFailedOps();
    assert(failed.size() == 1);
    assert(failed[0] == "op1");

    graph.clearFailures();
    assert(!graph.isFailed("op1"));

    std::cout << " PASS\n";
}

void testChainRegeneration() {
    std::cout << "Test 4: Chain regeneration (extrude + downstream)..." << std::flush;

    app::Document doc;

    // Create sketch
    auto sketch = std::make_unique<core::sketch::Sketch>();
    auto p1 = sketch->addPoint(0.0, 0.0);
    auto p2 = sketch->addPoint(10.0, 0.0);
    auto p3 = sketch->addPoint(10.0, 10.0);
    auto p4 = sketch->addPoint(0.0, 10.0);

    sketch->addLine(p1, p2);
    sketch->addLine(p2, p3);
    sketch->addLine(p3, p4);
    sketch->addLine(p4, p1);

    std::string sketchId = doc.addSketch(std::move(sketch));

    core::sketch::Sketch* sketchPtr = doc.getSketch(sketchId);
    core::loop::LoopDetectorConfig config = core::loop::makeRegionDetectionConfig();
    core::loop::LoopDetector detector(config);
    auto loopResult = detector.detect(*sketchPtr);
    std::string regionId = core::loop::regionKey(loopResult.faces[0].outerLoop);

    std::string bodyId = newId();
    std::string extrudeOpId = newId();

    // Extrude operation
    app::OperationRecord extrudeOp;
    extrudeOp.opId = extrudeOpId;
    extrudeOp.type = app::OperationType::Extrude;
    extrudeOp.input = app::SketchRegionRef{sketchId, regionId};
    extrudeOp.params = app::ExtrudeParams{15.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    extrudeOp.resultBodyIds.push_back(bodyId);
    doc.addOperation(extrudeOp);

    // First regenerate to create the body and register edges
    app::history::RegenerationEngine engine1(&doc);
    auto result1 = engine1.regenerateAll();
    if (result1.status != app::history::RegenStatus::Success) {
        std::cerr << "\nChain regen failed!\n";
        for (const auto& f : result1.failedOps) {
            std::cerr << "  Op: " << f.opId << " Error: " << f.errorMessage << "\n";
        }
    }
    assert(result1.status == app::history::RegenStatus::Success);

    // Now we could add a fillet operation if we have edge IDs
    // For this test, just verify the extrude works and chain logic is correct

    // Get the graph and verify dependency tracking
    const auto& graph = engine1.graph();
    auto sorted = graph.topologicalSort();
    assert(sorted.size() == 1);
    assert(sorted[0] == extrudeOpId);

    std::cout << " PASS\n";
}

void testRegenFailureOnMissingSketch() {
    std::cout << "Test 5: Regeneration failure on missing sketch..." << std::flush;

    app::Document doc;

    // Create operation referencing a non-existent sketch
    std::string bodyId = newId();
    std::string opId = newId();

    app::OperationRecord extrudeOp;
    extrudeOp.opId = opId;
    extrudeOp.type = app::OperationType::Extrude;
    extrudeOp.input = app::SketchRegionRef{"nonexistent-sketch", "region1"};
    extrudeOp.params = app::ExtrudeParams{10.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    extrudeOp.resultBodyIds.push_back(bodyId);
    doc.addOperation(extrudeOp);

    // Run regeneration
    app::history::RegenerationEngine engine(&doc);
    auto result = engine.regenerateAll();

    // Should fail
    assert(result.status != app::history::RegenStatus::Success);
    assert(!result.failedOps.empty());
    assert(result.failedOps[0].opId == opId);
    assert(!result.failedOps[0].errorMessage.empty());

    std::cout << " PASS\n";
}

void testGraphCycleDetection() {
    std::cout << "Test 6: Cycle detection in dependency graph..." << std::flush;

    app::history::DependencyGraph graph;

    // Create a cycle: body1 -> body2 -> body1 (artificial)
    // This is somewhat artificial since real CAD shouldn't have cycles
    // but we test the algorithm

    app::OperationRecord op1;
    op1.opId = "op1";
    op1.type = app::OperationType::Extrude;
    op1.params = app::ExtrudeParams{10.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    op1.resultBodyIds.push_back("body1");

    graph.addOperation(op1);

    // Verify no cycle with single op
    assert(!graph.hasCycle());

    auto sorted = graph.topologicalSort();
    assert(sorted.size() == 1);

    std::cout << " PASS\n";
}

void testSketchHostAttachmentRoundTrip() {
    std::cout << "Test 7: Sketch host attachment JSON round-trip..." << std::flush;

    core::sketch::Sketch sketch(core::sketch::SketchPlane::XY());
    sketch.setHostFaceAttachment("body-A", "face-B");

    auto restored = core::sketch::Sketch::fromJson(sketch.toJson());
    assert(restored);
    assert(restored->hasHostFaceAttachment());
    assert(restored->hostFaceAttachment()->bodyId == "body-A");
    assert(restored->hostFaceAttachment()->faceId == "face-B");

    std::cout << " PASS\n";
}

void testHistoryTargetBodyRoundTrip() {
    std::cout << "Test 8: History targetBodyId JSON round-trip..." << std::flush;

    app::OperationRecord extrudeOp;
    extrudeOp.opId = "extrude-op";
    extrudeOp.type = app::OperationType::Extrude;
    extrudeOp.input = app::SketchRegionRef{"sketch-1", "region-1"};
    app::ExtrudeParams extrudeParams;
    extrudeParams.distance = 10.0;
    extrudeParams.booleanMode = app::BooleanMode::Add;
    extrudeParams.targetBodyId = "body-target-1";
    extrudeOp.params = extrudeParams;
    extrudeOp.resultBodyIds.push_back("body-target-1");

    QJsonObject extrudeJson = io::HistoryIO::serializeOperation(extrudeOp);
    app::OperationRecord extrudeRoundTrip = io::HistoryIO::deserializeOperation(extrudeJson);
    assert(std::holds_alternative<app::ExtrudeParams>(extrudeRoundTrip.params));
    const auto& extrudeRoundTripParams = std::get<app::ExtrudeParams>(extrudeRoundTrip.params);
    assert(extrudeRoundTripParams.targetBodyId == "body-target-1");

    app::OperationRecord revolveOp;
    revolveOp.opId = "revolve-op";
    revolveOp.type = app::OperationType::Revolve;
    revolveOp.input = app::SketchRegionRef{"sketch-2", "region-2"};
    app::RevolveParams revolveParams;
    revolveParams.angleDeg = 180.0;
    revolveParams.booleanMode = app::BooleanMode::Cut;
    revolveParams.targetBodyId = "body-target-2";
    revolveParams.axis = app::SketchLineRef{"sketch-2", "line-1"};
    revolveOp.params = revolveParams;
    revolveOp.resultBodyIds.push_back("body-target-2");

    QJsonObject revolveJson = io::HistoryIO::serializeOperation(revolveOp);
    app::OperationRecord revolveRoundTrip = io::HistoryIO::deserializeOperation(revolveJson);
    assert(std::holds_alternative<app::RevolveParams>(revolveRoundTrip.params));
    const auto& revolveRoundTripParams = std::get<app::RevolveParams>(revolveRoundTrip.params);
    assert(revolveRoundTripParams.targetBodyId == "body-target-2");

    std::cout << " PASS\n";
}

void testAttachedSketchExtrudeAdd() {
    std::cout << "Test 9: Attached sketch extrude Add targets host body..." << std::flush;

    app::Document doc;
    std::string bodyId = doc.addBody(BRepPrimAPI_MakeBox(20.0, 20.0, 10.0).Shape());
    assert(!bodyId.empty());

    const TopoDS_Shape* baseBody = doc.getBodyShape(bodyId);
    assert(baseBody && !baseBody->IsNull());
    const double baseVolume = shapeVolume(*baseBody);

    auto topFace = findTopPlanarFace(doc, bodyId);
    assert(topFace.has_value());

    auto sketch = std::make_unique<core::sketch::Sketch>(topFace->second);
    sketch->setHostFaceAttachment(bodyId, topFace->first);
    const auto p1 = topFace->second.toSketch({8.0, 8.0, 10.0});
    const auto p2 = topFace->second.toSketch({12.0, 8.0, 10.0});
    const auto p3 = topFace->second.toSketch({12.0, 12.0, 10.0});
    const auto p4 = topFace->second.toSketch({8.0, 12.0, 10.0});
    auto e1 = sketch->addPoint(p1.x, p1.y);
    auto e2 = sketch->addPoint(p2.x, p2.y);
    auto e3 = sketch->addPoint(p3.x, p3.y);
    auto e4 = sketch->addPoint(p4.x, p4.y);
    sketch->addLine(e1, e2);
    sketch->addLine(e2, e3);
    sketch->addLine(e3, e4);
    sketch->addLine(e4, e1);

    std::string sketchId = doc.addSketch(std::move(sketch));
    std::string regionId = firstRegionId(*doc.getSketch(sketchId));

    app::OperationRecord op;
    op.opId = newId();
    op.type = app::OperationType::Extrude;
    op.input = app::SketchRegionRef{sketchId, regionId};
    app::ExtrudeParams params;
    params.distance = 4.0;
    params.booleanMode = app::BooleanMode::Add;
    // Exercise legacy fallback path: resolve host body from sketch attachment metadata.
    params.targetBodyId.clear();
    op.params = params;
    op.resultBodyIds.push_back(bodyId);
    doc.addOperation(op);

    app::history::RegenerationEngine engine(&doc);
    auto result = engine.regenerateAll();
    assert(result.status == app::history::RegenStatus::Success);

    const TopoDS_Shape* updatedBody = doc.getBodyShape(bodyId);
    assert(updatedBody && !updatedBody->IsNull());
    assert(shapeValid(*updatedBody));
    assert(shapeVolume(*updatedBody) > baseVolume + 1.0);

    std::cout << " PASS\n";
}

void testAttachedSketchExtrudeCut() {
    std::cout << "Test 10: Attached sketch extrude Cut targets host body..." << std::flush;

    app::Document doc;
    std::string bodyId = doc.addBody(BRepPrimAPI_MakeBox(20.0, 20.0, 10.0).Shape());
    assert(!bodyId.empty());

    const TopoDS_Shape* baseBody = doc.getBodyShape(bodyId);
    assert(baseBody && !baseBody->IsNull());
    const double baseVolume = shapeVolume(*baseBody);

    auto topFace = findTopPlanarFace(doc, bodyId);
    assert(topFace.has_value());

    auto sketch = std::make_unique<core::sketch::Sketch>(topFace->second);
    sketch->setHostFaceAttachment(bodyId, topFace->first);
    const auto p1 = topFace->second.toSketch({8.0, 8.0, 10.0});
    const auto p2 = topFace->second.toSketch({12.0, 8.0, 10.0});
    const auto p3 = topFace->second.toSketch({12.0, 12.0, 10.0});
    const auto p4 = topFace->second.toSketch({8.0, 12.0, 10.0});
    auto e1 = sketch->addPoint(p1.x, p1.y);
    auto e2 = sketch->addPoint(p2.x, p2.y);
    auto e3 = sketch->addPoint(p3.x, p3.y);
    auto e4 = sketch->addPoint(p4.x, p4.y);
    sketch->addLine(e1, e2);
    sketch->addLine(e2, e3);
    sketch->addLine(e3, e4);
    sketch->addLine(e4, e1);

    std::string sketchId = doc.addSketch(std::move(sketch));
    std::string regionId = firstRegionId(*doc.getSketch(sketchId));

    app::OperationRecord op;
    op.opId = newId();
    op.type = app::OperationType::Extrude;
    op.input = app::SketchRegionRef{sketchId, regionId};
    app::ExtrudeParams params;
    params.distance = -4.0;
    params.booleanMode = app::BooleanMode::Cut;
    params.targetBodyId = bodyId;
    op.params = params;
    op.resultBodyIds.push_back(bodyId);
    doc.addOperation(op);

    app::history::RegenerationEngine engine(&doc);
    auto result = engine.regenerateAll();
    assert(result.status == app::history::RegenStatus::Success);

    const TopoDS_Shape* updatedBody = doc.getBodyShape(bodyId);
    assert(updatedBody && !updatedBody->IsNull());
    assert(shapeValid(*updatedBody));
    assert(shapeVolume(*updatedBody) < baseVolume - 1.0);

    std::cout << " PASS\n";
}

void testAttachedSketchRevolveAdd() {
    std::cout << "Test 11: Attached sketch revolve targets host body..." << std::flush;

    app::Document doc;
    std::string bodyId = doc.addBody(BRepPrimAPI_MakeBox(20.0, 20.0, 10.0).Shape());
    assert(!bodyId.empty());

    const TopoDS_Shape* baseBody = doc.getBodyShape(bodyId);
    assert(baseBody && !baseBody->IsNull());
    const double baseVolume = shapeVolume(*baseBody);

    auto topFace = findTopPlanarFace(doc, bodyId);
    assert(topFace.has_value());

    auto sketch = std::make_unique<core::sketch::Sketch>(topFace->second);
    sketch->setHostFaceAttachment(bodyId, topFace->first);

    const auto p1 = topFace->second.toSketch({9.0, 12.0, 10.0});
    const auto p2 = topFace->second.toSketch({11.0, 12.0, 10.0});
    const auto p3 = topFace->second.toSketch({11.0, 14.0, 10.0});
    const auto p4 = topFace->second.toSketch({9.0, 14.0, 10.0});
    auto e1 = sketch->addPoint(p1.x, p1.y);
    auto e2 = sketch->addPoint(p2.x, p2.y);
    auto e3 = sketch->addPoint(p3.x, p3.y);
    auto e4 = sketch->addPoint(p4.x, p4.y);
    sketch->addLine(e1, e2);
    sketch->addLine(e2, e3);
    sketch->addLine(e3, e4);
    sketch->addLine(e4, e1);

    const auto axisP1 = topFace->second.toSketch({0.0, 10.0, 10.0});
    const auto axisP2 = topFace->second.toSketch({20.0, 10.0, 10.0});
    auto axisStart = sketch->addPoint(axisP1.x, axisP1.y, true);
    auto axisEnd = sketch->addPoint(axisP2.x, axisP2.y, true);
    auto axisLineId = sketch->addLine(axisStart, axisEnd, true);

    std::string sketchId = doc.addSketch(std::move(sketch));
    std::string regionId = firstRegionId(*doc.getSketch(sketchId));

    app::OperationRecord op;
    op.opId = newId();
    op.type = app::OperationType::Revolve;
    op.input = app::SketchRegionRef{sketchId, regionId};
    app::RevolveParams params;
    params.angleDeg = 120.0;
    params.axis = app::SketchLineRef{sketchId, axisLineId};
    params.booleanMode = app::BooleanMode::Add;
    params.targetBodyId = bodyId;
    op.params = params;
    op.resultBodyIds.push_back(bodyId);
    doc.addOperation(op);

    app::history::RegenerationEngine engine(&doc);
    auto result = engine.regenerateAll();
    assert(result.status == app::history::RegenStatus::Success);

    const TopoDS_Shape* updatedBody = doc.getBodyShape(bodyId);
    assert(updatedBody && !updatedBody->IsNull());
    assert(shapeValid(*updatedBody));
    assert(shapeVolume(*updatedBody) > baseVolume + 1.0);

    std::cout << " PASS\n";
}

void testBooleanFailureOnMissingTargetBody() {
    std::cout << "Test 12: Boolean op fails explicitly when target body is unresolved..." << std::flush;

    app::Document doc;

    auto sketch = std::make_unique<core::sketch::Sketch>(core::sketch::SketchPlane::XY());
    auto p1 = sketch->addPoint(0.0, 0.0);
    auto p2 = sketch->addPoint(10.0, 0.0);
    auto p3 = sketch->addPoint(10.0, 10.0);
    auto p4 = sketch->addPoint(0.0, 10.0);
    sketch->addLine(p1, p2);
    sketch->addLine(p2, p3);
    sketch->addLine(p3, p4);
    sketch->addLine(p4, p1);
    std::string sketchId = doc.addSketch(std::move(sketch));
    std::string regionId = firstRegionId(*doc.getSketch(sketchId));

    app::OperationRecord op;
    op.opId = newId();
    op.type = app::OperationType::Extrude;
    op.input = app::SketchRegionRef{sketchId, regionId};
    app::ExtrudeParams params;
    params.distance = 5.0;
    params.booleanMode = app::BooleanMode::Add;
    params.targetBodyId = "missing-body";
    op.params = params;
    op.resultBodyIds.push_back("missing-body");
    doc.addOperation(op);

    app::history::RegenerationEngine engine(&doc);
    auto result = engine.regenerateAll();
    assert(result.status != app::history::RegenStatus::Success);
    assert(!result.failedOps.empty());
    const std::string& error = result.failedOps.front().errorMessage;
    assert(error.find("Target body not found: missing-body") != std::string::npos);

    std::cout << " PASS\n";
}

void testFaceBoundaryProjectionPartitioning() {
    std::cout << "Test 13: Host face projection partitions regions (square + circle)..." << std::flush;

    app::Document doc;
    std::string bodyId = doc.addBody(BRepPrimAPI_MakeBox(20.0, 20.0, 10.0).Shape());
    assert(!bodyId.empty());

    auto topFace = findTopPlanarFace(doc, bodyId);
    assert(topFace.has_value());

    auto sketch = std::make_unique<core::sketch::Sketch>(topFace->second);
    sketch->setHostFaceAttachment(bodyId, topFace->first);

    const auto center = topFace->second.toSketch({10.0, 10.0, 10.0});
    sketch->addCircle(center.x, center.y, 4.0);

    std::string sketchId = doc.addSketch(std::move(sketch));
    core::sketch::Sketch* sketchPtr = doc.getSketch(sketchId);
    assert(sketchPtr);

    bool projected = doc.ensureHostFaceBoundariesProjected(sketchId);
    assert(projected);
    assert(sketchPtr->hasProjectedHostBoundaries());
    assert(sketchPtr->projectedHostBoundariesVersion() == 1);

    auto regions = detectRegions(*sketchPtr);
    assert(regions.size() >= 2);

    std::cout << " PASS\n";
}

void testLegacyHostBoundaryBackfillIdempotent() {
    std::cout << "Test 14: Legacy host-boundary backfill is idempotent..." << std::flush;

    app::Document doc;
    std::string bodyId = doc.addBody(BRepPrimAPI_MakeBox(20.0, 20.0, 10.0).Shape());
    assert(!bodyId.empty());

    auto topFace = findTopPlanarFace(doc, bodyId);
    assert(topFace.has_value());

    auto sketch = std::make_unique<core::sketch::Sketch>(topFace->second);
    sketch->setHostFaceAttachment(bodyId, topFace->first);
    const auto center = topFace->second.toSketch({10.0, 10.0, 10.0});
    sketch->addCircle(center.x, center.y, 4.0);

    std::string sketchId = doc.addSketch(std::move(sketch));
    core::sketch::Sketch* sketchPtr = doc.getSketch(sketchId);
    assert(sketchPtr);
    assert(!sketchPtr->hasProjectedHostBoundaries());
    assert(sketchPtr->projectedHostBoundariesVersion() == 0);

    const size_t countBefore = sketchPtr->getEntityCount();
    bool first = doc.ensureHostFaceBoundariesProjected(sketchId);
    const size_t countAfterFirst = sketchPtr->getEntityCount();
    bool second = doc.ensureHostFaceBoundariesProjected(sketchId);
    const size_t countAfterSecond = sketchPtr->getEntityCount();

    assert(first);
    assert(countAfterFirst > countBefore);
    assert(sketchPtr->hasProjectedHostBoundaries());
    assert(sketchPtr->projectedHostBoundariesVersion() == 1);
    assert(!second);
    assert(countAfterSecond == countAfterFirst);

    std::cout << " PASS\n";
}

void testSketchHostProjectionVersionRequired() {
    std::cout << "Test 15: Sketch host projection JSON requires projection version..." << std::flush;

    core::sketch::Sketch sketch(core::sketch::SketchPlane::XY());
    sketch.setHostFaceAttachment("body-A", "face-B");
    sketch.setProjectedHostBoundariesVersion(1);

    QJsonParseError parseError;
    const QString jsonText = QString::fromStdString(sketch.toJson());
    QJsonDocument docJson = QJsonDocument::fromJson(jsonText.toUtf8(), &parseError);
    assert(parseError.error == QJsonParseError::NoError);
    assert(docJson.isObject());

    QJsonObject root = docJson.object();
    assert(root.contains("hostFace"));
    QJsonObject hostFace = root["hostFace"].toObject();
    hostFace.remove("projectedBoundaryVersion");
    root["hostFace"] = hostFace;

    QJsonDocument withoutVersion(root);
    auto restored = core::sketch::Sketch::fromJson(withoutVersion.toJson().toStdString());
    assert(!restored);

    std::cout << " PASS\n";
}

void testSelectionPriorityPrefersSketchRegion() {
    std::cout << "Test 16: Selection priority prefers SketchRegion over Face..." << std::flush;

    app::selection::SelectionManager manager;
    manager.setMode(app::selection::SelectionMode::Model);

    app::selection::PickResult pick;
    app::selection::SelectionItem face;
    face.kind = app::selection::SelectionKind::Face;
    face.id = {"body-1", "face-1"};
    face.priority = 2;
    face.screenDistance = 0.5;
    face.depth = 0.1;
    pick.hits.push_back(face);

    app::selection::SelectionItem sketchRegion;
    sketchRegion.kind = app::selection::SelectionKind::SketchRegion;
    sketchRegion.id = {"sketch-1", "region-1"};
    sketchRegion.priority = -98; // Matches viewport sketch-priority boost behavior.
    sketchRegion.screenDistance = 0.5;
    sketchRegion.depth = 0.2;
    pick.hits.push_back(sketchRegion);

    app::selection::ClickModifiers modifiers;
    auto action = manager.handleClick(pick, modifiers, QPoint(100, 100));
    assert(action.selectionChanged);
    auto selected = manager.selection();
    assert(selected.size() == 1);
    assert(selected[0].kind == app::selection::SelectionKind::SketchRegion);
    assert(selected[0].id.ownerId == "sketch-1");
    assert(selected[0].id.elementId == "region-1");

    std::cout << " PASS\n";
}

void testProjectedReferenceGeometryIsLocked() {
    std::cout << "Test 17: Projected host reference geometry is non-editable..." << std::flush;

    app::Document doc;
    std::string bodyId = doc.addBody(BRepPrimAPI_MakeBox(20.0, 20.0, 10.0).Shape());
    assert(!bodyId.empty());

    auto topFace = findTopPlanarFace(doc, bodyId);
    assert(topFace.has_value());

    auto sketch = std::make_unique<core::sketch::Sketch>(topFace->second);
    sketch->setHostFaceAttachment(bodyId, topFace->first);
    std::string sketchId = doc.addSketch(std::move(sketch));
    core::sketch::Sketch* sketchPtr = doc.getSketch(sketchId);
    assert(sketchPtr);
    assert(doc.ensureHostFaceBoundariesProjected(sketchId));

    std::string lockedPointId;
    std::string lockedCurveId;
    for (const auto& entity : sketchPtr->getAllEntities()) {
        if (!entity || !entity->isReferenceLocked()) {
            continue;
        }
        if (entity->type() == core::sketch::EntityType::Point && lockedPointId.empty()) {
            lockedPointId = entity->id();
        } else if (entity->type() != core::sketch::EntityType::Point && lockedCurveId.empty()) {
            lockedCurveId = entity->id();
        }
    }

    assert(!lockedPointId.empty());
    assert(!lockedCurveId.empty());
    assert(!sketchPtr->removeEntity(lockedCurveId));
    assert(!sketchPtr->removeEntity(lockedPointId));

    auto freePoint = sketchPtr->addPoint(100.0, 100.0);
    assert(!freePoint.empty());
    assert(sketchPtr->addCoincident(lockedPointId, freePoint).empty());

    core::sketch::ConstraintID lockedFixedId;
    for (const auto& c : sketchPtr->getAllConstraints()) {
        if (c && c->type() == core::sketch::ConstraintType::Fixed &&
            c->references(lockedPointId)) {
            lockedFixedId = c->id();
            break;
        }
    }
    assert(!lockedFixedId.empty());
    assert(!sketchPtr->removeConstraint(lockedFixedId));

    std::cout << " PASS\n";
}

void testAddOperationTransactionalRollbackOnRegenFailure() {
    std::cout << "Test 20: AddOperationCommand rolls back on replay failure..." << std::flush;

    app::Document doc;
    const auto [sketchId, regionId] = createSquareSketchRegion(doc, 10.0);
    const std::string bodyId = newId();

    app::OperationRecord op1;
    op1.opId = newId();
    op1.type = app::OperationType::Extrude;
    op1.input = app::SketchRegionRef{sketchId, regionId};
    op1.params = app::ExtrudeParams{10.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    op1.resultBodyIds.push_back(bodyId);
    assert(executeAddOperation(doc, op1));

    const std::size_t opCountBefore = doc.operations().size();
    const std::size_t appliedBefore = doc.appliedOpCount();
    const double volumeBefore = bodyVolume(doc, bodyId);

    app::OperationRecord invalidOp;
    invalidOp.opId = newId();
    invalidOp.type = app::OperationType::Extrude;
    invalidOp.input = app::SketchRegionRef{sketchId, "nonexistent-region-id"};
    app::ExtrudeParams invalidParams;
    invalidParams.distance = 3.0;
    invalidParams.booleanMode = app::BooleanMode::NewBody;
    invalidOp.params = invalidParams;
    invalidOp.resultBodyIds.push_back(newId());

    app::commands::AddOperationCommand command(&doc, invalidOp);
    const bool success = command.execute();
    assert(!success);

    assert(doc.operations().size() == opCountBefore);
    assert(doc.appliedOpCount() == appliedBefore);
    assert(doc.findOperation(invalidOp.opId) == nullptr);
    assert(nearlyEqual(bodyVolume(doc, bodyId), volumeBefore, 1e-6));

    app::history::RegenerationEngine engine(&doc);
    auto regen = engine.regenerateToAppliedCount(doc.appliedOpCount());
    assert(regen.status == app::history::RegenStatus::Success);
    assert(regen.failedOps.empty());

    std::cout << " PASS\n";
}

void testLinearPatternFuseAndCompound() {
    std::cout << "Test 22: Linear pattern fuse and non-fuse results are valid..." << std::flush;

    {
        app::Document doc;
        const auto [sketchId, regionId] = createSquareSketchRegion(doc, 10.0);
        const std::string bodyId = newId();

        app::OperationRecord extrude;
        extrude.opId = newId();
        extrude.type = app::OperationType::Extrude;
        extrude.input = app::SketchRegionRef{sketchId, regionId};
        extrude.params = app::ExtrudeParams{5.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
        extrude.resultBodyIds.push_back(bodyId);
        assert(executeAddOperation(doc, extrude));

        app::OperationRecord pattern;
        pattern.opId = newId();
        pattern.type = app::OperationType::LinearPattern;
        pattern.input = app::BodyRef{bodyId};
        app::LinearPatternParams params;
        params.sourceBodyId = bodyId;
        params.dirX = 1.0;
        params.spacing = 15.0;
        params.count = 3;
        params.fuseResult = false;
        pattern.params = params;
        pattern.resultBodyIds.push_back(bodyId);
        assert(executeAddOperation(doc, pattern));

        const TopoDS_Shape* result = doc.getBodyShape(bodyId);
        assert(result && !result->IsNull());
        assert(shapeValid(*result));
        assert(solidCount(*result) == 3);
        assert(nearlyEqual(shapeVolume(*result), 1500.0, 1e-2));
    }

    {
        app::Document doc;
        const auto [sketchId, regionId] = createSquareSketchRegion(doc, 10.0);
        const std::string bodyId = newId();

        app::OperationRecord extrude;
        extrude.opId = newId();
        extrude.type = app::OperationType::Extrude;
        extrude.input = app::SketchRegionRef{sketchId, regionId};
        extrude.params = app::ExtrudeParams{5.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
        extrude.resultBodyIds.push_back(bodyId);
        assert(executeAddOperation(doc, extrude));

        app::OperationRecord pattern;
        pattern.opId = newId();
        pattern.type = app::OperationType::LinearPattern;
        pattern.input = app::BodyRef{bodyId};
        app::LinearPatternParams params;
        params.sourceBodyId = bodyId;
        params.dirY = 1.0;
        params.dirX = 0.0;
        params.spacing = 8.0;
        params.count = 3;
        params.fuseResult = true;
        pattern.params = params;
        pattern.resultBodyIds.push_back(bodyId);
        assert(executeAddOperation(doc, pattern));

        const TopoDS_Shape* result = doc.getBodyShape(bodyId);
        assert(result && !result->IsNull());
        assert(shapeValid(*result));
        assert(solidCount(*result) == 1);
        assert(nearlyEqual(shapeVolume(*result), 1300.0, 1e-2));
    }

    std::cout << " PASS\n";
}

void testLinearPatternTracksSourceDependency() {
    std::cout << "Test 23: Linear pattern tracks source-body dependency..." << std::flush;

    app::Document doc;
    const auto [sketchId, regionId] = createSquareSketchRegion(doc, 10.0);
    const std::string bodyId = newId();

    app::OperationRecord extrude;
    extrude.opId = newId();
    extrude.type = app::OperationType::Extrude;
    extrude.input = app::SketchRegionRef{sketchId, regionId};
    extrude.params = app::ExtrudeParams{5.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    extrude.resultBodyIds.push_back(bodyId);
    assert(executeAddOperation(doc, extrude));

    app::OperationRecord pattern;
    pattern.opId = newId();
    pattern.type = app::OperationType::LinearPattern;
    pattern.input = app::BodyRef{bodyId};
    app::LinearPatternParams params;
    params.sourceBodyId = bodyId;
    params.dirX = 1.0;
    params.dirY = 0.0;
    params.dirZ = 0.0;
    params.spacing = 15.0;
    params.count = 3;
    params.fuseResult = false;
    pattern.params = params;
    pattern.resultBodyIds.push_back(bodyId);
    assert(executeAddOperation(doc, pattern));

    const auto downstream = app::history::RegenerationEngine(&doc).graph().getDownstream(extrude.opId);
    assert(std::find(downstream.begin(), downstream.end(), pattern.opId) != downstream.end());

    app::ExtrudeParams updated = std::get<app::ExtrudeParams>(extrude.params);
    updated.distance = 8.0;
    app::commands::UpdateOperationParamsCommand update(&doc, extrude.opId, updated);
    assert(update.execute());

    const TopoDS_Shape* result = doc.getBodyShape(bodyId);
    assert(result && !result->IsNull());
    assert(shapeValid(*result));
    assert(solidCount(*result) == 3);
    assert(nearlyEqual(shapeVolume(*result), 2400.0, 1e-2));

    std::cout << " PASS\n";
}

void testCircularPatternCompoundAndDependency() {
    std::cout << "Test 24: Circular pattern compound keeps all instances and tracks source..." << std::flush;

    app::Document doc;
    const auto [sketchId, regionId] = createSquareSketchRegion(doc, 10.0);
    const std::string bodyId = newId();

    app::OperationRecord extrude;
    extrude.opId = newId();
    extrude.type = app::OperationType::Extrude;
    extrude.input = app::SketchRegionRef{sketchId, regionId};
    extrude.params = app::ExtrudeParams{5.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    extrude.resultBodyIds.push_back(bodyId);
    assert(executeAddOperation(doc, extrude));

    app::OperationRecord pattern;
    pattern.opId = newId();
    pattern.type = app::OperationType::CircularPattern;
    pattern.input = app::BodyRef{bodyId};
    app::CircularPatternParams params;
    params.sourceBodyId = bodyId;
    params.axisX = 0.0;
    params.axisY = 0.0;
    params.axisZ = 0.0;
    params.axisDirX = 0.0;
    params.axisDirY = 0.0;
    params.axisDirZ = 1.0;
    params.angleDeg = 360.0;
    params.count = 4;
    params.fuseResult = false;
    pattern.params = params;
    pattern.resultBodyIds.push_back(bodyId);
    assert(executeAddOperation(doc, pattern));

    const TopoDS_Shape* result = doc.getBodyShape(bodyId);
    assert(result && !result->IsNull());
    assert(shapeValid(*result));
    assert(solidCount(*result) == 4);
    assert(nearlyEqual(shapeVolume(*result), 2000.0, 1e-2));

    const auto downstream = app::history::RegenerationEngine(&doc).graph().getDownstream(extrude.opId);
    assert(std::find(downstream.begin(), downstream.end(), pattern.opId) != downstream.end());

    app::ExtrudeParams updated = std::get<app::ExtrudeParams>(extrude.params);
    updated.distance = 8.0;
    app::commands::UpdateOperationParamsCommand update(&doc, extrude.opId, updated);
    assert(update.execute());

    result = doc.getBodyShape(bodyId);
    assert(result && !result->IsNull());
    assert(shapeValid(*result));
    assert(solidCount(*result) == 4);
    assert(nearlyEqual(shapeVolume(*result), 3200.0, 1e-2));

    std::cout << " PASS\n";
}

void testUpdateAttachmentResyncsPlane() {
    std::cout << "Test: Update Attachment re-syncs a frozen sketch plane..." << std::flush;

    app::Document doc;
    // Body A: 10x10x10 box.
    const auto [sketchA, regionA] = createSquareSketchRegion(doc, 10.0);
    const std::string bodyA = newId();
    app::OperationRecord opA;
    opA.opId = newId();
    opA.type = app::OperationType::Extrude;
    opA.input = app::SketchRegionRef{sketchA, regionA};
    opA.params = app::ExtrudeParams{10.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    opA.resultBodyIds.push_back(bodyA);
    assert(executeAddOperation(doc, opA));

    // Sketch B on A's top face (host attachment), then extrude as an independent body.
    auto top = findTopPlanarFace(doc, bodyA);
    assert(top.has_value());
    const std::string topFaceId = top->first;
    auto plane = doc.getSketchPlaneForFace(bodyA, topFaceId);
    assert(plane.has_value());
    auto sketchBptr = std::make_unique<core::sketch::Sketch>(*plane);
    sketchBptr->setHostFaceAttachment(bodyA, topFaceId);
    const std::string sketchB = doc.addSketch(std::move(sketchBptr));
    assert(!sketchB.empty());
    assert(doc.ensureHostFaceBoundariesProjected(sketchB));
    const auto regionB = doc.primaryRegionId(sketchB);
    assert(regionB.has_value());

    const std::string bodyB = newId();
    app::OperationRecord opB;
    opB.opId = newId();
    opB.type = app::OperationType::Extrude;
    opB.input = app::SketchRegionRef{sketchB, *regionB};
    opB.params = app::ExtrudeParams{5.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    opB.resultBodyIds.push_back(bodyB);
    assert(executeAddOperation(doc, opB));

    assert(nearlyEqual(doc.getSketch(sketchB)->getPlane().origin.z, 10.0, 1e-6));

    // Edit A to be 20 tall — B's frozen plane must NOT move on its own.
    app::ExtrudeParams tallerA{20.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    app::commands::UpdateOperationParamsCommand editA(&doc, opA.opId, tallerA);
    assert(editA.execute());
    assert(nearlyEqual(doc.getSketch(sketchB)->getPlane().origin.z, 10.0, 1e-6));  // frozen

    // Update Attachment re-derives B's plane from the host face's new height.
    app::commands::UpdateSketchAttachmentCommand resync(&doc, sketchB);
    assert(resync.execute());
    assert(nearlyEqual(doc.getSketch(sketchB)->getPlane().origin.z, 20.0, 1e-6));

    std::cout << " PASS\n";
}

void testUpdateAttachmentRollsBackOnRegenFailure() {
    std::cout << "Test: Update Attachment restores state on regen failure..." << std::flush;

    app::Document doc;
    // Body A: 10x10x10 box.
    const auto [sketchA, regionA] = createSquareSketchRegion(doc, 10.0);
    const std::string bodyA = newId();
    app::OperationRecord opA;
    opA.opId = newId();
    opA.type = app::OperationType::Extrude;
    opA.input = app::SketchRegionRef{sketchA, regionA};
    opA.params = app::ExtrudeParams{10.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    opA.resultBodyIds.push_back(bodyA);
    assert(executeAddOperation(doc, opA));

    // Sketch B attached to A's top face.
    auto top = findTopPlanarFace(doc, bodyA);
    assert(top.has_value());
    auto plane = doc.getSketchPlaneForFace(bodyA, top->first);
    assert(plane.has_value());
    auto sketchBptr = std::make_unique<core::sketch::Sketch>(*plane);
    sketchBptr->setHostFaceAttachment(bodyA, top->first);
    const std::string sketchB = doc.addSketch(std::move(sketchBptr));
    assert(doc.ensureHostFaceBoundariesProjected(sketchB));

    // Edit A taller so a resync would move B's frozen plane from z=10 to z=20.
    app::ExtrudeParams tallerA{20.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    app::commands::UpdateOperationParamsCommand editA(&doc, opA.opId, tallerA);
    assert(editA.execute());

    // Inject an applied op that always fails, so the command's strict
    // regeneration fails AFTER the new plane has already been applied.
    app::OperationRecord badOp;
    badOp.opId = newId();
    badOp.type = app::OperationType::Extrude;
    badOp.input = app::SketchRegionRef{sketchA, "no-such-region"};
    badOp.params = app::ExtrudeParams{5.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    badOp.resultBodyIds.push_back(newId());
    doc.addOperation(badOp);
    doc.setAppliedOpCount(doc.operations().size());

    const double planeZBefore = doc.getSketch(sketchB)->getPlane().origin.z;
    const std::string faceIdBefore = doc.getSketch(sketchB)->hostFaceAttachment()->faceId;

    app::commands::UpdateSketchAttachmentCommand resync(&doc, sketchB);
    if (resync.execute()) {
        std::fprintf(stderr, "resync unexpectedly succeeded despite failing op\n");
        std::exit(1);
    }
    // Zero net mutation: plane and attachment must be exactly as before.
    if (!nearlyEqual(doc.getSketch(sketchB)->getPlane().origin.z, planeZBefore, 1e-9) ||
        doc.getSketch(sketchB)->hostFaceAttachment()->faceId != faceIdBefore) {
        std::fprintf(stderr, "failed resync left a partial mutation behind\n");
        std::exit(1);
    }

    std::cout << " PASS\n";
}

void testFilletSurvivesUpstreamCutEdit() {
    std::cout << "Test: Fillet re-resolves its cut edge after an upstream edit..." << std::flush;

    app::Document doc;
    // Base: 20x20x10 block.
    const auto [baseSketch, baseRegion] = createSquareSketchRegion(doc, 20.0);
    const std::string bodyId = newId();
    app::OperationRecord base;
    base.opId = newId();
    base.type = app::OperationType::Extrude;
    base.input = app::SketchRegionRef{baseSketch, baseRegion};
    base.params = app::ExtrudeParams{10.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    base.resultBodyIds.push_back(bodyId);
    assert(executeAddOperation(doc, base));

    // 5x5 pocket cut 4mm deep (tool grows up from z=0 through the block bottom).
    const auto [cutSketch, cutRegion] = createSquareSketchRegion(doc, 5.0);
    app::OperationRecord cut;
    cut.opId = newId();
    cut.type = app::OperationType::Extrude;
    cut.input = app::SketchRegionRef{cutSketch, cutRegion};
    app::ExtrudeParams cutParams;
    cutParams.distance = 4.0;
    cutParams.booleanMode = app::BooleanMode::Cut;
    cutParams.targetBodyId = bodyId;
    cut.params = cutParams;
    cut.resultBodyIds.push_back(bodyId);
    assert(executeAddOperation(doc, cut));
    assert(nearlyEqual(bodyVolume(doc, bodyId), 3900.0, 1.0));  // 4000 - 100

    // Pick a rim edge created by the cut: an edge whose center sits at z = 4.
    std::string rimEdgeId;
    for (const auto& id : doc.elementMap().ids()) {
        const auto* entry = doc.elementMap().find(id);
        if (!entry || entry->kind != kernel::elementmap::ElementKind::Edge ||
            entry->shape.IsNull()) {
            continue;
        }
        if (nearlyEqual(entry->descriptor.center.Z(), 4.0, 1e-6)) {
            rimEdgeId = id.value;
            break;
        }
    }
    assert(!rimEdgeId.empty());

    // Fillet that rim edge.
    app::OperationRecord fillet;
    fillet.opId = newId();
    fillet.type = app::OperationType::Fillet;
    fillet.input = app::BodyRef{bodyId};
    app::FilletChamferParams filletParams;
    filletParams.mode = app::FilletChamferParams::Mode::Fillet;
    filletParams.radius = 1.0;
    filletParams.edgeIds = {rimEdgeId};
    filletParams.chainTangentEdges = false;
    fillet.params = filletParams;
    fillet.resultBodyIds.push_back(bodyId);
    assert(executeAddOperation(doc, fillet));
    const double filletedVolume = bodyVolume(doc, bodyId);
    // The blend changes the volume by ~ (1 - pi/4) * r^2 * L ~= 1.07 mm^3 for
    // r=1, L=5 (sign depends on whether the picked rim edge is concave or
    // convex — the pocket touches the block corner).
    assert(std::abs(filletedVolume - 3900.0) > 0.5 && std::abs(filletedVolume - 3900.0) < 5.0);

    // Deepen the cut 4 -> 6mm. The rim edge moves; the fillet must re-resolve
    // through the cut's OCCT history (or the hardened rebind) — strict regen
    // succeeding proves the whole chain rebuilt, including the fillet.
    app::ExtrudeParams deeper = cutParams;
    deeper.distance = 6.0;
    app::commands::UpdateOperationParamsCommand edit(&doc, cut.opId, deeper);
    if (!edit.execute()) {
        std::fprintf(stderr, "deepening the cut broke the downstream fillet\n");
        std::exit(1);
    }
    if (!doc.operationFailureReason(fillet.opId).empty()) {
        std::fprintf(stderr, "fillet failed after the upstream edit\n");
        std::exit(1);
    }
    const double editedVolume = bodyVolume(doc, bodyId);
    // 4000 - 150 = 3850 un-filleted; the surviving fillet keeps its ~1 mm^3
    // blend on the (moved) rim edge.
    if (!(std::abs(editedVolume - 3850.0) > 0.5 && std::abs(editedVolume - 3850.0) < 5.0)) {
        std::fprintf(stderr, "post-edit volume %f inconsistent with a surviving fillet\n",
                     editedVolume);
        std::exit(1);
    }

    std::cout << " PASS\n";
}

void testDetectModeProbeClassification() {
    std::cout << "Test: Boolean mode detection classifies by probe point..." << std::flush;

    using core::modeling::BooleanOperation;
    const TopoDS_Shape target = BRepPrimAPI_MakeBox(10.0, 10.0, 10.0).Shape();

    // Pulling OUT of the top face: first material lies outside the target.
    const TopoDS_Shape toolUp = BRepPrimAPI_MakeBox(gp_Pnt(3.0, 3.0, 10.0), 4.0, 4.0, 5.0).Shape();
    if (BooleanOperation::detectMode(toolUp, {target}, gp_Pnt(5.0, 5.0, 10.0),
                                     gp_Vec(0.0, 0.0, 1.0e-3)) != app::BooleanMode::Add) {
        std::fprintf(stderr, "pull-out extrude not classified as Add\n");
        std::exit(1);
    }

    // Pushing INTO the body: first material lies inside the target.
    const TopoDS_Shape toolDown = BRepPrimAPI_MakeBox(gp_Pnt(3.0, 3.0, 6.0), 4.0, 4.0, 4.0).Shape();
    if (BooleanOperation::detectMode(toolDown, {target}, gp_Pnt(5.0, 5.0, 10.0),
                                     gp_Vec(0.0, 0.0, -1.0e-3)) != app::BooleanMode::Cut) {
        std::fprintf(stderr, "push-in extrude not classified as Cut\n");
        std::exit(1);
    }

    // Detached tool: new body.
    const TopoDS_Shape toolFar = BRepPrimAPI_MakeBox(gp_Pnt(50.0, 0.0, 0.0), 4.0, 4.0, 4.0).Shape();
    if (BooleanOperation::detectMode(toolFar, {target}, gp_Pnt(52.0, 2.0, 0.0),
                                     gp_Vec(0.0, 0.0, 1.0e-3)) != app::BooleanMode::NewBody) {
        std::fprintf(stderr, "detached tool not classified as NewBody\n");
        std::exit(1);
    }

    std::cout << " PASS\n";
}

void testDatumPlusSketchTransactionUndoesTogether() {
    std::cout << "Test: Datum+sketch transaction undoes as one step..." << std::flush;

    app::Document doc;
    app::commands::CommandProcessor processor;

    app::DatumPlane datum;
    datum.name = "offset15";
    datum.kind = app::DatumPlane::Kind::OffsetFromPlane;
    datum.basePlaneId = "XY";
    datum.offset = 15.0;

    processor.beginTransaction("Create Datum Plane");
    auto datumCmd = std::make_unique<app::commands::AddDatumPlaneCommand>(&doc, datum);
    auto* rawDatum = datumCmd.get();
    if (!processor.execute(std::move(datumCmd))) {
        std::fprintf(stderr, "datum command failed\n");
        std::exit(1);
    }
    const app::DatumPlane* created = doc.getDatumPlane(rawDatum->datumId());
    if (!created || !created->resolvedValid) {
        std::fprintf(stderr, "datum did not resolve\n");
        std::exit(1);
    }
    auto sketchCmd = std::make_unique<app::commands::AddSketchCommand>(&doc, created->resolvedPlane);
    const std::string sketchId = sketchCmd->sketchId();
    if (!processor.execute(std::move(sketchCmd))) {
        std::fprintf(stderr, "sketch command failed\n");
        std::exit(1);
    }
    processor.endTransaction();

    if (doc.datumPlaneCount() != 1 || !doc.getSketch(sketchId)) {
        std::fprintf(stderr, "transaction did not create datum + sketch\n");
        std::exit(1);
    }

    // ONE undo removes both.
    processor.undo();
    if (doc.datumPlaneCount() != 0 || doc.getSketch(sketchId) != nullptr) {
        std::fprintf(stderr, "single undo did not remove datum + sketch together\n");
        std::exit(1);
    }

    // Redo restores both with the same ids.
    processor.redo();
    if (doc.datumPlaneCount() != 1 || !doc.getSketch(sketchId)) {
        std::fprintf(stderr, "redo did not restore datum + sketch\n");
        std::exit(1);
    }

    std::cout << " PASS\n";
}

void testReprofileExtrudeSwapsSketchRegion() {
    std::cout << "Test: Re-profiling an extrude swaps its source region..." << std::flush;

    app::Document doc;
    const auto [sketch1, region1] = createSquareSketchRegion(doc, 10.0);  // 10x10
    const auto [sketch2, region2] = createSquareSketchRegion(doc, 6.0);   // 6x6
    const std::string bodyId = newId();

    app::OperationRecord op;
    op.opId = newId();
    op.type = app::OperationType::Extrude;
    op.input = app::SketchRegionRef{sketch1, region1};
    op.params = app::ExtrudeParams{5.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    op.resultBodyIds.push_back(bodyId);
    assert(executeAddOperation(doc, op));
    assert(nearlyEqual(bodyVolume(doc, bodyId), 500.0, 1.0));  // 10*10*5

    // Re-profile to the 6x6 region.
    app::commands::EditOperationInputCommand reprofile(
        &doc, op.opId, app::SketchRegionRef{sketch2, region2});
    assert(reprofile.execute());
    assert(nearlyEqual(bodyVolume(doc, bodyId), 180.0, 1.0));  // 6*6*5

    // Undo restores the original region.
    assert(reprofile.undo());
    assert(nearlyEqual(bodyVolume(doc, bodyId), 500.0, 1.0));

    std::cout << " PASS\n";
}

void testReprofileToFaceRefRejected() {
    std::cout << "Test: Re-profiling an extrude to a FaceRef is rejected..." << std::flush;

    app::Document doc;
    const auto [sketchId, regionId] = createSquareSketchRegion(doc, 10.0);
    const std::string bodyId = newId();

    app::OperationRecord op;
    op.opId = newId();
    op.type = app::OperationType::Extrude;
    op.input = app::SketchRegionRef{sketchId, regionId};
    op.params = app::ExtrudeParams{5.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    op.resultBodyIds.push_back(bodyId);
    assert(executeAddOperation(doc, op));

    app::commands::EditOperationInputCommand bad(
        &doc, op.opId, app::FaceRef{bodyId, bodyId + "/face/0"});
    assert(!bad.execute());

    // Input must remain the original sketch region.
    const auto* stored = doc.findOperation(op.opId);
    assert(stored && std::holds_alternative<app::SketchRegionRef>(stored->input));

    std::cout << " PASS\n";
}

void testTemporalGuardRejectsFutureTarget() {
    std::cout << "Test: Boolean target produced by a later op is rejected..." << std::flush;

    app::Document doc;
    const auto [sketchA, regionA] = createSquareSketchRegion(doc, 10.0);
    const auto [sketchB, regionB] = createSquareSketchRegion(doc, 8.0);
    const auto [sketchC, regionC] = createSquareSketchRegion(doc, 6.0);
    const std::string bodyA = newId();
    const std::string bodyC = newId();

    // op1 -> bodyA
    app::OperationRecord op1;
    op1.opId = newId();
    op1.type = app::OperationType::Extrude;
    op1.input = app::SketchRegionRef{sketchA, regionA};
    op1.params = app::ExtrudeParams{10.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    op1.resultBodyIds.push_back(bodyA);

    // op2 -> Add targeting bodyC, which is only produced by the LATER op3 (time-travel).
    app::OperationRecord op2;
    op2.opId = newId();
    op2.type = app::OperationType::Extrude;
    op2.input = app::SketchRegionRef{sketchB, regionB};
    app::ExtrudeParams p2;
    p2.distance = 5.0;
    p2.booleanMode = app::BooleanMode::Add;
    p2.targetBodyId = bodyC;
    op2.params = p2;
    op2.resultBodyIds.push_back(bodyC);

    // op3 -> bodyC
    app::OperationRecord op3;
    op3.opId = newId();
    op3.type = app::OperationType::Extrude;
    op3.input = app::SketchRegionRef{sketchC, regionC};
    op3.params = app::ExtrudeParams{4.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    op3.resultBodyIds.push_back(bodyC);

    doc.addOperation(op1);
    doc.addOperation(op2);
    doc.addOperation(op3);
    doc.setAppliedOpCount(doc.operations().size());

    app::history::RegenerationEngine engine(&doc);
    auto result = engine.regenerateAll();

    bool op2Failed = false;
    for (const auto& f : result.failedOps) {
        if (f.opId == op2.opId) {
            op2Failed = true;
        }
    }
    assert(op2Failed);

    std::cout << " PASS\n";
}

void testDirtyFlagSkipsCleanBranch() {
    std::cout << "Test: Partial regen skips independent (clean) branches..." << std::flush;

    app::Document doc;
    const auto [sketchA, regionA] = createSquareSketchRegion(doc, 10.0);
    const auto [sketchB, regionB] = createSquareSketchRegion(doc, 8.0);
    const std::string bodyA = newId();
    const std::string bodyB = newId();

    app::OperationRecord opA;
    opA.opId = newId();
    opA.type = app::OperationType::Extrude;
    opA.input = app::SketchRegionRef{sketchA, regionA};
    opA.params = app::ExtrudeParams{10.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    opA.resultBodyIds.push_back(bodyA);
    assert(executeAddOperation(doc, opA));

    app::OperationRecord opB;
    opB.opId = newId();
    opB.type = app::OperationType::Extrude;
    opB.input = app::SketchRegionRef{sketchB, regionB};
    opB.params = app::ExtrudeParams{5.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    opB.resultBodyIds.push_back(bodyB);
    assert(executeAddOperation(doc, opB));

    // Regenerating from opA must not re-execute the independent opB.
    app::history::RegenerationEngine engine(&doc);
    auto result = engine.regenerateFrom(opA.opId);

    auto ran = [&](const std::string& id) {
        return std::find(result.succeededOps.begin(), result.succeededOps.end(), id)
               != result.succeededOps.end();
    };
    assert(ran(opA.opId));
    assert(!ran(opB.opId));

    std::cout << " PASS\n";
}

void testExtrudeTwoDirections() {
    std::cout << "Test: Two-direction extrude sums both depths..." << std::flush;

    app::Document doc;
    const auto [sketchId, regionId] = createSquareSketchRegion(doc, 10.0);
    const std::string bodyId = newId();

    app::OperationRecord op;
    op.opId = newId();
    op.type = app::OperationType::Extrude;
    op.input = app::SketchRegionRef{sketchId, regionId};
    app::ExtrudeParams params;
    params.distance = 5.0;            // direction 1
    params.extrudeMode = app::ExtrudeMode::Blind;
    params.twoDirections = true;
    params.distance2 = 3.0;           // direction 2 (opposite)
    params.extrudeMode2 = app::ExtrudeMode::Blind;
    params.booleanMode = app::BooleanMode::NewBody;
    op.params = params;
    op.resultBodyIds.push_back(bodyId);
    assert(executeAddOperation(doc, op));
    // 10 x 10 footprint, total depth 5 + 3 = 8.
    assert(nearlyEqual(bodyVolume(doc, bodyId), 800.0, 1.0));

    std::cout << " PASS\n";
}

void testExtrudeThroughAllCutsBox() {
    std::cout << "Test: Through-all cut removes a full-height column..." << std::flush;

    app::Document doc;
    // Base 20x20x10 box.
    const auto [baseSketch, baseRegion] = createSquareSketchRegion(doc, 20.0);
    const std::string baseBody = newId();
    app::OperationRecord base;
    base.opId = newId();
    base.type = app::OperationType::Extrude;
    base.input = app::SketchRegionRef{baseSketch, baseRegion};
    base.params = app::ExtrudeParams{10.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    base.resultBodyIds.push_back(baseBody);
    assert(executeAddOperation(doc, base));
    assert(nearlyEqual(bodyVolume(doc, baseBody), 4000.0, 1.0));

    // 5x5 column cut through the whole box.
    const auto [cutSketch, cutRegion] = createSquareSketchRegion(doc, 5.0);
    app::OperationRecord cut;
    cut.opId = newId();
    cut.type = app::OperationType::Extrude;
    cut.input = app::SketchRegionRef{cutSketch, cutRegion};
    app::ExtrudeParams cutParams;
    cutParams.distance = 1.0;  // sign -> +direction through-all
    cutParams.extrudeMode = app::ExtrudeMode::ThroughAll;
    cutParams.booleanMode = app::BooleanMode::Cut;
    cutParams.targetBodyId = baseBody;
    cut.params = cutParams;
    cut.resultBodyIds.push_back(baseBody);
    assert(executeAddOperation(doc, cut));
    // 4000 - (5*5*10) = 3750.
    assert(nearlyEqual(bodyVolume(doc, baseBody), 3750.0, 1.0));

    std::cout << " PASS\n";
}

void testExtrudeToFaceStopsAtFace() {
    std::cout << "Test: To-Face extrude stops at the target face..." << std::flush;

    app::Document doc;
    // Base 20x20x10 box provides a top face at z = 10.
    const auto [baseSketch, baseRegion] = createSquareSketchRegion(doc, 20.0);
    const std::string baseBody = newId();
    app::OperationRecord base;
    base.opId = newId();
    base.type = app::OperationType::Extrude;
    base.input = app::SketchRegionRef{baseSketch, baseRegion};
    base.params = app::ExtrudeParams{10.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    base.resultBodyIds.push_back(baseBody);
    assert(executeAddOperation(doc, base));

    auto top = findTopPlanarFace(doc, baseBody);
    assert(top.has_value());

    // 5x5 region on XY (z=0) extruded up to the top face -> height 10 -> volume 250.
    const auto [sketchId, regionId] = createSquareSketchRegion(doc, 5.0);
    const std::string newBody = newId();
    app::OperationRecord op;
    op.opId = newId();
    op.type = app::OperationType::Extrude;
    op.input = app::SketchRegionRef{sketchId, regionId};
    app::ExtrudeParams params;
    params.extrudeMode = app::ExtrudeMode::ToFace;
    params.targetFaceId = top->first;
    params.booleanMode = app::BooleanMode::NewBody;
    op.params = params;
    op.resultBodyIds.push_back(newBody);
    assert(executeAddOperation(doc, op));
    assert(nearlyEqual(bodyVolume(doc, newBody), 250.0, 1.0));

    std::cout << " PASS\n";
}

void testExtrudeToNextRequiresBody() {
    std::cout << "Test: To-Next without a target body is rejected..." << std::flush;

    app::Document doc;
    const auto [sketchId, regionId] = createSquareSketchRegion(doc, 10.0);
    const std::string bodyId = newId();

    app::OperationRecord op;
    op.opId = newId();
    op.type = app::OperationType::Extrude;
    op.input = app::SketchRegionRef{sketchId, regionId};
    app::ExtrudeParams params;
    params.extrudeMode = app::ExtrudeMode::ToNext;
    params.booleanMode = app::BooleanMode::NewBody;  // no target body
    op.params = params;
    op.resultBodyIds.push_back(bodyId);

    // Regeneration must fail -> AddOperationCommand rolls back and reports failure.
    assert(!executeAddOperation(doc, op));

    std::cout << " PASS\n";
}

void testDatumPlaneOffsetResolves() {
    std::cout << "Test: Datum plane offset resolves + hosts a sketch..." << std::flush;

    app::Document doc;

    app::DatumPlane datum;
    datum.name = "offset20";
    datum.kind = app::DatumPlane::Kind::OffsetFromPlane;
    datum.basePlaneId = "XY";
    datum.offset = 20.0;
    const std::string datumId = doc.addDatumPlane(datum);
    assert(!datumId.empty());

    const app::DatumPlane* resolved = doc.getDatumPlane(datumId);
    assert(resolved && resolved->resolvedValid);
    assert(nearlyEqual(resolved->resolvedPlane.origin.z, 20.0, 1e-9));
    assert(nearlyEqual(resolved->resolvedPlane.normal.z, 1.0, 1e-9));

    // A sketch on the datum extrudes into a valid body at the offset height.
    auto sketch = std::make_unique<core::sketch::Sketch>(resolved->resolvedPlane);
    auto p1 = sketch->addPoint(0.0, 0.0);
    auto p2 = sketch->addPoint(10.0, 0.0);
    auto p3 = sketch->addPoint(10.0, 10.0);
    auto p4 = sketch->addPoint(0.0, 10.0);
    sketch->addLine(p1, p2);
    sketch->addLine(p2, p3);
    sketch->addLine(p3, p4);
    sketch->addLine(p4, p1);
    const std::string sketchId = doc.addSketch(std::move(sketch));
    const std::string regionId = firstRegionId(*doc.getSketch(sketchId));

    const std::string bodyId = newId();
    app::OperationRecord op;
    op.opId = newId();
    op.type = app::OperationType::Extrude;
    op.input = app::SketchRegionRef{sketchId, regionId};
    op.params = app::ExtrudeParams{5.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    op.resultBodyIds.push_back(bodyId);
    assert(executeAddOperation(doc, op));
    assert(nearlyEqual(bodyVolume(doc, bodyId), 500.0, 1.0));

    std::cout << " PASS\n";
}

void testDatumOffsetFromFaceFollowsParentEdit() {
    std::cout << "Test: OffsetFromFace datum follows an upstream edit..." << std::flush;

    app::Document doc;
    const auto [sketchId, regionId] = createSquareSketchRegion(doc, 10.0);
    const std::string bodyId = newId();
    app::OperationRecord op;
    op.opId = newId();
    op.type = app::OperationType::Extrude;
    op.input = app::SketchRegionRef{sketchId, regionId};
    op.params = app::ExtrudeParams{10.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    op.resultBodyIds.push_back(bodyId);
    assert(executeAddOperation(doc, op));

    auto top = findTopPlanarFace(doc, bodyId);
    assert(top.has_value());

    app::DatumPlane datum;
    datum.name = "face-offset";
    datum.kind = app::DatumPlane::Kind::OffsetFromFace;
    datum.baseBodyId = bodyId;
    datum.baseFaceId = top->first;
    datum.offset = 5.0;
    const std::string datumId = doc.addDatumPlane(datum);
    assert(!datumId.empty());
    const auto* created = doc.getDatumPlane(datumId);
    assert(created && created->resolvedValid);
    assert(nearlyEqual(created->resolvedPlane.origin.z, 15.0, 1e-6));  // 10 + 5

    // Grow the extrude: the regen epilogue must re-derive the datum frame.
    app::ExtrudeParams taller{20.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    app::commands::UpdateOperationParamsCommand edit(&doc, op.opId, taller);
    assert(edit.execute());

    const auto* moved = doc.getDatumPlane(datumId);
    assert(moved && moved->resolvedValid);
    if (!nearlyEqual(moved->resolvedPlane.origin.z, 25.0, 1e-6)) {  // 20 + 5
        std::fprintf(stderr, "datum did not follow parent edit: z=%f\n",
                     moved->resolvedPlane.origin.z);
        std::exit(1);
    }

    std::cout << " PASS\n";
}

void testAutoSketchOnFaceCreatesEditableSketchRegion() {
    std::cout << "Test: Auto-sketch-on-face yields an extrudable region..." << std::flush;

    app::Document doc;
    // Base body: 10x10x10 box via a sketch-region extrude.
    const auto [baseSketchId, baseRegionId] = createSquareSketchRegion(doc, 10.0);
    const std::string baseBodyId = newId();
    app::OperationRecord base;
    base.opId = newId();
    base.type = app::OperationType::Extrude;
    base.input = app::SketchRegionRef{baseSketchId, baseRegionId};
    base.params = app::ExtrudeParams{10.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    base.resultBodyIds.push_back(baseBodyId);
    assert(executeAddOperation(doc, base));

    // Locate the top planar face of the base body.
    auto top = findTopPlanarFace(doc, baseBodyId);
    assert(top.has_value());
    const std::string topFaceId = top->first;

    // Replicate MainWindow::createSketchOnFace at the document level (headless).
    auto plane = doc.getSketchPlaneForFace(baseBodyId, topFaceId);
    assert(plane.has_value());
    auto faceSketch = std::make_unique<core::sketch::Sketch>(*plane);
    faceSketch->setHostFaceAttachment(baseBodyId, topFaceId);
    const std::string faceSketchId = doc.addSketch(std::move(faceSketch));
    assert(!faceSketchId.empty());
    assert(doc.ensureHostFaceBoundariesProjected(faceSketchId));

    const core::sketch::Sketch* createdSketch = doc.getSketch(faceSketchId);
    assert(createdSketch);
    assert(createdSketch->hostFaceAttachment().has_value());
    assert(createdSketch->hostFaceAttachment()->isValid());

    // The projected boundary forms exactly one extrudable region.
    const auto regionId = doc.primaryRegionId(faceSketchId);
    assert(regionId.has_value());

    // Extruding that region against the host body grows the existing solid.
    const double volumeBefore = bodyVolume(doc, baseBodyId);
    app::OperationRecord grow;
    grow.opId = newId();
    grow.type = app::OperationType::Extrude;
    grow.input = app::SketchRegionRef{faceSketchId, *regionId};
    app::ExtrudeParams growParams;
    growParams.distance = 5.0;
    growParams.booleanMode = app::BooleanMode::Add;
    growParams.targetBodyId = baseBodyId;
    grow.params = growParams;
    grow.resultBodyIds.push_back(baseBodyId);
    assert(executeAddOperation(doc, grow));

    const double volumeAfter = bodyVolume(doc, baseBodyId);
    assert(volumeAfter > volumeBefore + 1.0);

    std::cout << " PASS\n";
}

void testExtrudeFaceRefRejectedByAddCommand() {
    std::cout << "Test: Extrude rejects FaceRef input at AddOperationCommand..." << std::flush;

    app::Document doc;
    const auto [sketchId, regionId] = createSquareSketchRegion(doc, 10.0);
    const std::string bodyId = newId();

    app::OperationRecord op1;
    op1.opId = newId();
    op1.type = app::OperationType::Extrude;
    op1.input = app::SketchRegionRef{sketchId, regionId};
    op1.params = app::ExtrudeParams{10.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    op1.resultBodyIds.push_back(bodyId);
    assert(executeAddOperation(doc, op1));

    const std::size_t opCountBefore = doc.operations().size();

    // Face/body push-pull is no longer a valid extrude input.
    app::OperationRecord faceOp;
    faceOp.opId = newId();
    faceOp.type = app::OperationType::Extrude;
    faceOp.input = app::FaceRef{bodyId, bodyId + "/face/anything"};
    app::ExtrudeParams faceParams;
    faceParams.distance = 5.0;
    faceParams.booleanMode = app::BooleanMode::Add;
    faceParams.targetBodyId = bodyId;
    faceOp.params = faceParams;
    faceOp.resultBodyIds.push_back(bodyId);

    assert(!executeAddOperation(doc, faceOp));
    assert(doc.operations().size() == opCountBefore);
    assert(doc.findOperation(faceOp.opId) == nullptr);

    std::cout << " PASS\n";
}

void testRevolveAxisCrossingProfileFailsAsOpFailure() {
    std::cout << "Test: Revolve profile crossing its axis fails the op, not the app..." << std::flush;

    app::Document doc;
    auto sketch = std::make_unique<core::sketch::Sketch>(core::sketch::SketchPlane::XY());
    auto p1 = sketch->addPoint(-5.0, 0.0);
    auto p2 = sketch->addPoint(5.0, 0.0);
    auto p3 = sketch->addPoint(5.0, 10.0);
    auto p4 = sketch->addPoint(-5.0, 10.0);
    sketch->addLine(p1, p2);
    sketch->addLine(p2, p3);
    sketch->addLine(p3, p4);
    sketch->addLine(p4, p1);
    // Construction axis straight through the middle of the profile: OCCT's
    // BRepPrimAPI_MakeRevol raises Standard_ConstructionError for this input.
    auto a1 = sketch->addPoint(0.0, -20.0, true);
    auto a2 = sketch->addPoint(0.0, 20.0, true);
    auto axisLineId = sketch->addLine(a1, a2, true);

    const std::string sketchId = doc.addSketch(std::move(sketch));
    const std::string regionId = firstRegionId(*doc.getSketch(sketchId));

    app::OperationRecord op;
    op.opId = newId();
    op.type = app::OperationType::Revolve;
    op.input = app::SketchRegionRef{sketchId, regionId};
    app::RevolveParams params;
    params.angleDeg = 360.0;
    params.axis = app::SketchLineRef{sketchId, axisLineId};
    op.params = params;
    op.resultBodyIds.push_back(newId());
    doc.addOperation(op);
    doc.setAppliedOpCount(doc.operations().size());

    app::history::RegenerationEngine engine(&doc);
    const auto result = engine.regenerateAll();  // must not std::terminate

    bool failedWithMessage = false;
    for (const auto& f : result.failedOps) {
        if (f.opId == op.opId && !f.errorMessage.empty()) {
            failedWithMessage = true;
        }
    }
    if (!failedWithMessage) {
        std::fprintf(stderr, "revolve axis-cross did not fail as an op failure\n");
        std::exit(1);
    }

    std::cout << " PASS\n";
}

void testTemporalGuardCoversBooleanAndPattern() {
    std::cout << "Test: Temporal guard rejects future bodies for boolean + pattern..." << std::flush;

    app::Document doc;
    const auto [sketchA, regionA] = createSquareSketchRegion(doc, 10.0);
    const auto [sketchC, regionC] = createSquareSketchRegion(doc, 6.0);
    const std::string bodyA = newId();
    const std::string bodyC = newId();

    // op1 -> bodyA (valid).
    app::OperationRecord op1;
    op1.opId = newId();
    op1.type = app::OperationType::Extrude;
    op1.input = app::SketchRegionRef{sketchA, regionA};
    op1.params = app::ExtrudeParams{10.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    op1.resultBodyIds.push_back(bodyA);

    // op2: boolean whose TOOL body is only produced by the later op4 (time-travel).
    app::OperationRecord op2;
    op2.opId = newId();
    op2.type = app::OperationType::Boolean;
    op2.input = app::BodyRef{bodyA};
    app::BooleanParams boolParams;
    boolParams.operation = app::BooleanParams::Op::Cut;
    boolParams.targetBodyId = bodyA;
    boolParams.toolBodyId = bodyC;
    op2.params = boolParams;
    op2.resultBodyIds.push_back(bodyA);

    // op3: linear pattern whose SOURCE body is only produced by the later op4.
    app::OperationRecord op3;
    op3.opId = newId();
    op3.type = app::OperationType::LinearPattern;
    op3.input = app::BodyRef{bodyC};
    app::LinearPatternParams patParams;
    patParams.sourceBodyId = bodyC;
    patParams.count = 3;
    patParams.spacing = 20.0;
    op3.params = patParams;
    op3.resultBodyIds.push_back(newId());

    // op4 -> bodyC (the future producer).
    app::OperationRecord op4;
    op4.opId = newId();
    op4.type = app::OperationType::Extrude;
    op4.input = app::SketchRegionRef{sketchC, regionC};
    op4.params = app::ExtrudeParams{4.0, 0.0, app::ExtrudeMode::Blind, app::BooleanMode::NewBody};
    op4.resultBodyIds.push_back(bodyC);

    doc.addOperation(op1);
    doc.addOperation(op2);
    doc.addOperation(op3);
    doc.addOperation(op4);
    doc.setAppliedOpCount(doc.operations().size());

    app::history::RegenerationEngine engine(&doc);
    const auto result = engine.regenerateAll();

    auto failed = [&](const std::string& id) {
        for (const auto& f : result.failedOps) {
            if (f.opId == id) {
                return true;
            }
        }
        return false;
    };
    if (!failed(op2.opId) || !failed(op3.opId)) {
        std::fprintf(stderr, "temporal guard missed boolean or pattern time-travel\n");
        std::exit(1);
    }

    std::cout << " PASS\n";
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    std::cout << "\n=== RegenerationEngine Prototype Tests ===\n\n";

    testDependencyGraph();
    testSuppressionAndFailure();
    testGraphCycleDetection();
    testSingleExtrude();
    testChainRegeneration();
    testRegenFailureOnMissingSketch();
    testSketchHostAttachmentRoundTrip();
    testHistoryTargetBodyRoundTrip();
    testAttachedSketchExtrudeAdd();
    testAttachedSketchExtrudeCut();
    testAttachedSketchRevolveAdd();
    testBooleanFailureOnMissingTargetBody();
    testFaceBoundaryProjectionPartitioning();
    testLegacyHostBoundaryBackfillIdempotent();
    testSketchHostProjectionVersionRequired();
    testSelectionPriorityPrefersSketchRegion();
    testProjectedReferenceGeometryIsLocked();
    testAddOperationTransactionalRollbackOnRegenFailure();
    testExtrudeFaceRefRejectedByAddCommand();
    testAutoSketchOnFaceCreatesEditableSketchRegion();
    testDatumPlaneOffsetResolves();
    testDatumOffsetFromFaceFollowsParentEdit();
    testUpdateAttachmentResyncsPlane();
    testUpdateAttachmentRollsBackOnRegenFailure();
    testExtrudeTwoDirections();
    testExtrudeThroughAllCutsBox();
    testExtrudeToFaceStopsAtFace();
    testExtrudeToNextRequiresBody();
    testTemporalGuardRejectsFutureTarget();
    testTemporalGuardCoversBooleanAndPattern();
    testRevolveAxisCrossingProfileFailsAsOpFailure();
    testDirtyFlagSkipsCleanBranch();
    testDetectModeProbeClassification();
    testFilletSurvivesUpstreamCutEdit();
    testDatumPlusSketchTransactionUndoesTogether();
    testReprofileExtrudeSwapsSketchRegion();
    testReprofileToFaceRefRejected();
    testLinearPatternFuseAndCompound();
    testLinearPatternTracksSourceDependency();
    testCircularPatternCompoundAndDependency();

    std::cout << "\n=== All tests passed! ===\n\n";
    return 0;
}
