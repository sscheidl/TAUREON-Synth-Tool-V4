# Changelog

All notable TAUREON V4 changes will be documented here.

## [Unreleased]

### Project reset

- Established TAUREON V4 as a clean native C++20 / Qt 6 rebuild.
- Separated the new V4 project from the legacy TAUREON repository.
- Reorganized project documentation into product, architecture, process, design, status, stage, and reference areas.
- Defined ChatGPT Classic as Project Manager/Supervisor, Codex as Implementation Lead, Claude Code as Architecture/Review Lead, and the User as Product Owner/Hardware Tester/final decision authority.
- Added the initial clickable GUI workflow mockup as a design-only artifact.

### SysEx/MIDI reference archive v1

- Added `reference/` as a development-only, machine-readable SysEx/MIDI reference archive
  (device identity, dump types, bank layout, checksum status, fixture and external-source
  cataloging), kept explicitly separate from the runtime device profiles in
  `resources/device_profiles/`. See `reference/README.md`.
- Added `reference/schema/device-reference.schema.json` and a first proof-of-concept entry,
  `reference/devices/novation/summit/device.json`, reusing the existing byte-verified
  `tests/fixtures/novation_summit_crazy_sine.syx` fixture rather than duplicating it.
- Added `tests/unit/ReferenceArchiveTests.cpp` (backed by the new dev/test-only
  `tests/support/MinimalJson.hpp`) proving the schema is valid and self-consistent, the
  example entry validates against it, its fixture reference is byte-accurate, an entry
  missing a required field is rejected, and neither `reference/` nor `tests/` leak into the
  build output the app deploys from.
