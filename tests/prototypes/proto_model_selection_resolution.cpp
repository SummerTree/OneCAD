#include "app/selection/SelectionManager.h"

#include <QPoint>

#include <cassert>
#include <iostream>

using namespace onecad::app::selection;

namespace {

SelectionItem makeItem(SelectionKind kind,
                       const std::string& ownerId,
                       const std::string& elementId,
                       int priority,
                       double screenDistance,
                       double depth) {
    SelectionItem item;
    item.kind = kind;
    item.id = {ownerId, elementId};
    item.priority = priority;
    item.screenDistance = screenDistance;
    item.depth = depth;
    return item;
}

void testFrontMostAreaWinsWithoutDeepSelect() {
    std::cout << "Test 1: Front-most area wins without deep select..." << std::flush;

    SelectionManager manager;
    manager.setMode(SelectionMode::Model);
    manager.setDeepSelectEnabled(false);

    SelectionFilter filter;
    filter.allowedKinds = {
        SelectionKind::SketchPoint,
        SelectionKind::SketchEdge,
        SelectionKind::SketchRegion,
        SelectionKind::Vertex,
        SelectionKind::Edge,
        SelectionKind::Face
    };
    manager.setFilter(filter);

    PickResult pick;
    pick.hits.push_back(makeItem(SelectionKind::Face, "body-1", "face-1", 2, 0.0, 1.0));
    pick.hits.push_back(makeItem(SelectionKind::SketchRegion, "sketch-1", "region-1", 2, 0.0, 2.0));

    const auto action = manager.handleClick(pick, {}, QPoint(100, 100));
    assert(action.selectionChanged);
    assert(!action.needsDeepSelect);
    assert(manager.selection().size() == 1);
    assert(manager.selection().front().kind == SelectionKind::Face);
    assert(manager.selection().front().id.elementId == "face-1");

    std::cout << " PASS\n";
}

void testRepeatedClickDoesNotCycleWhenDeepSelectDisabled() {
    std::cout << "Test 2: Repeated click does not cycle when deep select is disabled..." << std::flush;

    SelectionManager manager;
    manager.setMode(SelectionMode::Model);
    manager.setDeepSelectEnabled(false);

    SelectionFilter filter;
    filter.allowedKinds = {SelectionKind::Face};
    manager.setFilter(filter);

    PickResult pick;
    pick.hits.push_back(makeItem(SelectionKind::Face, "body-1", "face-front", 2, 0.0, 1.0));
    pick.hits.push_back(makeItem(SelectionKind::Face, "body-2", "face-back", 2, 0.0, 2.0));

    auto action = manager.handleClick(pick, {}, QPoint(200, 200));
    assert(action.selectionChanged);
    assert(manager.selection().size() == 1);
    assert(manager.selection().front().id.elementId == "face-front");

    action = manager.handleClick(pick, {}, QPoint(200, 200));
    assert(action.selectionChanged);
    assert(manager.selection().size() == 1);
    assert(manager.selection().front().id.elementId == "face-front");

    std::cout << " PASS\n";
}

void testBodyIsIgnoredBySingleClickFilter() {
    std::cout << "Test 3: Body is ignored by single-click model filter..." << std::flush;

    SelectionManager manager;
    manager.setMode(SelectionMode::Model);
    manager.setDeepSelectEnabled(false);

    SelectionFilter filter;
    filter.allowedKinds = {SelectionKind::Face};
    manager.setFilter(filter);

    PickResult pick;
    pick.hits.push_back(makeItem(SelectionKind::Body, "body-1", "body-1", 3, 0.0, 1.0));
    pick.hits.push_back(makeItem(SelectionKind::Face, "body-1", "face-1", 2, 0.0, 1.0));

    const auto top = manager.topCandidate(pick);
    assert(top.has_value());
    assert(top->kind == SelectionKind::Face);
    assert(top->id.elementId == "face-1");

    std::cout << " PASS\n";
}

} // namespace

int main() {
    testFrontMostAreaWinsWithoutDeepSelect();
    testRepeatedClickDoesNotCycleWhenDeepSelectDisabled();
    testBodyIsIgnoredBySingleClickFilter();

    std::cout << "Model selection resolution prototype passed.\n";
    return 0;
}
