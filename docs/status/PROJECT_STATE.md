# TAUREON V4 – Project State

**Last updated:** 2026-08-24  
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
Status: HOLD / targeted-review P1 remediated / PASS recommended / gate disposition pending
Implementation lead: Codex

Stage 3 – Realtime MIDI + SysEx engine
Status: PLANNED / NOT STARTED / not authorized
```

## Project location

Confirmed local project:

```text
D:\Eigene Dateien\Eigene Dokumente\Playground\TAUREON-Synth-Tool-V4
```

GitHub repository:

```text
https://github.com/sscheidl/TAUREON-Synth-Tool-V4 (private; `main` pushed through Stage 2 commit `9bcbf0d`)
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

The targeted Claude Code Stage 2 review found one P1 in WinMM partial-open `MIDIHDR` unwind and placed the gate
on HOLD. Codex confirmed and fixed the defect by registering prepared buffers under transport ownership before
submit, then added an exact transport-level regression. No unresolved P0/P1 remains in the closure candidate.
The clean CI suite and final local regressions pass; every uniquely named temporary WMS-native loopback was
removed after testing. Targeted re-review and Project Manager/User gate disposition are pending.

Pinned RC4 limitations remain visible but are not transport assumptions: the newer API-mode query is absent
from the pinned metadata, and the isolated WinMM/WMS correlation helpers fail-fast and are not used for
identity. Details are in [`STAGE_1_REPORT.md`](../stages/STAGE_1_REPORT.md).

## Mandatory later-stage inputs

- **Stage 2 implementation complete; gate on HOLD:** Backend-specific persistence/resolution, WinMM worker
  requeue/RAII, WMS MTA lifetime, P1 closure regression, and opt-in local regressions are implemented and
  recorded in [`STAGE_2_REPORT.md`](../stages/STAGE_2_REPORT.md).
- **Stage 3:** Prove byte-exact MIDI 1.0 `F0 ... F7` SysEx <-> UMP SysEx7 conversion, segmentation, and
  reassembly. Preserve the Stage 2 transport/ownership invariants. R-003 remains open.
- **Stage 5:** Repeat apartment/lifetime/close-active/shutdown validation in the actual `QApplication` host
  and measure actual active state at close.

## Stage 0 evidence correction (2026-08-24)

The recorded WMS integration route was re-derived from the installed `Microsoft.Windows.Devices.Midi2.winmd`.
The earlier entry named a wrong namespace root and three identifiers that do not exist in the SDK. The
Stage 0 report's review record was also corrected: the reviewer's verdict was HOLD with two P1 findings, not
the "PASS WITH NON-BLOCKING FOLLOW-UPS" that had been entered. Stage 0 closure per D-012 stands; only the
evidence and the review record changed. Details in [`STAGE_0_REPORT.md`](../stages/STAGE_0_REPORT.md).

## Exact next action

Claude Code/Project Manager/User reviews the documented P1 closure in
[`STAGE_2_REPORT.md`](../stages/STAGE_2_REPORT.md) and records the Stage 2 gate disposition. Stage 3 remains
planned, not started, and not authorized; its brief requires separate approval.

## Hardware validation

Not started for V4.

Previous hardware evidence belongs to the legacy/reference inventory until explicitly migrated as fixture/evidence.
