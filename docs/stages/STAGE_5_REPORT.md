# Stage 5 Report – Qt 6 Product GUI

**Stage:** 5
**Date opened:** 2026-08-24
**Implementation lead:** Codex
**Baseline revision:** `070cf6be1c4f562d7a3b8455616204e45b3f6d85`
**Status:** ACTIVE

## Start-gate record

Stage 4 is PASS/CLOSED at implementation revision `986115d`; the closure documentation handoff is pushed to
`origin/main`. Immediately before Stage-5 activation, `HEAD == origin/main` and the worktree was clean.

The User authorized Stage 5 and supplied the approved full `STAGE_5_BRIEF.md`. Its requirements are now tracked
in the repository. No Stage-5 product code has been changed at this opening record.

## Carried-forward Stage-4 follow-ups

- FU-1 (P2): implemented in Stage 4; no further GUI action beyond preserving the ordered-taint contract.
- FU-2 (P3): implemented in Stage 4; retain trend-based WMS resource evidence.
- FU-3 (P3): Stage 5 must preserve matching precedence; domain evidence must retain a temporary manual profile
  selection overridden by stronger fingerprint/identity evidence; the GUI must disclose it and offer deliberate
  promotion to a saved explicit binding. Tests must cover precedence, retained evidence, visible presentation, and
  explicit promotion.

## Initial architecture direction

The production target will be a Qt 6 Widgets `QApplication` host. Views will depend on Qt models and presentation
controllers; application services own workflow orchestration; the accepted Stage 2–4 transport, SysEx, transfer,
profile and route-identity core remains Qt-independent. The Stage-1 `taureon_wms_qt_harness` is retained only as
spike evidence and will not become the production host.

## Initial scope check

No conflict has yet been found between the approved brief and the accepted Stage-2–4 contracts. This report will
record composition-root, threading, test, performance, lifecycle, visual, and hardware-limit evidence as each
vertical slice is completed. Stage 6 is not authorized.

## Slice 1 — production host and shell baseline

The new `taureon_app` target is a real Qt 6 Widgets `QApplication` host. It is separate from the retained
Stage-1 `QCoreApplication` spike harness and links the Qt-independent `taureon_midi_core` only at the product
composition boundary. The initial shell provides a non-movable persistent connection bar and navigable pages for
the seven frozen workspaces. No transport is constructed, no route is selected, and no MIDI or hardware operation
can occur in this slice: connection selectors and Connect/Panic are intentionally disabled with an explanation.

`stage5_gui_smoke` constructs the production window offscreen and verifies the expected seven-workspace shell.
The first run exposed a runtime dependency issue: the Debug executable could not resolve `Qt6Cored.dll` when
CTest launched it with the ordinary process PATH. The observed Windows dialog named that missing DLL. The test
now prepends the existing `C:\Qt\6.10.3\msvc2022_64\bin` through CTest's process-local
`ENVIRONMENT_MODIFICATION`; this uses no DLL copying, Qt installation, permanent PATH change, driver, service,
registry, or MIDI configuration change. The resolved smoke test passes in 0.16 seconds.

For an interactive development start, `scripts/run-taureon-app.ps1` derives the configured Qt `bin` directory from
the local CMake cache and prepends it only to the launcher process before starting `taureon_app`. It likewise makes
no system-wide change and is not a release-deployment mechanism.

The host event-loop/lifetime proof is not claimed by this construction-only smoke test. It remains an explicit
later Stage-5 lifecycle slice under section 14 of the brief; no timeout or sleep was introduced to mask it.

### Current validation

```powershell
cmake --preset vs2022-x64
cmake --build --preset vs2022-x64-debug --config Debug --parallel
ctest --test-dir build/vs2022-x64 -C Debug -R '^stage5_gui_smoke$' --output-on-failure
git diff --check
```

Result: configure and Debug build pass; `stage5_gui_smoke` passes (1/1); `git diff --check` passes.
