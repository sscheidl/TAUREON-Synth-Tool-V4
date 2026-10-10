# Changelog

All notable TAUREON V4 changes will be documented here.

## [Unreleased]

## [0.1.0-alpha.2] - 2026-10-10

Second experimental Windows x64 preview (GitHub pre-release `v0.1.0-alpha.2`).
Product-path MIDI/SysEx transfer and device restore are still not validated;
Stage 5 remains open.

### Connection and WMS

- Auto now tries Windows MIDI Services first and falls back to WinMM when WMS is
  unavailable or lists no routes; it never opens a route automatically. Either
  port field offers the backend choice on a fresh start.
- After a failed connect or disconnect the route selectors and Connect stay
  usable for a retry.
- The WMS backend uses the Windows MIDI Services Developer Preview 9 API
  (`Windows.Devices.Midi2` `0.99.83-devpreview.9`) instead of the RC4 SDK and
  reports missing API activation, Legacy API mode and unreachable service
  separately. The release ZIP does not contain Microsoft's preview API files, so
  WMS works only where that API is provided by Windows or obtained separately;
  WinMM is unaffected.
- `--list-midi wms|winmm` lists routes or the initialization error without
  opening an endpoint.
- Port fields and dropdowns show long route names; the title shows the version
  and build revision.

### MIDI Monitor and SysEx

- Follow latest keeps up with incoming batches and pauses when you scroll up.
- RX/TX and event-type filters with "All event types" and "Notes only" presets;
  split WinMM SysEx chunks stay in the SysEx category and the channel filter
  covers MIDI 2.0 channel-voice messages.
- A manually selected profile can label known CCs; a conservative Waldorf Protein
  profile is included.
- SysEx Transfer shows raw bytes as the main view; the Raw Send prompt is short,
  the exact route stays visible and is rechecked before sending. The SysEx
  Manager tab is hidden.

### Documentation

- Reconciled project status with GitHub: PR-#12 review threads resolved and B-3
  closed, first alpha recorded, Summit fixture absent from all branch tips.

## [0.1.0-alpha.1] - 2026-10-02

First public, experimental Windows x64 preview (GitHub pre-release
`v0.1.0-alpha.1`, built from `bab37a5`). Product-path MIDI/SysEx transfer and
device restore are not yet validated; Stage 5 remains open.

### Release

- Set the product application version to `0.1.0-alpha.1`.
- Portable, unsigned Windows x64 ZIP with app-local Qt and MSVC runtime files,
  device profiles, license notices, and a SHA-256 sums file.
- Added a redacted GUI screenshot to the README and contributor guidance to
  `CONTRIBUTING.md`.

### Stage 5 product GUI

- Qt 6 desktop application with MIDI Monitor, SysEx transfer and file
  inspection, Devices & Profiles, Diagnostics, Settings, and a bounded
  Librarian foundation; the Summit profile supports identification and
  read-only inspection only.
- Product-host lifecycle regression proves receive-active close, successful
  native close, and GUI/WMS apartment ownership for WMS and WinMM.

### MIDI engine package

- Separated the generic MIDI/SysEx/transfer engine from the Qt product host and
  added an installed CMake package with independent Debug/Release verification.
- Assigned the experimental engine package its own version, independent of the
  product application version. Version `0.2.0` makes the one-shot transfer
  lifecycle explicit and rejects a second start after cancellation or completion.
- Version `0.3.0` prefixes installed headers with `taureon/`, adds honest
  provider-scoped external routes with exact persistence/resolution, and states
  the callback/close-lifetime contract for transport implementers.
- `MidiTransportDiagnostics` includes the WMS-only
  `worker_mta_apartment_observed` evidence field.
- Licensed the current repository source under MIT; the previously restricted
  real Summit dump was removed rather than included under that license.
- Replaced the restricted real Summit SysEx test fixture with an artificial
  frame generated only in the build directory; no `.syx` fixture remains tracked
  in the current source tree.

### Project reset

- Established TAUREON V4 as a clean native C++20 / Qt 6 rebuild.
- Separated the new V4 project from the legacy TAUREON repository.
- Reorganized project documentation into product, architecture, process, design, status, stage, and reference areas.
- Defined ChatGPT Classic as Project Manager/Supervisor, Codex as Implementation Lead, Claude Code as Architecture/Review Lead, and the User as Product Owner/Hardware Tester/final decision authority.
- Added the initial clickable GUI workflow mockup as a design-only artifact.
