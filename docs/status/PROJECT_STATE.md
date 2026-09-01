# TAUREON V4 – Project State

**Last updated:** 2026-09-01
**Maintained by:** ChatGPT Classic / Project Manager

## Current stage

```text
Stage 0 – Bootstrap, inventory, provenance, architecture verification
Status: PASS / CLOSED
Gate: PASS

Stage 1 – Native transport spikes (WMS + WinMM)
Status: PASS / CLOSED
Gate: PASS WITH NON-BLOCKING FOLLOW-UPS (mandatory Claude Code review; no P0/P1 findings)

Stage 2 – MIDI Core + transport abstraction
Status: PASS / CLOSED after targeted-review P1 remediation and User gate
Implementation lead: Codex

Stage 3 – Realtime MIDI + SysEx engine
Status: PASS / CLOSED after targeted HOLD remediation and User/Project Manager gate
Gate: PASS WITH NON-BLOCKING FOLLOW-UPS (targeted Claude Code re-review; no P0/P1 findings)
Implementation lead: Codex

Stage 4 – Device/profile isolation proof
Status: PASS / CLOSED at revision 986115d
Gate: PASS WITH NON-BLOCKING FOLLOW-UP (mandatory Claude Code review; FU-3 is a Stage-5 brief input)
Implementation lead: Codex
Target: Generic profile plus exactly one Novation Summit data profile

Stage 5 – Qt 6 Product GUI
Status: ACTIVE / Block A merged; targeted F-1–F-5 follow-up pending
Gate: HOLD — Block B native/local/hardware/visual evidence remains outstanding
Implementation lead: Codex
```

## Stage 5 readiness — Block A

Slice 9a was merged at `6b6b4d9191e592826afa1bfa2ea7f0ad5703e562`. Block A was merged at `bd4dd4f26bf19c0bd25ef2f2136e70cb470365b8` from reviewed PR #7 head `a4627d9113fa9191423171616e56b0a267fee763`. Windows CI #220 passed at the merge head with a full Debug build and 24/24 CTest, 0 failed, 0 skipped. A separate targeted follow-up handles F-1 through F-5; F-6 is a structural retained-evidence mapping with no action.

Block A closes deterministic cloud/software evidence: N-1, N-2, N-3, N-6, S7-3, S7-4, the fake close-while-in-flight proof, large SysEx malformed/incomplete/tainted rejection, and Stage-5-specific large cancellation. L-3 is unchanged: 256 MiB per document and 512 MiB aggregate raw workspace payload are resource limits, not MIDI/SysEx protocol limits and not total-process-memory bounds. Detailed traceability is in [`STAGE_5_REPORT.md`](../stages/STAGE_5_REPORT.md).

Still deliberately outstanding for Block B on the Product Owner's Windows machine after 09.09.2026:

- B-3: actual `QApplication` WMS/WinMM apartment, active-close, and shutdown-lifetime evidence.
- B-4 visual half: 1920×1080 Windows visual inspection at 125%, 150%, and 200% scaling.

The five hardware/loopback tests remain **NOT REGISTERED — NOT SKIPPED — NOT SIMULATED**. No real MIDI device, port, WMS/WinMM runtime/timing, visual Windows quality, or High-DPI validation is claimed.

## Project location

Confirmed local project:

```text
D:\Eigene Dateien\Eigene Dokumente\Playground\TAUREON-Synth-Tool-V4
```

GitHub repository:

```text
https://github.com/sscheidl/TAUREON-Synth-Tool-V4 (private; `main` includes the dedicated Stage-3 completion commit recorded in Git history)
```

Legacy local reference:

```text
D:\Eigene Dateien\Eigene Dokumente\Playground\TAUREON-Synth-Tool
```

Legacy GitHub:

```text
https://github.com/sscheidl/TAUREON-Synth-Tool
```

## Current architecture baseline

- clean native C++20 rebuild;
- Qt 6 Widgets;
- CMake;
- Windows 11 x64 primary target;
- direct Windows MIDI Services backend;
- native WinMM compatibility backend;
- one active backend per logical connection;
- device-specific logic above transport;
- generic SysEx byte integrity is a core invariant.

Stage 2 production implementation established:

- backend-specific, versioned persisted route identity with exact/missing/ambiguous/invalid resolution;
- no WinMM index-only persistence, fuzzy fallback, or WMS↔WinMM translation;
- a Qt-independent `IMidiTransport`, error/result model, lifecycle states, native message representation, and
  fake transport;
- WMS worker/MTA ownership with deterministic session/SDK/apartment teardown;
- WinMM callback→bounded queue→worker requeue and deterministic `MIDIHDR` owners;
- transport-level WinMM submit-failure coverage proving unprepare-before-close on partial-open unwind;
- ordinary CI unit tests plus separately opt-in local WMS/WinMM regressions.

Stage 0 recorded the current WMS C++/WinRT namespace, documented initialization/API-mode route, endpoint/group identity guidance, and SysEx7 helper in `docs/reference/ENVIRONMENT_REPORT.md`.

Stage 1 implementation established evidence for:

- exact pinned WMS package/runtime deployment pairing, and acquisition of any build-time SDK input (none present locally);
- unpackaged desktop initialization and API-mode observation entry points;
- immediate-send `MidiClock` metadata; timestamp normalization remains an explicit future decision;
- actual SysEx7 callback sequence and byte-exact UMP payload handling, but not MIDI 1.0 `F0`/`F7` framing
  conversion;
- backend-specific WMS and WinMM identity behavior;
- 100 WinMM open/close lifecycles, including one complete short/SysEx send/receive/`MOM_DONE` cycle;
- Qt Core/QCoreApplication plus dedicated WMS MTA-worker interaction, not the production QApplication host.

## Governance

- User: Product Owner, Hardware Tester, final decision authority.
- ChatGPT Classic: Project Manager / Supervisor / Context Keeper.
- Codex: Implementation Lead.
- Claude Code: Architecture & Review Lead.

## Documentation status

Stage 0 local evidence is now available:

- README
- product vision
- architecture baseline
- development process
- AI collaboration
- quality policy
- GUI design specification
- clickable GUI mockup
- C++20/CMake bootstrap and initial Windows CI workflow
- environment/toolchain report
- legacy inventory
- provenance record
- Stage 0 report

Earlier TAUREON2 planning documents are reference sources, not active specifications.

## Current blockers

No known Stage-4 implementation P0/P1 exists. Stage 4 is PASS/CLOSED at revision `986115d` after the mandatory
Claude Code architecture review, with FU-3 carried forward as a non-blocking Stage-5 GUI/evidence refinement.
No Stage-3 blocker or unresolved P0/P1 remains. The historical review P1 ordered-loss gap and both P2 findings
were corrected without an architecture deviation and accepted by targeted Claude Code re-review. The authorized
WMS investigation recorded complete 100- and 500-cycle handle series and confirmed reversible whole-process
runtime plateaus rather than cumulative ownership growth. The retained harnesses now test directional sustained
growth, not min/max dispersion; synthetic fast/slow leaks fail and all final local and clean-CI suites pass. The
Stage-2 partial-open P1 remains closed by its transport-level regression. Stage-3
ordered-loss, exact conversion, grouped decode-failure, `.syx` I/O-diagnostic, large-data, clean-CI, and local
WMS/WinMM evidence are recorded in [`STAGE_3_REPORT.md`](../stages/STAGE_3_REPORT.md).

Pinned RC4 limitations remain visible but are not transport assumptions: the newer API-mode query is absent
from the pinned metadata, and the isolated WinMM/WMS correlation helpers fail-fast and are not used for
identity. Details are in [`STAGE_1_REPORT.md`](../stages/STAGE_1_REPORT.md).

## Mandatory later-stage inputs

- **Stage 2 PASS / CLOSED:** Backend-specific persistence/resolution, WinMM worker
  requeue/RAII, WMS MTA lifetime, P1 closure regression, and opt-in local regressions are implemented and
  recorded in [`STAGE_2_REPORT.md`](../stages/STAGE_2_REPORT.md).
- **Stage 3 PASS / CLOSED:** Claude Code returned PASS WITH NON-BLOCKING FOLLOW-UPS with no P0/P1 findings;
  the two reviewer follow-ups are mandatory Stage-4-brief inputs and R-003 remains mitigated, not closed.
- **Stage-4 FU-1 implemented:** WinMM overflow markers coalesce only across consecutive dropped callbacks;
  non-droppable long-header events break the run so later drops receive a new ordered marker. The blocked-worker,
  sustained-overflow regression proves two affected frames are tainted and a later clean frame is not.
- **Stage-4 FU-2 implemented:** `new_steady_high=false` when `start == 0`, because no independent warm-up
  envelope exists; the short rising-series regression remains non-applicable rather than self-referential.
- **Stage 4 PASS / CLOSED:** strict profile schema/loader, deterministic registry/matching, Generic fallback, one
  bounded Summit profile, and the User-approved read-only fixture were accepted. FU-3 (P3) requires the future
  Stage-5 GUI to disclose a manual selection overridden by stronger fingerprint evidence and offer saved-binding
  promotion; the matching order itself remains unchanged.
- **Stage 5:** Repeat apartment/lifetime/close-active/shutdown validation in the actual `QApplication` host
  and measure actual active state at close.

## Stage 0 evidence correction (2026-08-24)

The recorded WMS integration route was re-derived from the installed `Microsoft.Windows.Devices.Midi2.winmd`.
The earlier entry named a wrong namespace root and three identifiers that do not exist in the SDK. The
Stage 0 report's review record was also corrected: the reviewer's verdict was HOLD with two P1 findings, not
the "PASS WITH NON-BLOCKING FOLLOW-UPS" that had been entered. Stage 0 closure per D-012 stands; only the
evidence and the review record changed. Details in [`STAGE_0_REPORT.md`](../stages/STAGE_0_REPORT.md).

## Exact next action

Complete the comprehensive independent review of the frozen Block-A PR head after its final Windows CI is green. Do not merge and do not begin Stage 6. Block B remains the later real Windows/local/manual validation.

## Hardware validation

Not started for V4.

Previous hardware evidence belongs to the legacy/reference inventory until explicitly migrated as fixture/evidence.