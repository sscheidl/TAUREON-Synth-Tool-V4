# ADR-0003 – Export the generic MIDI engine separately from the product host

**Status:** Proposed — internal reuse package exists at 0.3.0; lifetime review pending
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

This is only a shared internal library for the owner's future TAUREON/Synth
projects, not a public SDK or general MIDI framework. It does not change wire
formats or extend Stage 3 to MIDI 2.0 Channel Voice, MPE, SysEx8, or device
protocols. The current source tree is MIT licensed. Version 0.3.0 identifies
the package but does not promise source or binary compatibility across revisions.

## Consequences and review questions

- Existing product and test targets should preserve behavior; a build and the
  existing focused suites must verify linkage after the split.
- Installed public headers now live under `taureon/...`; review the generated
  install-copy approach and ensure no unprefixed headers escape the package.
- `MidiBackend::external` and provider-scoped endpoint IDs let another internal
  transport avoid impersonating WMS or WinMM. Exact route matching remains
  mandatory. Review schema-v1 compatibility and failure behavior; no global
  provider registration system is needed for our own projects.
- `IMidiTransport` now documents callback-thread freedom, in-flight handler
  replacement, and control serialization, but intentionally does not promise
  quiescence at `close()` return. Determine and enforce a usable close/destructor
  ownership rule against WMS, WinMM, fake, and an internal custom transport
  before reusing the native backends. Inspect native close-failure paths.
- `TransferEngine` is explicitly one-shot since 0.2.0. Its owner serializes
  `start()`/`wait()`/destruction, keeps the transport alive, and must not call
  `wait()` or destroy the engine from a progress handler. For internal reuse,
  document this ownership pattern and verify it in the consuming application.
- The installed package proves use of the generic engine, not deployment of
  native transports. A second project requiring WMS/WinMM must currently build
  those product-local targets or share their source separately. ABI policy,
  public SDK governance, and platform-neutral packaging are out of scope.
- Review the layer boundary, installed dependency closure, and transport
  lifetime rules for internal reuse before this ADR becomes Accepted.
