# ADR-0003 – Export the generic MIDI engine separately from the product host

**Status:** Proposed — independent architecture review and Product Owner gate pending
**Date:** 2026-09-27
**Owner:** Codex / Implementation Lead

## Context

The Stage-2/3 generic MIDI, SysEx, and transfer code is Qt-independent, but its
former CMake target also included Stage-4/5 application services, profiles, and
fake transport. Product builds required Qt even when a consumer needed only the
generic engine. No installed package or separate-consumer proof existed.

## Proposed decision

- Keep core source and public transport contracts unchanged.
- Build generic source as `taureon_midi_engine`, with an installed CMake package
  and exported `TaureonMidiEngine::MidiEngine` target.
- Keep application services/profiles/fake transport in the product-only
  `taureon_midi_core` target, linked to the engine.
- Link native WMS and WinMM transports to the engine rather than the product
  application-services target. Their own SDK/Windows requirements remain.
- Prove package use from a distinct consumer project, built without Qt.

This does **not** declare a stable SDK, change any wire-format behavior, or
extend Stage 3 to MIDI 2.0 Channel Voice, MPE, SysEx8, or device protocols.

## Consequences and review questions

- Existing product and test targets should preserve behavior; a build and the
  existing focused suites must verify linkage after the split.
- Installed public headers expose the existing `core/...` and
  `transports/IMidiTransport.hpp` paths. Review whether this namespace/layout
  is acceptable before freezing an API or assigning a nonzero package version.
- Native backend packaging, ABI policy, licensing, and cross-platform support
  remain separate decisions. This package currently proves reuse of the
  platform-neutral engine, not turnkey native transport deployment.
- Claude Code should review the layer boundary and exported dependency closure
  before this ADR becomes Accepted.
