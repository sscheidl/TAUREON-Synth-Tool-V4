# Alpha feedback — 2026-10-06

The Product Owner prioritised bugs 1–4. Bug 5 (SysEx Transfer button flicker) was
observed with the internal September test package and is already fixed. Requests
Request 7 remains deferred. Version visibility and hiding the Manager navigation
are included in the current GUI batch.

**2026-10-10 update:** the Preview-9 production client now enumerates 21 RX / 24 TX
WMS routes successfully. This supersedes the bug-2 initialization blocker below;
the old RC4 findings remain as historical evidence. See
[Preview 9 compatibility](WMS_PREVIEW9_20261010.md) for the cause and current checks.

## Current fix scope

1. **Auto blocks port selection:** Auto now enumerates Windows MIDI Services first
   and falls back to WinMM if WMS fails or lists no routes. The selected backend is
   visible in the Auto label. Only the Backend field selects the API; MIDI Input
   and Output list the available ports for that API. Port selection and Connect
   remain deliberate. Full saved-route restoration remains request 7.
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

## Later request

7. Automatic restoration of an exactly saved backend and routes; further WMS test
   activation/compatibility work if the current runtime requires it.

The window title now shows the alpha version and a short build revision. The
SysEx Manager tab is hidden; its implementation remains marked as a removal
candidate.

## MIDI Monitor filter follow-up (request 6)

The Monitor now has a permanently visible checkbox panel for RX/TX and Notes,
Control Change, Program Change, Pitch Bend, Aftertouch, SysEx, Clock, Active
Sensing and Other/System. Several types can be selected at once. "All event types"
resets to all; "No event types" leaves Notes visible and hides the other types.
The channel and existing free-text filter can be combined with these checkboxes.
The redundant monitor-backend dropdown has been removed; backend selection is
performed once in the connection bar. The workspace navigation is above the
content, and the monitor table fills the available width. Classification uses MIDI 1.0
status bytes or UMP message types and is a view-only proxy filter: source history,
raw bytes, capture and transfer are unchanged. Channel filtering excludes system
messages, which have no channel. MIDI 2.0 SysEx8 is included; UMP Mixed Data Set
is Other/System. MIDI 2.0 per-note pitch bend and controller statuses are assigned
to their respective event filters. Endpoint-level route filtering remains
separate future work.

Local MSVC/Qt build and 24 registered tests passed, including MIDI 1.0/UMP type
classification, compound filtering, source-history preservation and GUI action
wiring. Native visual readability and live hardware traffic remain Product Owner
checks.

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
