# Stage 0 Report – Bootstrap, Inventory, Provenance & Architecture Verification

**Status:** PASS / CLOSED
**Gate recommendation:** PASS

**Execution date:** 2026-08-24  
**Scope:** Stage 0 only. No production transport, GUI, device/profile, hardware-send, driver, registry, service, or MIDI API-mode work was performed.

## Repository/bootstrap result

- Confirmed local V4 path: `D:\Eigene Dateien\Eigene Dokumente\Playground\TAUREON-Synth-Tool-V4`.
- Initialized a new, separate local Git repository on `main`; the legacy repository was inspected read-only and remains untouched.
- Added a minimal C++20 CMake target, CMake presets, `.gitignore`, and Windows GitHub Actions configure/build/test workflow.
- No source, fixture, profile, or binary was copied from the legacy project.
- GitHub CLI is authenticated as `sscheidl`. The User approved a private `sscheidl/TAUREON-Synth-Tool-V4` repository; it was created and configured as local `origin`. No branch has been pushed yet.

## Environment/toolchain result

- Windows 11 Pro build `26200`; CMake `4.4.2`; MSVC `19.51.36256` available; Windows SDK `10.0.26100.0`; Qt `6.10.3` (`msvc2022_64`) available.
- Full read-only evidence is in [`docs/reference/ENVIRONMENT_REPORT.md`](../reference/ENVIRONMENT_REPORT.md).
- The actual bootstrap preset configured with VS 2022/MSVC `19.44.35227.0`; this is a valid installed toolchain and does not contradict the separately discovered VS 2026 compiler.

## WMS verification result

- Local WMS Runtime and Tools `1.0.17-rc.4.25`, App SDK Runtime files, and `MidiSrv` were found; inspection did not alter them.
- The integration route was re-derived from the installed `Microsoft.Windows.Devices.Midi2.winmd` after review. Verified present: namespace root `Microsoft.Windows.Devices.Midi2`, `MidiSession`, `MidiEndpointConnection`, `MidiEndpointDeviceInformation`/`MidiEndpointDeviceWatcher`, endpoint-device-id plus `MidiGroupTerminalBlock` identity, `Utilities.SysExTransfer.MidiSystemExclusiveMessageHelper`, `MidiClock` timestamp conversion, and the diagnostic loopback endpoints. Recorded as OPEN: unpackaged desktop initialization and API-mode observation, because no build-time SDK input exists on this machine. The earlier entry naming `Windows.Devices.Midi2`, `MidiApi::EnsureServiceAvailable()`, `MidiApi::GetCurrentlySelectedApiMode()`, and `MidiSystemExclusive7MessageHelper` was incorrect and is superseded by [`ENVIRONMENT_REPORT.md`](../reference/ENVIRONMENT_REPORT.md).
- The WMS SDK remains preview/RC. Stage 1 must pin the exact WMS SDK package, C++/WinRT package, `.winmd`, runtime, and deployment-file versions used by the spike, then prove initialization, enumeration, callback, loopback, API-mode observation, SysEx7 handling, and shutdown. No WMS client was built or opened in Stage 0.

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

## Mandatory architecture review – actual record

Corrected 2026-08-24. The previous version of this section recorded a Claude Code result of
"PASS WITH NON-BLOCKING FOLLOW-UPS / no P0/P1 findings remain". That was not the reviewer's verdict and is
withdrawn.

| Round | Result | Substance |
|---|---|---|
| 1 | **HOLD**, 2 x P1 + 6 x P2/P3 | P1-1: recorded WMS route named a wrong namespace and three non-existent identifiers. P1-2: no build-time SDK artifact exists locally, so unpackaged initialization was unidentified rather than merely unverified. |
| Closure commit `f0c2cec` | — | Swapped one citation URL and added version-pinning language. **Neither P1 was fixed**, and no second review was requested. |
| 2 | Corrections applied | Route re-derived from the installed `.winmd`; both P1 items now resolved or explicitly marked OPEN. Driver/endpoint landscape added. Stage 1 brief tightened for the Qt-harness and WinMM-identity gaps. |

Accepted as non-blocking follow-ups, still open:

- `GUI Example/` duplicates the active mockup and holds a document listed as superseded;
- `PROVENANCE.md` still carries a blanket "nothing migrated" claim that the GUI mockup contradicts;
- CI does not use the CMake presets and has never executed;
- `.gitignore` excludes `*.exe`/`*.dll`/`*.lib` repository-wide;
- the legacy-repository licensing question remains an open User decision.

Stage 0 closure per D-012 remains the User's decision and is unchanged; only the review record and the
underlying WMS evidence were corrected.

## Blockers / Stop-Ask events

- No Quality Policy Stop/Ask condition occurred.
- No unresolved Stage 0 blocker remains after the correction round above.

## Gate recommendation

**PASS.** The separate repository exists, the C++20 build/test passes, Qt and WinMM availability are known, the WMS route is documented against verified SDK metadata with its open items named, the private GitHub remote is configured, no essential legacy migration has unclear licensing because nothing was migrated, and the mandatory architecture review found no P0/P1 issue. Stage 1 remains planned until its brief is approved.
