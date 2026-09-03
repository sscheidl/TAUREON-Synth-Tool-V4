# TAUREON SysEx/MIDI Reference Archive v1

This directory is developer reference material for building device profiles, SysEx
recognition/classification, parsers, checksum handling, regression tests, and reverse
engineering. It is not user documentation, not a preset collection, and not a runtime
device profile.

## What lives here vs. elsewhere

| Concern | Where |
|---|---|
| Reference data (this archive): manufacturer/model facts, MIDI identity bytes, dump types, known sizes, bank layout, checksum status, external-source catalog | `reference/` |
| Runtime device profiles TAUREON actually loads at runtime | `resources/device_profiles/` (see `docs/architecture/PROFILE_SCHEMA_V1.md`) |
| Test fixtures (byte-exact `.syx` captures and their provenance) | `tests/fixtures/` (unchanged; see below) |
| Reference-archive schema/loading tests | `tests/unit/ReferenceArchiveTests.cpp` |

A `reference/devices/<manufacturer>/<model>/device.json` entry and a
`resources/device_profiles/*.profile.json` runtime profile describe the same device from
two different angles and are not required to carry the same fields, the same confidence
model, or the same lifecycle. Nothing here changes `PROFILE_SCHEMA_V1.md`, the profile
loader, or the profile registry. There is no reference-to-profile generator yet; that is a
deliberately deferred later step, not part of this v1 archive.

## Structure

```text
reference/
├─ schema/
│  └─ device-reference.schema.json
└─ devices/
   └─ <manufacturer>/<model>/
      ├─ device.json
      └─ fixtures/            (only when a device needs a fixture that isn't already
                                 an existing tests/fixtures/ entry — see below)
```

## `device.json`

One JSON object per device, validated against `schema/device-reference.schema.json`.
Kept deliberately small: manufacturer/model identity, MIDI manufacturer/family/model ID
bytes, device-ID behavior, known SysEx dump types and their headers/sizes, bank/slot
organization, checksum method, references to test fixtures, and a catalog of external
(manufacturer-provided) sources. No executable behavior, no protocol logic.

Every fact carries a `status`:

| Status | Meaning |
|---|---|
| `confirmed` | Directly verified against a byte-exact fixture or authoritative documentation |
| `observed` | Seen in real data, but its meaning or generality is not established |
| `inferred` | Reasoned from other evidence, not directly observed |
| `unknown` | Not established at all |

A `device.json` entry only ever gets more confident with new evidence — never upgrade a
status without a documented reason.

## Fixtures

Fixtures are technical test vectors only (valid single/bank dump, checksum test, parser
recognition, corrupted/truncated case, wrong manufacturer/model ID) — not a sound library
and not exhaustive coverage of every dump type.

`tests/fixtures/` already holds this repository's byte-exact, provenance-tracked SysEx
captures (see `docs/reference/PROVENANCE.md`), and Stage 4/5 tests already depend on them
by that path. A `device.json` entry reuses that existing fixture by repository-relative
path (its `fixtures[].path`) instead of duplicating the bytes under `reference/`. Add a
`reference/devices/<manufacturer>/<model>/fixtures/` directory only for a fixture that has
no existing home in `tests/fixtures/` — do not copy an existing fixture just to satisfy the
directory shape.

## External manufacturer sources

Not every `.syx` file or manufacturer document found online belongs in this repository.
When redistribution is unclear, `external_sources[]` catalogs it by source URL, original
filename, size, SHA-256, detected dump type, and a short technical note — without storing
the file itself. A file may only become a `fixtures[]` entry once it is a small,
technically useful test vector with a clear internal-use basis (an own dump, or a
manufacturer file whose redistribution is actually fine). Never copy a complete factory
sound library into this repository.

## Release separation

`reference/` (and `tests/`) must never be part of what ships to an end user. Today that
means: no `install()` or packaging rule references `reference/`, and the only file-copy
step in the build (`src/CMakeLists.txt`, deploying `resources/device_profiles/` beside
`taureon_app`) does not touch it. `tests/unit/ReferenceArchiveTests.cpp` checks both of
these mechanically; extend that check if real packaging (CPack, an installer) is added
later so it keeps proving the same thing.
