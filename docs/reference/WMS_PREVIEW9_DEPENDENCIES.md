# Production WMS dependency update — Preview 9

The production transport now builds against `Windows.Devices.Midi2`
`0.99.83-devpreview.9`, from Microsoft's
[Preview 9 release](https://github.com/microsoft/MIDI/releases/tag/inbox-dev-preview-9).
The installed Tools package on the owner's PC is `0.99.84-devpreview.9`;
the Tools patch number is distinct from the native NuGet package version.
The older Stage-1 RC4 spike and its acquisition script remain historical evidence.

## Reproducible acquisition

Run `tools/AcquireWmsDependencies.ps1`. It writes only under the project's ignored
`build/dependencies/wms-preview9` directory and verifies these SHA-256 hashes:

| Input | SHA-256 |
| --- | --- |
| `Windows.Devices.Midi2.0.99.83-devpreview.9.nupkg` | `CCB4D0A4358D16F7ECDBFEE0FF6B08278E4951B9D9C3A103478DE6F5E7CA99E8` |
| `ref/native/Windows.Devices.Midi2.winmd` | `8A08940ADCDE6CA9A4CAEB9B0F701F90CEA05FE4A170ED70B5520562DF2098F1` |
| `Microsoft.Windows.CppWinRT.2.0.240405.15.nupkg` | `E889007B5D9235931E7340DDF737D2C346EEBDD23C619F1F4F2426A2AAE47180` |

Microsoft's release API supplies the native NuGet archive digest. C++/WinRT remains
the previously verified version, generating the projection with Windows SDK
contracts `10.0.26100.0`. Set `TAUREON_ENABLE_WMS_TRANSPORT=ON` to require this build.

## Initialization and deployment

The worker initializes its MTA apartment, activates `MidiApi` through WinRT and
calls `EnsureServiceAvailable`, as in Microsoft's Preview-9 native samples.
Endpoint information, filters and Group Terminal Blocks moved into
`Windows.Devices.Midi2.Enumeration`. The RC4 desktop bootstrapper, registration and
minimum `1.0.17` check no longer apply. Activation failures preserve the HRESULT;
unavailable service and Legacy API mode produce separate messages.

The application and opt-in local integration executables receive
`Windows.Devices.Midi2.dll` and `.pri` beside their EXE, matching the NuGet native
targets. The portable internal test folder includes both files. No system
registration or MIDI configuration is written. WinRT activation chooses the
Windows-provided API where present and uses app-local files as its fallback.

The engine's static package does not install Microsoft's API binaries. A WMS
consumer must deploy an appropriate API under Microsoft's terms or use the
Windows-provided implementation. Route identities, callback ownership, MTA worker,
queue/data-loss semantics and teardown order remain the existing contracts.

## Preview distribution boundary

Microsoft permits these files for development and internal testing. The release
notes impose additional conditions on public customer previews, including an
expiration no later than 2027-01-15; production distribution needs Microsoft's
explicit permission. This change prepares an internal test build, not a public
release. Publishing a future alpha with these files requires checking and meeting
the applicable upstream terms first. The existing public alpha is unchanged.
