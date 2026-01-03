# OneCAD

> **⚠️ VibeCoding Alert**: Full codebase generated with AI. Project in active development—manual review & validation required.

## Overview
OneCAD is a C++ CAD application utilizing:
- **CMake** for build configuration.
- **Qt6** for the Graphical User Interface (GUI).
- **Eigen3** for linear algebra.
- **OpenCASCADE Technology (OCCT)** for geometric modeling.

## Prerequisites (macOS)

1.  **Xcode Command Line Tools**:
    ```bash
    xcode-select --install
    ```
2.  **Homebrew**:
    Visit [brew.sh](https://brew.sh).
3.  **Dependencies**:
    ```bash
    brew install cmake eigen opencascade qt
    ```

## Building

1.  Create a build directory:
    ```bash
    mkdir build
    cd build
    ```

2.  Configure with CMake:
    ```bash
    cmake ..
    ```

3.  Build:
    ```bash
    make
    ```

4.  Run:
    ```bash
    ./OneCAD
    ```

## Features

### Camera Angle Control (Shapr3D-style)

OneCAD includes a camera angle slider in the status bar that controls projection mode:

- **0°** = **Orthographic projection** (parallel projection, no perspective distortion)
- **>0°** = **Perspective projection** with Field of View = slider value
- **Range**: 0° to 90°
- **Default**: 45° (standard perspective)

**Scale-Preserving Behavior:**
The camera automatically adjusts its distance when changing angles to preserve the apparent scale of the model. This prevents unwanted zoom-in/zoom-out effects when adjusting the slider.

**Transition Formulas:**
- Ortho → Perspective: `D_new = S_ortho / tan(θ/2)`
- Perspective → Perspective: `D_new = D_old × tan(θ_old/2) / tan(θ_new/2)`
- Perspective → Ortho: `S_new = D_curr × tan(θ/2)`

**Implementation Details:**
- Orthographic scale: World units per viewport height
- Perspective FOV: Maps 1:1 with slider angle (minimum 5° to avoid degeneracy)
- All transitions preserve visible world height at target distance
- Setting persists across sessions via QSettings

**Settings Key:**
```
viewport/cameraAngle (float, default: 45.0)
```

### Grid System

OneCAD displays a **fixed 10mm × 10mm** grid on the XY plane:

- **Spacing**: Always 10mm (not adaptive)
- **Major lines**: Every 10th line (darker)
- **Origin axes**: X (red), Y (green), Z (blue)
- **Extent**: Scales with camera distance for performance
- **Toggle**: View → Toggle Grid (shortcut: G)

## Development

- **Dependencies**:
  - Qt6 is expected at `/opt/homebrew/opt/qt` (Homebrew default) or via `CMAKE_PREFIX_PATH`.
  - OCCT is expected via Homebrew paths.
- **Project Structure**:
  - `src/main.cpp`: Application entry point.
  - `src/MainWindow.cpp`/`.h`: Main application window setup.