# Current State

Last verified: 2026-07-13 10:20

- **Branch:** `master`, clean tree, ~31 commits ahead of `origin/master` (never pushed — pushing needs explicit user go-ahead)
- **Commit range this effort:** `da6f782` (.gitignore) → `0bcdb58` (H6 docs tick); revert pair `98b8083`/`bae765e` is the parked H5-B experiment
- **Build/test:** `cmake --build build -j8` → 0 errors · `ctest --test-dir build` → **33/33 pass** · `ONECAD_HEADLESS_SMOKE=1 ./build/OneCAD.app/Contents/MacOS/OneCAD` → exit 0
- **Test integrity (critical context):** exit codes only became real this session — CI was Release/NDEBUG (asserts inert, fixed via `-UNDEBUG` on test targets) AND linking OCCT's `libTKDraw` rewrote every process exit status to 0 (fixed by filtering `Draw|DRAW|Test|TKDCAF` libs out of `OpenCASCADE_LIBRARIES` in root CMakeLists). Any pre-session green result was unreliable.
- **Key files touched (by area):**
  - kernel: `src/kernel/elementmap/ElementMap.h` (rebind hardening, global assignment, tightened fallback), `src/kernel/topology/SelectionTopologyResolver.cpp`
  - regen: `src/app/history/RegenerationEngine.{h,cpp}` (exception boundaries, temporal guards, ThroughAll extent, datum recompute epilogue, dead preview API removed), `DependencyGraph` untouched since P0
  - io: `src/io/OneCADFileIO.cpp` (atomic save + version gate), `src/io/DocumentIO.cpp` (corrupt-brep guard, FaceRef legacy suppression), `src/io/ManifestIO.h` (1.1.0), `src/io/MigrationRegistry.cpp` (wired), `src/io/Package.{h,cpp}` (Format overload)
  - commands: `CommandProcessor` (busy signals, 200-cap), `SketchDragGestureCommand` (label + restoreBeginState), new `AddSketchCommand`, `UpdateSketchAttachmentCommand` (strict + rollback)
  - ui: `MainWindow.{h,cpp}` (closeEvent, transactions, gesture guards, ellipse hidden, KernelScheduler harness removed), `Viewport.*`/`ViewportSketchInteraction.cpp` (tool-gesture capture), `ExtrudeTool`/`RevolveTool` (probe-point detectMode), `TessellationCache.cpp` (reversed-face winding), theme files
  - ci: `.github/workflows/ci.yml` (Boost, ASan/UBSan lane, `-LE gl`), root `CMakeLists.txt` (`ONECAD_SANITIZE`, `-Wall -Wextra`, TKDraw filter), `tests/CMakeLists.txt` (`-UNDEBUG`, +4 registrations)
- **Key decisions:**
  - KernelScheduler deleted; async regen parked (needs doc snapshotting + thread-safe SceneMeshStore + TSan) — sync regen + wait-cursor busy feedback instead
  - Atomic save = temp+rename at `OneCADFileIO::save` (not QSaveFile — QuaZip needs filenames)
  - ElementMap rebind: loud failure (clearShape) over wrong-face match; reject threshold max(1mm, 0.5×bbox diag); re-match 1.0mm + uniqueness margin
  - **H5-B parked** (OCCT-history `update()` wiring reverted): three blockers documented in TODO.md — regen pre-reset erases all element entries (structural root: descriptor-hash ids can't survive parameter edits), descriptor drift through an op's own Modified chain, tombstone adoption contention. Design direction: parametric anchors. Repro test in `98b8083` (`testFilletSurvivesUpstreamCutEdit`)
  - Ellipse creation hidden (entity not solver-registered → uncontrainable); entity/IO kept
  - Legacy FaceRef extrudes: suppress-on-load + message (no true migration)
- **Blockers:** none. Remaining phases H7–H9 fully specced in the plan file.
