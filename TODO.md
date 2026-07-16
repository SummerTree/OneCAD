# OneCAD Maturity Roadmap

Updated: 2026-07-13 · Handoff context: HANDOFF.md · Snapshot: CURRENT_STATE.md

## Now

- [ ] Manual visual pass of everything landed since UI A–D (theme/⌘K/Inspector/Fit-Home/sketch→extrude→edit loop) — most UI work has only headless-smoke coverage
- [ ] Start **H7** (edit-dialog coverage + pickers + failure recovery) — full spec in `~/.claude/plans/act-as-senior-c-floating-fountain.md` Phase 7

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
- [x] ~~MeasureTool~~ (DELETED in H2 — click-to-measure was a dead stub with zero references; selection-based measurement lives in MassPropertiesPanel)
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

## ✅ Landed: Sketch-Only Extrude + Parametric Core Hardening

Plan: `~/.claude/plans/act-as-senior-c-hashed-shell.md`

- [x] **P0** Validation contract: `OperationValidation.h` + `AddOperationCommand` hook (forbid Extrude+FaceRef). Test: `testExtrudeFaceRefRejectedByAddCommand`. ✅ 29/29 ctest green.
- [x] **P1** Sever extrude FaceRef path: `ExtrudeTool` (face branch + push-pull alias + patch members removed), `Viewport` auto-activation (Face dropped from canExtrude), `RegenerationEngine::buildExtrude` (FaceRef arm + merged-profile removed). Deleted 3 FaceRef tests + helpers, repointed rollback test. `resolveBooleanTargetBodyId` FaceRef arm KEPT (shared with Revolve). ✅
- [x] **P2** Auto-sketch-on-face fast path: `MainWindow::createSketchOnFace` + `Document::primaryRegionId` + `Viewport::activateExtrudeToolForRegion`; wired into `extrudeRequested` (explicit intent only, not passive face selection). Test: `testAutoSketchOnFaceCreatesEditableSketchRegion`. ✅ 29/29 green.
- [x] **P3** `DatumPlane` entity (OffsetFromPlane/OffsetFromFace/AngledFromEdge; ThreePoint reserved) + `Document` API (add/get/remove/recompute) + `document.json` round-trip (resolved frame stored) + `AddDatumPlaneCommand` + "Insert ▸ Datum Plane…" menu (offset + auto-sketch). Tests: `testDatumPlaneOffsetResolves`, datum in roundtrip compat. ✅ 29/29. (3D plane-picker overlay listing datums = follow-up.)
- [x] **P4** Frozen attachment + `UpdateSketchAttachmentCommand` + `Document::resolveHostFaceResync` (literal-id → geometric re-pick fallback by frozen-plane orientation; already covers P7's host-face case) + "Insert ▸ Update Sketch Attachment" menu. Test: `testUpdateAttachmentResyncsPlane`. ✅ 29/29.
- [x] **P5** Full end modes: `ExtrudeParams` two-direction fields + `HistoryIO`; `buildExtrude` refactor — Two-Directions (fused prisms), Symmetric, ThroughAll, and To Face / To Next via **compute-distance-then-prism** (planar-target projection + `distanceToNextPlanarFace`; avoids `BRepFeat` fragility). Distance guard relaxed for non-Blind modes. Tests: `testExtrudeTwoDirections/ThroughAllCutsBox/ToFaceStopsAtFace/ToNextRequiresBody`. ✅ 29/29. (Curved-target up-to-surface + mode UI in EditParameterDialog = P8/follow-up.)
- [x] **P6** Parametric core: face/edge → producer DAG edges (id-prefix owner) for `regenerateFrom` completeness; temporal-order guard `DependencyGraph::producesBefore` wired into `buildExtrude` boolean target. Confirmed `regenerateFrom` already does affected-set partial regen (skips clean branches). Tests: `testTemporalGuardRejectsFutureTarget`, `testDirtyFlagSkipsCleanBranch`. ✅ 29/29. (ElementMap snapshot in preview backup/restore deferred — current addBodyWithId rebuild is functional; flagged.)
- [x] **P7** Broken-reference fallback: `ElementMap::resolveWithFallback` (descriptor re-match among same-owner live entries, `kRematchRejectScore` threshold, `sources`/owner-scoped) wired into `RegenerationEngine::resolveFace`/`resolveEdge` (logged "reference-remapped"). Host-face case already via P4. Test: `testResolveWithFallbackRematchesAndRejects` (+ gate `proto_elementmap_rigorous`). ✅ 29/29.
- [x] **P8** Edit-in-place + re-profile: `Document::updateOperationInput` + `EditOperationInputCommand` (validates input type, regenerates, undo); `EditParameterDialog` exposes End Condition combo + preserves all `ExtrudeParams` on edit. Tests: `testReprofileExtrudeSwapsSketchRegion`, `testReprofileToFaceRefRejected`. ✅ 29/29; full `OneCAD` app links. (Boolean-mode picker + "Re-profile" selection button in dialog + previewFrom-rewire = follow-ups; commands exist + tested.)

Gate every phase: `proto_regeneration`, `proto_elementmap_rigorous`, `proto_face_builder`, `proto_loop_detector`; `make test`; `test_compile` for UI.

## ✅ ALL PHASES P0–P8 COMPLETE — 29/29 ctest green, full app links.

Follow-ups (non-blocking): datum planes in 3D plane-picker overlay; curved-surface sketch/up-to-surface; ElementMap snapshot in preview backup/restore; EditParameterDialog boolean-mode + re-profile buttons + previewFrom rewire.

## ✅ Landed: UI Design Refinement (Phases A–D)

Plan: `~/.claude/plans/act-as-senior-c-whimsical-toucan.md`. Refines existing UI infra (no rebuilds). Adopt accent cyan #2E9BDA; full design-token extraction.

- [x] **A** Visual system: accent→#2E9BDA; ThemeButtonColors/ThemeMetrics/ThemeTypography structs; buildStyleSheet token extraction + primary/ghost button rules; SF Pro app font; ModalOverlay primary/ghost; CommandPalette theme+shortcuts+match-bold+badge; ToggleSwitch/RegenFailureDialog tokenization. ✅ test_compile green, grep-gate clean.
- [x] **B** Selection-driven Inspector: wire PropertyInspector to viewport/history/navigator selection; View-menu toggle + QSettings; debounce/guard MassPropertiesPanel; body name/visibility editors via existing commands. ✅ test_compile green.
- [x] **C** Viewport/sketch fixes: de-dup DOF (status bar numeric, panel qualitative); ConstraintPanel/SketchModePanel consume mouse events; Viewport::fitToView() + Fit/Home buttons (bottom-left, ic_fit/ic_view_iso). ✅ test_compile green.
- [x] **D** Start screen: logo+version header; search+sort+thumbnail cache; responsive grid; Import STEP tile; ProjectTile middle-elision. ✅ test_compile green.

Gate: `test_compile`, full app build, headless smoke; `proto_elementmap_rigorous` + `proto_regeneration` before commit.

✅ **ALL PHASES A–D COMPLETE & VERIFIED** — full app builds+links; headless smoke exit 0 (SF Pro applied, no malformed-QSS); 29/29 ctest green. Caught+fixed: `Qt::UniqueConnection` is illegal with lambdas (ToggleSwitch/CommandPalette → switched to PMF).
Manual visual checks remaining for user: theme toggle (cyan accent everywhere, contrast), ⌘K shortcuts/badges, modal primary/ghost buttons, Inspector on body/op select, sketch panels no click-through, Fit/Home buttons, start-screen search/sort/import.

**Both workstreams landed on master as 8 sliced commits** (da6f782..6c2dc20): .gitignore, assert-liveness (-UNDEBUG), extrude end-modes + FaceRef severance, datum plane (+ `Document::clear()` datum-leak fix), parametric robustness, UI 5a theme/5b palette+start/5c panels+MainWindow (+ merge-artifact repair at the history-collapse handler).

## Active: Hardening Roadmap → Daily-Driver CAD

Plan: `~/.claude/plans/act-as-senior-c-floating-fountain.md` (full findings + decisions + phase detail). Phase 0 (validate/slice/land WIP diff) ✅ done.

- [x] **H1** Crash + data-loss criticals, CI truth: buildRevolve/checkedBooleanResult/executeOperation OCCT exception boundaries; temporal-guard parity (revolve/boolean/patterns/mirror); closeEvent + Quit → maybeSave; atomic save (temp+rename at OneCADFileIO::save); AutosaveManager QPointer UAF fix; CI Boost dep + Linux ASan/UBSan lane + register 3 orphaned viewport protos; new tests (revolve-crash, temporal, proto_document_lifecycle, atomic-save).
- [x] **H2** Lifecycle/undo coherence + dead-code purge (also: fix(build) — OCCT Draw/Test harness libs unlinked; TKDraw's Tcl atexit rewrote every process exit status to 0, masking all non-abort test failures): delete KernelScheduler + shadow harness; busy feedback (commandStarted/Finished → wait cursor); UpdateSketchAttachmentCommand rollback-on-failure; AddSketchCommand + datum+sketch transaction; delete empty-lambda Edit actions / MeasureTool / appearance stub / dead G-handler / exploratory comments; ThemeUiColors::textSecondary.
- [x] **H3** Persistence: corrupt .brep guard; legacy FaceRef suppress-on-load + message; wire MigrationRegistry + FORMAT_VERSION 1.1.0; compat tests.
- [x] **H4** Kernel math: reversed-face normals; vertex double-transform; detectMode probe classification; ThroughAll bbox extent; DOF via PlaneGCS diagnose(); delete previewFrom/backup API + solveAsync + calculateDOF; hide ellipse creation; datum recompute in regen epilogue (+ OffsetFromFace geometric re-pick when the face id remaps).
- [x] **H5-A** ElementMap rebind hardening SHIPPED: identity pre-pass; cross-type hard-skip; translation compensation; reject threshold max(1mm, 0.5×bboxDiag) → loud clearShape; resolveWithFallback 5.0→1.0 + uniqueness margin; global best-first assignment with deterministic tie-breaks (per-entry greedy stole candidates by id order and tied nondeterministically).
- [ ] **H5-B PARKED** — OCCT-history update() wiring attempted (98b8083) and reverted (bae765e). Blockers found, needing a design pass, not a mechanical fix:
  1. **Pre-reset erasure is the structural root**: `regenerateToAppliedCount` erases every body element entry before replay (`removeElementsForBody`, RegenerationEngine pre-reset loop). Ids are deterministic descriptor-hash functions of geometry, so they reproduce only for IDENTICAL geometry — any parameter edit changes the hash and permanently orphans downstream references (fillet edgeIds, host faces). resolveWithFallback can't recover erased entries (no stored descriptor).
  2. **Descriptor drift**: following a fillet's own Modified chain drags its input edge id onto the blend boundary (z=4 → z=3 in the repro), corrupting the id's identity for the next replay's re-adoption.
  3. **Tombstone adoption contention**: with tombstones instead of erasure, sibling tombstones (blend-boundary lines) outscore the true orphan for the recreated rim edge — per-candidate adoption needs op-input-aware anchoring.
  Direction for the redesign: treat ids referenced by operation params as *parametric anchors* resolved against PRE-op geometry at each replay step (pin their descriptors; exclude them from the executing op's own Modified/Deleted drift), and keep tombstones + global assignment for everything else. Repro test lives in 98b8083 (`testFilletSurvivesUpstreamCutEdit`).
- [x] **H6** Sketch undo: command-per-gesture via generalized SketchDragGestureCommand; 3 capture choke points (tool press→geometryCreated, constraint funnels, dimension confirm); cancel restores begin snapshot; 200-entry cap.
- [ ] **H7** Edit-dialog coverage (Fillet/Chamfer/Shell/Boolean/CircularPattern/MirrorBody/Loft); twoDirections UI; ToFace one-shot picker; re-profile button; suppress-and-apply failure recovery; tools deactivate after commit.
- [ ] **H8** Action architecture: MainWindowActions + ActionRegistry convergence; Edit>Delete/Select All wired; Move-Sketch button; LinearPattern entry; CircularPattern/MirrorBody ModalOverlay forms.
- [ ] **H9** Datum minimal UI (ModalOverlay + navigator + quad render); DocumentSession extraction (buffer); docs truth-up (FILE_FORMAT §16, CLAUDE.md, AGENTS.md, README); final dead-code sweep; manual daily-driver acceptance script.
