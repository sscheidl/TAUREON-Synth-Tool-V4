# Stage 0 Report – Bootstrap, Inventory, Provenance & Architecture Verification

**Status:** LOCAL VALIDATION COMPLETE / REVIEW PENDING  
**Gate recommendation:** HOLD

**Execution date:** 2026-08-24  
**Scope:** Stage 0 only. No production transport, GUI, device/profile, hardware-send, driver, registry, service, or MIDI API-mode work was performed.

## Repository/bootstrap result

- Confirmed local V4 path: `D:\Eigene Dateien\Eigene Dokumente\Playground\TAUREON-Synth-Tool-V4`.
- Initialized a new, separate local Git repository on `main`; the legacy repository was inspected read-only and remains untouched.
- Added a minimal C++20 CMake target, CMake presets, `.gitignore`, and Windows GitHub Actions configure/build/test workflow.
- No source, fixture, profile, or binary was copied from the legacy project.
- GitHub CLI is authenticated as `sscheidl`. Neither `TAUREON-Synth-Tool-V4` nor `TAUREON-Synth-Tool4-V4` exists under that account, but no remote was created because the final remote name and visibility were not specified after the local-name correction.

## Environment/toolchain result

- Windows 11 Pro build `26200`; CMake `4.4.2`; MSVC `19.51.36256` available; Windows SDK `10.0.26100.0`; Qt `6.10.3` (`msvc2022_64`) available.
- Full read-only evidence is in [`docs/reference/ENVIRONMENT_REPORT.md`](../reference/ENVIRONMENT_REPORT.md).
- The actual bootstrap preset configured with VS 2022/MSVC `19.44.35227.0`; this is a valid installed toolchain and does not contradict the separately discovered VS 2026 compiler.

## WMS verification result

- Local WMS Runtime and Tools `1.0.17-rc.4.25`, App SDK Runtime files, and `MidiSrv` were found; inspection did not alter them.
- Official documentation identifies a C++/WinRT C++20 integration route using `Windows.Devices.Midi2`, `MidiApi::EnsureServiceAvailable()`, `MidiApi::GetCurrentlySelectedApiMode()`, `MidiSession`, endpoint-device-id plus group identity, and `MidiSystemExclusive7MessageHelper`.
- The WMS SDK remains preview/RC. Stage 1 must pin the package/runtime pair and prove initialization, enumeration, callback, loopback, API-mode observation, SysEx7 handling, and shutdown. No WMS client was built or opened in Stage 0.

## WinMM verification result

- `C:\Windows\System32\winmm.dll` is present, version `10.0.26100.8875`.
- The old `engine3_native` PoC confirms useful research topics only. V4 will independently prove native enumeration, long-message buffering, `MIDIHDR` ownership, and teardown in Stage 1.

## Legacy/provenance result

- The legacy reference at commit `756b477e6d6de733f4ad5df12dd08b24ae449ae4` was sampled narrowly; its user-owned untracked files were not changed.
- No root license file was found. Legacy code is therefore not eligible for copying. No essential reuse is required.
- Three root-level SysEx dumps are explicitly incomplete and excluded as V4 fixtures. Nineteen generated profiles and historical device notes remain reference-only pending per-item provenance review.
- See [`docs/reference/LEGACY_INVENTORY.md`](../reference/LEGACY_INVENTORY.md) and [`docs/reference/PROVENANCE.md`](../reference/PROVENANCE.md).

## ADRs created/changed

None. Stage 0 recorded supported facts and retained implementation-sensitive choices for Stage 1 spikes; no architecture boundary changed.

## Tests/builds run

- `cmake --preset vs2022-x64` — configured successfully.
- `cmake --build --preset vs2022-x64-debug --parallel` — built `taureon_bootstrap` successfully.
- `ctest --preset vs2022-x64-debug` — passed `1/1` test, `taureon_bootstrap_runs`.

## Blockers / Stop-Ask events

- No Quality Policy Stop/Ask condition occurred.
- Gate prerequisites pending: mandatory Claude Code Stage 0 review and User decision on final GitHub repository name/visibility. These are not a WMS or toolchain failure.

## Gate recommendation

**HOLD.** Local technical acceptance evidence is sufficient to prepare the review handoff: the separate repository exists, the C++20 build/test passes, Qt and WinMM availability are known, the official WMS route is documented, and no essential legacy migration has unclear licensing because nothing was migrated. Do not start Stage 1 until the mandatory Claude Code review is complete and the GitHub repository decision is resolved.
