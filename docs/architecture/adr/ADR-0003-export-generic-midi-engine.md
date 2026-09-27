# ADR-0003 – Export the generic MIDI engine separately from the product host

**Status:** Proposed — package exists at 0.2.0; stable SDK contract and Product Owner gate pending
**Date:** 2026-09-27
**Owner:** Codex / Implementation Lead

## Context

The Stage-2/3 generic MIDI, SysEx, and transfer code is Qt-independent, but its
former CMake target also included Stage-4/5 application services, profiles, and
fake transport. Product builds required Qt even when a consumer needed only the
generic engine. No installed package or separate-consumer proof existed.

## Proposed decision

- Separate the current core source without changing wire formats or persisted route identity.
- Build generic source as `taureon_midi_engine`, with an installed CMake package
  and exported `TaureonMidiEngine::MidiEngine` target.
- Keep application services and profiles in the product-only `taureon_midi_core`
  target, linked to the engine. Build fake transport in a separate, non-installed
  testing target when top-level tests are enabled.
- Link native WMS and WinMM transports to the engine rather than the product
  application-services target. Their own SDK/Windows requirements remain.
- Prove package use from a distinct consumer project, built without Qt.

This does **not** declare a stable SDK, change any wire-format behavior, or
extend Stage 3 to MIDI 2.0 Channel Voice, MPE, SysEx8, or device protocols.
The current source tree is MIT licensed. The independently versioned package is
0.2.0; version zero does not imply a frozen source or binary interface.

## Consequences and review questions

- Existing product and test targets should preserve behavior; a build and the
  existing focused suites must verify linkage after the split.
- Installed public headers still expose `core/...` and
  `transports/IMidiTransport.hpp` without a project prefix. Select and review a
  prefixed layout before a stable SDK release.
- `MidiBackend` and persisted route identity currently model WMS and WinMM only.
  A third-party transport must not impersonate either backend in a stable SDK;
  any generic identity design must preserve ADR-0001's exact-selection rule.
- `IMidiTransport` lacks a cross-backend callback thread, handler replacement,
  and close-quiescence contract. Specify and verify these against the native
  implementations before promising them to package consumers.
- `TransferEngine` is explicitly one-shot at 0.2.0. Its owner serializes
  `start()`/`wait()`/destruction, keeps the transport alive, and must not call
  `wait()` or destroy the engine from a progress handler. Stable SDK review must
  decide whether these restrictions are sufficient or need enforcement.
- Native backend packaging, ABI policy, and cross-platform support remain
  separate decisions. The installed package proves use of the generic engine,
  not native transport deployment or non-Windows compatibility.
- Claude Code should review the layer boundary, exported dependency closure,
  and public transport/lifetime decisions before this ADR becomes Accepted.
