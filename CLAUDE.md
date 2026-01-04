# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```bash
make init              # Install deps (macOS/Homebrew) + configure CMake
make run               # Build + run
make test              # Build + run prototype tests

# Manual build
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build .
./OneCAD  # or ./OneCAD.app/Contents/MacOS/OneCAD
```

Qt path override:
- Use env var: `CMAKE_PREFIX_PATH=/path/to/qt cmake ..`
- Or use flag: `cmake .. -DCMAKE_PREFIX_PATH=/path/to/qt`

Examples:
- macOS (Intel Homebrew): `-DCMAKE_PREFIX_PATH=/usr/local/opt/qt`
- Linux (distro packages): `-DCMAKE_PREFIX_PATH=/usr/lib/qt6`
- Windows (Qt installer): `-DCMAKE_PREFIX_PATH=C:\\Qt\\6.6.0\\msvc2019_64`
- Windows (vcpkg): use `-DCMAKE_TOOLCHAIN_FILE=.../vcpkg.cmake` and vcpkg Qt6 triplet

## Project Overview

C++ CAD application. C++20. Dependencies: Qt6, OpenCASCADE (OCCT), Eigen3.

**Platform**: Tested on macOS 14+ (Apple Silicon). Intel macOS, Linux, and Windows are currently out of scope / untested; support may be added in a future phase. Qt path default: `/opt/homebrew/opt/qt`

## Architecture

```
src/
├── app/           # Application lifecycle, singleton controller
├── core/
│   ├── sketch/    # Sketch entities (Point, Line, Arc, Circle, Ellipse)
│   │   ├── tools/ # LineTool, ArcTool, CircleTool, RectangleTool, EllipseTool, MirrorTool, TrimTool
│   │   ├── solver/        # ConstraintSolver, PlaneGCS adapter
│   │   └── constraints/   # Constraint types (Distance, Angle, Coincident, etc.)
│   └── loop/      # LoopDetector for region detection
├── kernel/        # OCCT wrappers, ElementMap (topological naming)
├── render/        # Camera3D, Grid3D, OpenGL 4.1 Core, SketchRenderer
├── ui/
│   ├── mainwindow/    # MainWindow
│   ├── viewport/      # Viewport (3D + sketch interaction)
│   ├── toolbar/       # ContextToolbar (sketch tools)
│   ├── sketch/        # ConstraintPanel, DimensionEditor, SketchModePanel
│   ├── viewcube/      # ViewCube (3D navigation)
│   └── navigator/     # ModelNavigator
└── io/            # STEP import/export, native format

tests/             # Prototype executables (proto_custom_map, proto_tnaming, proto_elementmap_rigorous)
```

### Key Layers
1. **UI** → Qt6 widgets. ContextToolbar manages sketch tools. Viewport handles both 3D and 2D sketch interaction.
2. **Render** → Camera3D (orbit/pan/zoom), Grid3D (10mm fixed), SketchRenderer (entities + constraints + dimensions)
3. **Core/Sketch** →
   - **Entities**: Point, Line, Arc, Circle, Ellipse (all non-copyable, movable)
   - **Tools**: LineTool, ArcTool, CircleTool, RectangleTool, EllipseTool, MirrorTool, TrimTool
   - **SnapManager**: 2mm radius, priority Vertex > Endpoint > Midpoint > Center > Quadrant > Intersection > Grid
   - **AutoConstrainer**: Infers Horizontal, Vertical, Coincident, Perpendicular, Tangent constraints
   - **ConstraintSolver**: PlaneGCS wrapper (Phase 2 integration pending)
4. **Kernel** → OCCT geometry, ElementMap for persistent topology IDs

### Important Patterns
- **EntityID = std::string (UUID)** for all sketch entities
- **ElementMap** = topological naming system. Descriptor hashing is regression-sensitive (example: a hash ordering change can remap IDs; validate with a unit test + golden descriptor comparison + migration notes).
- **Direct parameter binding** in constraint solver (pointers to coordinates)
- **Non-copyable, movable** entities
- **Scale-preserving camera transitions** (ortho ↔ perspective)
- **Tool pattern**: Each sketch tool inherits from `SketchTool`, managed by `SketchToolManager`
- **Snap system**: SnapManager returns `SnapResult` with type, position, and entity IDs
- **Auto-constraints**: AutoConstrainer infers constraints during drawing (±5° tolerance for H/V, 2mm for coincidence)

## Critical Implementation Notes

- **Sketch coordinate system**: Non-standard mapping for XY plane:
  - Sketch X → World Y+ (0,1,0)
  - Sketch Y → World X- (-1,0,0)
  - Normal → World Z+ (0,0,1)
  - See `SketchPlane::XY()` in `src/core/sketch/Sketch.h:64`
- **ElementMap**: Topological naming for persistent geometry. Descriptor logic changes need thorough validation
- **PlaneGCS integration**: Phase 2 blocker. Solver skeleton exists in `src/core/sketch/solver/`
- **LoopDetector**: Graph-based (DFS cycles, point-in-polygon holes). Implementation pending in `src/core/loop/LoopDetector.h`
- **OCCT**: WARNING: Always null-check `Handle<>` objects before dereferencing shapes
- **Qt signals**: Queued for cross-thread, Direct for same-thread. Ensure parent ownership
- **Sketch tools**: Use `SketchToolManager` to activate tools. Each tool has `handleMousePress/Move/Release/DoubleClick` lifecycle
- **SnapManager**: Snap radius is 2mm in sketch coordinates (constant regardless of zoom). Always check `snapped` flag before using position
- **AutoConstrainer**: Returns inferred constraints as ghost entities (50% opacity). Applied on commit. Can be undone separately from geometry

## Status / Roadmap

**Current Phase**: Phase 7 complete (all 7 sketch tools integrated into UI)

**Completed**:
- ✅ Phase 1: Architecture foundation (entities: Point, Line, Arc, Circle, Ellipse)
- ✅ Phase 4-7: All sketch tools (Line, Arc, Circle, Rectangle, Ellipse, Mirror, Trim)
- ✅ SnapManager (2mm radius, full snap priority system)
- ✅ AutoConstrainer (H/V/Coincident/Perpendicular/Tangent inference)
- ✅ ConstraintPanel (displays constraints, ghost rendering)
- ✅ SketchRenderer (entities, constraints, dimensions, ghost feedback)

**Pending**:
- ⏳ PlaneGCS: Phase 2 blocker, integration pending in `src/core/sketch/solver/`
- ⏳ LoopDetector: Phase 3, interface defined in `src/core/loop/LoopDetector.h`
- ⏳ DimensionEditor: UI exists, needs solver integration

## Specifications

- `SPECIFICATION.md` - Full software spec (3500+ lines)
- `SKETCH_IMPLEMENTATION_PLAN.md` - 7-phase roadmap
- `PHASES.md` - Development phases overview

## Code Standards

- C++20 features are allowed; use `enum class`, `std::optional`, and `std::span` where it improves clarity.
- Ownership: prefer `std::unique_ptr` for single-owner, `std::shared_ptr` only when required.
- Const correctness: pass by `const&` for large types, keep member functions `const` when no mutation occurs.
- Error handling: return `bool`/`std::optional` for recoverable errors; avoid exceptions in hot paths unless required.
- Qt: use parent ownership for QObject lifetimes and avoid raw new without parent.

## Developer Guidance

### Setup
- Use `CMAKE_PREFIX_PATH` for Qt and `OpenCASCADE_DIR` if OCCT is not auto-detected
- Default Qt path on Apple Silicon: `/opt/homebrew/opt/qt`
- Run `make init` to install deps via Homebrew (macOS only) and configure CMake

### Testing
- Prototype targets: `proto_custom_map`, `proto_tnaming`, `proto_elementmap_rigorous`
- Run all: `make test`
- Build specific: `cmake --build build --target proto_custom_map`
- Execute: `./build/tests/proto_custom_map`

### Debugging
- Build type defaults to Debug (see Makefile `BUILD_TYPE`)
- Override: `make run BUILD_TYPE=Release`
- Qt Creator: open root `CMakeLists.txt`, set build directory to `build/`

### Troubleshooting
- **Qt not found**: verify `CMAKE_PREFIX_PATH` or Qt installation location
- **OCCT not found**: set `OpenCASCADE_DIR` to CMake config path (e.g., `/opt/homebrew/lib/cmake/opencascade`)
- **Build cache stale**: `rm -rf build/` and reconfigure
- **Linker errors on macOS**: ensure Xcode Command Line Tools installed (`xcode-select --install`)

### Git/PRs
- Conventional Commit prefixes: `feat:`, `fix:`, `chore:`, `refactor:`, `docs:`
- Include build notes and screenshots for UI changes
- Review profile: assertive (coderabbit)
- Excluded from review: `third_party/`, `build/`, `resources/`, MOC/UI generated files
