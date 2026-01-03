# OneCAD Context

## Project Overview
OneCAD is a free, open-source 3D CAD application designed for makers and hobbyists. It aims to provide an intuitive "sketch-first" workflow similar to Shapr3D, built on a robust, industry-standard technology stack.

**Core Technologies:**
- **Language:** C++20
- **Build System:** CMake (3.25+)
- **UI Framework:** Qt 6 (Widgets + OpenGL)
- **Geometry Kernel:** OpenCASCADE Technology (OCCT)
- **Math:** Eigen3
- **Constraint Solver:** PlaneGCS (planned integration)

## Directory Structure
- `src/app/`: Application layer (entry point logic, commands, tools).
- `src/core/`: Core CAD logic (sketching, modeling operations).
- `src/kernel/`: Geometry kernel wrappers and the **ElementMap** topological naming system.
- `src/render/`: Qt RHI rendering pipeline (Metal on macOS).
- `src/ui/`: Qt Widgets user interface (MainWindow, Inspector, Toolbar).
- `src/io/`: Import/Export (Native .onecad, STEP).
- `tests/`: Prototype executables and future test suites.

## Build & Run
The project uses a standard CMake out-of-source build workflow.

### Prerequisites (macOS)
- Xcode Command Line Tools
- Homebrew
- Dependencies: `cmake`, `qt`, `opencascade`, `eigen`

### Commands
```bash
# 1. Create build directory
mkdir build && cd build

# 2. Configure (ensure Qt6 path is found)
cmake ..

# 3. Build
make -j$(sysctl -n hw.ncpu)

# 4. Run Application
./OneCAD

# 5. Run Prototypes (e.g., Topological Naming test)
cmake --build . --target proto_tnaming
./tests/prototypes/proto_tnaming
```

## Development Conventions

### Coding Style
- **Standard:** C++20
- **Formatting:** 4-space indentation, braces on same line (K&R/1TBS variant).
- **Naming:**
    - Classes: `PascalCase` (e.g., `MainWindow`)
    - Methods/Variables: `camelCase` (e.g., `registerElement`)
    - Constants: `kPascalCase` or `UPPER_CASE`
    - Namespaces: `snake_case` (e.g., `onecad::kernel::elementmap`)
- **File Structure:** Class names match filenames (`MyClass.h` / `MyClass.cpp`).

### Architecture Notes
1.  **ElementMap (Topological Naming):**
    - Located in `src/kernel/elementmap/ElementMap.h`.
    - **CRITICAL:** This system tracks topology (Faces, Edges, Vertices) through boolean operations using persistent UUIDs and geometric descriptors. It is the foundation for parametric modeling. **Do not modify without understanding the descriptor matching and hashing logic.**
    - Use `ElementId` for all persistent references to geometry.

2.  **Application Singleton:**
    - `onecad::app::Application` manages the app lifecycle and global state.
    - Initialized in `src/main.cpp` before the UI.

3.  **Qt Integration:**
    - `QSurfaceFormat` is configured in `main.cpp` to ensure correct OpenGL/Metal context versions (Core Profile 4.1+).

## Current Status (as of Jan 2026)
- **Foundation:** Setup complete. `ElementMap` is implemented. App shell exists.
- **Missing:**
    - Sketching Engine (Phase 2)
    - Solid Modeling Operations (Phase 3)
    - Full RHI Rendering Pipeline

## Documentation
- `SPECIFICATION.md`: The Single Source of Truth for features and UX.
- `PHASES.md`: Implementation roadmap.
- `AGENTS.md`: Guidelines for AI agents.
