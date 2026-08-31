# TAUREON V4 – Project State

**Last updated:** 2026-08-31  
**Maintained by:** Product team

## Current stage

```text
Stage 0 – Bootstrap, inventory, provenance, architecture verification
Status: PASS / CLOSED

Stage 1 – Native transport spikes (WMS + WinMM)
Status: PASS / CLOSED

Stage 2 – MIDI Core + transport abstraction
Status: PASS / CLOSED

Stage 3 – Realtime MIDI + SysEx engine
Status: PASS / CLOSED

Stage 4 – Device/profile isolation proof
Status: PASS / CLOSED

Stage 5 – Qt 6 Product GUI
Status: ACTIVE / Slice 9a ready for targeted review
Gate: NOT COMPLETE
```

## Stage 5 readiness

Slice 8 is merged at `2ca2c246f15ab9ee5be6e9b04fb38e102d361032`. Slice 9a is the Draft software-completion review on `codex/stage5-software-completion` ([PR #6](https://github.com/sscheidl/TAUREON-Synth-Tool-V4/pull/6)).

The software-side Librarian foundation, model/view evidence, bounded diagnostics, generic SysEx workflow evidence, and L-3 resource policy are implemented. L-3 limits are product safety limits: 256 MiB per document and 512 MiB aggregate raw workspace payload; they do not claim protocol limits or bound total process memory.

Stage 5 is **not complete**. The following are deliberately outstanding for Slice 9b on the Product Owner's Windows hardware after 09.09.2026:

- **B-3:** real `QApplication` WMS/WinMM apartment, active-close, and shutdown-lifetime evidence.
- **B-4 visual half:** Windows visual captures/inspection at 1920×1080 and 125%, 150%, and 200% scaling.

The five hardware/loopback tests remain **NOT REGISTERED — NOT SKIPPED — NOT SIMULATED**. No real MIDI device, port, WMS/WinMM runtime/timing, visual Windows quality, or High-DPI validation is claimed.

## Architecture baseline

- C++20, CMake, Qt 6 Widgets, Windows x64 primary target.
- Exact backend-specific route identity; no fuzzy or cross-backend rebinding.
- Generic SysEx byte integrity, malformed/taint/DataLoss propagation, and transfer safety are core invariants.
- Device semantics stay above transport. Raw SysEx recognition is not a decoded Librarian preset.
- No Stage 6 work is authorized.

## Exact next action

Independently review Draft PR #6 for Stage 5 Slice 9a. Do not merge until that targeted review passes. Slice 9b remains the only authorized route to the outstanding B-3 and visual B-4 evidence.
