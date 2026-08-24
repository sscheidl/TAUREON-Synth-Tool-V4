# Stage 1 WMS dependency and deployment record

**Recorded:** 2026-08-24
**Scope:** Stage 1 native WMS spike only
**Acquisition:** project-local, reproducible, no machine-wide installation

## Pinned build inputs

| Input | Pinned version | Official source | SHA-256 |
|---|---:|---|---|
| Microsoft Windows MIDI Services App SDK NuGet package | `Microsoft.Windows.Devices.Midi2` `1.0.17-rc.4.25` | Microsoft MIDI GitHub release `rc-4`, asset `Microsoft.Windows.Devices.Midi2.1.0.17-rc.4.25.nupkg` | `D0A420E724154AAF707CBCEEFDE0E355B0B6D9DCD37160F767BFAC6A9C9A86E6` |
| Microsoft C++/WinRT NuGet package | `Microsoft.Windows.CppWinRT` `2.0.240405.15` | NuGet v3 flat-container package | `E889007B5D9235931E7340DDF737D2C346EEBDD23C619F1F4F2426A2AAE47180` |
| WMS SDK metadata | `Microsoft.Windows.Devices.Midi2.winmd` from the pinned SDK package | Inside the WMS package at `ref/native/` | `EC63F2C944ECD678A88D73A9D2BFF736606558E5FAAB6AEB3FF287DE199FF750` |

The versions are the exact pair referenced by Microsoft's C++ RC4 samples. Run
`powershell -NoProfile -ExecutionPolicy Bypass -File tools/AcquireStage1Dependencies.ps1` from the
repository to acquire them beneath the ignored `build/dependencies/wms-rc4/` tree. The script rejects an
archive or SDK metadata file whose hash differs from this record.

The official RC4 sample archive used as corroborating evidence was
`Windows.MIDI.Services.SDK.Samples-1.0.17-rc.4.25.zip`, SHA-256
`C7396EF3F2A98A370EF81AAF8FEDD0168DC1B8768DB8A67A2DF5D2FE0343DB6D`. It is reference evidence, not a
build dependency.

## Installed runtime observed before the spike

| Item | Observed version or hash |
|---|---|
| WMS runtime/product version | `1.0.17-rc.4.25` |
| `Microsoft.Windows.Devices.Midi2.dll` | file version `1.0.17.25`, product version `1.0.17-rc.4.25`, SHA-256 `684A6C894ABB999808B04396894E6BF7FB2043F1833D9357FB0067BDBEEE58DD` |
| Installed `Microsoft.Windows.Devices.Midi2.winmd` | SHA-256 `EC63F2C944ECD678A88D73A9D2BFF736606558E5FAAB6AEB3FF287DE199FF750` |
| `Microsoft.Windows.Devices.Midi2.pri` | SHA-256 `EEB8D8F1C27E77CED9090984E47648086CF9AEBE296386706266A712967B8954` |
| Runtime PDB | SHA-256 `59569CC8D270981FFF2542D29106F5966DF85C2859DA7FE413EB6ADCCA3504C4` |

The packaged SDK `.winmd` and installed runtime `.winmd` are byte-identical. The existing runtime therefore
matches the pinned SDK metadata on this host. No driver, service, registry, API-mode, installer, or global
configuration change was made to establish the build environment.

## Initialization and deployment boundary

The pinned package supplies
`Microsoft.Windows.Devices.Midi2.Initialization.MidiDesktopAppSdkInitializer` through
`build/native/include/winmidi/init/Microsoft.Windows.Devices.Midi2.Initialization.hpp`. Stage 1 uses this
official unpackaged-desktop initializer and records its result. It must not install, update, reconfigure, or
restart MIDI Services. `EnsureServiceAvailable()` is used only if the already-installed service is present;
the spike must stop with visible evidence if initialization would require a system change.

The package carries metadata, generated-header integration, and initializer declarations. The matching WMS
runtime deployment remains the already-installed `1.0.17-rc.4.25` runtime above. Stage 1 does not redistribute
or replace it and makes no production deployment claim.

## Status of architecture hypotheses

The package exposes WinMM/WMS correlation helpers and `MidiClock` metadata. Their presence proves only API
availability. Whether they provide sufficient route correlation or timestamp semantics is a Stage 1 test
hypothesis, not an accepted architecture decision.
