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

### Integration route – verified against the installed SDK metadata

**Verification method (2026-08-24):** type and member names below were extracted directly from the
installed `Microsoft.Windows.Devices.Midi2.winmd` (`Windows MIDI Services Runtime and Tools 1.0.17-rc.4.25`,
`C:\Program Files\Windows MIDI Services\Desktop App SDK Runtime\`). Documentation prose was used only for
intent, never as the source of an identifier. Items that could not be verified locally are marked OPEN and
must not be treated as facts.

#### Verified present

1. **Namespace root is `Microsoft.Windows.Devices.Midi2`.** The unprefixed `Windows.Devices.Midi2` does not
   occur in the metadata. The corresponding NuGet package identifier carries the same `Microsoft.` prefix.
   Target a 64-bit C++20 desktop process consuming the C++/WinRT projection.
2. **Session and connection:** `MidiSession`, `MidiEndpointConnection`, `MidiEndpointConnectionBasicSettings`,
   `MidiMessageReceivedEventArgs`.
3. **Enumeration and identity:** `MidiEndpointDeviceInformation`, `MidiEndpointDeviceWatcher` (with
   `Added`/`Removed`/`Updated` event args), `MidiEndpointDeviceIdHelper`, `MidiGroupTerminalBlock`
   (`FirstGroup`), `MidiFunctionBlock`, `MidiGroup`. Persist routes on `EndpointDeviceId` plus group;
   names and numeric port indexes remain non-persistent.
4. **SysEx7:** `Microsoft.Windows.Devices.Midi2.Utilities.SysExTransfer.MidiSystemExclusiveMessageHelper`
   with `GetDataBytesFromSingleSystemExclusive7Message`,
   `GetDataBytesFromMultipleSystemExclusive7Messages`,
   `GetDataByteCountFromSystemExclusive7MessageFirstWord`, and `BuildSystemExclusive7Message`.
   A separate `MidiSystemExclusiveSender` exists for driving larger transfers.
   The previously recorded `MidiSystemExclusive7MessageHelper` does not exist in this SDK.
5. **Timestamps:** `MidiClock` exposes `Now`, `TimestampFrequency`,
   `ConvertTimestampTicksTo{Nanoseconds,Microseconds,Milliseconds,Seconds}`,
   `OffsetTimestampBy{Microseconds,Milliseconds,Seconds}`, `TimestampConstantSendImmediately`, and
   `TimestampConstantMessageQueueMaximumFutureTicks`. The last constant is the SDK's own scheduling
   horizon and is a direct input to the "maximum transmission constraints" question.
6. **Diagnostic loopback:** `DiagnosticsLoopbackAEndpointDeviceId` and `DiagnosticsLoopbackBEndpointDeviceId`
   are defined, so a software-only send/receive spike is possible without hardware.
7. **Runtime/version reporting:** `Utilities.RuntimeInformation.MidiRuntimeInformation` with
   `MidiRuntimeVersion` and `GetHighestAvailableRelease`; `Reporting.MidiReporting` and
   `MidiServiceSessionInfo` for service-session visibility.
8. **WinMM correlation (architecturally significant):** the SDK itself bridges legacy ports to WMS
   endpoints via `MidiEndpointAssociatedPortDeviceInformation` (`PortName`, `PortNumber`, `Midi1PortFlow`),
   `FindEndpointDeviceIdForAssociatedMidi1PortNumber`,
   `FindAllEndpointDeviceIdsForAssociatedMidi1PortName`, and
   `FindAllAssociatedMidi1PortsForThisEndpoint`. This is a candidate cross-check for the WinMM
   persistent-identity problem on hosts where WMS is present.

#### OPEN – not verifiable from local evidence

- **Unpackaged desktop initialization.** No `MidiApi` type, no `EnsureServiceAvailable`, and no
  `GetCurrentlySelectedApiMode` occur in the metadata; no `ApiMode` string occurs anywhere in it. The WMS
  desktop initializer ships outside the `.winmd` (header/static-library side), and a recursive search of
  `C:\Program Files\Windows MIDI Services\` found no `.h`, `.hpp`, `.lib`, `.nupkg`, `.targets`, or `.props`
  file. **The machine currently holds runtime artifacts only and no build-time SDK inputs at all.**
  Stage 1 must therefore acquire the pinned SDK package first and derive the initialization and
  API-mode-observation entry points from that package, not from this report.
- **Service-availability and API-mode semantics.** Read-only observation remains a Stage 1 requirement, but
  the calling surface is unknown until the package above is acquired.

Documentation consulted for intent only (checked 2026-08-24):

- [Windows MIDI Services App SDK overview](https://microsoft.github.io/MIDI/sdk-overview/)
- [Moving from WinMM to Windows MIDI Services](https://microsoft.github.io/MIDI/kb/moving-from-winmm-to-wms/)
- [Persistent endpoint identifiers](https://microsoft.github.io/MIDI/kb/identifiers/)

The current WMS SDK/release is still documented as preview/RC material. Stage 1 must record and pin the exact
WMS SDK package, C++/WinRT package, `.winmd`, runtime, and deployment-file versions used by the spike, then
prove a clean executable deployment. Stage 0 makes no production-readiness claim.

## WinMM

`C:\Windows\System32\winmm.dll` exists, version `10.0.26100.8875`. The legacy native PoC links `winmm`, but V4 did not reuse its code. WinMM enumeration, `MIDIHDR` ownership, and teardown remain explicit Stage 1 spike requirements.

## MIDI driver and virtual-endpoint landscape of the spike host

Read-only inventory (installed-product registry query, 2026-08-24). No driver was installed, removed, or
reconfigured. This section exists because Stage 1 enumeration spikes run on **this** machine and the host is
not a clean MIDI environment.

| Component | Version | Relevance to Stage 1 |
|---|---|---|
| `loopMIDIBlockLegacy` | `9.9.9.9` | Legacy-block entry; suppresses legacy WinMM enumeration of the affected endpoints |
| `rtpMIDIBlockLegacy` | `9.9.9.9` | Legacy-block entry; same effect |
| Bome Virtual MIDI | `2.1.0.44` | Virtual multi-client ports |
| teVirtualMIDI64 / teVirtualMIDI for Presonus | `1.3.0.43` | Virtual multi-client ports, two installations |
| ESI Midiport USB Driver | `1.7.0.0` | Vendor driver for the multiport DIN interface |
| Novation USB MIDI | `2.30.0.68` | Vendor class driver |
| Arturia USB MIDI | `4.66.0` | Vendor class driver |
| Moog Sub 37 USB MIDI | `4.35.0` | Vendor class driver |
| ROLI MIDI Driver | `1.0.14.2` | Vendor class driver |
| DublerMidiCapture | `1.3.2` and `1.4.0` | Two versions registered simultaneously |

### Consequences that Stage 1 must assume, not rediscover

- **WinMM and WMS will not enumerate the same endpoint set on this host.** The two `*BlockLegacy` entries
  exist specifically to hide endpoints from the legacy path. A divergent count between the two spikes is
  expected environment behavior and must not be reported as an enumeration defect.
- **Multi-client behavior is live here.** Virtual drivers permit multiple simultaneous openers, so a WinMM
  open that unexpectedly succeeds proves nothing about exclusive-access behavior on real DIN hardware.
- **Duplicate/legacy vendor registrations exist.** Two DublerMidiCapture versions and two teVirtualMIDI
  installations are registered; identity spikes must tolerate near-duplicate names.
- Endpoint identity work should cross-check against the SDK's own legacy-port correlation members listed in
  the verified-route section above rather than assuming name uniqueness.

## GitHub state

The authenticated GitHub account is `sscheidl`. Following the User's Stage 0 decision, private repository `https://github.com/sscheidl/TAUREON-Synth-Tool-V4` was created and configured as local `origin`. No branch has been pushed yet. This is not a technical WMS blocker.

## Limitations and next proof

- This report is environment evidence only. API-mode detection was **not** executed, and its calling surface is recorded as OPEN rather than as a documented method.
- Identifier-level WMS facts in this report were extracted from the installed `.winmd` on 2026-08-24. If Stage 1 pins a different SDK version, re-extract rather than trusting this section.
- No local WMS client was compiled, no endpoint was opened, and no MIDI was sent.
- WMS timestamps, maximum transmission constraints, endpoint enumeration behavior, SysEx7 packet sequences, and Qt/WMS lifecycle coexistence remain Stage 1 spike questions.
