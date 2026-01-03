# OneCAD Detailed Implementation Roadmap

This document outlines the phased implementation strategy for OneCAD, ensuring a robust foundation before adding complexity.

**Last Updated:** 2026-01-03

---

## Current Status Summary

| Phase | Status | Progress |
|-------|--------|----------|
| Phase 1.1 Project & Rendering | In Progress | ~70% |
| Phase 1.2 OCCT Kernel | Partial | ~30% |
| Phase 1.3 Topological Naming | Partial | ~40% |
| Phase 1.4 Command & Document | Not Started | ~10% |
| Phase 2 Sketching Engine | **Complete** | ~95% |
| Phase 3 Solid Modeling | Not Started | 0% |
| Phase 4 Advanced Modeling | Not Started | 0% |
| Phase 5 Optimization & Release | Not Started | 0% |

**Key Achievements:**
- PlaneGCS constraint solver fully integrated and operational
- Loop detection (DFS cycle finding) fully implemented
- All core constraint types working (10/19 types)

---

## Phase 1: Core Foundation & Topological Infrastructure
**Focus:** establishing the critical architectural bedrock—specifically the geometry kernel integration and the topological naming system to prevent future refactoring.

### 1.1 Project & Rendering Setup
- [x] **Build System**: CMake with Qt6, OCCT, Eigen3, PlaneGCS.
- [x] **App Shell**: Main window layout (MainWindow, Viewport, Navigator, Inspector).
- [x] **OpenGL Viewport**: OpenGLWidgets-based rendering pipeline.
- [ ] **Qt RHI Viewport**: Metal-based rendering pipeline (deferred to optimization phase).
- [~] **Camera Controls**: Camera3D with Orbit, Pan, Zoom, standard views. *Missing: inertia physics, sticky pivot.*
- [~] **Grid System**: Grid3D exists (fixed 10mm). *Missing: adaptive spacing per spec.*

### 1.2 OCCT Kernel Integration
- [~] **Shape Wrappers**: Basic ElementMap structure exists. *Missing: full `onecad::kernel::Shape` decoupled wrapper.*
- [ ] **Geometry Factory**: Utilities for creating primitives (Box, Cylinder, Plane).
- [ ] **BREP Utilities**: Explorer for traversing Faces, Edges, Vertices.

### 1.3 Topological Naming System (ElementMap)
*Critical Path Item: Must be rigorous.*
- [~] **ElementMap Architecture**:
    - Basic structure exists (`src/kernel/elementmap/ElementMap.h`).
    - *Missing: full persistent UUID system, history tracking.*
- [ ] **Evolution Tracking**:
    - Integration with OCCT history (`BRepTools_History` / `BRepAlgoAPI`).
- [ ] **Serialization**: Logic to save/load UUID mappings.
- [ ] **Rigorous Testing Suite**:
    - Automated tests for stable IDs across boolean ops.
    - Regression tests for parametric updates.

### 1.4 Command & Document Infrastructure
- [~] **Document Model**: Basic entity registry exists in Sketch class. *Missing: full document ownership hierarchy.*
- [ ] **Selection Manager**: Ray-casting picking, highlighting logic.
- [ ] **Command Processor**: Transaction-based Undo/Redo stack.

---

## Phase 2: Sketching Engine
**Focus:** Creating a robust 2D constraint-based sketcher.
**Status:** **COMPLETE** — PlaneGCS + Loop Detection fully working.

### 2.1 Sketch Infrastructure ✅
- [x] **Sketch Entity**: Complete data model (894 lines in `Sketch.cpp`).
- [x] **Workplane System**: Coordinate transforms (3D World <-> 2D Local) via `SketchPlane`.
- [ ] **Snap System**: Object snapping (Endpoint, Midpoint, Center). *Deferred to Phase 5.*

### 2.2 Sketch Tools (Partial)
- [x] **Primitives**: Line, Arc, Circle implemented. *Missing: Rectangle tool, Ellipse.*
- [x] **Construction Geometry**: Toggling construction/normal mode via `isConstruction` flag.
- [ ] **Visual Feedback**: Real-time preview while drawing (SketchRenderer interface only).

### 2.3 Constraint Solver (PlaneGCS) ✅
**STATUS: COMPLETE**
- [x] **Solver Integration**: PlaneGCS fully linked and operational (`third_party/planegcs/`).
- [x] **Constraint Types**: 10 major types implemented:
    - Coincident, Horizontal, Vertical
    - Parallel, Perpendicular, Tangent, Equal
    - Distance, Angle, Radius
- [x] **ConstraintSolver.cpp**: Full 981-line implementation with:
    - Direct parameter binding to sketch entities
    - DogLeg → Levenberg-Marquardt fallback
    - Backup/restore for failed solves
    - Conflict and redundancy detection
- [x] **SolverAdapter**: Sketch → PlaneGCS translation complete.
- [~] **Interactive Solver**: Basic solving works. *Missing: 30 FPS throttling during drag.*
- [ ] **DOF Visualization**: DOF calculation works. *Missing: Color-coding UI (Green/Blue/Orange).*

### 2.4 Loop Detection & Face Creation ✅
**STATUS: COMPLETE** (969 lines in LoopDetector.cpp)
- [x] **LoopDetector.h/cpp**: Full implementation with Wire, Loop, Face structures.
- [x] **Graph Analysis**: `buildGraph()` creates adjacency graph from sketch.
- [x] **Cycle Detection**: `findCycles()` uses DFS with backtracking (lines 560-628).
- [x] **Area Calculation**: Shoelace formula in `computeSignedArea()`.
- [x] **Face Hierarchy**: `buildFaceHierarchy()` with hole detection.
- [x] **Point-in-Polygon**: Ray casting in `isPointInPolygon()`.
- [ ] **Auto-Highlighting**: Visual cues for closed regions. *Needs SketchRenderer.*
- [ ] **MakeFace Command**: Converting Face → `TopoDS_Face`. *Needs OCCT wrapper.*

---

## Phase 3: Solid Modeling Operations
**Focus:** Enabling 3D geometry creation and manipulation.
**Status:** Not Started

### 3.1 Feature Management
- [ ] **Feature History Tree**: Parametric history management.
- [ ] **Dependency Graph**: Tracking relationships between sketches and features.
- [ ] **Regeneration Engine**: Replaying history on modification.

### 3.2 Core Operations
- [ ] **Extrude**:
    - Vector-based extrusion.
    - Draft angle support.
    - Boolean options (New/Add/Cut/Intersect).
- [ ] **Revolve**: Axis selection, angle parameters.
- [ ] **Boolean Operations**: Union, Subtract, Intersect (Multi-body).

### 3.3 Direct Modeling (Hybrid)
- [ ] **Push/Pull**:
    - Face detection under cursor.
    - Context-aware extrude/offset.
- [ ] **Direct Face Manipulation**: Moving faces with valid topology.

---

## Phase 4: Advanced Modeling & Refinement
**Focus:** Adding depth to modeling capabilities and UI polish.
**Status:** Not Started

### 4.1 Modification Tools
- [ ] **Fillet/Chamfer**: Variable radius, edge chain selection.
- [ ] **Shell**: Hollow solids with thickness.
- [ ] **Patterns**: Linear and Circular duplication features.

### 4.2 UI/UX Polish
- [ ] **Contextual Toolbar**: Floating tool palette near selection.
- [ ] **Property Inspector**: Fully functional attribute editor.
- [ ] **Visual Styles**: Wireframe, Shaded, Shaded with Edges.
- [ ] **Dark/Light Themes**: Full UI consistency.

### 4.3 I/O & Persistence
- [ ] **Native File Format**: Full serialization of Document, ElementMap, and History.
- [ ] **STEP Support**: Import/Export with interoperability.

---

## Phase 5: Optimization & Release
**Focus:** Performance tuning and stability.
**Status:** Not Started

### 5.1 Performance
- [ ] **Tessellation Control**: Adjustable LOD (Coarse/Fine).
- [ ] **Background Processing**: Threading complex boolean ops.
- [ ] **Rendering Optimization**: Instance rendering for patterns.

### 5.2 Stability & Release
- [ ] **Crash Recovery**: Autosave and backup systems.
- [ ] **Memory Management**: Cleanup of large OCCT structures.
- [ ] **Packaging**: macOS Bundle creation and signing.

---

## Prototype Tests

| Test | Status | Description |
|------|--------|-------------|
| `proto_tnaming.cpp` | Exists | Basic topological naming |
| `proto_elementmap_rigorous.cpp` | Exists | ElementMap validation |
| `proto_custom_map.cpp` | Exists | Custom mapping tests |
| `proto_sketch_constraints.cpp` | Exists | Constraint system tests |
| `proto_sketch_geometry.cpp` | Exists | Geometry entity tests |
| `proto_sketch_solver.cpp` | Exists | Solver integration |
| `proto_planegcs_integration.cpp` | Exists | PlaneGCS direct test |
| `proto_loop_detector.cpp` | Exists | Loop detection tests |

---

## Legend

- [x] Complete
- [~] Partial / In Progress
- [ ] Not Started

---

## Priority Recommendations

### Immediate Next Steps (High Priority)
1. **Implement SketchRenderer** — Visual feedback for sketch editing (blocking for UI).
2. **Create MakeFace wrapper** — Bridge LoopDetector→OCCT (blocking for Phase 3).
3. **Add Rectangle/Ellipse tools** — Complete primitive set.

### Short-term (Medium Priority)
4. Finish Camera inertia physics and sticky pivot.
5. Implement Selection Manager with ray-casting.
6. Add 30 FPS throttling to interactive solver.
7. Implement SnapManager for object snapping.

### Blocking Items for Phase 3
- MakeFace wrapper needed to convert LoopDetector Face → `TopoDS_Face`.
- SketchRenderer needed before any interactive sketch editing.

---

*Document Version: 3.0*
*Last Updated: 2026-01-03*
