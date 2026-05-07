# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```bash
make init              # Install deps (macOS/Homebrew or Linux/apt) + configure CMake
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
│   ├── modeling/      # BooleanOperation (real); other headers are forwarding shims to src/kernel/
│   └── loop/          # LoopDetector (region detection from closed sketch loops)
├── kernel/
│   ├── elementmap/    # ElementMap (topological naming) — see src/kernel/AGENTS.md
│   ├── geometry/      # EdgeChainer (tangent-continuous chain expansion)
│   ├── modeling/      # FaceExtrudeProfileBuilder
│   └── topology/      # CoplanarFacePatch, FacePatchResolver, SelectionTopologyResolver, TopologyVisibility
├── render/            # Camera3D, Grid3D, OpenGL 4.1 Core, SketchRenderer, TessellationCache
├── ui/                # Qt6 widgets: Viewport, ContextToolbar, ViewCube, ModelNavigator, tools/ (Extrude/Revolve/Fillet/Shell/LinearPattern/Measure)
└── io/                # Package (ZIP/Directory), HistoryIO (JSONL), DocumentIO, SketchIO, ElementMapIO
tests/                 # ~27 standalone prototype executables for regression testing
```

### Key Subsystems

**Document & History** (`src/app/`):

- `Document` is the central QObject storing sketches (UUID map), bodies (TopoDS_Shape), and operations (`OperationRecord` vector)
- `OperationRecord`: opId, type (Extrude/Revolve/Fillet/Chamfer/Shell/Boolean/LinearPattern), input (`std::variant`: SketchRegionRef/FaceRef/BodyRef), params (`std::variant` of typed params), resultBodyIds
- `DependencyGraph`: forward/backward adjacency, Kahn's topological sort, suppression propagation, failure tracking
- `RegenerationEngine`: replays operations in topo-sorted order → produces bodies. Supports `regenerateAll()`, `regenerateFrom(opId)`, `previewFrom(opId, newParams)`
- `KernelScheduler`: single-writer background thread for non-blocking regeneration

**Command System** (`src/app/commands/`):

- `Command` interface: `execute()`, `undo()`, `label()`
- `CommandProcessor`: undo/redo stacks, transaction batching
- Operation commands: AddOperation, RemoveOperation, UpdateOperationParams, Rollback, SetOperationSuppression
- Rollback = suppress downstream ops without deletion (maintains dependency links)
- Applied op count tracks insertion cursor (≤ total ops; ops beyond cursor are drafts)

**Topological Naming** (`src/kernel/elementmap/`):

- Persistent, deterministic IDs for faces/edges/vertices across regeneration
- ID scheme: `"bodyId/kind-reason-opId-hash-ordinal"`
- Descriptor (14 fields) matching: center, size, magnitude, surface/curve type, normal/tangent, adjacency hash; quantization 1e-6, FNV-1a hashing
- **Regression-sensitive**: changing quantization, hash seeds, or descriptor field order remaps IDs — see `src/kernel/AGENTS.md` and run `proto_elementmap_rigorous` before commit
- Not thread-safe; serialize access. Normalize TopoDS to `TopAbs_FORWARD` before reverse-binding lookup

**Modeling** (split between `src/kernel/` and `src/core/modeling/`):

- `CoplanarFacePatch` (`kernel/topology/`): extracts connected coplanar faces (normal dot 0.9999, plane dist 1e-3)
- `FaceExtrudeProfileBuilder` (`kernel/modeling/`): merges coplanar patch into single extrude profile
- `FacePatchResolver` (`kernel/topology/`): bridges ElementMap IDs ↔ coplanar patches
- `SelectionTopologyResolver` (`kernel/topology/`): resolves picks to ElementMap IDs with promotion rules
- `EdgeChainer` (`kernel/geometry/`): builds tangent-continuous edge chains for fillet/chamfer auto-expansion
- `BooleanOperation` (`core/modeling/`): perform + detectMode (NewBody/Add/Cut/Intersect)
- Headers in `src/core/modeling/` (except `BooleanOperation.h`) are thin forwarding shims to `src/kernel/` — edit the kernel originals

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
- **OCCT**: Always null-check `Handle<>` before use. Check `IsDone()` on builders (BRepBuilderAPI, BRepAlgoAPI) before calling `Shape()`
- **OpenGL 4.1 Core**: No fixed-function pipeline. Always pair bind/unbind (VAO, VBO, shader). Call `makeCurrent()` before GL ops. GL context is single-threaded
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

~27 prototype executables in `tests/`. No test framework — each is a standalone binary with assert-style checks.

```bash
cmake --build build --target <name> && ./build/tests/<name>   # Build + run single
make test                                                      # Core 3 (elementmap, tnaming, custom_map)
ctest --test-dir build                                         # Run all registered tests
```

**Which test for which change:**

| Changed subsystem   | Run these                                                    |
| ------------------- | ------------------------------------------------------------ |
| Kernel / ElementMap | `proto_elementmap_rigorous` (required before commit)         |
| Sketch entities     | `proto_sketch_geometry`                                      |
| Constraints         | `proto_sketch_constraints`                                   |
| Solver              | `proto_sketch_solver`                                        |
| Loop / region       | `proto_loop_detector`, `proto_face_builder`                  |
| History / regen     | `proto_regeneration`                                         |
| I/O                 | `proto_history_io_compat`, `proto_document_roundtrip_compat` |
| UI compile check    | `test_compile`                                               |
| Full pipeline       | `proto_regeneration`                                         |

**Adding a new prototype:**

1. Create `tests/prototypes/proto_<name>.cpp`
2. In `tests/CMakeLists.txt`: `add_executable`, `target_link_libraries(... PRIVATE onecad_core)`, `target_include_directories`, `add_test`

## Troubleshooting

- **Qt not found**: verify `CMAKE_PREFIX_PATH`
- **OCCT not found**: set `OpenCASCADE_DIR` (e.g., `/opt/homebrew/lib/cmake/opencascade`)
- **Build cache stale**: `rm -rf build/` and reconfigure
- **Linker errors on macOS**: `xcode-select --install`

## Git

- Conventional Commits: `feat:`, `fix:`, `chore:`, `refactor:`, `docs:`
- Excluded from review: `third_party/`, `build/`, `resources/`, MOC/UI generated files
