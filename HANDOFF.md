# Handoff — OneCAD Hardening Roadmap (daily-driver milestone)

Session 1 · 2026-07-13

## Goal

Make OneCAD a usable personal daily-driver parametric CAD: harden existing
features (no new capability categories), bulletproof the sketch→extrude→edit
loop, clean minimal UI. User priorities: modeling robustness, UI/UX friction,
stability/perf. Sketching itself was deemed acceptable.

## Original plan

Approved 10-phase roadmap in `~/.claude/plans/act-as-senior-c-floating-fountain.md`
(full findings from a 59-agent adversarially-verified review, 15 architecture
decisions, per-phase gates). Phases: 0 land-WIP-diff → H1 criticals → H2
lifecycle → H3 persistence → H4 kernel math → H5 ElementMap → H6 sketch undo →
H7 dialogs/recovery → H8 action architecture → H9 datum UI + docs + acceptance.

## Done so far (and why)

- **Phase 0**: the pre-existing ~2.7k-line WIP diff (extrude end-modes, FaceRef
  severance, datum planes, parametric robustness, UI theme/palette/panels)
  validated and landed as 8 sliced, individually test-gated commits — index-only
  staging (`git hash-object` + `apply --cached`), gates run on the committed
  tree with remainder stashed.
- **H1–H4, H6**: complete — see TODO.md checkboxes and CURRENT_STATE.md key
  decisions. Every phase closed with full ctest green + headless smoke.
- **H5**: A-half shipped (rebind hardening — loud failure over wrong-face);
  **B-half (OCCT-history wiring) implemented, failed acceptance, reverted**
  (`98b8083` → revert `bae765e`, salvage `156d306`).
- **Two systemic discoveries** (do not re-learn these):
  1. Linking OCCT's `libTKDraw` (Tcl test harness) installed an atexit handler
     that rewrote EVERY process exit status to 0 — all non-abort test failures
     and the app's own exit code read as success on macOS. Filtered from the
     link in root CMakeLists. Together with CI's Release/NDEBUG assert problem
     (fixed via `-UNDEBUG`), test results before these fixes were unreliable.
  2. Regeneration pre-reset ERASES all ElementMap entries per replay, so
     descriptor-hash ids structurally cannot survive parameter edits — the true
     root of "edit upstream, downstream reference dies". Full analysis +
     "parametric anchor" design direction in TODO.md (H5-B PARKED section).
- **Dead-ends ruled out**: QSaveFile for atomic save (QuaZip needs real file
  paths); dependency-cycle-based CriticalFailure tests (cycles now prevented by
  construction); H+V-on-one-line as a solver-conflict test case (satisfiable by
  a degenerate line); tombstones + adoption alone for H5-B (descriptor drift
  through an op's own Modified chain corrupts identity — needs anchoring).

## How to resume

1. Run the `handoff` skill with "resume" (or `/handoff resume`).
2. Re-read `CLAUDE.md`, this file, `CURRENT_STATE.md`, `TODO.md` "Now" section,
   and plan file Phases 7–9.
3. Verify: `cmake --build build -j8 && ctest --test-dir build` (expect 33/33)
   and `ONECAD_HEADLESS_SMOKE=1 ./build/OneCAD.app/Contents/MacOS/OneCAD`.
4. First: a manual visual pass of the landed UI (theme toggle, ⌘K palette,
   Inspector on selection, Fit/Home, sketch→extrude→edit, datum creation,
   sketch undo/redo feel) — headless smoke can't see rendering.
5. Then H7 per the plan: EditParameterDialog coverage for
   Fillet/Chamfer/Shell/Boolean/CircularPattern/MirrorBody/Loft (Sweep →
   Inspector), twoDirections UI, ToFace one-shot picker (RAII filter restore),
   re-profile button (EditOperationInputCommand exists + tested), and the
   suppress-and-apply failure-recovery transaction (prerequisite —
   UpdateSketchAttachmentCommand strict+rollback — already landed in H2).

## Open questions

- Unpushed: ~31 local commits on `master`; push only on explicit user request.
- Plan's unresolved questions (user never answered): Loft/Sweep creation UI
  hidden OK? Ellipse-creation removal OK long-term? Datum navigator-only
  fallback acceptable? Undo cap 200 fine? Legacy FaceRef suppress-only OK?
  Defaults were taken per plan; revisit if the user objects.
- H5-B redesign (parametric anchors) is future work, not scheduled in H7–H9.

## Pointers

- Tasks → TODO.md ("Now" + `Active: Hardening Roadmap` section)
- Snapshot → CURRENT_STATE.md
- Full plan/findings → `~/.claude/plans/act-as-senior-c-floating-fountain.md`
