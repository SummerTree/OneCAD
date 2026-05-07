#include "core/sketch/Sketch.h"
#include "core/sketch/SketchRenderer.h"
#include "core/loop/RegionUtils.h"

#include <algorithm>
#include <cassert>
#include <iostream>

using namespace onecad::core;

int main() {
    sketch::Sketch sketch;

    auto p1 = sketch.addPoint(0.0, 0.0);
    auto p2 = sketch.addPoint(10.0, 0.0);
    auto p3 = sketch.addPoint(10.0, 10.0);
    auto p4 = sketch.addPoint(0.0, 10.0);
    sketch.addLine(p1, p2);
    sketch.addLine(p2, p3);
    sketch.addLine(p3, p4);
    sketch.addLine(p4, p1);

    auto h1 = sketch.addPoint(3.0, 3.0);
    auto h2 = sketch.addPoint(7.0, 3.0);
    auto h3 = sketch.addPoint(7.0, 7.0);
    auto h4 = sketch.addPoint(3.0, 7.0);
    sketch.addLine(h1, h2);
    sketch.addLine(h2, h3);
    sketch.addLine(h3, h4);
    sketch.addLine(h4, h1);

    loop::LoopDetector detector;
    detector.setConfig(loop::makeRegionDetectionConfig());
    auto detection = detector.detect(sketch);
    assert(detection.success);
    auto regions = loop::buildRegionDefinitions(detection, sketch::constants::COINCIDENCE_TOLERANCE);
    auto ringRegion = std::find_if(regions.begin(), regions.end(), [](const loop::RegionDefinition& region) {
        return !region.holes.empty();
    });
    assert(ringRegion != regions.end());
    assert(!ringRegion->signature.empty());
    assert(ringRegion->signature == loop::regionSignature(*ringRegion));
    assert(ringRegion->signature.find(loop::regionKey(ringRegion->outerLoop)) != std::string::npos);
    assert(ringRegion->signature.find(loop::regionKey(ringRegion->holes.front())) != std::string::npos);

    sketch::SketchRenderer renderer;
    renderer.setSketch(&sketch);
    renderer.updateGeometry();

    renderer.setRegionDisplayMode(sketch::RegionDisplayMode::HoverFirst);
    assert(renderer.regionDisplayMode() == sketch::RegionDisplayMode::HoverFirst);
    renderer.setRegionFillSuppressed(true);
    assert(renderer.isRegionFillSuppressed());
    renderer.setRegionFillSuppressed(false);
    assert(!renderer.isRegionFillSuppressed());

    auto ringHits = renderer.pickRegionCandidates({1.0, 1.0});
    assert(ringHits.size() == 1);
    assert(ringHits.front().hasHoles);
    assert(ringHits.front().containmentDepth == 0);

    auto innerHits = renderer.pickRegionCandidates({5.0, 5.0});
    assert(innerHits.size() == 1);
    assert(!innerHits.front().hasHoles);
    assert(innerHits.front().containmentDepth > 0);
    assert(innerHits.front().area < ringHits.front().area);

    auto innerInfo = renderer.getRegionPickHit(innerHits.front().id);
    assert(innerInfo.has_value());
    assert(innerInfo->id == innerHits.front().id);
    assert(innerInfo->containmentDepth == innerHits.front().containmentDepth);

    auto outsideHits = renderer.pickRegionCandidates({12.0, 12.0});
    assert(outsideHits.empty());

    std::cout << "Sketch region candidate prototype: OK" << std::endl;
    return 0;
}
