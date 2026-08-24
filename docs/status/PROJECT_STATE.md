# TAUREON V4 – Project State

**Last updated:** 2026-08-24  
**Maintained by:** ChatGPT Classic / Project Manager

## Current stage

```text
Stage 0 – Bootstrap, inventory, provenance, architecture verification
Status: LOCAL VALIDATION COMPLETE / mandatory Claude Code review pending
Gate: HOLD RECOMMENDED
```

## Project location

Confirmed local project:

```text
D:\Eigene Dateien\Eigene Dokumente\Playground\TAUREON-Synth-Tool-V4
```

GitHub repository:

```text
https://github.com/sscheidl/TAUREON-Synth-Tool-V4 (private; origin configured locally; no branch has been pushed yet)
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

Stage 0 recorded the current WMS C++/WinRT namespace, documented initialization/API-mode route, endpoint/group identity guidance, and SysEx7 helper in `docs/reference/ENVIRONMENT_REPORT.md`.

Still requiring Stage 1 spike evidence:

- exact pinned WMS package/runtime deployment pairing;
- timestamp normalization;
- WMS maximum transmission constraints and actual SysEx7 callback sequences;
- WinMM persistent identity and `MIDIHDR` lifecycle;
- Qt/WMS initialization/lifetime interaction.

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

No P0/P1 technical blocker was found in local Stage 0 validation.

Gate prerequisite pending: mandatory Claude Code Stage 0 review.

## Exact next action

Complete the Stage 0 review and resolve the Stage 0 gate. Do not begin Stage 1 before that gate is resolved.

## Hardware validation

Not started for V4.

Previous hardware evidence belongs to the legacy/reference inventory until explicitly migrated as fixture/evidence.
