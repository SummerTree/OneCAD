# OneCAD Maturity Roadmap

## M1: Renderer & 3D Interaction Bug Fixes (P0)

- [x] 1a. Transform light directions to view space in BodyRenderer
- [x] 1b. Transform ambient gradient direction to view space
- [x] 1c. Lower crease edge threshold from 30° to 15°
- [x] 1d. Skip degenerate triangles instead of fallback normal
- [x] 1e. Add zoom anchor near-parallel guard + distance cap
- [x] 1f. Tune polygon offset to 0.5/0.5
- [x] 1g. Split smooth normals at face color boundaries

## M2: Viewport Decomposition (P0)

- [x] Extract ViewportNavigation (~400 LOC)
- [x] Extract ViewportSketchInteraction (~800 LOC)
- [x] Extract ViewportRendering (~600 LOC)
- [x] Update CMakeLists.txt
- [x] Verify all tests pass

## M3: Autosave & Crash Recovery (P1)

- [x] AutosaveManager (timer + dirty tracking)
- [x] CrashRecoveryDialog (startup scan)
- [x] OCCT exception boundaries in RegenerationEngine (already comprehensive)
- [x] File format version + MigrationRegistry stub

## M4: STL/OBJ Export + Measurement Tools (P1)

- [x] StlExporter (binary + ASCII)
- [x] ObjExporter (with normals, groups)
- [x] Export dialog (MeshExportDialog)
- [x] MeasureTool (edge length, face area, face centroid)
- [x] MassPropertiesPanel (volume, surface area, CoM, bbox, topology counts)

## M5: Shell/Fillet Polish + Tool Wiring (P1)

- [x] Shell multi-face wiring (Shift+click, Enter) — already implemented
- [x] Fillet/Chamfer Tab toggle — already implemented
- [x] Edge chain feedback highlight — EdgeChainer + tool preview
- [x] Regen failure user-friendly messages — RegenFailureDialog integrated

## M6: Advanced Extrude Modes (P2)

- [x] ExtrudeMode enum (Blind, ThroughAll, Symmetric, ToNext, ToFace)
- [x] Through All / Symmetric in RegenerationEngine
- [ ] ExtrudeTool UI mode selector (deferred — modes work via params)
- [x] HistoryIO serialization (extrudeMode + targetFaceId)

## M7: PropertyInspector + Command Palette (P2)

- [x] PropertyInspector rewrite
- [x] CommandPalette (Cmd+K, fuzzy filter)
- [x] ActionRegistry

## M8: Test Infrastructure + CI (P2)

- [x] CTest integration
- [x] GitHub Actions macOS CI
- [x] Linux CI foundation
- [x] Integration test fixture

## M9: Linear & Circular Pattern (P2)

- [x] LinearPatternParams / CircularPatternParams
- [x] buildLinearPattern / buildCircularPattern
- [ ] PatternTool UI (deferred — patterns work via params)
- [ ] ElementMap instance naming (deferred — uses opId-based naming)

## M10: Loft & Sweep (P3)

- [x] buildLoft (BRepOffsetAPI_ThruSections)
- [x] buildSweep (BRepOffsetAPI_MakePipe)
- [ ] LoftTool UI (deferred — works via params)
- [ ] SweepTool UI (deferred — works via params)

## M11: Sketch Enhancements (P3)

- [ ] Spline entity + PlaneGCS (deferred — PlaneGCS adapter work needed)
- [ ] Offset tool (deferred)
- [ ] Sketch fillet/chamfer (deferred)
- [x] Box selection (L→R window, R→L crossing)

## M12: Section View, Mirror Body, DXF Export (P3)

- [ ] Section view (clip plane + hatch) — deferred
- [x] Mirror body (BRepBuilderAPI_Transform + optional fuse)
- [ ] DXF export (HLR) — deferred

## M13: Linux Platform Support (P3)

- [x] Linux gesture handling (already cross-platform via Q_OS_MACOS guards + QPinchGesture/QPanGesture fallback)
- [x] OpenGL version check (runtime check in main.cpp after context creation)
- [x] AppImage packaging (.desktop file + CMake install rules)
- [x] PlatformPaths utility (QStandardPaths-based cross-platform paths)
- [x] Linux apt deps in Makefile (apt-get install for Qt6/OCCT/Eigen3/GL)

## Active: MVP Stabilization Commit Slices

- [ ] Slice 1: Kernel target + moved topology/modeling helpers
- [ ] Slice 2: Document/regeneration/tessellation rollback safety
- [ ] Slice 3: Selection/topology promotion + viewport picking regressions
- [ ] Slice 4: Beginner MVP UI gating + Sketch→Extrude→Edit stabilization
