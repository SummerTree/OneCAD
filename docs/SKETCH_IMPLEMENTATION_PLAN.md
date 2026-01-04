# OneCAD Sketch System Implementation Plan

Status: **Phase 4 Complete** - Rendering System Implemented

**Last Updated:** 2026-01-04 *(Detailed UX Specifications Added)*

---

## Detailed UX Specifications (Shapr3D-Style)

### Snap System
| Setting | Value |
|---------|-------|
| Snap radius | **2mm** (sketch coordinates, constant regardless of zoom) |
| Snap visual | Cursor changes to snap icon (○ vertex, ⊕ midpoint, ◎ center) |
| Priority order | Vertex > Endpoint > Midpoint > Center > Quadrant > Intersection > OnCurve > Grid |

### Auto-Constraining
| Setting | Value |
|---------|-------|
| Default state | **ON** (Shapr3D style) |
| Ghost icon opacity | **50%** |
| Horizontal/Vertical tolerance | **±5°** |
| Perpendicular tolerance | **~90±5°** |
| Undo behavior | Single Cmd+Z undoes constraint only (keeps geometry) |

### Inference Rules
| Condition | Inferred Constraint |
|-----------|---------------------|
| Line within ±5° of horizontal | Horizontal |
| Line within ±5° of vertical | Vertical |
| Endpoint within 2mm of existing point | Coincident |
| Arc starts at line endpoint, tangent direction | Tangent |
| Lines meet at ~90° | Perpendicular |

### Drawing Behavior
| Setting | Behavior |
|---------|----------|
| Polyline mode | **Auto-chain ON** (double-click/Esc to finish) |
| Empty sketch | **Don't create** until geometry exists |
| Minimum line length | **0.01mm** |
| Click empty area | **Deselect** (standard behavior) |
| Shortcuts during drawing | **Immediate apply** (press 'H' → horizontal) |

### Dimension Editing
| Setting | Behavior |
|---------|----------|
| Activation | **Double-click segment** → dimension input appears |
| Display | **On geometry** (Shapr3D style, label near constrained entity) |
| Expression support | Basic math (+,-,*,/) in v1, variables in v2 |

### Rectangle Tool
| Setting | Behavior |
|---------|----------|
| Auto-constraints | Perpendicular corners + equal opposite sides |

### Conflict Handling
| Setting | Behavior |
|---------|----------|
| Over-constrained | **Block + show dialog** with Remove buttons |
| Under-defined extrude | **Allow** (direct modeling style) |

### Region Selection
| Setting | Behavior |
|---------|----------|
| Hover | Shows preview highlight |
| Click | Commits selection |
| Multi-select | Shift+Click |

### Visual Feedback
| Element | Style |
|---------|-------|
| Construction geometry | Light blue dashed |
| Lock/Fix indicator | Small padlock icon |
| Constraint panel | Floating right side, auto-show in sketch mode |

### Additional Behaviors
| Setting | Behavior |
|---------|----------|
| Trim tool | Click segment to delete (removes portion between intersections) |
| Mirror tool | Creates geometry + symmetric constraint (linked) |
| Delete point with lines | Cascade delete (removes connected geometry) |
| Arc tool primary mode | 3-Point Arc (start → point-on-arc → end) |
| Tangent inference | Auto when line starts from arc endpoint in tangent direction |

---

## Implementation Status Overview

### ✅ COMPLETED - Phase 1: Architecture Foundation

| Component | File | Status |
|-----------|------|--------|
| Type Definitions | `src/core/sketch/SketchTypes.h` | ✅ Complete |
| Entity Base Class | `src/core/sketch/SketchEntity.h/cpp` | ✅ Complete |
| Point Entity | `src/core/sketch/SketchPoint.h/cpp` | ✅ Complete (277 lines) |
| Line Entity | `src/core/sketch/SketchLine.h/cpp` | ✅ Complete (350 lines) |
| Arc Entity | `src/core/sketch/SketchArc.h/cpp` | ✅ Complete (477 lines) |
| Circle Entity | `src/core/sketch/SketchCircle.h/cpp` | ✅ Complete (282 lines) |
| Ellipse Entity | *Declared in API* | ❌ **NOT IMPLEMENTED** |
| Constraint Base | `src/core/sketch/SketchConstraint.h/cpp` | ✅ Complete |
| Concrete Constraints | `src/core/sketch/constraints/Constraints.h/cpp` | ✅ Complete (1485 lines) |
| Sketch Manager | `src/core/sketch/Sketch.h/cpp` | ✅ Complete (1370 lines) |
| Solver Interface | `src/core/sketch/solver/ConstraintSolver.h` | ✅ Complete |
| CMake Configuration | `src/core/CMakeLists.txt` | ✅ Complete |

### ✅ COMPLETED - Phase 2: PlaneGCS Integration & Core Implementation

| Component | File | Status |
|-----------|------|--------|
| PlaneGCS Library | `third_party/planegcs/` | ✅ Complete |
| Constraint Solver | `src/core/sketch/solver/ConstraintSolver.cpp` | ✅ Complete (1014 lines) |
| Solver Adapter | `src/core/sketch/solver/SolverAdapter.h/cpp` | ✅ Complete (85 lines) |
| Sketch.cpp | `src/core/sketch/Sketch.cpp` | ✅ Complete (902 lines) |
| Solve & Drag | `Sketch::solve()`, `Sketch::solveWithDrag()` | ✅ Complete |
| DOF Calculation | `Sketch::getDegreesOfFreedom()` | ✅ Complete |
| Conflict Detection | `ConstraintSolver::findRedundantConstraints()` | ✅ Complete |
| Serialization | `Sketch::toJson()`, `Sketch::fromJson()` | ✅ Complete |

### PlaneGCS-Mapped Constraints (12 types integrated)

| OneCAD Constraint | PlaneGCS Constraint | Status |
|-------------------|---------------------|--------|
| Coincident | `GCS::addConstraintP2PCoincident` | ✅ |
| Horizontal | `GCS::addConstraintHorizontal` | ✅ |
| Vertical | `GCS::addConstraintVertical` | ✅ |
| Parallel | `GCS::addConstraintParallel` | ✅ |
| Perpendicular | `GCS::addConstraintPerpendicular` | ✅ |
| Distance | `GCS::addConstraintP2PDistance/P2LDistance` | ✅ (3 variants) |
| Angle | `GCS::addConstraintL2LAngle` | ✅ |
| Radius | `GCS::addConstraintCircleRadius/ArcRadius` | ✅ (2 variants) |
| Tangent | `GCS::addConstraintTangent` | ✅ (8 combinations) |
| Equal | `GCS::addConstraintEqualLength/EqualRadius` | ✅ (5 variants) |
| Fixed | `GCS::addConstraintCoordinateX/Y` | ✅ |
| Midpoint | `GCS::addConstraintPointOnLine+PointOnPerpBisector` | ✅ |

**Not Implemented (v1 Scope):**

| Constraint | PlaneGCS Mapping | Lines Est. | Status |
|------------|------------------|------------|--------|
| **Concentric** | `addConstraintP2PCoincident` on center points | ~50 | ❌ NOT IMPLEMENTED |
| **Diameter** | `addConstraintCircleDiameter` or radius × 2 | ~50 | ❌ NOT IMPLEMENTED |

**Deferred to v2:**
- OnCurve, HorizontalDistance, VerticalDistance, Symmetric

### ✅ COMPLETED - Phase 3: Loop Detection Algorithms

| Component | File | Status |
|-----------|------|--------|
| Loop Detector | `src/core/loop/LoopDetector.h/cpp` | ✅ Complete (1895 lines) |
| Adjacency Graph | `src/core/loop/AdjacencyGraph.h/cpp` | ✅ Complete (98 lines) |
| Face Builder | `src/core/loop/FaceBuilder.h/cpp` | ✅ Complete (719 lines) |
| DFS Cycle Detection | `LoopDetector::findCycles()` | ✅ Complete |
| Area Calculation | `computeSignedArea()` (Shoelace) | ✅ Complete |
| Point-in-Polygon | `isPointInPolygon()` (Ray casting) | ✅ Complete |
| Face Hierarchy | `buildFaceHierarchy()` | ✅ Complete |
| Wire Building | `buildWire()` | ✅ Complete |
| Loop Validation | `validateLoop()` | ✅ Complete |
| OCCT Face Generation | `FaceBuilder::buildFace()` | ✅ Complete |

### ✅ COMPLETED - Phase 4: Rendering System

| Component | File | Lines | Status |
|-----------|------|-------|--------|
| **SketchRenderer Implementation** | `src/core/sketch/SketchRenderer.cpp` | **1897** | ✅ Complete |
| SketchRenderer Header | `src/core/sketch/SketchRenderer.h` | 628 | ✅ Complete |
| Inline GLSL Shaders | Embedded in SketchRenderer.cpp | ~120 | ✅ Complete |
| VBO Batching System | `buildVBOs()`, `render()` methods | — | ✅ Complete |
| Adaptive Arc Tessellation | 8-256 segments based on radius | — | ✅ Complete |
| Selection State Colors | Blue/Green/Orange feedback | — | ✅ Complete |
| Preview Rendering | Line/Circle/Rectangle preview | — | ✅ Complete |
| Hit Testing | `pickEntity()` basic implementation | — | ✅ Complete |
| Region Rendering | Loop-based region fill | — | ✅ Complete |

**Key Implementation Details:**
- **GLSL Shaders**: Inline vertex & fragment shaders (OpenGL 4.1 Core / macOS Metal)
- **Geometry Batching**: Separate VBOs for lines, points, construction geometry
- **State-based Coloring**: Hover, selected, construction modes
- **Constraint Icons**: Positioned via `getIconPosition()` (texture rendering pending)

### ✅ PARTIAL - Phase 5: Sketch Tools

| Component | File | Lines | Status |
|-----------|------|-------|--------|
| Tool Base & Manager | `SketchTool.h`, `SketchToolManager.h/cpp` | 367 | ✅ Complete |
| Line Tool | `tools/LineTool.h/cpp` | 167 | ✅ Complete (polyline mode) |
| Circle Tool | `tools/CircleTool.h/cpp` | 157 | ✅ Complete (center-radius) |
| Rectangle Tool | `tools/RectangleTool.h/cpp` | 205 | ✅ Complete (auto-constrained) |
| **Arc Tool** | `tools/ArcTool.h/cpp` | ~250 | ❌ **NOT IMPLEMENTED** |
| **Ellipse Tool** | `tools/EllipseTool.h/cpp` | ~180 | ❌ **NOT IMPLEMENTED** |
| **Trim Tool** | `tools/TrimTool.h/cpp` | ~150 | ❌ **NOT IMPLEMENTED** |
| **Mirror Tool** | `tools/MirrorTool.h/cpp` | ~150 | ❌ **NOT IMPLEMENTED** |

#### ArcTool Specification
- **Primary mode:** 3-Point Arc (start → point-on-arc → end)
- **State machine:** `WaitingForStart → WaitingForMiddle → WaitingForEnd → Complete`
- **Auto-tangent:** When starting from line endpoint in tangent direction
- **Preview:** Live arc preview during drag
- **Snap:** Integrates with SnapManager for precision placement

#### TrimTool Specification
- **Behavior:** Click segment to delete (removes portion between intersections)
- **Intersection detection:** Uses LoopDetector adjacency graph
- **Multiple trim:** Can click multiple segments in sequence

#### MirrorTool Specification
- **Behavior:** Select entities → select mirror line → creates mirrored copies
- **Constraints:** Auto-applies symmetric constraint (linked geometry)
- **Multi-select:** Supports mirroring multiple entities at once

### ⚠️ PARTIAL - Phase 6: Snap & Auto-Constrain

| Component | File | Lines | Status |
|-----------|------|-------|--------|
| **SnapManager** | `src/core/sketch/SnapManager.h/cpp` | ~400 | ✅ **IMPLEMENTED** |
| **AutoConstrainer** | `src/core/sketch/AutoConstrainer.h/cpp` | ~350 | ✅ **IMPLEMENTED** |
| **Tool Integration** | `tools/*` + `SketchToolManager` | — | ⚠️ **PARTIAL** (Line/Circle) |
| **Ghost Icons** | (in SketchRenderer) | ~100 | ❌ **NOT IMPLEMENTED** |

#### SnapManager Architecture
```cpp
class SnapManager {
public:
    SnapResult findBestSnap(const Vec2d& cursor, const Sketch& sketch,
                            const std::unordered_set<EntityID>& excludeEntities = {}) const;
    void setSnapRadius(double radiusMM);  // Default 2.0mm
    void setSnapEnabled(SnapType type, bool enabled);
    void setGridSnapEnabled(bool enabled);
    void setGridSize(double gridSize);

private:
    std::vector<SnapResult> findAllSnaps(...) const;
    SnapResult findVertexSnaps(...) const;
    SnapResult findMidpointSnaps(...) const;
    SnapResult findCenterSnaps(...) const;
    SnapResult findIntersectionSnaps(...) const;
    SnapResult findOnCurveSnaps(...) const;
    SnapResult findGridSnaps(...) const;
};
```

#### AutoConstrainer Architecture
```cpp
class AutoConstrainer {
public:
    struct InferredConstraint {
        ConstraintType type;
        EntityID entity1;
        std::optional<EntityID> entity2;
        double confidence;  // 0.0-1.0 for UI preview intensity
    };

    std::vector<InferredConstraint> inferConstraints(
        const Vec2d& newPoint, const Sketch& sketch, EntityID activeEntity) const;

    void setEnabled(bool enabled);  // Master toggle
    void setTypeEnabled(ConstraintType type, bool enabled);  // Per-type

private:
    bool inferHorizontal(const Vec2d& p1, const Vec2d& p2, double tolerance) const;
    bool inferVertical(const Vec2d& p1, const Vec2d& p2, double tolerance) const;
    bool inferCoincident(const Vec2d& p, const Sketch& sketch) const;
    bool inferTangent(EntityID arc, EntityID line, const Sketch& sketch) const;
    bool inferPerpendicular(EntityID line1, EntityID line2, const Sketch& sketch) const;
};
```

#### Tool Integration Pattern
```cpp
void LineTool::onMouseMove(const Vec2d& rawPos) {
    // 1. Find best snap
    SnapResult snap = m_snapManager->findBestSnap(rawPos, *m_sketch, m_activeEntity);
    Vec2d pos = snap.snapped ? snap.position : rawPos;

    // 2. Infer constraints
    auto inferred = m_autoConstrainer->inferConstraints(pos, *m_sketch, m_activeEntity);

    // 3. Update preview
    m_endPoint = pos;
    m_inferredConstraints = inferred;

    // 4. Show visual feedback
    m_renderer->showSnapIndicator(snap.position, snap.type);
    m_renderer->setGhostConstraints(inferred);
}

void LineTool::onMouseRelease(const Vec2d& pos) {
    // Apply inferred constraints (confidence > 0.5)
    for (const auto& ic : m_inferredConstraints) {
        if (ic.confidence > 0.5) {
            m_sketch->addConstraint(createConstraint(ic));
        }
    }
}
```

### ❌ NOT STARTED - Phase 7: UI Integration

| Component | File | Lines | Status |
|-----------|------|-------|--------|
| **Sketch Mode Panel** | `src/ui/sketch/SketchModePanel.h/cpp` | ~300 | ❌ **NOT IMPLEMENTED** |
| **Dimension Editor** | `src/ui/sketch/DimensionEditor.h/cpp` | ~150 | ❌ **NOT IMPLEMENTED** |
| **DOF Indicator** | `src/ui/sketch/DOFIndicator.h/cpp` | ~100 | ❌ **NOT IMPLEMENTED** |
| **pickConstraint()** | (in SketchRenderer) | ~50 | ❌ **NOT IMPLEMENTED** |
| **Constraint Icon Rendering** | (in SketchRenderer) | ~150 | ❌ **NOT IMPLEMENTED** |

#### DimensionEditor Widget
- **Activation:** Double-click on segment shows inline editor
- **Display:** On geometry (Shapr3D style)
- **Expression:** Basic math (+,-,*,/) in v1
- **Submit:** Enter confirms → sketch re-solves → rerender

#### SketchModePanel Layout
```
┌─────────────────────────┐
│ CONSTRAINTS             │
│ ─────────────────────── │
│ ⊣ Horizontal      [H]   │
│ ⊤ Vertical        [V]   │
│ ∥ Parallel        [P]   │
│ ⊥ Perpendicular   [N]   │
│ ○ Tangent         [T]   │
│ ● Coincident      [C]   │
│ = Equal           [E]   │
│ ⟂ Midpoint        [M]   │
│ ─────────────────────── │
│ 📏 Distance       [D]   │
│ 📐 Angle          [A]   │
│ ⊕ Radius          [R]   │
│ ─────────────────────── │
│ 🔒 Lock/Fix       [F]   │
└─────────────────────────┘
```

---

## Next Implementation Priorities

### Immediate (Phase 6 - Snap & Auto-Constrain)
```cpp
// Find all snap candidates within radius
std::vector<SnapResult> findAllSnaps(const Vec2d& cursor, const Sketch& sketch) {
    std::vector<SnapResult> results;

    // Check points (highest priority)
    for (auto* pt : sketch.getPoints()) {
        double dist = distance(cursor, pt->pos());
        if (dist < snapRadius_)
            results.push_back({SnapType::Vertex, pt->pos(), pt->id(), dist});
    }

    // Check endpoints, midpoints, centers
    // Check intersections
    // Check grid

    // Sort by priority then distance
    std::sort(results.begin(), results.end());
    return results;
}
```

#### Auto-Constraint Detection
```cpp
// Per SPECIFICATION.md §5.14
struct AutoConstraint {
    ConstraintType type;
    EntityID entity1;
    EntityID entity2;
    double confidence;  // For UI preview
};

std::vector<AutoConstraint> detectAutoConstraints(
    const Vec2d& newPoint,
    const Sketch& sketch,
    EntityID drawingEntity
) {
    // Detect potential coincident
    // Detect horizontal/vertical alignments
    // Detect tangent/perpendicular to nearby
}
```

Files Implemented:
- `src/core/sketch/SnapManager.h/cpp`
- `src/core/sketch/AutoConstrainer.h/cpp`

---

### Phase 7: UI Integration

**Priority: Medium**
**Dependencies: Phases 5, 6**

#### Sketch Mode Panel
```
UI Elements:
- Tool buttons (Line, Arc, Circle, Rectangle)
- Constraint buttons (when entities selected)
- DOF indicator with color coding
- Constraint list with edit/delete
- Expression editor for dimensions
```

#### Dimension Editor
```cpp
// Click-to-edit dimensional constraints
// Per SPECIFICATION.md §5.15
class DimensionEditor : public QLineEdit {
    // Popup at constraint position
    // Parse expression: "10", "10+5", "width/2"
    // Support basic math: +, -, *, /, ()
};
```

#### Constraint Conflict Dialog
```
When over-constrained:
- Show conflicting constraints list
- Suggest which to remove
- "Remove" button per constraint
- "Remove All Conflicts" button
```

---

## Algorithm Implementation Notes

### Critical Algorithms - Implementation Status

1. **PlaneGCS Direct Parameter Binding** ✅ DONE
   - Uses pointers directly to sketch coordinates
   - Backup/restore mechanism implemented
   - Thread safety via atomic flags

2. **Graph-Based Loop Detection** ✅ DONE
   - Planarization with intersection detection
   - Half-edge face extraction
   - Face hierarchy (outer/inner loops)

3. **Rubber-Band Dragging with Spring Resistance** ✅ DONE
   - Per §5.13: Progressive resistance as constraints fight
   - Implementation in `solveWithDrag()` (ConstraintSolver.cpp:360)

4. **Redundancy Analysis** ✅ DONE
   - PlaneGCS `getRedundant()` integrated
   - Conflict identification working

5. **OCCT Face Generation** ✅ DONE
   - FaceBuilder converts loops to TopoDS_Face
   - Wire orientation (CCW outer, CW holes)
   - Edge creation for lines, arcs, circles

### Performance Targets (from SPECIFICATION.md)

| Metric | Target | Current |
|--------|--------|---------|
| Solve time (≤100 entities) | <33ms (30 FPS) | ✅ Achievable |
| Background threshold | >100 entities | Implemented |
| Arc tessellation | 8-256 segments | ✅ Implemented |
| Snap radius | 2mm | Not implemented |
| Solver tolerance | 1e-4mm | ✅ Configured |

---

## Testing Strategy

### Unit Tests (Existing Prototypes)
```
tests/prototypes/
├── proto_sketch_geometry.cpp    # Entity creation tests
├── proto_sketch_constraints.cpp # Constraint validation
├── proto_sketch_solver.cpp      # Solver integration
└── proto_planegcs_integration.cpp # Direct PlaneGCS test
```

### Integration Tests (Needed)
Cross-phase contracts (Phase 2 → Phase 3):

- Contract: Solver output provides solved 2D geometry that LoopDetector consumes.
- Input: closed rectangle (4 lines, 4 points) → Output: 1 outer loop.
- Input: rectangle with inner circle → Output: 1 outer + 1 inner loop (hole).
- Input: open polyline → Output: 0 loops.
- Input: arc + line chain forming closed profile → Output: 1 loop with mixed edges.

Planned tests:
- `tests/integration/sketch_solver_loop.cpp`
- `tests/integration/sketch_renderer_contract.cpp`

### Performance Tests
- `tests/bench/bench_sketch_solver.cpp`:
  - 100 entities: solve < 33ms
  - 500 entities: solve < 200ms

---

## File Structure Summary

```
src/core/
├── sketch/
│   ├── SketchTypes.h           [✅ COMPLETE]
│   ├── SketchEntity.h/cpp      [✅ COMPLETE]
│   ├── SketchPoint.h/cpp       [✅ COMPLETE] (277 lines)
│   ├── SketchLine.h/cpp        [✅ COMPLETE] (350 lines)
│   ├── SketchArc.h/cpp         [✅ COMPLETE] (477 lines)
│   ├── SketchCircle.h/cpp      [✅ COMPLETE] (282 lines)
│   ├── SketchEllipse.h/cpp     [❌ NOT IMPLEMENTED] (~200 lines)
│   ├── SketchConstraint.h/cpp  [✅ COMPLETE]
│   ├── Sketch.h/cpp            [✅ COMPLETE] (1370 lines)
│   ├── SketchRenderer.h        [✅ COMPLETE] (628 lines)
│   ├── SketchRenderer.cpp      [✅ COMPLETE] (1897 lines)
│   ├── SketchTool.h            [✅ COMPLETE]
│   ├── SnapManager.h/cpp       [❌ NOT IMPLEMENTED] (~400 lines)
│   ├── AutoConstrainer.h/cpp   [❌ NOT IMPLEMENTED] (~350 lines)
│   ├── tools/
│   │   ├── SketchToolManager.h/cpp [✅ COMPLETE] (263 lines)
│   │   ├── LineTool.h/cpp      [✅ COMPLETE] (167 lines)
│   │   ├── RectangleTool.h/cpp [✅ COMPLETE] (205 lines)
│   │   ├── CircleTool.h/cpp    [✅ COMPLETE] (157 lines)
│   │   ├── ArcTool.h/cpp       [❌ NOT IMPLEMENTED] (~250 lines)
│   │   ├── EllipseTool.h/cpp   [❌ NOT IMPLEMENTED] (~180 lines)
│   │   ├── TrimTool.h/cpp      [❌ NOT IMPLEMENTED] (~150 lines)
│   │   └── MirrorTool.h/cpp    [❌ NOT IMPLEMENTED] (~150 lines)
│   ├── constraints/
│   │   └── Constraints.h/cpp   [✅ COMPLETE] (1485 lines)
│   │       + ConcentricConstraint  [❌ NOT IMPLEMENTED] (~50 lines)
│   │       + DiameterConstraint    [❌ NOT IMPLEMENTED] (~50 lines)
│   └── solver/
│       ├── ConstraintSolver.h  [✅ COMPLETE] (436 lines)
│       ├── ConstraintSolver.cpp[✅ COMPLETE] (1014 lines)
│       ├── SolverAdapter.h     [✅ COMPLETE]
│       └── SolverAdapter.cpp   [✅ COMPLETE] (85 lines)
├── loop/
│   ├── LoopDetector.h          [✅ COMPLETE] (389 lines)
│   ├── LoopDetector.cpp        [✅ COMPLETE] (1506 lines)
│   ├── AdjacencyGraph.h/cpp    [✅ COMPLETE] (98 lines)
│   └── FaceBuilder.h/cpp       [✅ COMPLETE] (719 lines)
└── CMakeLists.txt              [✅ COMPLETE]

src/ui/sketch/                  [❌ NEW DIRECTORY]
├── SketchModePanel.h/cpp       [❌ NOT IMPLEMENTED] (~300 lines)
├── DimensionEditor.h/cpp       [❌ NOT IMPLEMENTED] (~150 lines)
└── DOFIndicator.h/cpp          [❌ NOT IMPLEMENTED] (~100 lines)

third_party/
└── planegcs/                   [✅ COMPLETE]
```

---

## Implementation Order (Prioritized)

### Priority 1: Core Precision (Must Have)
| # | Component | Lines | Rationale |
|---|-----------|-------|-----------|
| 1 | **SnapManager** | ~400 | Foundation for all precision drawing |
| 2 | **AutoConstrainer** | ~350 | Core Shapr3D UX differentiator |
| 3 | **Ghost constraint icons** | ~100 | Visual feedback for inference |

### Priority 2: Tool Completion (Must Have)
| # | Component | Lines | Rationale |
|---|-----------|-------|-----------|
| 4 | **ArcTool** | ~250 | Required for most real CAD work |
| 5 | **TrimTool** | ~150 | Essential for sketch cleanup |
| 6 | **MirrorTool** | ~150 | With symmetric constraint link |

### Priority 3: Entity Completion (Should Have)
| # | Component | Lines | Rationale |
|---|-----------|-------|-----------|
| 7 | **SketchEllipse** | ~200 | Entity class |
| 8 | **EllipseTool** | ~180 | Drawing tool |
| 9 | **ConcentricConstraint** | ~50 | P2PCoincident on centers |
| 10 | **DiameterConstraint** | ~50 | Radius × 2 |

### Priority 4: Dimension Editing (Should Have)
| # | Component | Lines | Rationale |
|---|-----------|-------|-----------|
| 11 | **pickConstraint()** | ~50 | Enable clicking constraints |
| 12 | **DimensionEditor** | ~150 | Double-click to edit |

### Priority 5: UI Polish (Nice to Have)
| # | Component | Lines | Rationale |
|---|-----------|-------|-----------|
| 13 | **SketchModePanel** | ~300 | Floating constraint panel |
| 14 | **DOFIndicator** | ~100 | Status feedback widget |
| 15 | **Constraint icon rendering** | ~150 | Texture atlas billboards |

### Total Estimated Effort

| Phase | Lines | Files |
|-------|-------|-------|
| Priority 1 | ~850 | 3 |
| Priority 2 | ~550 | 3 |
| Priority 3 | ~480 | 4 |
| Priority 4 | ~200 | 2 |
| Priority 5 | ~550 | 3 |
| **TOTAL** | **~2,630** | **15** |

---

## Resolved Design Questions

All major UX questions have been resolved. See **Detailed UX Specifications** section at the top of this document.

### Summary of Key Decisions
| Category | Decision |
|----------|----------|
| Snap radius | 2mm in sketch coords (constant regardless of zoom) |
| Auto-constrain default | ON (Shapr3D style) |
| Ghost icon opacity | 50% |
| Constraint panel | Floating right side, auto-show in sketch mode |
| Arc tool mode | 3-Point primary |
| Undo granularity | Single Cmd+Z undoes constraint only |
| Snap visual | Cursor changes to snap icon |
| Region selection | Click to select (hover previews) |
| Conflict handling | Block + show dialog |
| Dimension display | On geometry (Shapr3D style) |
| Tangent inference | Auto when drawing from arc endpoint |
| Polyline mode | Auto-chain ON |
| Construction color | Light blue dashed |
| Under-defined extrude | Allow (direct modeling) |
| Perpendicular inference | Auto at ~90±5° |
| Rectangle auto-constraints | Perpendicular + equal opposite sides |
| Shortcuts during drawing | Immediate apply |
| Min line length | 0.01mm |
| Trim tool | Click segment to delete |
| Mirror tool | Creates geometry + symmetric constraint |
| Delete cascade | Removes connected geometry |
| Lock visual | Small padlock icon |
| Empty sketch | Don't create until geometry exists |
| Double-click segment | Shows dimension input |

### Outstanding Technical Questions
1. **Arc tessellation during loop detection** — 8+ segments per π radians sufficient?
2. **Performance threshold for background solve** — Keep 100 entities or adjust?

---

*Document Version: 5.0*
*Last Updated: 2026-01-04*
*Status: Detailed UX Specifications Added*
