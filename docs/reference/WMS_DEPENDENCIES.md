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
| x64 `Windows.Devices.Midi2.dll` | `DB3544CAC751F174DA338FB43E754974A067432CE30ECE9845CFCF3B3DAA4CDC` |
| x64 `Windows.Devices.Midi2.pri` | `D04A030D84C77C6BE8C0F2018A6D70FCD142AB5C940644B74223523F975DA9FA` |

The NuGet hash was computed from the asset downloaded from Microsoft's release page.
C++/WinRT remains the previously verified version, generating the projection with
Windows SDK contracts `10.0.26100.0`. Set `TAUREON_ENABLE_WMS_TRANSPORT=ON` to require
this build.

Reconfiguration migrates the former project-local `wms-preview10` default in
the CMake cache to `wms-preview11`. Explicit custom dependency roots are retained.
Every WMS-enabled configure checks the selected WINMD against the Preview-11
hash above and rejects stale/modified metadata before generating the projection.

## Initialization and API lookup

The worker initializes its MTA apartment, activates `MidiApi` through WinRT and calls
`EnsureServiceAvailable`, as in Microsoft's native samples. Activation failures
preserve the HRESULT; unavailable service and Legacy API mode produce separate
messages.

Before activation the worker checks the actual OS build through `RtlGetVersion`,
as in Microsoft's [detection sample](https://github.com/microsoft/MIDI/blob/inbox-preview-11/samples/cpp-winrt/detect-midi-services/main_detect_midi_services.cpp).
Builds below 26200 (Windows 11 25H2), including 24H2, return `backend_unavailable`
without loading the API. Auto can then select WinMM, without translating saved routes.

WinRT activation first uses the Windows-registered/system-provided API, even when
an app-local copy exists. Otherwise C++/WinRT finds the DLL beside the executable;
no additional activation manifest, bootstrapper, `AddDllDirectory`, Tools path,
system registration or System32 modification is needed. This follows Microsoft's
[Preview-11 release notes](https://github.com/microsoft/MIDI/releases/tag/inbox-preview-11)
and [manifest sample](https://github.com/microsoft/MIDI/blob/inbox-preview-11/samples/app-manifest/app.manifest).
The old PR-23 Tools-directory search and its file/environment probing are removed.
Installing the Tools alone no longer makes a DLL-free TAUREON folder WMS-capable.
Auto falls back to WinMM when activation/service availability fails or no routes exist.

`IMidiApiStatics2` is queried defensively with `try_as`. Its `IsSystemProvided`
observation is included in transport diagnostics, the existing engine check and
`--list-midi wms` output. True confirms the Windows copy; false does not prove an
exact version, and an absent interface leaves the observation unset. It does not
select a DLL, change API mode or alter route identity.

Local builds deploy the hash-checked x64 `Windows.Devices.Midi2.dll` and `.pri`
beside the application and opt-in local integration executables, matching the
NuGet native targets. The PRI is mandatory (currently en-US resources); the WINMD
is used to generate the C++/WinRT projection, and PDB symbols are optional debugging
inputs, so neither is needed in TAUREON's runtime folder. No Microsoft installers,
tools or transports are copied. Example folder:

```text
TAUREON-Synth-Tool-V4.exe
Windows.Devices.Midi2.dll
Windows.Devices.Midi2.pri
Windows-MIDI-Services-MIT.txt
WMS-PREVIEW-NOTICE.txt
resources/
platforms/                 # Qt plugin layout
...                       # Qt and app-local VC++ runtime
```

For an existing development/validation portable folder on supported Windows:

```powershell
.\tools\SetWmsAppLocalDeployment.ps1 -Destination '<TAUREON-folder>' -Mode Preview
```

This verifies both runtime hashes and stages the DLL, PRI, license and preview
notice on the destination volume before replacing files. Each replacement is
atomic; if a later write fails, earlier replacements are rolled back and newly
created files are removed. Read-only targets are refused before replacement.
If an external lock also prevents rollback, the script reports failure and
retains original backups in the named `.wms-deployment-*` directory for recovery;
keep TAUREON closed until restored. Close TAUREON before replacing its files.
It prepares local validation;
it is not authorization to publish a preview package. `-Mode SystemOnly` removes
only the named preview DLL, PRI, WINMD, PDB and preview notice after preflighting
the targets. User SysEx data and the executable remain. Linked/system directories
are refused. The helper is included in public portable folders for future cleanup.

The engine's static package does not install Microsoft's API binaries. A WMS consumer
uses a Windows-provided API or deploys an appropriate app-local API under
Microsoft's terms. Route identities, callback ownership, MTA worker,
queue/data-loss semantics and teardown order remain the existing contracts.

## Preview distribution boundary

Preview 11 permits application-local redistribution under its
[Developer Go-live Agreement](https://github.com/microsoft/MIDI/releases/tag/inbox-preview-11),
read together with [Preview 10's exclusions](https://github.com/microsoft/MIDI/releases/tag/inbox-preview-10).
MIT source licensing alone is not the complete distribution authorization. This
is a record of upstream requirements and TAUREON's readiness, not legal approval.
The current Preview-11 agreement has six obligations:

| Obligation | TAUREON handling / condition before public inclusion |
| --- | --- |
| Update mechanism that removes preview files when Windows delivers the API and handles further previews | Build option and portable cleanup helper exist; Product Owner must own and document the release/update process. |
| Make all possible efforts to follow further previews, at least through API lock | Preview 11 is locked and pinned; Product Owner must monitor Microsoft's updates and repeat validation when required. |
| Tell customers the product uses preview APIs until they are removed | Preview notice and read-only diagnostics exist; a future public preview package needs matching release/README/customer notices. |
| Do not ship the API below Windows 11 25H2 | The portable Preview helper refuses older hosts and runtime declines activation there. CMake can stage build inputs on an older CI host for software checks; public CI packages remove those files before upload. A public delivery channel still needs an effective OS restriction: a freely downloadable portable ZIP plus a runtime check alone does not prevent delivery to older Windows. |
| Report encountered bugs to Microsoft and help debugging | Product Owner must accept and maintain that support obligation; no automatic external report is sent. |
| Encourage customers to update to a new build against Windows' 1.0 API, unless automatic | Product Owner must define and use a customer notification/update channel. |

Preview 10 also excludes redistributing Microsoft's installers, tools and
transports. Redistribution as a reusable MIDI library needs Microsoft's separate
case-by-case permission; TAUREON's installed engine package never includes the API
binaries. C# projections are irrelevant to this native C++ application. Preview
10's superseded previous agreement contained a 90-day deadline; it is not a term
of the current Preview-11 agreement. Microsoft gives no guarantee of compatibility
between this preview and the final in-box API/service transports.

Public Actions artifacts and release ZIPs continue to exclude DLL/PRI and fail
packaging if present. CI verifies app-local file hashes, removal and the unsupported
OS failure path before uploading the API-free folder. WMS-enabled and disabled
software suites run; hardware and native visual tests remain excluded. A public
preview package requires Product Owner Go-live acceptance, all delivery/update/
notice/support conditions above, review of the then-current upstream terms and
explicit release approval. No new release is published by this change.

## Migration to the Windows-delivered API

Once available, acquire Microsoft's final metadata, rebuild the projection and
revalidate both backends. Configure `-DTAUREON_DEPLOY_WMS_PREVIEW_API=OFF` for the
product build: the post-build step removes stale preview DLL/PRI/WINMD/PDB from
that output directory. A WMS-disabled product rebuild also removes those files.
The engine remains a static package without API binaries. Confirm the actual
Windows API through `IsSystemProvided` and read-only enumeration on a supported
PC; this cannot currently be claimed from a preview-only test.

Portable updates must be extracted into a **new empty folder**; overlay extraction
does not delete files omitted from the new archive. Close the old app and use
`tools/SetWmsAppLocalDeployment.ps1 -Destination '<old-TAUREON-folder>' -Mode SystemOnly`
if reusing a folder. Verify the four preview binary/metadata names are absent,
preserve user data, and tell customers to update. Never delete or register the
Windows copy. API-free releases on PCs without an in-box API use WinMM.
