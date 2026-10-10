# Preview 9 compatibility — 2026-10-10

The Product Owner authorized updating the production WMS client after confirming
that Windows MIDI Services Developer Preview 9 is installed. This work starts
from PR #18 head `4fc07116505c3e018c86b9a6bb1cb7eae551d4a8`; the fixes for Auto
selection, long port names and Monitor auto-follow are included in the test build.

## Cause and correction

The installed Tools/PowerShell packages identify themselves as
`0.99.84-devpreview.9`. The previous production client used
`Microsoft.Windows.Devices.Midi2` RC4 and failed `InitializeSdkRuntime()` despite
a running MidiSrv and working Microsoft console enumeration. That diagnosis did
not mean the Preview-9 API was missing: the old client's initialization contract
was incompatible with this environment.

The production transport now uses `Windows.Devices.Midi2`
`0.99.83-devpreview.9`, the native NuGet asset published under Preview 9, and its
`Enumeration` namespace. It initializes on the existing MTA worker using `MidiApi`
WinRT activation and service availability. The internal portable build supplies
the native API DLL and PRI beside the application. Dependency pins, hashes,
deployment and distribution limits are in
[WMS_PREVIEW9_DEPENDENCIES.md](../reference/WMS_PREVIEW9_DEPENDENCIES.md).

## Local software and read-only evidence

- MSVC 19.44.35227 / Qt 6.10.3 / Windows SDK 10.0.26100.0,
  WMS-enabled RelWithDebInfo full build: passed.
- Existing registered software tests: **24/24 passed**, no failed or skipped tests.
- Separate consumer of the installed Core/WinMM/WMS static package: all three
  executables compile and link successfully. This proves dependency closure,
  not hardware behavior.
- Production `taureon_app --list-midi wms`: exit 0, **21 RX / 24 TX routes**.
  AF16Rig supplies 2 RX / 5 TX, M8U eX 16 RX / 16 TX, and KONTROL S61 MK3
  3 RX / 3 TX. Microsoft's console sees the same three UMP endpoints.
- Production `--list-midi winmm`: exit 0, **21 RX / 25 TX routes** in this snapshot.
  The additional TX is the Microsoft GS Wavetable Synth. The older 0 RX / 1 TX
  snapshot is historical evidence, not the current enumeration result.
- Launching a copy of the product executable without the API files on this PC:
  exit 1 with the specific activation/deployment error, without a crash.
- Portable folder with Qt/MSVC and API files beside the EXE: shell smoke test and
  WMS enumeration both pass with a PATH containing only Windows directories.
  The deployed API DLL hash is
  `F260B5D0540A59636D5E859B3342380EE9B7EF2334AEA07920A7EB7A49BD7613`.

No endpoint was opened or MIDI/SysEx sent. These results close the reproduced
SDK-initialization/empty-port-list cause of alpha bug 2. Native GUI selection and
real product-path receive/send remain for the Product Owner's test. This does not
claim a completed Stage-5 gate. Saved-route Auto restoration, structured Monitor
filters, visible versioning and Manager navigation remain separate requests.
