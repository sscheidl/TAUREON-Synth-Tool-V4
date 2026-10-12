# Production WMS dependencies — In-box Preview 11

The production transport builds against `Windows.Devices.Midi2`
`0.99.92-preview.11`, from Microsoft's
[In-box Preview 11 release](https://github.com/microsoft/MIDI/releases/tag/inbox-preview-11).
The matching Tools package is `0.99.92-preview.11`; the Transports package is
`1.0.41-preview.9`. Preview 11 locks the API metadata and interface contracts.
Its new `MidiApi.IsSystemProvided()` method is on `IMidiApiStatics2`; the existing
`IMidiApiStatics` interface used by this transport remains unchanged. Its
`MidiResourceListEntry` default handling also changed, but TAUREON does not use
that MIDI-CI helper. The transport's existing `MidiApi`, `MidiSession`,
`MidiEndpointConnection`, enumeration, group terminal block and `MidiClock`
calls require no migration from Preview 10. The older Stage-1 RC4 spike and its
acquisition script remain historical evidence.

## Reproducible acquisition

Run `tools/AcquireWmsDependencies.ps1`. It writes only under the project's ignored
`build/dependencies/wms-preview11` directory and verifies these SHA-256 hashes:

| Input | SHA-256 |
| --- | --- |
| `Windows.Devices.Midi2.0.99.92-preview.11.nupkg` | `13AFC6E6DA2ADAC309AB8C174352F46F1C0EBE2D79E65475BD6274DEFB73DF74` |
| `ref/native/Windows.Devices.Midi2.winmd` | `49AB3BBE0954BF4F4D0D1AB933564974495762A11B9B02602F493C6C755C47BC` |
| `Microsoft.Windows.CppWinRT.2.0.240405.15.nupkg` | `E889007B5D9235931E7340DDF737D2C346EEBDD23C619F1F4F2426A2AAE47180` |

The NuGet hash was computed from the asset downloaded from Microsoft's release page.
C++/WinRT remains the previously verified version, generating the projection with
Windows SDK contracts `10.0.26100.0`. Set `TAUREON_ENABLE_WMS_TRANSPORT=ON` to require
this build.

## Initialization and API lookup

The worker initializes its MTA apartment, activates `MidiApi` through WinRT and calls
`EnsureServiceAvailable`, as in Microsoft's native samples. Activation failures
preserve the HRESULT; unavailable service and Legacy API mode produce separate
messages.

WinRT activation first uses a Windows-registered/system-provided API, even when
an app-local copy exists. If none is registered,
C++/WinRT loads `Windows.Devices.Midi2.dll` with `LOAD_LIBRARY_SEARCH_DEFAULT_DIRS`:
the application directory first, then directories added with `AddDllDirectory`. The
Windows MIDI Services Tools install the API in
`%ProgramFiles%\Windows MIDI Services\Tools` without registering it. When no API sits
beside the executable and that installed copy exists, the transport adds the Tools
directory once per process, so a user with the Tools installed gets WMS without any
redistributed API files. An API beside the executable still takes precedence.

Local builds deploy `Windows.Devices.Midi2.dll` and `.pri` beside the application and
the opt-in local integration executables, matching the NuGet native targets. No
system registration or MIDI configuration is written.

The engine's static package does not install Microsoft's API binaries. A WMS consumer
uses the installed Tools API, a Windows-provided API, or deploys an appropriate API
under Microsoft's terms. Route identities, callback ownership, MTA worker,
queue/data-loss semantics and teardown order remain the existing contracts.

## Preview distribution boundary

Preview 11 carries the same developer go-live agreement as Preview 10, with
conditions including preview disclosure, update handling and no API distribution
below Windows 11 25H2. Refer to the upstream release for the complete terms;
older preview expiration/permission wording is not the current agreement.
TAUREON's public CI artifacts and release ZIPs continue to exclude the API DLL
and PRI and fail packaging if those files are present. This dependency update
does not authorize their public redistribution or install/update any system
Tools, transports, drivers or services.
