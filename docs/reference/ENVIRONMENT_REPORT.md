# Stage 0 Environment and Toolchain Report

**Recorded:** 2026-08-24  
**Scope:** Read-only local inspection; no driver, registry, service, API-mode, or MIDI-route change was made.

## Host and build toolchain

| Item | Observed state | Evidence |
|---|---|---|
| Host OS | Windows 11 Pro, `10.0.26200`, build `26200` | `Win32_OperatingSystem` |
| CMake | `4.4.2` | `cmake --version` |
| MSVC | `19.51.36256` x64 | `VsDevCmd.bat -arch=x64` then `cl` |
| Visual Studio Build Tools | VS 2022 `17.14.37314.3` and VS 2026 `18.9.12105.275` installed | `vswhere -all -products *` |
| Windows SDK | `10.0.26100.0` | `C:\Program Files (x86)\Windows Kits\10\Include` |
| Qt | Qt `6.10.3`, kit `msvc2022_64` | `C:\Qt\6.10.3\msvc2022_64\bin\qtpaths.exe --qt-version` |
| Dependency manager | No active `vcpkg` or Conan root/environment marker found | PATH/environment and conventional-root inspection |

The Stage 0 skeleton deliberately has no Qt or WMS dependency. Qt availability is established; Qt application wiring remains Stage 5 and WMS consumption remains Stage 1.

## Windows MIDI Services (WMS)

| Item | Observed state |
|---|---|
| Installed runtime/tools | `Windows MIDI Services Runtime and Tools 1.0.17-rc.4.25` plus App SDK Runtime, Console, PowerShell, and Settings components |
| Service | `MidiSrv` was already `Running`, start type `Automatic`; it was only queried |
| App SDK files | `Microsoft.Windows.Devices.Midi2.dll`, `.pri`, and `.winmd` are present under `C:\Program Files\Windows MIDI Services\Desktop App SDK Runtime\` |
| Tools | Console and diagnostic tools are installed under `C:\Program Files\Windows MIDI Services\Tools\`; none was used to send MIDI or modify configuration |

### Current documented integration route

1. Use the WMS App SDK's C++/WinRT projection and the `Windows.Devices.Midi2` namespace; target a 64-bit C++20 desktop process. The official consumption guide states that CMake consumers generate projections with a compatible `cppwinrt.exe` and the supplied WMS `.winmd`.
2. On a dedicated MTA-capable MIDI worker, initialize the WinRT apartment, call `MidiApi::EnsureServiceAvailable()`, then query `MidiApi::GetCurrentlySelectedApiMode()` before creating a `MidiSession`. A `LegacyMode` result must be reported/fallback-handled, never changed by TAUREON.
3. Persist WMS routes using `MidiEndpointDeviceInformation.EndpointDeviceId` and, where applicable, `MidiGroupTerminalBlock.FirstGroup.Index`; names and numeric port indexes are not persistent identifiers.
4. Use `MidiSystemExclusive7MessageHelper` for SysEx7 UMP byte extraction/reassembly. Its correctness, transfer limits, timestamp conversion, and Qt worker lifecycle still require Stage 1 spike evidence.

Official sources (checked 2026-08-24):

- [Consuming the MIDI SDK from C++/CMake](https://microsoft.github.io/MIDI/kb/consuming-midi-api/)
- [Moving from WinMM to Windows MIDI Services](https://microsoft.github.io/MIDI/kb/moving-from-winmm-to-wms/)
- [MidiApiMode](https://microsoft.github.io/MIDI/sdk-reference/MidiApiModeEnum/)
- [Persistent endpoint identifiers](https://microsoft.github.io/MIDI/kb/identifiers/)
- [SysEx7 message helper](https://microsoft.github.io/MIDI/sdk-reference/Utilities/Messages/MidiSystemExclusive7MessageHelper/)

The current WMS SDK/release is still documented as preview/RC material. Stage 1 must pin the exact package/runtime pairing and prove a clean executable deployment; Stage 0 makes no production-readiness claim.

## WinMM

`C:\Windows\System32\winmm.dll` exists, version `10.0.26100.8875`. The legacy native PoC links `winmm`, but V4 did not reuse its code. WinMM enumeration, `MIDIHDR` ownership, and teardown remain explicit Stage 1 spike requirements.

## GitHub state

The authenticated GitHub account is `sscheidl`. Neither `sscheidl/TAUREON-Synth-Tool-V4` nor the earlier candidate `sscheidl/TAUREON-Synth-Tool4-V4` currently resolves through GitHub CLI. No remote repository was created because its final name and visibility were not specified. This is a project-identity decision, not a technical WMS blocker.

## Limitations and next proof

- This report is environment evidence only; it does not claim API-mode detection was executed. The documented method is recorded for Stage 1.
- No local WMS client was compiled, no endpoint was opened, and no MIDI was sent.
- WMS timestamps, maximum transmission constraints, endpoint enumeration behavior, SysEx7 packet sequences, and Qt/WMS lifecycle coexistence remain Stage 1 spike questions.
