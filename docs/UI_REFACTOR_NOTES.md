# UI Refactor Notes

Progress log for the UI-only modernization (see plan). Scope-locked: `src/ui/**`,
`src/render/` overlay positioning only, `resources/**`, CMake/qrc. Forbidden (read-only):
`src/core/`, `src/kernel/`, `src/io/`, `src/app/commands/`, `src/app/document/`,
`third_party/`.

## Build / run / test

```bash
# configure (once) — build/ already configured
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build build                                            # full app
cmake --build build --target onecad_ui                         # UI lib fast check
cmake --build build --target test_compile && ./build/tests/test_compile   # UI validate
make test                                                      # core regression (3)
ONECAD_HEADLESS=1 make run                                     # headless smoke
make run                                                       # launch (user screenshots)
```

Baseline (pre-change): `test_compile` builds clean on `master`, working tree clean.

## Verification gate (every phase)

1. `cmake --build build` + `test_compile` green.
2. `make test` green + headless smoke.
3. `git diff --stat` → zero changes under forbidden dirs.
4. (Phase 2+) `grep -rnE '#[0-9a-fA-F]{3,8}|QColor\(' src/ui | grep -v ThemeConfig | grep -v IconLoader` → empty.
5. Commit (Conventional `ui:`), one per bug/component.
6. Update this file; pause for user light+dark screenshots.

## Style-leak inventory (Phase 2 targets)

Palette source of truth (keep raw QColor): `theme/ThemeConfig.cpp` (~200), `ThemeConfig.h` (2).
Accent seed (keep): `components/IconLoader.cpp:15,18`.

Real leaks to tokenize:
- `viewport/ViewportRendering.cpp:470-471,567` — rubber-band + fallback bg QColors
- `start/ProjectTile.cpp:167,169` — hover fallback QColor
- `components/ToggleSwitch.cpp:40` — knob white
- `components/ModalOverlay.cpp:215` — `#e05252` error fallback
- `viewport/SnapSettingsPanel.cpp:139` — `#333` separator (fixed in P1.2)
- `mainwindow/MainWindow.cpp:1428,1438` — `font-size:10px`

Inline `setStyleSheet` w/ literal px/radii/fonts: `StartOverlay`, `ProjectTile`, `ModalOverlay`,
`SnapSettingsPanel`, `RenderDebugPanel`, `SketchModePanel`, `ConstraintPanel`, `DimensionEditor`,
`CommandPalette`, `HistoryPanel`, `FeatureCard`.

Widgets using raw `palette(...)` roles: `SketchModePanel`, `ConstraintPanel`, `RenderDebugPanel`.

Generated-sheet literals: `ThemeManager.cpp` L266(8px), L378/384/396(font), L417/420/432/438/450/456, L361-362(42px).

## Phase → file map

| Phase | Files |
|---|---|
| P1.1 search overlap | `start/StartOverlay.cpp` |
| P1.2 snap headers | `viewport/SnapSettingsPanel.cpp` |
| P1.3 nav selection | `theme/ThemeManager.cpp`, `navigator/ModelNavigator.cpp` |
| P1.4 status label | `mainwindow/MainWindow.cpp` |
| P1.5 panel context | `mainwindow/MainWindow.cpp`, `history/HistoryPanel.*`, `inspector/PropertyInspector.*` |
| P1.6 overlay anchor | new `viewport/OverlayAnchorLayout.*`, `mainwindow/MainWindow.cpp`, `viewport/ViewportRendering.cpp` |
| P2 tokens | `theme/ThemeConfig.*`, `theme/ThemeManager.cpp` + sweep |
| P3 components | toolbar/navigator/inspector/statusbar/popover/overlay |
| P4 start screen | `start/StartOverlay.*`, `start/ProjectTile.*` |

## Changed

_(append per phase)_

## Deferred

_(append)_

## Blocked — needs core change

_(none yet)_
