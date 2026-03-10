# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```bash
make init              # Install deps (macOS/Homebrew) + configure CMake
make run               # Build + run
make test              # Build + run 3 core prototype tests (elementmap, tnaming, custom_map)

# Build + run a single test
cmake --build build --target proto_regeneration && ./build/tests/proto_regeneration

# Headless smoke test (CI)
ONECAD_HEADLESS=1 make run
```

Qt path override: `cmake .. -DCMAKE_PREFIX_PATH=/path/to/qt`

## Project Overview

C++ CAD application. C++20. Dependencies: Qt6, OpenCASCADE (OCCT), Eigen3, PlaneGCS (vendored from FreeCAD in `third_party/planegcs/`).

**Platform**: macOS 14+ Apple Silicon. Default Qt path: `/opt/homebrew/opt/qt`

## Architecture

```
src/
├── app/
│   ├── commands/      # Command pattern (undo/redo via CommandProcessor)
│   ├── document/      # Document model: sketches, bodies, operations, ElementMap
│   ├── history/       # DependencyGraph, RegenerationEngine, KernelScheduler
│   └── selection/     # SelectionManager (sketch/model modes, deep select)
├── core/
│   ├── sketch/        # Entities, tools, solver, constraints, SnapManager, AutoConstrainer
│   ├── modeling/      # CoplanarFacePatch, FaceExtrudeProfileBuilder, EdgeChainer, BooleanOperation
│   └── loop/          # LoopDetector (region detection from closed sketch loops)
├── kernel/            # OCCT wrappers, ElementMap (topological naming)
├── render/            # Camera3D, Grid3D, OpenGL 4.1 Core, SketchRenderer
├── ui/                # Qt6 widgets: Viewport, ContextToolbar, ViewCube, ModelNavigator
└── io/                # Package (ZIP/Directory), HistoryIO (JSONL), DocumentIO, SketchIO, ElementMapIO
tests/                 # ~27 standalone prototype executables for regression testing
```

### Key Subsystems

**Document & History** (`src/app/`):

- `Document` is the central QObject storing sketches (UUID map), bodies (TopoDS_Shape), and operations (`OperationRecord` vector)
- `OperationRecord`: opId, type (Extrude/Revolve/Fillet/Chamfer/Shell/Boolean), input (`std::variant`: SketchRegionRef/FaceRef/BodyRef), params (`std::variant` of typed params), resultBodyIds
- `DependencyGraph`: forward/backward adjacency, Kahn's topological sort, suppression propagation, failure tracking
- `RegenerationEngine`: replays operations in topo-sorted order → produces bodies. Supports `regenerateAll()`, `regenerateFrom(opId)`, `previewFrom(opId, newParams)`
- `KernelScheduler`: single-writer background thread for non-blocking regeneration

**Command System** (`src/app/commands/`):

- `Command` interface: `execute()`, `undo()`, `label()`
- `CommandProcessor`: undo/redo stacks, transaction batching
- Operation commands: AddOperation, RemoveOperation, UpdateOperationParams, Rollback, SetOperationSuppression
- Rollback = suppress downstream ops without deletion (maintains dependency links)
- Applied op count tracks insertion cursor (≤ total ops; ops beyond cursor are drafts)

**Topological Naming** (`src/kernel/ElementMap`):

- Persistent, deterministic IDs for faces/edges/vertices across regeneration
- ID scheme: `"bodyId/kind-reason-opId-hash-ordinal"`
- Descriptor-based matching: center distance, size, surface/curve type, normal/tangent, adjacency hash
- **Regression-sensitive**: descriptor hashing order changes can remap IDs; validate with golden comparisons

**Modeling** (`src/core/modeling/`):

- `CoplanarFacePatch`: extracts connected coplanar faces (normal dot 0.9999, plane dist 1e-3)
- `FaceExtrudeProfileBuilder`: merges coplanar patch into single extrude profile
- `FacePatchResolver`: bridges ElementMap IDs ↔ coplanar patches
- `EdgeChainer`: builds tangent-continuous edge chains for fillet/chamfer auto-expansion
- `BooleanOperation`: perform + detectMode (NewBody/Add/Cut/Intersect)

**Selection** (`src/app/selection/`):

- `SelectionKind`: SketchPoint, SketchEdge, SketchRegion, SketchConstraint, Vertex, Edge, Face, Body
- Modes: Sketch vs Model. Filters limit selectable kinds. Deep select cycles through ambiguous hits

**I/O** (`src/io/`):

- `Package` abstraction: `ZipPackage` (.onecad) / `DirectoryPackage` (.onecadpkg, Git-friendly)
- File format: manifest.json, document.json, history/ops.jsonl, history/state.json, elementmap, sketches/
- `HistoryIO`: JSONL format (one op per line)

### Data Flow

1. Sketch created → UUID → `Document.sketches_`
2. Operation created → `OperationRecord` → `Document.operations_`
3. Regeneration: `DependencyGraph.topologicalSort()` → execute each op → output bodies to `Document.bodies_`
4. ElementMap tracks shape identity through regeneration cycles
5. UI tools (ExtrudeTool, etc.) create commands → `CommandProcessor` → regenerate

### Important Patterns

- **EntityID = std::string (UUID)** for all sketch entities and operations
- **Non-copyable, movable** entities
- **Tool pattern**: `SketchTool` subclasses managed by `SketchToolManager`, lifecycle: `handleMousePress/Move/Release/DoubleClick`
- **SnapManager**: 2mm radius (sketch coords), priority Vertex > Endpoint > Midpoint > Center > Quadrant > Intersection > Grid. Always check `snapped` flag
- **AutoConstrainer**: infers H/V (±5°), Coincident (2mm), Perpendicular, Tangent. Ghost entities at 50% opacity, applied on commit
- **Suppression vs Deletion**: suppress for rollback (preserves deps), delete for permanent removal
- **Preview state**: RegenerationEngine backs up/restores bodies for non-destructive parameter preview

## Critical Implementation Notes

- **Sketch coordinate system**: Non-standard mapping for XY plane:
  - Sketch X → World Y+ (0,1,0), Sketch Y → World X- (-1,0,0), Normal → World Z+ (0,0,1)
  - See `SketchPlane::XY()` in `src/core/sketch/Sketch.h`
- **OCCT**: Always null-check `Handle<>` objects before dereferencing shapes
- **Qt signals**: Queued for cross-thread, Direct for same-thread. Ensure parent ownership
- **Boolean target resolution**: priority chain: explicit param → FaceRef.bodyId → sketch host body
- **Applied op count** is distinct from total op count — allows draft ops beyond cursor

## Specifications

- `SPECIFICATION.md` - Full software spec (3500+ lines)
- `SKETCH_IMPLEMENTATION_PLAN.md` - 7-phase roadmap
- `PHASES.md` - Development phases overview

## Code Standards

- C++20: `enum class`, `std::optional`, `std::span` where appropriate
- Ownership: `std::unique_ptr` for single-owner, `std::shared_ptr` only when required
- Const correctness: `const&` for large types, `const` member functions when no mutation
- Error handling: `bool`/`std::optional` for recoverable errors; avoid exceptions in hot paths
- Qt: parent ownership for QObject lifetimes, no raw `new` without parent

## Testing

~27 prototype executables in `tests/`. Key groups:

- **ElementMap/OCCT**: proto_custom_map, proto_tnaming, proto_elementmap_rigorous
- **Sketch**: proto_sketch_geometry, proto_sketch_constraints, proto_sketch_snap, proto_sketch_solver, proto_sketch_group_drag, proto_sketch_drag_undo
- **History/Regen**: proto_regeneration, proto_history_io_compat, proto_document_roundtrip_compat, proto_timeline_rollback_dirty
- **Viewport/Selection**: proto_model_picker, proto_viewport_drag_state, proto_pickmesh_integration, proto_pick_topology_promotion

Build single: `cmake --build build --target <name>`
Run single: `./build/tests/<name>`
Run core 3: `make test`

## Troubleshooting

- **Qt not found**: verify `CMAKE_PREFIX_PATH`
- **OCCT not found**: set `OpenCASCADE_DIR` (e.g., `/opt/homebrew/lib/cmake/opencascade`)
- **Build cache stale**: `rm -rf build/` and reconfigure
- **Linker errors on macOS**: `xcode-select --install`

## Git

- Conventional Commits: `feat:`, `fix:`, `chore:`, `refactor:`, `docs:`
- Excluded from review: `third_party/`, `build/`, `resources/`, MOC/UI generated files
