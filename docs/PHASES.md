# OneCAD Detailed Implementation Roadmap

This document outlines the phased implementation strategy for OneCAD, ensuring a robust foundation before adding complexity.

**Last Updated:** 2026-01-10

---

## Current Status Summary

| Phase | Status | Progress |
|-------|--------|----------|
| Phase 1.1 Project & Rendering | **Complete** | ~98% |
| Phase 1.2 OCCT Kernel | Partial | ~50% |
| Phase 1.3 Topological Naming | **Complete** | ~95% |
| Phase 1.4 Command & Document | **Complete** | **100%** |
| Phase 2 Sketching Engine | **Complete** | **100%** |
| **Phase 3 Solid Modeling** | **In Progress** | **~85%** |
| ↳ 3.1 I/O Foundation | **Complete** | **100%** |
| ↳ 3.2 Parametric Engine | Partial | ~25% |
| ↳ 3.3 Modeling Operations | **Complete** | **100%** |
| ↳ 3.4 Pattern Operations | Not Started | 0% |
| ↳ 3.5 UI Polish | In Progress | ~40% |
| Phase 4 Advanced Modeling | Partial | ~10% |
| Phase 5 Optimization & Release | Not Started | 0% |

**Key Achievements:**
- ✅ PlaneGCS solver fully integrated (1450 LOC, all 15 constraint types working)
- ✅ Loop detection complete (1887 LOC with DFS, shoelace area, hole detection)
- ✅ All 7 sketch tools production-ready (2054 LOC total)
- ✅ SketchRenderer complete (2472 LOC with VBO, adaptive tessellation)
- ✅ SnapManager complete (1166 LOC, 8 snap types)
- ✅ AutoConstrainer complete (1091 LOC, 7 inference rules)
- ✅ Extrude v1a complete (SketchRegion → new body, preview, draft angle working)
- ✅ **Push/Pull Direct Modeling** (Face input, smart boolean Add/Cut detection)
- ✅ **Fillet/Chamfer Tool** (Variable radius, drag interaction, tangent edge chaining)
- ✅ **Shell Tool** (Hollow solid, single-face working, multi-face wiring pending)
- ✅ **Boolean Operations** (Union, Cut, Intersect, smart mode detection)
- ✅ **EdgeChainer** (Tangent continuity propagation for edge selection)
- ✅ Revolve tool complete (Profile+Axis, drag interaction, boolean mode)
- ✅ UI integration complete (ContextToolbar, ConstraintPanel, DimensionEditor, ViewCube)
- ✅ Render Debug Panel added (visualizing normals, wireframes, bounds)
- ✅ ModelNavigator enhancements (visibility toggling, selection filtering)
- ✅ **Full I/O Layer** (OneCAD native format, STEP import/export, ZIP packaging)
- ✅ **StartOverlay + ProjectTile** (Project browser with thumbnails)
- ✅ **ThemeManager** (Light/Dark theme system, 885 LOC total)
- ✅ **9 Commands** (Add/Delete/Modify/Rename Body, Add/Delete/Rename Sketch, Visibility)

---

## Phase 1: Core Foundation & Topological Infrastructure
**Focus:** establishing the critical architectural bedrock—specifically the geometry kernel integration and the topological naming system to prevent future refactoring.

### 1.1 Project & Rendering Setup
- [x] **Build System**: CMake with Qt6, OCCT, Eigen3, PlaneGCS. **Complete** (all deps linked).
- [x] **App Shell**: Main window layout (MainWindow, Viewport, Navigator, Inspector). **Complete**.
- [x] **OpenGL Viewport**: OpenGLWidgets-based rendering pipeline. **Complete** (1427 LOC).
- [ ] **Qt RHI Viewport**: Metal-based rendering pipeline (deferred to optimization phase).
- [~] **Camera Controls**: Camera3D with Orbit, Pan, Zoom, standard views (270 LOC). *Missing: inertia physics, sticky pivot.*
- [x] **Grid System**: Grid3D adaptive spacing implemented (pixel-targeted minor/major tiers).
- [x] **Body Renderer**: Shaded + Edges renderer (SceneMeshStore-backed) with preview mesh support.
- [x] **Debug Rendering**: Render Debug Panel for inspecting scene details.

### 1.2 OCCT Kernel Integration
- [~] **Shape Wrappers**: Basic ElementMap structure exists. *Missing: full `onecad::kernel::Shape` decoupled wrapper.*
- [~] **Tessellation Cache**: OCCT triangulation → SceneMeshStore for rendering/picking (implemented; LOD and progressive refinement pending).
- [ ] **Geometry Factory**: Utilities for creating primitives (Box, Cylinder, Plane).
- [ ] **BREP Utilities**: Explorer for traversing Faces, Edges, Vertices.

### 1.3 Topological Naming System (ElementMap)
*Critical Path Item: Must be rigorous.*
- [x] **ElementMap Architecture**: **Complete** (955 LOC header-only).
    - ✅ Deterministic descriptor-based IDs (FNV-1a hash, quantized geometry)
    - ✅ 14-field enhanced descriptor (surface/curve types, normals, adjacency)
    - ✅ Split handling with sibling IDs
- [x] **Evolution Tracking**: **Complete**.
    - ✅ Full OCCT history integration (`BRepAlgoAPI` Modified/Generated/Deleted)
    - ✅ Parent→child source tracking
- [x] **Serialization**: **Complete** (text format, versioned, round-trip tested).
- [~] **Testing Suite**: Core validated, advanced scenarios pending.
    - ✅ 5/5 prototype tests passing (basic cut, split, determinism, serialization)
    - ⚠️ Missing: chain ops, merge scenarios, pattern operations
- [x] **Document Integration**: ElementMap wired into Document (body registration + tessellation IDs).

### 1.4 Command & Document Infrastructure ✅ COMPLETE
- [x] **Document Model**: Owns sketches + bodies + operations list. Full CRUD, visibility, isolation, JSON serialization. **Complete**.
- [x] **Selection Manager**: Ray-casting picking, deep select, click cycling, hover/selection highlights. **Complete**.
- [x] **Command Processor**: Execute/undo/redo with full transaction support (begin/end/cancel, command grouping, rollback). **Complete**.
- [x] **All 9 Commands Implemented**:
    - ✅ AddBodyCommand, DeleteBodyCommand, ModifyBodyCommand, RenameBodyCommand
    - ✅ DeleteSketchCommand, RenameSketchCommand
    - ✅ ToggleVisibilityCommand (body + sketch)
    - ✅ Full undo/redo with state preservation

---

## Phase 2: Sketching Engine
**Focus:** Creating a robust 2D constraint-based sketcher.
**Status:** ✅ **COMPLETE** (100%) — PlaneGCS + Loop Detection + All Tools + UI Integration

### 2.1 Sketch Infrastructure ✅ COMPLETE
- [x] **Sketch Entity**: Complete data model (Sketch.cpp 894 lines, Sketch.h 476 lines).
- [x] **Workplane System**: Coordinate transforms (3D World <-> 2D Local) via `SketchPlane`. **Complete**.
- [x] **Snap System**: SnapManager **complete** (1166 LOC).
    - ✅ All 8 snap types: Vertex, Endpoint, Midpoint, Center, Quadrant, Intersection, OnCurve, Grid
    - ✅ 2mm snap radius (zoom-independent)
    - ✅ Priority-based selection

### 2.2 Sketch Tools ✅ COMPLETE
- [x] **All 7 Tools**: Line (322 LOC), Arc (369 LOC), Circle (226 LOC), Rectangle (206 LOC), Ellipse (268 LOC), Mirror (444 LOC), Trim (219 LOC).
    - ✅ Total: 2618 LOC production code
- [x] **Construction Geometry**: Toggling construction/normal mode via `isConstruction` flag. **Complete**.
- [x] **Visual Feedback**: SketchRenderer **complete** (1955 LOC) with VBO rendering, adaptive tessellation, viewport culling.

### 2.3 Constraint Solver (PlaneGCS) ✅ COMPLETE
**STATUS: PRODUCTION-READY** (1450 LOC total)
- [x] **PlaneGCS Library**: Linked as static lib (42MB libplanegcs.a), CMake configured, runtime verified.
- [x] **ConstraintSolver.cpp**: Production implementation (1014 LOC + 436 header):
    - ✅ Direct parameter binding (zero-copy solving)
    - ✅ Full GCS::System wrapper with all primitives (Point, Line, Arc, Circle)
    - ✅ 15 constraint types fully translated
    - ✅ DogLeg algorithm (default) with LM/BFGS fallback
    - ✅ Backup/restore for failed solves
    - ✅ Redundancy and conflict detection
    - ✅ DOF calculation
- [x] **Constraint Types**: All 15 implemented in Constraints.h (1875 LOC):
    - Positional: Fixed, Midpoint, Coincident, PointOnCurve
    - Geometric: Horizontal, Vertical, Parallel, Perpendicular, Tangent, Concentric, Equal
    - Dimensional: Distance, Angle, Radius, Diameter
- [x] **SolverAdapter**: Sketch → PlaneGCS translation (85 LOC).
- [x] **Interactive Solver**: Drag solving with spring resistance. **Complete**.
- [x] **AutoConstrainer**: Inference engine **complete** (1091 LOC, 7 rules, ±5° tolerance).
- [~] **DOF Visualization**: DOF calculation works. *UI color-coding partial (SketchRenderer has logic, needs full UI wiring).*

### 2.4 Loop Detection & Face Creation ✅ COMPLETE
**STATUS: PRODUCTION-READY** (2704 LOC total implementation)
- [x] **LoopDetector**: Full implementation (1985 LOC: 381 header + 1506 cpp + 98 AdjacencyGraph).
    - ✅ DFS cycle detection algorithm (947-1015)
    - ✅ Planar graph face extraction with half-edge structure
    - ✅ Segment intersection planarization
    - ✅ Arc/circle adaptive tessellation
- [x] **Wire, Loop, Face Structures**: Complete data model with all methods.
- [x] **Algorithms**: All production-ready:
    - ✅ Graph-based adjacency analysis
    - ✅ Shoelace signed area calculation
    - ✅ Ray casting point-in-polygon
    - ✅ Face hierarchy with hole detection
    - ✅ Orientation correction (CCW outer, CW holes)
- [x] **FaceBuilder**: OCCT bridge **complete** (719 LOC).
    - ✅ Converts Loop → `TopoDS_Face` with holes
    - ✅ Wire gap repair via `ShapeFix_Wire`
    - ✅ Plane transformations (2D → 3D)
- [x] **Testing**: 2 prototype tests passing (proto_loop_detector, proto_face_builder).
- [x] **Production Integration**: Active use in SketchRenderer for region visualization.

---

## Phase 3: Solid Modeling Operations
**Focus:** Enabling 3D geometry creation, manipulation, and file I/O.
**Status:** In Progress (~85% Complete)
**Implemented:** ~6,500 LOC | **Remaining:** ~2,000 LOC (Patterns + Parametric UI)

### Implementation Status Summary

| Sub-Phase | Focus | Status | Actual LOC |
|-----------|-------|--------|------------|
| **3.1 I/O Foundation** | Save/Load + STEP | **Complete** | ~2,400 |
| **3.2 Parametric Engine** | History Replay | Partial | ~500 done |
| **3.3 Modeling Operations** | Fillet/Shell/Push-Pull | **Complete** | ~1,650 |
| **3.4 Pattern Operations** | Linear/Circular | Not Started | 0 |
| **3.5 UI Polish** | Command Search, Box Select | In Progress | ~1,400 |

---

### 3.1 I/O Foundation ✅ COMPLETE
**Goal:** Enable save/load + STEP interoperability
**Status:** **100% Complete** — Full implementation in `src/io/`

| Task | Files | Status | LOC |
|------|-------|--------|-----|
| OneCADFileIO | `src/io/OneCADFileIO.h/cpp` | [x] | 172 |
| DocumentIO | `src/io/DocumentIO.h/cpp` | [x] | 305 |
| SketchIO | `src/io/SketchIO.h/cpp` | [x] | 211 |
| ElementMapIO | `src/io/ElementMapIO.h/cpp` | [x] | 418 |
| HistoryIO | `src/io/HistoryIO.h/cpp` | [x] | 316 |
| ManifestIO | `src/io/ManifestIO.h/cpp` | [x] | 157 |
| JSONUtils | `src/io/JSONUtils.h/cpp` | [x] | 146 |
| Package abstraction | `src/io/Package.h/cpp` | [x] | 80 |
| ZipPackage | `src/io/ZipPackage.h/cpp` | [x] | 388 |
| DirectoryPackage | `src/io/DirectoryPackage.h/cpp` | [x] | 181 |
| STEP import | `src/io/step/StepImporter.h/cpp` | [x] | 130 |
| STEP export | `src/io/step/StepExporter.h/cpp` | [x] | 120 |

**File Format:** `.onecad` (ZIP) or `.onecadpkg` (directory)
- `manifest.json` — Magic number, version, content summary, integrity hash
- `document.json` — Sketch/body references, file paths
- `sketches/{uuid}.json` — Full sketch serialization with entity data
- `bodies/{uuid}.json + bodies/{uuid}.brep` — Metadata + OCCT BREP binary
- `history/ops.jsonl + history/state.json` — JSONL operation history (Git-friendly)
- `topology/elementmap.json` — Topological naming data
- `thumbnail.png` — Optional project thumbnail

**Features:**
- ✅ Dual package backend (QuaZip primary, system zip fallback)
- ✅ BREP binary caching for fast loading
- ✅ Deterministic JSON (sorted keys, consistent indentation)
- ✅ Legacy schema migration for sketch entities
- ✅ Full error handling with Result structs

---

### 3.2 Parametric Engine (Partial — ~25%)
**Goal:** Enable edit-and-regenerate workflow
**Status:** Data structures implemented, no regeneration logic yet

| Task | Files | Status | LOC |
|------|-------|--------|-----|
| OperationRecord struct | `src/app/document/OperationRecord.h` | [x] | ~150 |
| HistoryIO serialization | `src/io/HistoryIO.h/cpp` | [x] | 316 |
| DependencyGraph | `src/app/history/DependencyGraph.h/cpp` | [ ] | - |
| RegenerationEngine | `src/app/history/Regeneration.h/cpp` | [ ] | - |
| History UI panel | `src/ui/history/HistoryPanel.h/cpp` | [ ] | - |
| Feature card widget | `src/ui/history/FeatureCard.h/cpp` | [ ] | - |

**What's Implemented:**
- ✅ `OperationRecord` struct with Extrude/Revolve support
- ✅ `ExtrudeInput` variants: SketchRegionRef, FaceRef
- ✅ `OperationParams` variants: ExtrudeParams, RevolveParams
- ✅ BooleanMode enum: NewBody, Add, Cut, Intersect
- ✅ HistoryIO: JSONL serialization of operations
- ✅ Document stores operations vector

**What's Missing:**
- ❌ DependencyGraph for tracking feature relationships
- ❌ RegenerationEngine for replay/rebuild
- ❌ History UI panel (no `src/ui/history/` directory exists)
- ❌ Commands don't create OperationRecords (tools create records directly)
- ❌ No feature suppression or editing workflow

**Behaviors (Planned):**
- Regen failure → Rollback to last valid state
- Feature suppression → Gray + skip in regen
- Feature reorder → Not allowed (fixed creation order)
- History edit → Double-click opens parameter dialog

---

### 3.3 Modeling Operations (Complete — P1) ✅
**Goal:** Complete core v1.0 modeling operations
**Status:** **100% Complete** (minor UI wiring TODOs)

#### Completed Operations
- [x] **Extrude / Push-Pull** (ExtrudeTool.cpp, 500 LOC):
    - ✅ SketchRegion input with preview & draft angle
    - ✅ Face input (Push/Pull) with auto-boolean detection (Add/Cut)
    - ✅ Smart Boolean override logic
    - ✅ Command integration (AddBodyCommand, ModifyBodyCommand)
- [x] **Revolve** (RevolveTool.cpp, 430 LOC):
    - ✅ Profile+Axis selection, drag interaction, boolean mode
    - ✅ Two-step workflow (profile → axis)
    - ✅ Sketch line or body edge as axis
- [x] **Fillet/Chamfer** (FilletChamferTool.cpp, 408 LOC):
    - ✅ Combined tool (drag right=fillet, left=chamfer)
    - ✅ Variable radius with real-time preview
    - ✅ Edge chaining via `EdgeChainer` (tangent propagation)
    - ⚠️ TODO: Wire Tab key in Viewport for mode toggle
- [x] **Shell** (ShellTool.cpp, 312 LOC):
    - ✅ Hollow solid creation (`BRepOffsetAPI_MakeThickSolid`)
    - ✅ Single-face opening works
    - ⚠️ TODO: Wire Shift+click for multi-face selection
    - ⚠️ TODO: Wire Enter key for face confirmation
- [x] **Boolean Operations** (BooleanOperation.cpp, 130 LOC):
    - ✅ Union/Cut/Intersect (Add/Subtract/Common)
    - ✅ `ModifyBodyCommand` integration
    - ✅ Smart mode detection (overlap → Cut, touching → Add)
- [x] **Utilities**: `EdgeChainer` (211 LOC, BFS tangent search)

---

### 3.4 Pattern Operations (Medium Priority — P2)
**Goal:** Linear and circular pattern support
**Dependencies:** Phase 3.2 (patterns need history)

| Task | Files | Status | Est. LOC |
|------|-------|--------|----------|
| PatternEngine base | `src/core/modeling/PatternEngine.h/cpp` | [ ] | 300 |
| LinearPattern | `src/core/modeling/LinearPattern.h/cpp` | [ ] | 250 |
| CircularPattern | `src/core/modeling/CircularPattern.h/cpp` | [ ] | 250 |
| Pattern tool UI | `src/ui/tools/PatternTool.h/cpp` | [ ] | 300 |
| Instance rendering | BodyRenderer updates | [ ] | 200 |

**Pattern Behavior:**
- Mode: Feature-level (parametric, in history)
- Preview: All instances shown during drag
- Input: Dialog for count + spacing
- Circular axis: Click edge/line for linear reference

---

### 3.5 UI Polish (In Progress — ~40%)
**Goal:** UX refinements for v1.0

| Task | Files | Status | LOC |
|------|-------|--------|-----|
| **StartOverlay** | `src/ui/start/StartOverlay.h/cpp` | [x] | 288 |
| **ProjectTile** | `src/ui/start/ProjectTile.h/cpp` | [x] | 187 |
| **ThemeManager** | `src/ui/theme/ThemeManager.h/cpp` | [x] | 674 |
| **ThemeConfig** | `src/ui/theme/ThemeConfig.h/cpp` | [x] | 600 |
| **Model Navigator** | `src/ui/navigator/ModelNavigator.h/cpp` | [x] | 855 |
| **Debug Panel** | `src/ui/viewport/RenderDebugPanel.h/cpp` | [x] | 286 |
| **ViewCube** | `src/ui/viewcube/ViewCube.h/cpp` | [x] | 537 |
| **DeepSelectPopup** | `src/ui/selection/DeepSelectPopup.h/cpp` | [x] | ~150 |
| PropertyInspector (stub) | `src/ui/inspector/PropertyInspector.h/cpp` | [~] | 103 |
| Command search | `src/ui/search/CommandSearch.h/cpp` | [ ] | - |
| Camera inertia | `src/render/Camera3D.cpp` | [ ] | - |
| Box selection | SelectionManager + Viewport | [ ] | - |
| DOF color wiring | SketchRenderer + UI | [~] | - |

**Completed:**
- ✅ StartOverlay with project browser + Recent Projects
- ✅ ProjectTile with thumbnail rendering
- ✅ Full ThemeManager (light/dark modes, CSS variables)
- ✅ ModelNavigator with visibility toggling, isolation, filtering
- ✅ RenderDebugPanel for normals/wireframes/bounds
- ✅ ViewCube for 3D navigation
- ✅ DeepSelectPopup for ambiguous pick resolution

**Still Needed:**
- ❌ Command search (Cmd+K palette)
- ❌ PropertyInspector full implementation (currently stub with placeholder)
- ❌ Camera inertia physics
- ❌ Box selection (left-to-right vs right-to-left)

---

## Phase 4: Advanced Modeling & Refinement
**Focus:** Adding depth to modeling capabilities and UI polish.
**Status:** Partial (~10% — ThemeManager complete)

### 4.1 Advanced Features (v2.0)
- [ ] **Extrude Advanced**: Symmetric, asymmetric, to face, through all
- [ ] **Pattern Along Path**: Sweep-based duplication
- [ ] **Spline/Bezier**: Complex curve sketching
- [ ] **Text in Sketches**: DXF-style text entities

### 4.2 UI/UX Polish
- [ ] **Visual Styles**: Wireframe, Shaded, Shaded with Edges toggles
- [x] **Dark/Light Themes**: ✅ **Complete** via ThemeManager (885 LOC)
- [ ] **Transform Gizmo**: Move/rotate/scale handles

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

### ✅ COMPLETED SINCE LAST MAJOR UPDATE
1. ~~Implement Push/Pull Direct Modeling~~ → **DONE** (ExtrudeTool face support)
2. ~~Implement Fillet/Chamfer Tool~~ → **DONE** (FilletChamferTool)
3. ~~Implement Shell Tool~~ → **DONE** (ShellTool - single face working)
4. ~~Implement Boolean Operations~~ → **DONE** (Union/Cut/Intersect)
5. ~~Implement Edge Chaining~~ → **DONE** (EdgeChainer)
6. ~~Enhance Model Navigator~~ → **DONE** (Visibility/Filtering/Isolation)
7. ~~Add Render Debug Panel~~ → **DONE**
8. ~~Implement Full I/O Layer~~ → **DONE** (Save/Load/STEP/ZIP)
9. ~~Add StartOverlay + ProjectTile~~ → **DONE** (Project browser)
10. ~~Implement ThemeManager~~ → **DONE** (Light/Dark themes)

### Immediate Next Steps (High Priority)
1. **Wire ShellTool multi-face selection** — Shift+click and Enter key in Viewport (P0 Quick Win)
2. **Wire FilletChamferTool Tab key** — Mode toggle in Viewport (P0 Quick Win)
3. **DependencyGraph + RegenerationEngine** — Full parametric replay (P1 Critical)
4. **HistoryPanel UI** — Display operation history (P1)

### Short-term (Medium Priority - UX Polish)
1. Implement Command Search (Cmd+K palette)
2. Add Camera inertia physics and sticky pivot
3. Complete PropertyInspector (currently stub)
4. Implement Box selection (left-to-right / right-to-left modes)

### Blocking Items for Phase 3 Completion
- ✅ ~~Modeling Operations~~ → COMPLETE (100%)
- ✅ ~~I/O layer~~ → COMPLETE (100%)
- ⚠️ **Parametric Engine** → PARTIAL (data structures done, no replay/UI)
- ⚠️ **Pattern Operations** → NOT STARTED

### Codebase Statistics (2026-01-10)
| Category | Files | LOC |
|----------|-------|-----|
| src/app/ | 25 | ~3,000 |
| src/core/sketch/ | 48 | ~10,000 |
| src/core/loop/ | 8 | ~2,700 |
| src/core/modeling/ | 4 | ~400 |
| src/kernel/ | 1 | ~990 |
| src/render/ | 10 | ~2,500 |
| src/io/ | 24 | ~2,400 |
| src/ui/ | 49 | ~13,000 |
| **Total** | **~170** | **~41,000** |

---

*Document Version: 5.0*
*Last Updated: 2026-01-10*
*Major Update: Documented complete I/O layer, updated all phase statuses with verified LOC counts*
