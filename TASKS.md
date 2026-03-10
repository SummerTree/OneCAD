# OneCAD — Feature Implementation Tasks

> Organized by priority tier. Each task = one feature.
> Dependencies reference task IDs (e.g., T1.3 depends on T0.4).

---

## Tier 0 — Bug Fixes & Quick Wins

### T0.1 — Fix Construction Geometry Default

**Priority**: Critical bug fix
**Effort**: ~1 hour
**Dependencies**: None

**Description**: All new sketch geometry defaults to construction mode (`m_isConstruction = true` in `SketchEntity.h:198`). This means every new line/circle/arc is reference-only by default and can't form closed regions for extrusion. Every other CAD tool defaults to regular geometry. Construction should be opt-in via a toggle key (`C` or `X`).

**Technical approach**:

- Change `bool m_isConstruction = true;` to `bool m_isConstruction = false;` in `src/core/sketch/SketchEntity.h:198`
- Verify that existing construction-specific logic (e.g., LoopDetector filtering) still works correctly since it checks `isConstruction()` explicitly
- Ensure serialization/deserialization preserves construction flag for existing files (backward compat — the flag is already serialized as a field, so old files with `"construction": true` will load correctly)
- Add keyboard shortcut `X` to toggle construction mode on selected entities in ContextToolbar/SketchModePanel

**Files**:

- `src/core/sketch/SketchEntity.h` — change default value
- `src/ui/toolbar/ContextToolbar.cpp` — add construction toggle button/shortcut
- `src/ui/sketch/SketchModePanel.cpp` — add construction toggle if not present

**Acceptance criteria**:

- New sketch entities are regular (solid lines) by default
- Pressing `X` toggles selected entities between construction/regular
- Construction entities still render as dashed lines
- LoopDetector still excludes construction geometry from region detection
- Old .onecad files with construction entities load correctly

---

### T0.2 — Comprehensive Keyboard Shortcuts

**Priority**: Critical UX
**Effort**: 1–2 days
**Dependencies**: None

**Description**: Currently mostly mouse-driven with only Ctrl+Z/Y basics. Users need fast access to all sketch tools and modeling operations via keyboard. The `ActionRegistry` singleton (`src/ui/palette/ActionRegistry.h`) already provides infrastructure for registering named actions with QAction pointers.

**Technical approach**:

- Define all shortcuts in MainWindow where actions are created. Use `QAction::setShortcut()` on each action registered via `ActionRegistry::registerAction()`.
- Sketch tools (context-dependent, active only in sketch mode):
  - `L` = Line, `R` = Rectangle, `C` = Circle, `A` = Arc, `E` = Ellipse
  - `T` = Trim, `M` = Mirror, `X` = Construction toggle
- Modeling tools (active in model mode):
  - `P` = Extrude (Pad), `V` = Revolve, `F` = Fillet, `H` = Chamfer, `S` = Shell
- General:
  - `Esc` = Cancel current tool, `Enter` = Confirm/commit
  - `Delete` / `Backspace` = Delete selected entity
  - `Ctrl+Z` = Undo, `Ctrl+Shift+Z` = Redo (already exists)
  - `Ctrl+K` = Command palette (already exists)
- View navigation:
  - `1` = Front, `2` = Back, `3` = Right, `4` = Left, `5` = Top, `6` = Bottom
  - `0` = Isometric, `.` = Fit all / zoom extents, `Z` = Zoom to selection
- Use `QShortcutContext::WidgetWithChildrenShortcut` to scope sketch shortcuts to sketch mode
- Consider a shortcut reference overlay (press `?` to show all shortcuts)

**Files**:

- `src/ui/mainwindow/MainWindow.cpp` — register all shortcuts via QAction
- `src/ui/palette/ActionRegistry.cpp` — ensure actions carry shortcut metadata
- `src/ui/toolbar/ContextToolbar.cpp` — display shortcut hints in tooltips

**Acceptance criteria**:

- All listed shortcuts work in their respective modes
- Sketch-mode shortcuts don't trigger in model mode and vice versa
- Tooltips on toolbar buttons show shortcut key
- No shortcut conflicts (test all combinations)
- `?` key shows shortcut reference (optional, nice-to-have)

---

### T0.3 — Fully-Constrained Sketch Indicator

**Priority**: High UX
**Effort**: ~1 day
**Dependencies**: None

**Description**: When a sketch reaches DOF=0 (fully constrained), there's no visual feedback. This is a critical UX cue — users need to know when their sketch is locked down before extruding. Every CAD tool (Fusion 360, SolidWorks, FreeCAD) shows this clearly with color changes.

**Technical approach**:

- The `ConstraintSolver` already computes total DOF after each solve. Expose a `bool isFullyConstrained()` on the `Sketch` class (check DOF == 0 after solve).
- In `SketchRenderer`, change entity colors when DOF == 0:
  - Under-constrained (DOF > 0): current blue/white color
  - Fully constrained (DOF == 0): green tint on all non-construction entities
  - Over-constrained (solver conflict): red tint
- In status bar (already has DOF label area), show:
  - "DOF: 3" (normal), "Fully Constrained" (green badge), "Over-constrained" (red badge)
- Per-entity DOF coloring (stretch goal): color individual entities based on which ones still have freedom (requires solver to report per-entity DOF — PlaneGCS supports this)

**Files**:

- `src/core/sketch/Sketch.h/cpp` — add `isFullyConstrained()`, `isOverConstrained()` methods
- `src/core/sketch/SketchRenderer.cpp/h` — conditional color per constraint state
- `src/ui/mainwindow/MainWindow.cpp` — update status bar label with DOF state

**Acceptance criteria**:

- Status bar shows current DOF count during sketch editing
- Entities turn green when sketch is fully constrained (DOF=0)
- Entities turn red when over-constrained (solver reports conflict)
- Color updates immediately after adding/removing constraints
- Works with all entity types (line, arc, circle, ellipse)

---

### T0.4 — Complete Sketch-on-Face

**Priority**: High
**Effort**: 2–3 days
**Dependencies**: None

**Description**: Sketching on body faces partially works but is unreliable. The `Document::getSketchPlaneForFace()` method exists and `Viewport` has a plane selection mode, but the flow from face click → sketch plane creation → sketch entry needs debugging and completion. This is essential for multi-feature parts (e.g., extruding a pocket on the top face of a previous extrude).

**Technical approach**:

- Debug `Document::getSketchPlaneForFace()` — verify it correctly extracts the face's underlying surface (planar faces only for now), computes origin, xAxis, yAxis, normal
- In Viewport plane selection mode: after user clicks a face, construct `SketchPlane` from that face's geometry. Use OCCT `BRep_Tool::Surface()` → cast to `Geom_Plane` → extract origin + axes
- Handle non-planar faces gracefully: show tooltip "Only planar faces can host sketches"
- Ensure correct normal orientation (outward from body) and consistent xAxis/yAxis selection
- After sketch creation, project the host face boundary edges into the sketch as locked reference entities (via `Document::projectHostFaceBoundaries()` which already exists)
- Test with: top face of extrude, side face of extrude, angled face (draft extrude), cylindrical face (should reject)

**Files**:

- `src/app/document/Document.cpp` — `getSketchPlaneForFace()`, `projectHostFaceBoundaries()`
- `src/ui/viewport/Viewport.cpp` — plane selection mode handler
- `src/ui/viewport/ViewportSketchInteraction.cpp` — sketch creation from face
- `src/core/sketch/Sketch.h` — `SketchPlane` construction from face geometry

**Acceptance criteria**:

- Click any planar body face → "New Sketch" creates sketch on that face
- Sketch plane origin is at face center, axes align with face edges
- Face boundary edges appear as locked reference geometry in the sketch
- Non-planar faces (cylindrical, spherical) show rejection tooltip
- Sketches created on faces work correctly with Extrude (boolean Add/Cut)
- Normal direction ensures extrude goes "outward" by default

---

## Tier 1 — Core Modeling Completeness

### T1.1 — Linear Pattern UI Tool

**Priority**: High
**Effort**: ~4 days
**Dependencies**: None (parametric backend already exists in `RegenerationEngine`)

**Description**: `LinearPattern` operation exists in `OperationRecord.h` with `LinearPatternParams` (sourceBodyId, direction, spacing, count, fuseResult) and is executed in `RegenerationEngine`. But there's no interactive UI tool — users can't access this feature. Need a `LinearPatternTool` following the `ModelingTool` pattern.

**Technical approach**:

- Create `LinearPatternTool` extending `ModelingTool` (see `src/ui/tools/ModelingTool.h`):
  1. **Activation**: User selects a body → clicks "Linear Pattern" button or presses shortcut
  2. **Direction selection**: Click an edge to define direction (extract edge tangent vector), or click a face to use its normal. Also offer X/Y/Z axis defaults.
  3. **Parameter input**: Drag to set spacing interactively, or type in PropertyInspector. Input count via spinner.
  4. **Preview**: Use `RegenerationEngine::previewFrom()` to show ghost copies during drag
  5. **Confirm**: Enter key or double-click creates `AddOperationCommand` with `LinearPatternParams`
- Register tool in `ModelingToolManager` and add button to ContextToolbar (Model mode)
- Add indicator showing direction arrow + instance positions

**Files**:

- New: `src/ui/tools/LinearPatternTool.h`, `src/ui/tools/LinearPatternTool.cpp`
- `src/ui/tools/ModelingToolManager.h/cpp` — register new tool
- `src/ui/toolbar/ContextToolbar.cpp` — add button (Model context)
- `src/ui/mainwindow/MainWindow.cpp` — wire action + shortcut

**Acceptance criteria**:

- Select body → activate tool → click edge for direction → drag/type spacing → set count → Enter
- Preview shows translucent copies at correct positions during editing
- Resulting pattern fuses into single body when fuseResult=true
- Undo removes entire pattern operation
- Works with edge direction + manual XYZ axis selection
- Count > 1 required (validate)

---

### T1.2 — Circular Pattern UI Tool

**Priority**: High
**Effort**: ~4 days
**Dependencies**: None (backend exists)

**Description**: `CircularPattern` operation exists with `CircularPatternParams` (sourceBodyId, axis point+direction, angle, count, fuseResult). Needs interactive UI tool.

**Technical approach**:

- Create `CircularPatternTool` extending `ModelingTool`:
  1. **Activation**: Select body → "Circular Pattern" button
  2. **Axis selection**: Click an edge (uses edge line as axis) or a face (uses face normal through centroid as axis). Default to Z-axis if nothing selected.
  3. **Parameter input**: Drag to set total angle, spinner for count
  4. **Preview**: Ghost instances around the axis. Show axis line indicator.
  5. **Confirm**: Creates `AddOperationCommand` with `CircularPatternParams`
- Indicator: axis line + arc showing total angle + numbered instance markers

**Files**:

- New: `src/ui/tools/CircularPatternTool.h`, `src/ui/tools/CircularPatternTool.cpp`
- `src/ui/tools/ModelingToolManager.h/cpp` — register
- `src/ui/toolbar/ContextToolbar.cpp` — add button
- `src/ui/mainwindow/MainWindow.cpp` — wire action

**Acceptance criteria**:

- Select body → activate → pick axis → set count + angle → confirm
- Preview shows instances correctly distributed around axis
- 360° with count=6 gives 60° spacing (includes original)
- Partial angles work (e.g., 180° with count=4 = 3 copies over half-circle + original)
- Fuse result works correctly
- Undo removes entire pattern

---

### T1.3 — Loft UI Tool

**Priority**: High
**Effort**: ~5 days
**Dependencies**: T0.4 (sketch-on-face needed for multi-plane profiles)

**Description**: `Loft` operation exists with `LoftParams` (profile sketch/region IDs, solid/ruled flags, boolean mode). Users need to select multiple sketch profiles on different planes and create a smooth solid between them.

**Technical approach**:

- Create `LoftTool` extending `ModelingTool`:
  1. **Profile selection**: Multi-click mode — user clicks sketch regions or body faces in order. Each click adds to profile list. Visual numbering on each profile.
  2. **Ordering**: Profiles are lofted in click order. Show numbered badges (1, 2, 3...) and connecting guide lines between profile centers.
  3. **Options**: Toggle solid/surface, ruled/smooth (in PropertyInspector sidebar or overlay)
  4. **Preview**: After 2+ profiles selected, show preview mesh of lofted shape
  5. **Confirm**: Enter creates operation
- Challenge: profiles must be on different planes. Validate this and show error if coplanar.
- Use `RegenerationEngine` loft execution which calls OCCT `BRepOffsetAPI_ThruSections`

**Files**:

- New: `src/ui/tools/LoftTool.h`, `src/ui/tools/LoftTool.cpp`
- `src/ui/tools/ModelingToolManager.h/cpp` — register
- `src/ui/toolbar/ContextToolbar.cpp` — add button
- `src/ui/mainwindow/MainWindow.cpp` — wire action

**Acceptance criteria**:

- Select 2+ sketch regions on different planes → preview → confirm
- Smooth interpolation between profiles (default)
- Ruled mode produces straight-ruled surface
- Solid result (closed profiles) and surface result (open profiles) both work
- Undo removes loft operation
- Error message if fewer than 2 profiles selected

---

### T1.4 — Sweep UI Tool

**Priority**: High
**Effort**: ~4 days
**Dependencies**: None

**Description**: `Sweep` operation exists with `SweepParams` (profile sketch/region, path sketch/edge). Users need to select a profile and a path curve to sweep along.

**Technical approach**:

- Create `SweepTool` extending `ModelingTool`:
  1. **Phase 1 — Profile selection**: Click a sketch region (highlight available regions)
  2. **Phase 2 — Path selection**: Click a sketch wire or body edge chain (highlight available paths). Path must be a continuous wire.
  3. **Preview**: Show swept solid mesh
  4. **Confirm**: Enter creates operation
- Two-phase interaction like `RevolveTool` (which also has profile → axis selection)
- Path validation: must be a single continuous wire (no branches). Use `EdgeChainer` from `src/core/modeling/EdgeChainer` if path is body edges.

**Files**:

- New: `src/ui/tools/SweepTool.h`, `src/ui/tools/SweepTool.cpp`
- `src/ui/tools/ModelingToolManager.h/cpp` — register
- `src/ui/toolbar/ContextToolbar.cpp` — add button
- `src/ui/mainwindow/MainWindow.cpp` — wire action

**Acceptance criteria**:

- Select profile → select path → preview → confirm
- Works with sketch wire as path
- Works with body edge (chain) as path
- Profile stays normal to path tangent (default OCCT behavior)
- Error if path is not a single continuous wire
- Undo removes sweep operation

---

### T1.5 — Reference Geometry (Construction Planes & Axes)

**Priority**: High
**Effort**: 1–2 weeks
**Dependencies**: T0.4 (sketch-on-face, as reference planes become sketch targets)

**Description**: Users need construction planes and axes to create sketches at arbitrary positions/orientations and to define pattern/mirror axes. Currently only origin XY/XZ/YZ planes exist. This is essential for any non-trivial multi-feature part.

**Technical approach**:

- **Data model**: New entity type in Document. Define `ReferenceGeometry` struct:
  ```cpp
  enum class RefGeoType { Plane, Axis, Point };
  struct ReferenceGeometry {
      std::string id;       // UUID
      std::string name;     // "Plane 1"
      RefGeoType type;
      // Plane: origin + normal (derived from creation method)
      // Axis: origin + direction
      // Point: position
      Vec3d origin;
      Vec3d direction;      // normal (plane) or direction (axis)
      Vec3d secondaryDir;   // xAxis for plane
  };
  ```
- **Creation methods**:
  - **Offset Plane**: Select face/plane → input offset distance → new plane parallel at distance
  - **Mid-Plane**: Select two parallel faces → plane halfway between
  - **Plane through 3 points**: Select 3 vertices → plane through them
  - **Axis through edge**: Select linear edge → axis along it
  - **Axis through 2 points**: Select 2 vertices → axis between them
- **Rendering**: Semi-transparent rectangles for planes (sized to model bounding box). Lines with arrowheads for axes. Use alpha=0.2 with grid pattern.
- **Integration**: Reference planes become valid targets for "New Sketch" (same as face selection). Reference axes become valid targets for revolve/pattern axes.
- **Document storage**: Add `std::vector<ReferenceGeometry>` to Document. Serialize in document.json.
- **Commands**: AddReferenceGeometry, RemoveReferenceGeometry, UpdateReferenceGeometry

**Files**:

- New: `src/app/document/ReferenceGeometry.h`
- `src/app/document/Document.h/cpp` — add reference geometry storage + signals
- New: `src/app/commands/AddReferenceGeometryCommand.h/cpp`
- New: `src/render/ReferenceGeometryRenderer.h/cpp` — rendering
- `src/ui/toolbar/ContextToolbar.cpp` — add creation buttons
- `src/ui/navigator/ModelNavigator.cpp` — show reference geometry in tree
- `src/io/DocumentIO.cpp` — serialize/deserialize
- `src/ui/viewport/Viewport.cpp` — integrate plane picking for sketch creation

**Acceptance criteria**:

- Create offset plane from body face → shows as translucent rectangle
- Create sketch on reference plane → sketch appears at correct position/orientation
- Reference planes appear in ModelNavigator tree
- Can use reference axis for revolve/pattern operations
- Delete reference geometry → removes it + doesn't break dependent sketches (show warning)
- Save/load preserves reference geometry
- Undo/redo works for all reference geometry commands

---

### T1.6 — PropertyInspector Full Wiring

**Priority**: High
**Effort**: 2–3 days
**Dependencies**: None

**Description**: PropertyInspector is partially implemented — works for some operation types but not all 11. Users need to edit parameters for every operation type via the right panel. Currently selecting a Pattern/Loft/Sweep operation may show nothing or a placeholder.

**Technical approach**:

- For each `OperationType`, create a parameter editing widget:
  - **Extrude**: distance, draft angle, extrude mode dropdown, boolean mode dropdown
  - **Revolve**: angle, axis display, boolean mode
  - **Fillet/Chamfer**: radius/distance, edge list (read-only), chain toggle
  - **Shell**: thickness, open face list
  - **Boolean**: operation type dropdown, target/tool body selection
  - **LinearPattern**: direction display, spacing, count, fuse toggle
  - **CircularPattern**: axis display, angle, count, fuse toggle
  - **Loft**: profile list (read-only), solid/ruled toggles, boolean mode
  - **Sweep**: profile/path display, boolean mode
  - **MirrorBody**: plane display, fuse toggle
- Each widget should:
  - Show current values from `OperationRecord.params`
  - On value change, create `UpdateOperationParamsCommand` → regenerate
  - Support live preview (preview on change, commit on focus-out or Enter)

**Files**:

- `src/ui/inspector/PropertyInspector.h/cpp` — extend with all operation type widgets
- May need sub-widget files if PropertyInspector becomes too large

**Acceptance criteria**:

- Select any operation in HistoryPanel → PropertyInspector shows correct editing UI
- Changing a parameter → live preview updates viewport
- Enter/focus-out commits change as undoable command
- All 11 operation types have editing widgets
- Invalid values (negative radius, count < 2) show validation error

---

### T1.7 — Command Palette Completion

**Priority**: Medium
**Effort**: 2–3 days
**Dependencies**: T0.2 (shortcuts should appear in palette entries)

**Description**: Cmd+K palette exists (`src/ui/palette/`) with `ActionRegistry` and fuzzy search. But not all actions are registered, and some UI polish is needed (shortcut display, categorization, recent actions).

**Technical approach**:

- Audit all user-triggerable actions and ensure each is registered via `ActionRegistry::registerAction()` with proper id, displayName, and category
- Categories: "Sketch", "Modeling", "View", "File", "Edit"
- Show keyboard shortcut next to each action in the palette list (from QAction::shortcut())
- Add "recently used" section at top (track last 5 actions used from palette)
- Improve fuzzy matching: score by position of match, prefer starts of words

**Files**:

- `src/ui/palette/CommandPalette.h/cpp` — UI improvements
- `src/ui/palette/ActionRegistry.cpp` — ensure all actions registered
- `src/ui/mainwindow/MainWindow.cpp` — register missing actions

**Acceptance criteria**:

- Cmd+K opens palette, typing filters actions
- All sketch tools, modeling tools, view commands, file operations appear
- Shortcuts shown next to each action
- Selecting action triggers it immediately
- Esc closes palette
- Recently used actions appear at top

---

## Tier 2 — Sketch System Improvements

### T2.1 — Sketch Constraint UX Improvements

**Priority**: High (user pain point)
**Effort**: ~1 week
**Dependencies**: T0.3 (DOF indicator)

**Description**: Sketch constraint workflow is clunky compared to Fusion 360/SolidWorks. Key issues: dimension editing requires double-click (should be immediate), no conflict feedback, no over-constraint prevention, overlapping dimension labels.

**Technical approach**:

- **Immediate dimension input**: When a dimensional constraint (Distance, Radius, Angle) is created, immediately open the inline editor at the constraint's position. Don't require double-click. The `DimensionEditor` (`src/ui/sketch/DimensionEditor.cpp`) already supports inline QLineEdit — trigger it on creation, not just on double-click.
- **Conflict highlighting**: After solver runs, if constraints are conflicting (solver returns Conflicting/Failed status), color the involved constraints red. `ConstraintSolver` should return which constraints are in conflict (PlaneGCS provides this via `getConflicting()`).
- **Over-constraint warning**: Before applying a new constraint, run a trial solve. If adding it would cause DOF < 0 or solver failure, show a warning tooltip: "This constraint would over-constrain the sketch. Apply anyway?" with Yes/Cancel.
- **Smart dimension placement**: When placing dimension labels, check for overlap with existing labels. Offset overlapping dimensions vertically or horizontally. Maintain a label placement manager in `SketchRenderer`.
- **Constraint drag**: Allow dragging dimension labels to reposition them (cosmetic only, doesn't change the constraint value).

**Files**:

- `src/ui/sketch/DimensionEditor.cpp/h` — auto-open on constraint creation
- `src/core/sketch/ConstraintSolver.cpp/h` — expose conflict info from PlaneGCS
- `src/core/sketch/SketchRenderer.cpp/h` — conflict coloring, label placement
- `src/core/sketch/Sketch.cpp` — trial solve before constraint addition

**Acceptance criteria**:

- Adding a distance constraint → dimension editor opens immediately for value input
- Conflicting constraints render in red with tooltip explaining the conflict
- Adding an over-constraining constraint shows warning before application
- Dimension labels don't overlap with each other
- Dimension labels are draggable for repositioning

---

### T2.2 — Spline Entity

**Priority**: Medium
**Effort**: 1–2 weeks
**Dependencies**: None

**Description**: B-spline curves for organic shapes, smooth transitions, and complex profiles. Required for: bottle shapes, ergonomic handles, aerodynamic profiles. PlaneGCS (vendored in `third_party/planegcs/`) supports B-spline constraints.

**Technical approach**:

- **Entity class**: New `SketchSpline` extending `SketchEntity`. Storage: vector of control points (SketchPoint references), degree (default 3 = cubic), optional knot vector.
- **PlaneGCS integration**: Map spline to PlaneGCS B-spline representation. Each control point becomes a GCS point parameter. Spline-specific constraints: point-on-spline, tangent-at-endpoint.
- **Sketch tool**: `SplineTool` — click to place control points, double-click or Enter to finish. Show control polygon while editing. Optionally support "through-points" mode (interpolating spline, not B-spline control polygon).
- **Rendering**: Tessellate spline at sufficient resolution (adaptive based on curvature). Show control polygon as dashed lines in edit mode. Show control points as smaller squares.
- **OCCT conversion**: For extrusion/face-building, convert to `Geom2d_BSplineCurve`. LoopDetector needs to handle spline segments for region detection.
- **Serialization**: Store control points, degree, knot vector in sketch JSON.

**Files**:

- New: `src/core/sketch/SketchSpline.h`, `src/core/sketch/SketchSpline.cpp`
- New: `src/core/sketch/tools/SplineTool.h`, `src/core/sketch/tools/SplineTool.cpp`
- `src/core/sketch/Sketch.h/cpp` — add spline entity management
- `src/core/sketch/ConstraintSolver.cpp` — PlaneGCS B-spline constraint mapping
- `src/core/sketch/SketchRenderer.cpp` — spline rendering + control polygon
- `src/core/loop/LoopDetector.cpp` — handle spline edges
- `src/io/SketchIO.cpp` — serialize/deserialize spline entities

**Acceptance criteria**:

- Create spline by clicking control points → smooth curve appears
- Moving control points updates spline in real-time via solver
- Spline + line segments can form closed loops → regions detected
- Extruding a spline-containing region produces correct 3D shape
- Constraints work: coincident at endpoints, tangent to lines/arcs
- Save/load preserves spline geometry
- Minimum 2 control points required (error if less)

---

### T2.3 — Sketch Offset Tool

**Priority**: Medium
**Effort**: 3–5 days
**Dependencies**: None

**Description**: Offset selected edges or wires by a distance, creating parallel geometry. Extremely common in every CAD tool for creating walls, pockets, and padding. Example: offset a rectangle inward by 2mm to create a slot profile.

**Technical approach**:

- **Selection**: User selects one or more connected sketch edges → tool highlights them as a wire
- **Offset direction**: Drag perpendicular to the wire to set offset distance. Show preview of offset geometry. Distance can be positive (one side) or negative (other side). Visual indicator shows which side.
- **Geometry creation**: Use OCCT `BRepOffsetAPI_MakeOffset2D` on the 2D wire in sketch plane:
  1. Convert selected sketch entities to an OCCT `TopoDS_Wire` in sketch 2D coords
  2. Apply offset
  3. Extract result edges → convert back to sketch entities (lines, arcs)
  4. Add coincident constraints to connect offset entities
- **Corner handling**: OCCT handles arc corners (fillet) by default. Options: arc (default), sharp (extend+trim).
- **Multiple offsets**: Each invocation creates new independent entities (not linked parametrically to source).

**Files**:

- New: `src/core/sketch/tools/OffsetTool.h`, `src/core/sketch/tools/OffsetTool.cpp`
- `src/core/sketch/tools/SketchToolManager.h/cpp` — register tool
- `src/ui/toolbar/ContextToolbar.cpp` — add button (Sketch context)
- `src/core/sketch/Sketch.cpp` — helper to build wire from entity selection

**Acceptance criteria**:

- Select edges → activate offset → drag to set distance → creates parallel geometry
- Offset of line = parallel line at distance
- Offset of arc = concentric arc at distance
- Closed wire offset creates correctly closed offset wire
- Corner transitions are smooth arcs
- Negative offset (inward) works for closed wires
- Resulting entities are properly connected with coincident constraints

---

### T2.4 — Sketch Fillet/Chamfer

**Priority**: Medium
**Effort**: 3–4 days
**Dependencies**: None

**Description**: Round (fillet) or bevel (chamfer) sketch corners — the intersection vertex between two lines/arcs. This is sketch-level, not 3D. Used to add rounded corners to profiles before extrusion.

**Technical approach**:

- **Selection**: Click near a vertex where two entities meet (auto-detect the two adjacent entities)
- **Fillet mode**: Replace the vertex with an arc tangent to both entities. Radius set by drag distance from vertex.
  1. Compute tangent points on both entities at specified radius
  2. Trim both entities to tangent points
  3. Insert new arc between tangent points
  4. Add tangent constraints at both connections
- **Chamfer mode**: Replace vertex with a straight line (bevel).
  1. Compute cut points at specified distance from vertex on each entity
  2. Trim both entities to cut points
  3. Insert new line between cut points
- **Preview**: Show ghost arc/line during drag
- **Constraint preservation**: Remove coincident constraint at vertex, add tangent constraints to arc (fillet) or coincident to chamfer endpoints
- **Mode toggle**: Use `Tab` to switch fillet ↔ chamfer (like 3D FilletChamferTool)

**Files**:

- New: `src/core/sketch/tools/SketchFilletTool.h`, `src/core/sketch/tools/SketchFilletTool.cpp`
- `src/core/sketch/tools/SketchToolManager.h/cpp` — register tool
- `src/core/sketch/Sketch.cpp` — helper to trim entity at parameter, insert entity
- `src/ui/toolbar/ContextToolbar.cpp` — add button

**Acceptance criteria**:

- Click corner vertex → drag → fillet arc appears → release to commit
- Tab switches to chamfer mode (straight line instead of arc)
- Both meeting entities are trimmed correctly
- Tangent constraints added automatically for fillets
- Works for: line-line, line-arc, arc-arc intersections
- Radius/distance shown during drag
- Undo reverses all changes (restore original entities + constraints)

---

### T2.5 — Polygon & Slot Primitives

**Priority**: Low-Medium
**Effort**: 2–3 days
**Dependencies**: None

**Description**: Regular polygon (N-sided) and slot/obround shape tools. Common shortcuts that save time vs drawing individual lines. Polygon needed for hex bolt holes, nut profiles. Slot needed for machine screw slots, mounting brackets.

**Technical approach**:

- **PolygonTool**:
  1. Click center point
  2. Drag to set radius + rotation. Show vertex count in property panel (default 6).
  3. Creates N lines + N points with equal-length constraints and coincident constraints
  4. Option: inscribed (vertices on circle) or circumscribed (edges tangent to circle)
  5. Press up/down arrow to change vertex count during creation
- **SlotTool**:
  1. Click first center, click second center (defines slot axis)
  2. Drag to set width (radius of end arcs)
  3. Creates 2 lines + 2 semicircular arcs + tangent/coincident constraints
  4. Horizontal/Vertical constraints auto-applied if aligned within tolerance

**Files**:

- New: `src/core/sketch/tools/PolygonTool.h`, `src/core/sketch/tools/PolygonTool.cpp`
- New: `src/core/sketch/tools/SlotTool.h`, `src/core/sketch/tools/SlotTool.cpp`
- `src/core/sketch/tools/SketchToolManager.h/cpp` — register tools
- `src/ui/toolbar/ContextToolbar.cpp` — add buttons

**Acceptance criteria**:

- Polygon: click center → drag radius → creates regular N-gon with constraints
- Up/down arrows change vertex count (3–64 range)
- Both inscribed and circumscribed modes work
- Slot: click two centers → drag width → creates slot profile
- All generated entities have proper constraints (equal, coincident, tangent)
- Construction geometry toggle works for generated entities

---

### T2.6 — Symmetric Constraint

**Priority**: Medium
**Effort**: 2–3 days
**Dependencies**: None

**Description**: `ConstraintType::Symmetric` is declared in `SketchTypes.h:60` but not implemented. Constrains two points/entities to be symmetric about a line. Essential for symmetric part profiles — used constantly in real-world sketch workflows.

**Technical approach**:

- **PlaneGCS mapping**: PlaneGCS supports `addConstraintSymmetric()` — takes two points and a line. Map to: `GCS::addConstraintSymmetric(p1, p2, lineP1, lineP2)`.
- **Selection**: User selects two points + one line (axis of symmetry). Order: point A, point B, then symmetry line. Or: select two entities (lines/arcs) + symmetry line → applies symmetric to matching point pairs.
- **Constraint class**: Create `SymmetricConstraint` storing: entityId1, entityId2, axisEntityId. The axis must be a line entity.
- **Rendering**: Show symmetric markers — small triangles or arrows pointing at the two points with a dashed line connecting them through the axis.
- **Solver integration**: Add case to `ConstraintSolver::applyConstraint()` for Symmetric type → call PlaneGCS `addConstraintSymmetric`.
- **Serialization**: Add symmetric constraint to SketchIO (type name "Symmetric", stores 3 entity IDs).

**Files**:

- `src/core/sketch/SketchConstraint.h/cpp` — add SymmetricConstraint data
- `src/core/sketch/ConstraintSolver.cpp` — PlaneGCS mapping
- `src/core/sketch/SketchRenderer.cpp` — visual representation
- `src/io/SketchIO.cpp` — serialization
- `src/ui/sketch/SketchModePanel.cpp` — add button with applicability filter

**Acceptance criteria**:

- Select point A, point B, axis line → constraint created
- Both points move symmetrically when one is dragged
- Works with: point-point symmetry, line-line symmetry (endpoint pairs)
- Renders visual indicator showing symmetry relationship
- Save/load preserves constraint
- Solver correctly reduces DOF (removes 2 DOF per point pair)

---

## Tier 3 — Export & 3D Printing Workflow

### T3.1 — 3MF Export

**Priority**: High (3D printing essential)
**Effort**: 3–5 days
**Dependencies**: None

**Description**: 3MF (3D Manufacturing Format) is the modern replacement for STL, supported by all major slicers (PrusaSlicer, Cura, Bambu Studio). Advantages over STL: smaller files, color/material support, metadata, units, no manifold issues. Essential for any 3D-printing-oriented CAD tool.

**Technical approach**:

- **Library choice**: Use lib3mf (open-source, C++, MIT license, actively maintained by the 3MF Consortium). Alternative: manual XML+ZIP generation (simpler but no validation).
- **Implementation with lib3mf**:
  1. Add lib3mf as dependency (CMake `FetchContent` or vendored)
  2. Create `ThreeMFExporter` class
  3. For each visible body:
     - Tessellate via `TessellationCache` at export quality settings
     - Create lib3mf mesh resource with triangles + vertices
     - Set body name as mesh name
     - Apply per-face colors if available (from STEP imports)
  4. Write to .3mf file (ZIP archive containing XML)
- **Export dialog**: Extend `MeshExportDialog` to include 3MF format option. Show quality presets (Draft/Normal/Fine/Custom). For 3MF-specific: option to include color data.
- **Units**: 3MF uses millimeters internally — match OneCAD's internal units.

**Files**:

- New: `src/io/mesh/ThreeMFExporter.h`, `src/io/mesh/ThreeMFExporter.cpp`
- `CMakeLists.txt` — add lib3mf dependency
- `src/io/CMakeLists.txt` — link lib3mf
- `src/ui/dialogs/MeshExportDialog.cpp/h` — add 3MF format option
- `src/ui/mainwindow/MainWindow.cpp` — wire "Export 3MF" action

**Acceptance criteria**:

- File → Export → 3MF creates valid .3mf file
- File opens correctly in PrusaSlicer, Cura, and Bambu Studio
- Multi-body export includes all visible bodies as separate mesh objects
- Face colors preserved (if present)
- Export quality settings affect mesh resolution
- File size is smaller than equivalent binary STL

---

### T3.2 — Mesh Export Quality Settings

**Priority**: Medium
**Effort**: 1–2 days
**Dependencies**: None

**Description**: Users need to control mesh quality during STL/OBJ/3MF export. Current tessellation uses internal adaptive settings, but there's no user-facing quality control. For 3D printing, fine control over mesh resolution matters (smooth curves vs file size).

**Technical approach**:

- Expose two OCCT tessellation parameters in the export dialog:
  - **Angular deflection** (degrees): Controls curve approximation. Lower = smoother arcs. Default: 12° (Draft), 6° (Normal), 2° (Fine).
  - **Linear deflection** (mm): Maximum deviation from true surface. Default: 0.5 (Draft), 0.1 (Normal), 0.02 (Fine).
- **Quality presets**: Draft (fast, coarse), Normal (balanced), Fine (smooth, large), Custom (manual sliders)
- **Preview**: Show triangle count estimate before export. Optionally show wireframe preview of tessellation quality.
- The `TessellationCache` already accepts these parameters — just need to pass user-selected values instead of defaults.

**Files**:

- `src/ui/dialogs/MeshExportDialog.cpp/h` — add quality preset dropdown + custom sliders
- `src/render/tessellation/TessellationCache.h/cpp` — expose `tessellate(shape, angularDef, linearDef)` overload if not present
- `src/io/mesh/StlExporter.cpp` (or wherever STL export lives) — accept quality params

**Acceptance criteria**:

- Export dialog shows Draft/Normal/Fine/Custom quality options
- Draft export is visibly coarser than Fine export (inspect in mesh viewer)
- Triangle count preview updates when changing quality
- Custom mode shows angular/linear deflection sliders with live triangle count
- Settings persist between exports (QSettings)

---

### T3.3 — DXF Export

**Priority**: Medium
**Effort**: ~1 week
**Dependencies**: None

**Description**: DXF (Drawing Exchange Format) export for 2D profiles. Essential for laser cutting, CNC routing, and waterjet cutting workflows. Users need to export sketch profiles or projected 3D views as 2D vector graphics.

**Technical approach**:

- **Scope 1 — Sketch export** (simpler, do first): Export current sketch entities directly as DXF entities (LINE, ARC, CIRCLE, ELLIPSE, SPLINE). No 3D projection needed.
- **Scope 2 — 3D projection** (harder, defer): Use OCCT HLR (Hidden Line Removal) to project 3D body to 2D → export resulting edges as DXF.
- **DXF writer**: Use dxflib (lightweight C++ DXF library) or write minimal DXF output manually (DXF format is well-documented ASCII).
- **Sketch → DXF mapping**:
  - SketchLine → DXF LINE entity
  - SketchArc → DXF ARC entity
  - SketchCircle → DXF CIRCLE entity
  - SketchEllipse → DXF ELLIPSE entity
  - Construction geometry: optionally export to separate layer "CONSTRUCTION"
- **Layer support**: Regular geometry on layer "0", construction on "CONSTRUCTION", dimensions on "DIMENSIONS"

**Files**:

- New: `src/io/dxf/DxfExporter.h`, `src/io/dxf/DxfExporter.cpp`
- `src/io/CMakeLists.txt` — add dxf directory + optional dxflib dependency
- `src/ui/mainwindow/MainWindow.cpp` — wire "Export DXF" action
- New: `src/ui/dialogs/DxfExportDialog.h/cpp` — export options (which sketch, layer settings)

**Acceptance criteria**:

- Export active sketch as DXF → opens correctly in LibreCAD, AutoCAD, Inkscape
- Lines, arcs, circles export with correct geometry
- Construction geometry on separate layer (optional include/exclude)
- Units are millimeters
- Closed profiles can be used for laser cutting (verify in a CAM tool)

---

### T3.4 — SVG Export

**Priority**: Low-Medium
**Effort**: 2–3 days
**Dependencies**: T3.3 (shares sketch-to-2D conversion logic)

**Description**: SVG (Scalable Vector Graphics) export for sketch profiles. Useful for laser cutting, vinyl cutting, web documentation, and design interchange. SVG is simpler than DXF and universally viewable in browsers.

**Technical approach**:

- Convert sketch entities to SVG path elements:
  - Line → `<line>` or `<path d="M x1 y1 L x2 y2">`
  - Arc → SVG arc command `A rx ry rotation large-arc sweep x y`
  - Circle → `<circle>`
  - Ellipse → `<ellipse>`
- Use sketch coordinate system directly (Y-flip for SVG's top-left origin)
- Set viewBox based on sketch bounding box with margin
- Styling: stroke-only (no fill), configurable stroke width
- Optional: include dimension annotations as SVG text

**Files**:

- New: `src/io/svg/SvgExporter.h`, `src/io/svg/SvgExporter.cpp`
- `src/ui/mainwindow/MainWindow.cpp` — wire action

**Acceptance criteria**:

- Export sketch as SVG → renders correctly in browser and Inkscape
- Geometry accuracy: lines, arcs, circles, ellipses all correct
- Viewbox fits content with appropriate margin
- File is valid SVG (W3C validator)

---

### T3.5 — IGES Import/Export

**Priority**: Low
**Effort**: 2–3 days
**Dependencies**: None

**Description**: IGES (Initial Graphics Exchange Specification) is a legacy but still common interchange format, especially in older manufacturing workflows and some 3D printing services that accept IGES alongside STEP.

**Technical approach**:

- OCCT has built-in `IGESControl_Reader` and `IGESControl_Writer` — same pattern as STEP import/export.
- **Import**: `IGESControl_Reader` → iterate shapes → `Document::addBody()` for each solid. Handle colors via `IGESData_ReadWriteModule` if available.
- **Export**: Iterate visible bodies → `IGESControl_Writer` → add shapes → write file.
- Mirror the `StepImporter`/`StepExporter` class structure.

**Files**:

- New: `src/io/iges/IgesImporter.h`, `src/io/iges/IgesImporter.cpp`
- New: `src/io/iges/IgesExporter.h`, `src/io/iges/IgesExporter.cpp`
- `src/io/CMakeLists.txt` — add iges directory
- `src/ui/mainwindow/MainWindow.cpp` — wire import/export actions

**Acceptance criteria**:

- Import .iges file → bodies appear in viewport with correct geometry
- Export bodies as .iges → opens correctly in FreeCAD, Fusion 360
- Multi-body import creates separate bodies
- Color preservation (best effort)

---

## Tier 4 — Performance & Visual Quality

### T4.1 — Partial Regeneration Caching

**Priority**: High (user pain point: performance)
**Effort**: ~1 week
**Dependencies**: None

**Description**: Currently `RegenerationEngine::regenerateAll()` replays every operation from scratch. For a model with 20 operations, editing op #18 replays all 20. Should only replay from the changed operation downstream. `DependencyGraph` already tracks dependencies — use it to skip unchanged subtrees.

**Technical approach**:

- **Cache structure**: Store `{opId → (paramsHash, resultShape)}` map. After regeneration, cache each operation's input hash and output TopoDS_Shape.
- **Dirty detection**: When `regenerateFrom(opId)` is called, mark that op and all downstream dependents (via `DependencyGraph::dependentsOf()`) as dirty. Skip non-dirty ops — use cached shapes.
- **Hash computation**: Hash the operation's `OperationParams` + all upstream body shapes (or their hashes). If hash matches cache, skip execution.
- **Cache invalidation**: Clear cache entry when operation is modified, added, removed, or reordered. Clear all downstream entries too.
- **Memory management**: Cache shapes consume memory. Limit cache size to N most recent operations, or total mesh budget. Evict least-recently-used.
- **Correctness**: ElementMap must still be correctly maintained even when skipping operations. Verify that cached shapes have correct ElementMap entries.

**Files**:

- `src/app/history/RegenerationEngine.h/cpp` — add cache map, dirty tracking, skip logic
- `src/app/history/DependencyGraph.h/cpp` — add `dependentsOf()` if not present
- New: `src/app/history/RegenerationCache.h` — cache data structure

**Acceptance criteria**:

- Editing operation #18 of 20 only re-executes ops 18-20 (verify via logging/timing)
- Cached operations produce identical results to full regeneration
- Adding/removing operations correctly invalidates downstream cache
- 10x+ speedup measured on 15+ operation model (benchmark before/after)
- ElementMap consistency: topological names survive cache-hit operations
- Memory usage stays bounded (cache eviction works)

---

### T4.2 — Background Constraint Solving

**Priority**: Medium
**Effort**: 3–5 days
**Dependencies**: None

**Description**: For large sketches (100+ entities), the PlaneGCS solver blocks the UI thread during dragging. Users experience lag when dragging points in complex sketches. Move solving to a background thread with UI feedback.

**Technical approach**:

- Use `QtConcurrent::run()` to offload `ConstraintSolver::solve()` to a worker thread
- During solve: UI shows "solving..." indicator, entities remain at their pre-solve positions
- On completion: signal back to main thread → update entity positions → re-render
- **Debouncing**: During mouse drag, debounce solve requests. Don't start a new solve until the previous one completes. Queue the latest mouse position.
- **Cancellation**: If user moves mouse again before solve completes, cancel current solve (PlaneGCS supports `GCS::maxIterReached` check) and start new one with latest position.
- **Threshold**: Only use background solving for sketches with > 50 entities. Below that, synchronous solving is fast enough.
- **Thread safety**: Sketch entities must not be mutated while solver reads them. Use a snapshot/copy of entity positions for the solver, then apply results back on main thread.

**Files**:

- `src/core/sketch/ConstraintSolver.h/cpp` — async solve API, cancellation
- `src/core/sketch/Sketch.cpp` — call async solve, handle completion callback
- `src/ui/viewport/ViewportSketchInteraction.cpp` — debounce drag events
- `src/ui/mainwindow/MainWindow.cpp` — status bar "solving..." indicator

**Acceptance criteria**:

- Dragging a point in a 100+ entity sketch doesn't block the UI
- Entities update smoothly after solve completes (no visible jump)
- Small sketches (<50 entities) still solve synchronously (no overhead)
- Cancellation works: rapid mouse movements don't queue up many solves
- No data races: thread sanitizer clean

---

### T4.3 — Screen-Space Ambient Occlusion (SSAO)

**Priority**: Medium (user pain point: visual quality)
**Effort**: ~1 week
**Dependencies**: None

**Description**: Add SSAO for depth perception and visual quality. SSAO darkens creases and corners, making 3D shapes much more readable. Currently the renderer uses basic Phong-like shading which looks flat on complex models.

**Technical approach**:

- **Render pipeline change**: Need a two-pass approach:
  1. **Geometry pass**: Render scene to G-buffer (position, normal, depth textures via FBO)
  2. **SSAO pass**: Sample hemisphere around each fragment, check depth occlusion, produce AO texture
  3. **Composition pass**: Multiply AO texture with the original color output
- **Algorithm**: Use SSAO with random sampling kernel (32-64 samples) + noise texture for randomization + bilateral blur to remove noise artifacts.
- **Performance**: Use half-resolution AO for performance. Toggle on/off in settings.
- **OpenGL 4.1**: All required features available (FBOs, MRT, texture sampling in fragment shader).
- **Integration**: Add as optional render pass in `BodyRenderer`. Toggle via View menu or RenderDebugPanel.

**Files**:

- `src/render/BodyRenderer.h/cpp` — add SSAO pipeline, FBO management
- New: `src/render/shaders/ssao.vert`, `src/render/shaders/ssao.frag`
- New: `src/render/shaders/ssao_blur.frag`
- `src/render/shaders/` — modify main fragment shader for G-buffer output
- `src/ui/viewport/RenderDebugPanel.cpp` — SSAO toggle + radius/samples controls

**Acceptance criteria**:

- Creases and internal corners show soft shadows
- Flat surfaces remain unaffected
- Toggle on/off works without artifacts
- Performance: <2ms overhead at 1080p on Apple M1/M2
- No visual artifacts at model edges (bilateral blur handles this)
- Works with both perspective and orthographic projection

---

### T4.4 — Edge Anti-Aliasing

**Priority**: Medium
**Effort**: 3–4 days
**Dependencies**: None

**Description**: Body edges and sketch lines have visible aliasing (jagged edges). Add anti-aliasing for smoother rendering. Important for CAD where edges are prominent visual elements.

**Technical approach**:

- **Option 1: MSAA** (Multi-Sample Anti-Aliasing): Set `QSurfaceFormat::setSamples(4)` on the OpenGL context. Simple, hardware-accelerated, no shader changes. May impact performance on complex scenes. Already supported by QOpenGLWidget.
- **Option 2: FXAA** (Fast Approximate Anti-Aliasing): Post-processing shader. No multi-sample overhead. Lower quality but more consistent performance. Single fragment shader pass.
- **Recommendation**: Start with MSAA 4x (simplest, best quality). Add FXAA as fallback for performance-constrained systems.
- **Line rendering**: For sketch lines and body edges, also consider increasing line width slightly (1.5px instead of 1px) and using GL_LINE_SMOOTH (deprecated but effective on macOS).

**Files**:

- `src/ui/viewport/Viewport.cpp` — set QSurfaceFormat samples
- `src/render/BodyRenderer.cpp` — enable GL_LINE_SMOOTH if needed
- New (if FXAA): `src/render/shaders/fxaa.frag`

**Acceptance criteria**:

- Body edges and sketch lines appear smooth (no jaggies)
- FPS impact < 10% on typical models
- Works on macOS Apple Silicon
- Toggle available in settings/debug panel

---

### T4.5 — Instanced Rendering for Patterns

**Priority**: Low
**Effort**: 3–4 days
**Dependencies**: T1.1 or T1.2 (pattern tools must exist first)

**Description**: Pattern operations (linear, circular) can create many copies of a body. Currently each copy is tessellated and stored separately, duplicating geometry. GPU instancing renders multiple copies from a single mesh buffer with different transforms, reducing memory and improving render performance.

**Technical approach**:

- Detect pattern operations in render pipeline. For pattern result bodies, store only one mesh + N transform matrices.
- Use OpenGL `glDrawElementsInstanced()` with instance attribute buffer containing 4x4 transform matrices.
- Modify `SceneMeshStore` to support instanced entries: `{meshId, std::vector<glm::mat4> transforms}`.
- Only apply instancing for pattern ops where all instances have identical geometry (same source, fuse=false). If fused, fall back to regular rendering.

**Files**:

- `src/render/BodyRenderer.h/cpp` — instanced draw calls
- `src/render/scene/SceneMeshStore.h` — instance transform storage
- `src/render/shaders/body.vert` — accept instance transform attribute

**Acceptance criteria**:

- 10-instance linear pattern uses ~1/10th the GPU memory vs non-instanced
- Visual output identical to non-instanced rendering
- Works with per-body colors (instance color uniform)
- Selection/picking still works on individual instances

---

### T4.6 — LOD (Level of Detail) System

**Priority**: Low
**Effort**: ~1 week
**Dependencies**: None

**Description**: For complex models with many bodies, distant or small bodies should use coarser tessellation. This reduces GPU load and improves frame rates for large assemblies.

**Technical approach**:

- Compute screen-space size (pixels) for each body's bounding box every frame
- Define 3 LOD tiers: Full (>100px), Medium (30-100px), Low (<30px)
- `TessellationCache` stores multiple tessellation levels per body: `{bodyId → [fullMesh, medMesh, lowMesh]}`
- During render, select appropriate LOD per body based on screen size
- Smooth transitions: crossfade between LODs over 2-3 frames (or just snap — crossfade is a stretch goal)
- Background tessellation: generate lower LODs lazily in a worker thread

**Files**:

- `src/render/tessellation/TessellationCache.h/cpp` — multi-level storage, LOD selection
- `src/render/BodyRenderer.cpp` — LOD selection per frame
- `src/render/Camera3D.h` — add screen-space size computation helper

**Acceptance criteria**:

- Distant bodies render with fewer triangles (verify with wireframe mode)
- Close-up bodies still render at full quality
- No visible popping when LOD changes (smooth enough threshold)
- FPS improvement measurable on 20+ body scene
- LOD computation doesn't add significant CPU overhead

---

## Tier 5 — Advanced Modeling Features

### T5.1 — Hole Wizard

**Priority**: Medium
**Effort**: 1–2 weeks
**Dependencies**: T1.5 (reference geometry useful for hole positioning)

**Description**: Parametric hole creation tool. Holes are the most common feature in mechanical parts (screws, bolts, pins, bearings). A dedicated hole wizard with standard sizes is far more efficient than sketch+cut-extrude for each hole.

**Technical approach**:

- **New operation type**: Add `OperationType::Hole` to enum, `HoleParams` struct:
  ```cpp
  struct HoleParams {
      enum class HoleType { Simple, Counterbore, Countersink };
      enum class DepthType { Blind, ThroughAll };
      HoleType holeType = HoleType::Simple;
      DepthType depthType = DepthType::Blind;
      double diameter = 5.0;
      double depth = 10.0;           // For Blind
      double cbDiameter = 10.0;      // Counterbore diameter
      double cbDepth = 3.0;          // Counterbore depth
      double csAngle = 90.0;         // Countersink angle
      double csDiameter = 10.0;      // Countersink diameter
      std::string targetBodyId;
      std::string faceId;            // Face to drill into
      double posX = 0.0;             // Position on face (UV or projected)
      double posY = 0.0;
  };
  ```
- **UI tool**: Click face → position hole (draggable marker on face) → set type/diameter/depth in property panel → preview → confirm.
- **OCCT implementation**: Create hole profile (circle or counterbore/countersink profile) → revolve → cut-boolean from target body. Or use OCCT `BRepFeat_MakeCylindricalHole`.
- **Standard sizes** (stretch goal): Dropdown with ISO metric screw sizes (M3, M4, M5, M6, M8...) that auto-fill diameter and counterbore/countersink dimensions.

**Files**:

- `src/app/document/OperationRecord.h` — add Hole to OperationType enum, HoleParams struct
- `src/app/history/RegenerationEngine.cpp` — hole execution logic
- New: `src/ui/tools/HoleTool.h`, `src/ui/tools/HoleTool.cpp`
- `src/io/HistoryIO.cpp` — serialize HoleParams
- `src/ui/inspector/PropertyInspector.cpp` — hole parameter editing

**Acceptance criteria**:

- Click face → place hole → preview → confirm
- Simple through/blind holes work correctly
- Counterbore: wider cylinder + deeper narrow cylinder
- Countersink: conical entry + cylindrical hole
- Hole position is parametric (editable in PropertyInspector)
- Undo removes entire hole operation
- Works on planar and cylindrical faces

---

### T5.2 — Draft/Taper on Faces

**Priority**: Low-Medium
**Effort**: 3–5 days
**Dependencies**: None

**Description**: Add draft angle to selected faces. Needed for injection molding (parts must have draft to release from mold). Also useful for general modeling (tapered walls, pyramidal shapes). Different from extrude draft angle — this modifies existing faces.

**Technical approach**:

- **New operation type**: Add `OperationType::Draft` and `DraftParams`:
  ```cpp
  struct DraftParams {
      std::vector<std::string> faceIds;  // Faces to draft
      std::string neutralFaceId;         // Neutral plane (face that stays fixed)
      double angleDeg = 3.0;             // Draft angle
      std::string directionBodyId;       // Pull direction from body/face normal
  };
  ```
- **OCCT**: Use `BRepOffsetAPI_DraftAngle`. Set neutral plane, add face + angle pairs, build modified shape.
- **UI**: Select faces → select neutral plane (base face) → input angle → preview → confirm. Show direction arrow indicator.

**Files**:

- `src/app/document/OperationRecord.h` — add Draft type + DraftParams
- `src/app/history/RegenerationEngine.cpp` — draft execution
- New: `src/ui/tools/DraftTool.h`, `src/ui/tools/DraftTool.cpp`
- `src/io/HistoryIO.cpp` — serialize DraftParams

**Acceptance criteria**:

- Select faces + neutral plane → apply draft angle → faces tilt correctly
- Preview shows tapered shape during editing
- Common angles work: 1°, 2°, 3°, 5°
- Undo removes draft operation
- ElementMap correctly tracks drafted faces

---

### T5.3 — Section View

**Priority**: Medium
**Effort**: ~1 week
**Dependencies**: None

**Description**: Clip plane visualization that cuts through the model showing internal features. Cross-section fill (hatching) on cut surfaces. Essential for inspecting internal geometry (hollow shells, counterbored holes, internal channels).

**Technical approach**:

- **Clip plane**: Use OpenGL `gl_ClipDistance` (GLSL built-in). Define clip plane equation (normal + offset). Fragments behind plane are discarded.
- **Cross-section fill**: Where the clip plane intersects solid bodies, draw a filled region with hatch pattern:
  1. Compute section shape using OCCT `BRepAlgoAPI_Section` (body intersected with plane)
  2. Tessellate the section contour → render as filled triangles with hatch texture
  3. Or: simpler approach — use stencil buffer to detect cross-section regions, fill with color
- **UI**: Section view tool — drag a plane through the model. Arrow indicator shows clip direction. Toggle on/off. Adjustable plane position (slider or drag).
- **Non-destructive**: Section view is a visualization mode, doesn't modify geometry.

**Files**:

- `src/render/BodyRenderer.h/cpp` — clip plane uniform, gl_ClipDistance in vertex shader
- `src/render/shaders/body.vert` — add clip distance output
- New: `src/ui/tools/SectionViewTool.h/cpp` — tool for positioning section plane
- New: `src/render/SectionRenderer.h/cpp` — cross-section fill rendering

**Acceptance criteria**:

- Activate section view → drag plane → model clips cleanly
- Cross-section surfaces show filled hatch pattern
- Toggle on/off without artifacts
- Works with multiple bodies simultaneously
- Section plane can be positioned along X, Y, or Z axis
- Non-destructive: deactivating restores full model view

---

### T5.4 — Text / Engrave

**Priority**: Low
**Effort**: 1–2 weeks
**Dependencies**: None

**Description**: Place text on body faces, then extrude as raised boss or recessed pocket. For labels, part numbers, logos, decorative text. Common in 3D printing (name tags, custom parts).

**Technical approach**:

- **Font loading**: Use FreeType library to load TTF/OTF fonts → extract glyph outlines as vector paths (polylines + bezier curves)
- **Text to wire**: Convert font glyph outlines → OCCT `TopoDS_Wire` for each character. Position characters with proper kerning and spacing.
- **Placement**: User clicks a face → positions text baseline → types text. Project text wires onto the face.
- **3D operation**: Extrude text wires as boss (positive) or pocket (negative) from the face. Small depth (0.5–2mm typical for engraving).
- **New operation**: `OperationType::TextEmboss` with params: faceId, text string, font, size, depth, position, bold/italic.
- **Complexity note**: Multi-contour glyphs (O, A, B, etc.) have inner loops that must be handled as pockets within the outer extrude. OCCT boolean handles this.

**Files**:

- `CMakeLists.txt` — add FreeType dependency
- New: `src/core/modeling/TextWireBuilder.h/cpp` — font → wire conversion
- `src/app/document/OperationRecord.h` — TextEmboss type + params
- `src/app/history/RegenerationEngine.cpp` — text emboss execution
- New: `src/ui/tools/TextTool.h/cpp` — text placement UI

**Acceptance criteria**:

- Select face → type text → preview shows raised/recessed text → confirm
- Common characters work (A-Z, a-z, 0-9, punctuation)
- Multi-contour letters (O, B, P, etc.) render correctly with inner holes
- Font size adjustable (mm height)
- Boss (raised) and pocket (recessed) modes
- At least one bundled font (monospace or sans-serif)

---

### T5.5 — Helix / Spring

**Priority**: Low
**Effort**: ~1 week
**Dependencies**: T1.4 (Sweep UI tool helps, as helix = sweep along helical path)

**Description**: Create helical geometry for threads, springs, coils, spiral features. Common in mechanical modeling. Can be implemented as a helical sweep path + regular sweep operation.

**Technical approach**:

- **Helix curve**: OCCT can create helical curves via `Geom_Line` + `Geom_Circle` combination, or more precisely via `GeomFill_Pipe`. Alternatively, construct a helical `TopoDS_Edge` from parametric definition.
- **Option A — Helical sweep**: Create a helix as a sweep path (new path type), then use existing Sweep operation. Requires adding helix as a path option in SweepParams.
- **Option B — Dedicated operation**: `OperationType::Helix` with `HelixParams`:
  ```cpp
  struct HelixParams {
      double height = 10.0;       // Total helix height
      double pitch = 2.0;         // Distance per revolution
      double radius = 5.0;        // Helix radius
      bool clockwise = false;
      std::string profileSketchId;  // Cross-section profile
      std::string profileRegionId;
  };
  ```
- **UI**: Define helix axis (click face/edge for center + direction) → set height, pitch, radius → select profile → preview → confirm.

**Files**:

- `src/app/document/OperationRecord.h` — add Helix type + params
- `src/app/history/RegenerationEngine.cpp` — helix execution
- New: `src/ui/tools/HelixTool.h/cpp`
- `src/io/HistoryIO.cpp` — serialize HelixParams

**Acceptance criteria**:

- Create helix with circular cross-section → produces spring geometry
- Adjustable height, pitch, radius
- Clockwise and counterclockwise options
- Custom cross-section profiles (square, hexagonal, etc.)
- Works for: springs, threads (fine pitch), coils
- Undo removes helix operation

---

## Tier 6 — Basic Assembly (Multi-Body Positioning)

### T6.1 — Body Transform Support

**Priority**: Medium
**Effort**: 3–5 days
**Dependencies**: None

**Description**: Currently all bodies exist at their creation position with no independent transform. For basic assembly (positioning imported STEP parts), each body needs a position + rotation transform. This is the architectural foundation for assembly features.

**Technical approach**:

- **Data model**: Add `Transform3D` to body storage in Document:
  ```cpp
  struct Transform3D {
      double tx = 0, ty = 0, tz = 0;  // Translation
      double rx = 0, ry = 0, rz = 0;  // Rotation (Euler angles, degrees)
      // Or use quaternion: double qw=1, qx=0, qy=0, qz=0;
  };
  ```
- Add `Transform3D` to `Document::BodyEntry`. Default = identity (no change to current behavior).
- **Rendering**: Apply transform before rendering each body. In `BodyRenderer`, pass per-body model matrix = translation × rotation.
- **Picking**: Transform pick ray into body-local coordinates before hit testing.
- **Serialization**: Save/load transforms in document.json per body.
- **No UI yet** — this task is infrastructure. Transform UI comes in T6.2.

**Files**:

- `src/app/document/Document.h/cpp` — add Transform3D per body, get/set methods
- `src/render/BodyRenderer.cpp` — apply per-body transform matrix
- `src/render/scene/SceneMeshStore.h` — store transform per mesh entry
- `src/io/DocumentIO.cpp` — serialize transforms
- Selection/picking code — transform pick ray

**Acceptance criteria**:

- Bodies can have transforms (set programmatically)
- Rendering applies transforms correctly
- Picking works on transformed bodies
- Save/load preserves transforms
- Default transform = identity (no regression on existing models)

---

### T6.2 — 3D Transform Gizmo

**Priority**: Medium
**Effort**: 1–2 weeks
**Dependencies**: T6.1 (body transform support)

**Description**: Interactive gizmo for moving and rotating bodies in 3D. Three translation arrows (X/Y/Z) and three rotation rings. Standard CAD/3D tool gizmo pattern. Essential for positioning imported parts.

**Technical approach**:

- **Gizmo geometry**: Render at selected body's origin. Three colored arrows (R/G/B for X/Y/Z) + three rotation rings. Scale gizmo size to remain constant screen size regardless of zoom level.
- **Interaction**:
  - Click + drag arrow → translate along that axis. Show distance value during drag.
  - Click + drag ring → rotate around that axis. Show angle value during drag.
  - Click + drag center box → free translate in screen plane.
  - Shift+drag → snap to grid (translation) or 15° increments (rotation)
- **Rendering**: Render gizmo on top of scene (disable depth test). Use simple GL_LINES + GL_TRIANGLES.
- **Pick gizmo elements**: Ray-cylinder intersection for arrows, ray-torus intersection for rings. Priority: gizmo picks before model picks when gizmo is visible.
- **Commands**: `MoveBodyCommand(bodyId, delta)`, `RotateBodyCommand(bodyId, axis, angle)` for undo support.
- **Activation**: Select body → press `G` (grab/move) or click move tool button.

**Files**:

- New: `src/ui/tools/TransformGizmo.h`, `src/ui/tools/TransformGizmo.cpp`
- New: `src/ui/tools/MoveTool.h`, `src/ui/tools/MoveTool.cpp`
- New: `src/app/commands/MoveBodyCommand.h/cpp`
- `src/render/BodyRenderer.cpp` — render gizmo overlay
- `src/ui/viewport/Viewport.cpp` — gizmo pick handling

**Acceptance criteria**:

- Select body → gizmo appears at body origin
- Drag X arrow → body moves along X axis (red)
- Drag Y arrow → body moves along Y (green), Z → blue
- Drag rotation ring → body rotates around axis
- Shift+drag snaps to grid/angle increments
- Distance/angle shown during drag
- Undo reverses each move/rotate operation
- Gizmo scales to constant screen size

---

### T6.3 — Multi-Part Import Positioning

**Priority**: Medium
**Effort**: 2–3 days
**Dependencies**: T6.1, T6.2

**Description**: Import multiple STEP files as separate bodies and position them in the scene. Basic assembly workflow: import Part A, import Part B, move Part B into position relative to Part A.

**Technical approach**:

- Extend `ImportStepCommand` to place imported body at a user-specified position (or at origin by default, then moveable via gizmo)
- After import, automatically activate the transform gizmo on the imported body so user can position it immediately
- Add "Import and Place" workflow: File → Import STEP → body appears → gizmo active → position → click to confirm
- Multiple imports accumulate bodies in the Document (already supported)

**Files**:

- `src/app/commands/ImportStepCommand.h/cpp` — support initial transform
- `src/ui/mainwindow/MainWindow.cpp` — wire "Import and Place" workflow
- `src/ui/tools/MoveTool.cpp` — auto-activate after import

**Acceptance criteria**:

- Import STEP → body appears → gizmo auto-activates for positioning
- Import second STEP → independent body, can be positioned separately
- Both bodies render correctly with independent transforms
- Save/load preserves multi-body assembly with positions

---

### T6.4 — Snap-to-Face Alignment

**Priority**: Low-Medium
**Effort**: 3–5 days
**Dependencies**: T6.2

**Description**: When positioning bodies, users need to align faces — e.g., place the bottom face of Part B flush against the top face of Part A. More ergonomic than manual coordinate entry.

**Technical approach**:

- **Workflow**: Activate align tool → click source face on moving body → click target face on stationary body → body snaps so faces are coincident (touching, normals opposing).
- **Math**: Compute transform that aligns source face's plane (normal + point) to target face's plane (inverted normal + point). Translation = target point - source point (after rotation). Rotation = align source normal to -target normal.
- **Preview**: Show ghost body at aligned position before confirming.
- **Modes**: Coincident (faces touching), Offset (faces parallel at distance), Concentric (cylindrical face alignment).

**Files**:

- New: `src/ui/tools/AlignTool.h/cpp`
- `src/ui/tools/ModelingToolManager.h/cpp` — register
- `src/app/commands/MoveBodyCommand.h/cpp` — reuse for alignment transform

**Acceptance criteria**:

- Click face A on body 1 → click face B on body 2 → bodies align faces
- Faces end up coincident (touching, normals opposing)
- Preview shows final position before confirmation
- Undo reverses alignment
- Works with planar faces (minimum). Cylindrical face alignment is stretch goal.

---

## Tier 7 — UX Polish & Ecosystem

### T7.1 — Undo/Redo Visual Feedback

**Priority**: Low
**Effort**: ~1 day
**Dependencies**: None

**Description**: When undoing/redoing, there's no visual confirmation of what changed. A brief toast notification (e.g., "Undid: Extrude") helps users track their position in history.

**Technical approach**:

- After CommandProcessor executes undo/redo, emit signal with command label
- Show a temporary overlay label (1.5 seconds, fade out) at top of viewport: "Undo: {command.label()}" / "Redo: {command.label()}"
- Use QPropertyAnimation for fade effect
- Position: top-center of viewport, semi-transparent dark background

**Files**:

- `src/ui/mainwindow/MainWindow.cpp` — connect to CommandProcessor signals, show toast
- New: `src/ui/widgets/ToastNotification.h/cpp` — reusable toast widget (useful for other notifications too)

**Acceptance criteria**:

- Ctrl+Z shows "Undo: Extrude" (or whatever the command was) briefly
- Ctrl+Shift+Z shows "Redo: ..."
- Toast fades out after 1.5 seconds
- Multiple rapid undos show latest toast (don't stack)

---

### T7.2 — Project Thumbnails

**Priority**: Low
**Effort**: 1–2 days
**Dependencies**: None

**Description**: StartOverlay shows recent projects but without visual thumbnails. Save a viewport screenshot on File → Save and display it in the project browser for quick visual identification.

**Technical approach**:

- On save: capture current viewport framebuffer via `QOpenGLWidget::grabFramebuffer()`, scale to 256x256, save as PNG alongside the project file (inside .onecad ZIP or .onecadpkg directory as `thumbnail.png`)
- In StartOverlay: load thumbnail from recent project paths, display in project tile
- Handle missing thumbnails gracefully (show generic CAD icon)

**Files**:

- `src/ui/viewport/Viewport.cpp` — add `captureThumbnail()` method
- `src/ui/mainwindow/MainWindow.cpp` — call capture on save
- `src/io/DocumentIO.cpp` or Package — store/load thumbnail in project
- `src/ui/start/StartOverlay.cpp` — display thumbnails
- `src/ui/start/ProjectTile.cpp` — thumbnail image in tile

**Acceptance criteria**:

- Saving a project captures viewport thumbnail
- StartOverlay shows thumbnails next to project names
- Thumbnails update on each save
- Missing thumbnails show placeholder icon
- Thumbnail doesn't significantly increase save time (<100ms)

---

### T7.3 — Preferences Dialog

**Priority**: Medium
**Effort**: 3–5 days
**Dependencies**: None

**Description**: Centralized preferences dialog for all user settings. Currently scattered across various panels or hardcoded. Users need one place to configure the application.

**Technical approach**:

- **Dialog layout**: Tabbed interface with categories:
  - **General**: Theme (Light/Dark/System), language (future), startup behavior
  - **Units**: Default units (mm/cm/inch), decimal places, angle unit (degrees/radians)
  - **Sketch**: Snap settings, auto-constraint settings, grid spacing, construction default
  - **Modeling**: Default extrude mode, default boolean mode, fillet preview quality
  - **Export**: Default mesh quality, default STEP schema
  - **Viewport**: Background color, edge display, SSAO toggle, anti-aliasing
  - **Shortcuts**: Shortcut customization table (action → key binding)
  - **Autosave**: Interval (minutes), max backup count
- **Storage**: Use QSettings for persistence. Group by category.
- **Apply**: Live preview for visual settings (theme, viewport). Other settings apply on OK.

**Files**:

- New: `src/ui/dialogs/PreferencesDialog.h`, `src/ui/dialogs/PreferencesDialog.cpp`
- `src/ui/mainwindow/MainWindow.cpp` — wire Edit → Preferences action
- Various files that read hardcoded defaults → read from QSettings instead

**Acceptance criteria**:

- Edit → Preferences opens tabbed dialog
- Theme change applies immediately (live preview)
- Settings persist across sessions
- All listed categories have at least basic controls
- Cancel reverts any live-previewed changes
- Keyboard shortcut editing works (click field, press new key combo)

---

### T7.4 — Units System

**Priority**: Medium
**Effort**: ~1 week
**Dependencies**: T7.3 (preferences dialog for unit selection)

**Description**: Currently all dimensions are implicitly millimeters with no unit display or conversion. Users working in imperial units (inches) or other metric units (cm, m) need conversion support. Affects all dimension displays, property inputs, and exports.

**Technical approach**:

- **Core unit engine**: Create `Units` class with:
  - Internal representation: always millimeters (no conversion of stored data)
  - Display conversion: mm → user's preferred unit for display
  - Input conversion: user's unit → mm for storage
  - Format: `toDisplay(double mm) → string`, `fromInput(string) → double mm`
- **Supported units**: mm, cm, m, inch, foot
- **Display format**: "10.00 mm" or "0.394 in" — configurable decimal places
- **Integration points**:
  - DimensionEditor: show/accept values in user units
  - PropertyInspector: all length fields in user units
  - Status bar coordinates: in user units
  - Export dialogs: show conversion info
  - Measurement tool output: in user units
- **No internal conversion**: All stored values remain in mm. Only display/input is converted.

**Files**:

- New: `src/core/Units.h`, `src/core/Units.cpp`
- `src/ui/sketch/DimensionEditor.cpp` — use Units for display/input
- `src/ui/inspector/PropertyInspector.cpp` — use Units for all length fields
- `src/ui/mainwindow/MainWindow.cpp` — status bar coordinate display
- `src/ui/tools/MeasureTool.cpp` — measurement output in user units

**Acceptance criteria**:

- Switch to inches in preferences → all dimensions display in inches
- Typing "1 in" in a dimension field correctly converts to 25.4mm internally
- Status bar shows coordinates in selected unit
- Switching units doesn't change model geometry (only display changes)
- Export formats that specify units (3MF, STEP) correctly declare the internal mm values

---

### T7.5 — Expressions / Formulas in Parameters

**Priority**: Low-Medium
**Effort**: 1–2 weeks
**Dependencies**: T1.6 (PropertyInspector must be fully wired)

**Description**: Allow mathematical expressions in dimension and parameter fields. Type `width * 2` or `baseHeight + 5` instead of computing values manually. Enables true parametric design where changing one dimension cascades through the model. DimensionEditor already supports basic math (+, -, \*, /). This extends it to named variables and cross-parameter references.

**Technical approach**:

- **Expression engine**:
  - Parser: tokenize + recursive descent parser for arithmetic expressions
  - Variables: named parameters that can be referenced (e.g., `$width`, `$depth`)
  - Functions: `min()`, `max()`, `sqrt()`, `sin()`, `cos()`, `tan()`, `abs()`, `pi`
  - Storage: parameters stored as string expression + cached double value
  - Re-evaluation: when a referenced variable changes, re-evaluate all dependents
- **Variable scope**: Document-level named parameters (separate from operation params). User can define `width = 50`, then use `$width` in extrude distance.
- **Dependency tracking**: Build expression dependency graph. On variable change, topologically sort dependents and re-evaluate in order.
- **UI**: In PropertyInspector and DimensionEditor, allow typing expressions. Show resolved value in tooltip. Show formula icon when field contains expression (not just a number).
- **Variable manager**: New panel or section in PropertyInspector listing all named parameters with current values.

**Files**:

- New: `src/core/ExpressionEngine.h`, `src/core/ExpressionEngine.cpp`
- New: `src/core/ExpressionParser.h/cpp` — tokenizer + parser
- `src/app/document/Document.h/cpp` — store named parameters
- `src/ui/sketch/DimensionEditor.cpp` — expression input support
- `src/ui/inspector/PropertyInspector.cpp` — expression input support
- `src/io/DocumentIO.cpp` — serialize expressions + named params
- New: `src/ui/inspector/ParameterManager.h/cpp` — named parameter editing panel

**Acceptance criteria**:

- Type `50 + 10` in dimension field → evaluates to 60
- Define `$width = 50` → use `$width` in extrude distance → resolves to 50
- Change `$width = 100` → extrude distance updates to 100 → model regenerates
- Circular references detected and reported as error
- Functions work: `sqrt(2)`, `sin(45)`, `min(10, 20)`
- Expressions survive save/load
- Invalid expressions show red highlight with error tooltip

---

### T7.6 — Linux Packaging (AppImage / Flatpak)

**Priority**: Low
**Effort**: 3–5 days
**Dependencies**: None (CI already builds on Ubuntu)

**Description**: Easy installation for Linux users. Currently Linux builds from source only. AppImage provides a single portable binary. Flatpak provides sandboxed distribution via Flathub.

**Technical approach**:

- **AppImage**:
  1. Use `linuxdeployqt` or `appimage-builder` in CI
  2. Bundle Qt6 libs, OCCT libs, and Eigen3 headers
  3. Create .desktop file with icon
  4. Output: `OneCAD-x86_64.AppImage` (single file, chmod +x and run)
- **Flatpak** (stretch goal):
  1. Create `com.onecad.OneCAD.yml` manifest
  2. Build in Flatpak runtime (KDE Platform for Qt6)
  3. Publish to Flathub
- **Desktop file**: `resources/onecad.desktop` already exists. Ensure it has correct categories, icon, and MIME types (.onecad, .onecadpkg, .step).

**Files**:

- `.github/workflows/` — add AppImage build step to Linux CI
- New: `packaging/appimage/` — AppImage configuration
- New: `packaging/flatpak/com.onecad.OneCAD.yml` — Flatpak manifest
- `resources/onecad.desktop` — verify/update desktop entry

**Acceptance criteria**:

- CI produces AppImage artifact on each build
- AppImage runs on Ubuntu 22.04+ without additional dependencies
- Desktop file integrates correctly (shows in app menu after install)
- File associations work (.onecad files open with OneCAD)
- Flatpak manifest builds successfully (stretch goal)

---

## Dependency Graph Summary

```
T0.1 ──────────────────────────────────────────── (standalone)
T0.2 ──────────────────────────────────────────── (standalone)
T0.3 ──────────────────────────────────────────── (standalone)
T0.4 ──────────────────────────────────────────── (standalone)

T1.1 ──────────────────────────────────────────── (standalone)
T1.2 ──────────────────────────────────────────── (standalone)
T1.3 ──── depends on T0.4 (sketch-on-face)
T1.4 ──────────────────────────────────────────── (standalone)
T1.5 ──── depends on T0.4 (sketch-on-face as target)
T1.6 ──────────────────────────────────────────── (standalone)
T1.7 ──── depends on T0.2 (shortcuts in palette)

T2.1 ──── depends on T0.3 (DOF indicator)
T2.2 ──────────────────────────────────────────── (standalone)
T2.3 ──────────────────────────────────────────── (standalone)
T2.4 ──────────────────────────────────────────── (standalone)
T2.5 ──────────────────────────────────────────── (standalone)
T2.6 ──────────────────────────────────────────── (standalone)

T3.1 ──────────────────────────────────────────── (standalone)
T3.2 ──────────────────────────────────────────── (standalone)
T3.3 ──────────────────────────────────────────── (standalone)
T3.4 ──── depends on T3.3 (shared 2D conversion)
T3.5 ──────────────────────────────────────────── (standalone)

T4.1 ──────────────────────────────────────────── (standalone)
T4.2 ──────────────────────────────────────────── (standalone)
T4.3 ──────────────────────────────────────────── (standalone)
T4.4 ──────────────────────────────────────────── (standalone)
T4.5 ──── depends on T1.1 or T1.2 (patterns)
T4.6 ──────────────────────────────────────────── (standalone)

T5.1 ──── benefits from T1.5 (reference geometry)
T5.2 ──────────────────────────────────────────── (standalone)
T5.3 ──────────────────────────────────────────── (standalone)
T5.4 ──────────────────────────────────────────── (standalone)
T5.5 ──── benefits from T1.4 (sweep UI)

T6.1 ──────────────────────────────────────────── (standalone)
T6.2 ──── depends on T6.1 (body transforms)
T6.3 ──── depends on T6.1, T6.2
T6.4 ──── depends on T6.2 (gizmo)

T7.1 ──────────────────────────────────────────── (standalone)
T7.2 ──────────────────────────────────────────── (standalone)
T7.3 ──────────────────────────────────────────── (standalone)
T7.4 ──── depends on T7.3 (preferences for unit selection)
T7.5 ──── depends on T1.6 (PropertyInspector wired)
T7.6 ──────────────────────────────────────────── (standalone)
```

---

## Task Count Summary

| Tier      | Name                         | Tasks  | Estimated Effort |
| --------- | ---------------------------- | ------ | ---------------- |
| 0         | Bug Fixes & Quick Wins       | 4      | ~1 week          |
| 1         | Core Modeling Completeness   | 7      | ~4 weeks         |
| 2         | Sketch System Improvements   | 6      | ~4 weeks         |
| 3         | Export & 3D Printing         | 5      | ~3 weeks         |
| 4         | Performance & Visual Quality | 6      | ~4 weeks         |
| 5         | Advanced Modeling            | 5      | ~5 weeks         |
| 6         | Basic Assembly               | 4      | ~3 weeks         |
| 7         | UX Polish & Ecosystem        | 6      | ~4 weeks         |
| **Total** |                              | **43** | **~28 weeks**    |
