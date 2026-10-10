# Production WMS dependencies — In-box Preview 10

The production transport builds against `Windows.Devices.Midi2`
`0.99.88-preview.10`, from Microsoft's
[In-box Preview 10 release](https://github.com/microsoft/MIDI/releases/tag/inbox-preview-10).
The matching Tools package is `0.99.88-preview.10`; the Transports package is
`1.0.35-preview.8`. Between Preview 9 (`0.99.83-devpreview.9`) and Preview 10 the
API surface this transport uses (`MidiApi`, `MidiSession`, `MidiEndpointConnection`,
`Enumeration.MidiEndpointDeviceInformation`, group terminal blocks, `MidiClock`) is
unchanged; Microsoft's IDL changes in that range are additive. The older Stage-1 RC4
spike and its acquisition script remain historical evidence.

## Reproducible acquisition

Run `tools/AcquireWmsDependencies.ps1`. It writes only under the project's ignored
`build/dependencies/wms-preview10` directory and verifies these SHA-256 hashes:

| Input | SHA-256 |
| --- | --- |
| `Windows.Devices.Midi2.0.99.88-preview.10.nupkg` | `6DAF121A3F76A4A1E0D46521C5072AC576477448C9332048383BB58960A4CE69` |
| `ref/native/Windows.Devices.Midi2.winmd` | `497D9E7C2FB219388F748C585D7AAD537D3A6A576DE854BD16BFE1BC73F8D9A9` |
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

WinRT activation first uses a Windows-registered API. If none is registered,
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

Microsoft permits these files for development, internal testing and use on the
customer's own PC. Public CI artifacts and release ZIPs are distribution, so the
workflow never includes these API files and fails if they are present. Microsoft's
release notes impose additional conditions on public customer previews, including an
expiration no later than 2027-01-15; production distribution needs Microsoft's
explicit permission. Shipping the API files publicly requires checking and meeting
the applicable upstream terms first.
