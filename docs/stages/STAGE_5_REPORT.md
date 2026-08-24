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
