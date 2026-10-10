# Alpha feedback — 2026-10-06

The Product Owner prioritised bugs 1–4. Bug 5 (SysEx Transfer button flicker) was
observed with the internal September test package and is already fixed. Requests
6–9 remain deferred; the Manager-navigation change is prepared separately.

**2026-10-10 update:** the Preview-9 production client now enumerates 21 RX / 24 TX
WMS routes successfully. This supersedes the bug-2 initialization blocker below;
the old RC4 findings remain as historical evidence. See
[Preview 9 compatibility](WMS_PREVIEW9_20261010.md) for the cause and current checks.

## Current fix scope

1. **Auto blocks port selection:** with no concrete backend, either port field now
   offers Windows MIDI Services or WinMM. Choosing one visibly changes the backend
   and enumerates its routes. Port selection and Connect remain deliberate. Full
   saved-route restoration remains request 7.
2. **WMS lists no routes:** report backend-not-built, transport registration, SDK
   initialization, minimum version and service-availability failures separately;
   retain the failure rather than replacing it with a disconnected status. Report
   successful empty enumeration explicitly. Correct Group Terminal Block direction
   mapping: device BlockInput is application TX; BlockOutput is application RX.
   A read-only `taureon_app --list-midi wms` / `winmm` command uses the production
   transport factory and enumeration code without opening an endpoint or sending.
3. **Port names are clipped:** observed at 1920-wide fullscreen, 125%. Give port
   fields a readable minimum width, resize their popup to the labels within screen
   bounds, and show concise names with full route identity in item/selection tooltips.
4. **Monitor does not follow:** observed at native 150%. Follow latest is enabled
   initially and follows batched insertions, history resets and filter changes.
   Scrolling up pauses it; the checkbox or scrolling to the end resumes it.

WMS Group Terminal Block direction is defined from the device's viewpoint in
[Microsoft's SDK reference](https://microsoft.github.io/MIDI/sdk-reference/Enumeration/MidiGroupTerminalBlockDirectionEnum/).

## Deferred requests

6. Structured MIDI Monitor filters: Clock, Active Sensing, SysEx, Notes, CC, Program
   Change, Pitch Bend, Aftertouch and other system/realtime messages.
7. Automatic restoration of an exactly saved backend and routes; further WMS test
   activation/compatibility work if the current runtime requires it.
8. Visible application version and build identity in the title/About UI.
9. Hide SysEx Manager navigation; retain its code as a removal candidate.

## Display-scaling disposition

The Product Owner completed the native 150% impression test and chose to defer
native 200% testing. This changes the current acceptance scope: 200% is not claimed
as tested or passed. Existing earlier process-local 200% simulations are historical
software evidence only. A native 4K/200% test can be added when a concrete problem
is reported. This record supersedes a mandatory native 200% check for this alpha,
without claiming a completed Stage-5 or hardware gate.

## Validation

- MSVC 19.44 / Qt 6.10.3 Debug build with the pinned WMS SDK enabled: passed.
- Registered software tests: 24/24 passed. After adding the failed/empty-backend
  cases, the updated connection UI test passed again, including the process-local
  Qt 150% run. These are automated/offscreen checks, not a native visual inspection.
- Microsoft console enumeration on this PC: two UMP endpoints (AF16Rig and M8U eX)
  with Group Terminal Blocks.
- **Bug 2 remains environment-blocked:** the production WMS enumeration command
  exits 1 with `Windows MIDI Services SDK runtime is not installed/registered`.
  `IsServiceInstalled()` succeeds; `InitializeSdkRuntime()` fails for the pinned
  Microsoft.Windows.Devices.Midi2 RC4 client. A running MidiSrv and working Microsoft
  tools do not prove that this client's required SDK registration is present.
  The tools present here use Windows.Devices.Midi2.dll (file version 1.0.16.0).
  The next step is a supported SDK/runtime compatibility decision under request 7;
  no driver, service, registry, API-mode or runtime installation was changed.
- Production WinMM enumeration on this snapshot exits 0 and reports 0 input / 1
  output route (Microsoft GS Wavetable Synth). This is a time-specific enumeration,
  not a contradiction of the user's earlier WinMM list or a hardware acceptance.
- No endpoint was opened and no physical MIDI/SysEx was sent during this work.
