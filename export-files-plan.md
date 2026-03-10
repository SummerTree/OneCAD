# Export Formats Implementation Plan

## Context

OneCAD supports STEP, STL, OBJ export. Adding: IGES (manufacturing), 3MF (modern 3D printing), SVG (sketch 2D with constraints), DXF (2D CNC). OCCT 7.9.3 provides native IGES; 3MF via lib3mf (FetchContent).

## Current State

| Format | Status | Implementation                                           |
| ------ | ------ | -------------------------------------------------------- |
| STEP   | Done   | XDE-based, colors/names, `src/io/step/StepExporter`      |
| STL    | Done   | Binary/ASCII, custom writer, `src/io/mesh/StlExporter`   |
| OBJ    | Done   | Normals/groups, custom writer, `src/io/mesh/ObjExporter` |

**Pattern:** Static class, `exportDocument(filepath, doc, opts)`, result struct.

---

## Phase 1: IGES Export (OCCT-native, no new deps)

- **Files:** `src/io/iges/IgesExporter.h|cpp`
- **Approach:** Mirror `StepExporter` — same XDE document setup (shapes + colors + names), swap `STEPCAFControl_Writer` → `IGESCAFControl_Writer`
- **Reuse:** `StepExportResult`, XDE boilerplate from `StepExporter.cpp`
- **OCCT:** `IGESCAFControl_Writer` with `SetColorMode(true)`, `SetNameMode(true)`
- **~120 lines**

---

## Phase 2: 3MF Export (lib3mf via FetchContent)

- **Files:** `src/io/mesh/ThreeMfExporter.h|cpp`
- **Approach:** Read `SceneMeshStore::Mesh`, use lib3mf API for mesh objects + base materials (per-face colors)
- **CMake:** FetchContent from `https://github.com/3MFConsortium/lib3mf`, guarded with `HAS_LIB3MF`
- **Reuse:** `MeshExportResult`
- **~150 lines + CMake setup**

---

## Phase 3: SVG Sketch Export (full geometry + constraints)

- **Files:** `src/io/svg/SvgExporter.h|cpp`
- **Result:** `SvgExportResult` { success, errorMessage, entityCount, constraintCount }

### Geometry Export (all entity types)

| Entity          | SVG Element                                                                                                     |
| --------------- | --------------------------------------------------------------------------------------------------------------- |
| `SketchPoint`   | `<circle cx cy r="1"/>` (marker dot)                                                                            |
| `SketchLine`    | `<line x1 y1 x2 y2/>` — resolve start/end via `getEntityAs<SketchPoint>(startPointId())`                        |
| `SketchCircle`  | `<circle cx cy r/>` — center from centerPointId, radius from `radius()`                                         |
| `SketchArc`     | `<path d="M startX startY A rx ry rotation largeArc sweep endX endY"/>` — CCW, use `startPoint()`, `endPoint()` |
| `SketchEllipse` | `<ellipse cx cy rx ry transform="rotate(rotation)"/>`                                                           |

- **Construction geometry:** Always included, `stroke-dasharray="4,3"` + lighter color
- **Check:** `entity->isConstruction()`

### Constraint Annotations

Rendered as `<g class="constraints">`.

**Positional/Relational (icon-based):**
| Constraint | SVG Annotation |
|-----------|---------------|
| Horizontal | "H" text near line midpoint |
| Vertical | "V" text near line midpoint |
| Parallel | "∥" between lines |
| Perpendicular | "⊥" at intersection |
| Tangent | "T" at tangent point |
| Equal | "=" on both entities |
| Coincident | Filled dot at point |
| Fixed | "⊗" at fixed point |
| Midpoint | "M" at midpoint |
| Concentric | Concentric rings at center |

**Dimensional (leader lines + value text):**
| Constraint | SVG Annotation |
|-----------|---------------|
| Distance | Leader line between entities + value text (mm) |
| Angle | Arc between lines + degree value |
| Radius | Leader from center to edge + "R" + value |
| Diameter | Leader through center + "⌀" + value |

- **Positioning:** `constraint->getDimensionTextPosition(sketch)` for labels, `getIconPosition(sketch)` for symbols
- **Values:** `DimensionalConstraint::value()` (mm), `AngleConstraint::angleDegrees()` (°)

### Coordinate System

- Sketch-local 2D, negate Y for SVG (Y-down), viewBox from bounding box + margin

### **~350 lines**

---

## Phase 4: DXF Sketch Export (R12 ASCII, full geometry + constraints)

- **Files:** `src/io/dxf/DxfExporter.h|cpp`
- **Result:** `DxfExportResult` { success, errorMessage, entityCount, constraintCount }

### DXF R12 Structure

```
SECTION/HEADER    — $ACADVER=AC1009, $INSUNITS=4 (mm)
SECTION/TABLES    — LTYPE (Continuous, Dashed), LAYER (0, CONSTRUCTION, CONSTRAINTS)
SECTION/ENTITIES  — geometry + constraint annotations
EOF
```

### Geometry (Layer 0)

| Entity          | DXF Entity                                          |
| --------------- | --------------------------------------------------- |
| `SketchLine`    | `LINE` (10/20 start, 11/21 end)                     |
| `SketchCircle`  | `CIRCLE` (10/20 center, 40 radius)                  |
| `SketchArc`     | `ARC` (10/20 center, 40 radius, 50 start°, 51 end°) |
| `SketchEllipse` | Polyline approximation (72 segments, 5° steps)      |
| `SketchPoint`   | `POINT` (10/20 position)                            |

- **Construction:** Layer `CONSTRUCTION`, linetype `DASHED`

### Constraint Annotations (Layer CONSTRAINTS)

- Dimensional: `TEXT` at `getDimensionTextPosition()` with leader `LINE`s
- Relational: `TEXT` with symbols ("H","V","∥","⊥","T","=") at `getIconPosition()`

### **~300 lines**

---

## UI Integration

### Menu Structure

```
Import STEP...
---
Export STEP...
Export IGES...
Export Mesh...              ← extended (STL/OBJ/3MF)
---
Export Sketch as SVG...     ← enabled only in sketch mode
Export Sketch as DXF...     ← enabled only in sketch mode
```

### MainWindow Changes (`src/ui/mainwindow/MainWindow.h|cpp`)

- New slots: `onExportIges()`, `onExportSketchSvg()`, `onExportSketchDxf()`
- `onExportIges()` mirrors `onExportStep()`, simpler options (visibleOnly)
- Sketch export slots: get active sketch → file dialog → call exporter
- Sketch items disabled when not editing a sketch

### MeshExportDialog Changes (`src/ui/dialogs/MeshExportDialog`)

- Add `ThreeMF` to `Format` enum (`#if HAS_LIB3MF`)
- Update combo box, `fileFilter()`, `defaultExtension()`

---

## CMake Changes (`src/io/CMakeLists.txt`)

```cmake
# New sources
iges/IgesExporter.cpp
svg/SvgExporter.cpp
dxf/DxfExporter.cpp

# Optional 3MF
include(FetchContent)
FetchContent_Declare(lib3mf
    GIT_REPOSITORY https://github.com/3MFConsortium/lib3mf.git
    GIT_TAG v2.3.1)
option(ONECAD_3MF "Enable 3MF export" ON)
if(ONECAD_3MF)
    FetchContent_MakeAvailable(lib3mf)
    list(APPEND IO_SOURCES mesh/ThreeMfExporter.cpp)
    target_link_libraries(onecad_io PRIVATE lib3mf)
    target_compile_definitions(onecad_io PRIVATE HAS_LIB3MF=1)
endif()
```

---

## Implementation Order

1. IGES export (mirror STEP)
2. 3MF export (FetchContent + mesh writer)
3. SVG sketch export (geometry + constraints)
4. DXF sketch export (R12 ASCII + constraints)
5. UI update (menu items, dialog extension, handlers)

**Total: ~10 new files, ~1000-1200 lines**

---

## Key Files to Reference

- `src/io/step/StepExporter.cpp` — XDE pattern for IGES
- `src/io/mesh/StlExporter.cpp` — SceneMeshStore pattern for 3MF
- `src/core/sketch/Sketch.h` — `getAllEntities()`, `getAllConstraints()`
- `src/core/sketch/SketchEntity.h` — base, `isConstruction()`
- `src/core/sketch/SketchPoint.h|SketchLine.h|SketchArc.h|SketchCircle.h|SketchEllipse.h`
- `src/core/sketch/constraints/Constraints.h` — all constraints, `DimensionalConstraint::value()`
- `src/core/sketch/SketchConstraint.h` — `getIconPosition()`, `getDimensionTextPosition()`
- `src/ui/mainwindow/MainWindow.cpp:1491-1584` — export handler pattern
- `src/ui/dialogs/MeshExportDialog.h|cpp` — format dialog
- `src/io/CMakeLists.txt` — build config

---

## Verification

- IGES: export multi-body, open in FreeCAD/Fusion 360, verify colors
- 3MF: export colored model, open in PrusaSlicer/Cura, verify mesh + colors
- SVG: export sketch with all entity types + constraints, open in browser, verify geometry, dashed construction, dimension values, symbol positions
- DXF: open in LibreCAD, verify layers (0, CONSTRUCTION, CONSTRAINTS), geometry, annotations
- STL/OBJ: verify still work after MeshExportDialog changes

---

## Unresolved Questions

1. **lib3mf FetchContent tag** — verify latest stable (v2.3.1 or newer?)
2. **Ellipse in DXF R12** — approximate with polyline; 72 segments (5° steps) sufficient?
3. **Constraint annotation styling** — SVG font size/color for dimensions? Suggest 3mm height, blue (#0066CC)
