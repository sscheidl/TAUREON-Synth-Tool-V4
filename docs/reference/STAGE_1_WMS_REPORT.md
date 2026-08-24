# Stage 1 WMS spike evidence

**Recorded:** 2026-08-24
**Status:** PASS
**Safety boundary:** direct WMS sends used only the built-in diagnostic loopback A -> B

## Build and dependency evidence

- C++20, CMake `4.4.2`, Visual Studio 2022/MSVC, Windows SDK `10.0.26100.0`, x64 Debug.
- Official WMS SDK `Microsoft.Windows.Devices.Midi2` `1.0.17-rc.4.25` and C++/WinRT
  `2.0.240405.15` were acquired project-locally before client code was written.
- Exact package/runtime versions and hashes are in
  [`STAGE_1_WMS_DEPENDENCIES.md`](STAGE_1_WMS_DEPENDENCIES.md).
- The packaged and installed `Microsoft.Windows.Devices.Midi2.winmd` hashes are identical.
- `tools/AcquireStage1Dependencies.ps1` deterministically verifies the packages and does not install or
  change the machine.

Commands:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/AcquireStage1Dependencies.ps1
cmake --preset vs2022-x64 -DCMAKE_PREFIX_PATH=C:/Qt/6.10.3/msvc2022_64
cmake --build --preset vs2022-x64-debug --parallel
build/vs2022-x64/spikes/wms_direct/Debug/taureon_wms_spike.exe --identity-only
build/vs2022-x64/spikes/wms_direct/Debug/taureon_wms_spike.exe --cycles 100
ctest --preset vs2022-x64-debug -R stage1_qt_wms -V
```

## Direct WMS lifecycle evidence

- Explicit MTA apartment initialization and the official unpackaged-desktop
  `MidiDesktopAppSdkInitializer` were used in-process; no `midi.exe` subprocess is used by this spike.
- Enumeration returned diagnostic loopback A, diagnostic loopback B, and ESI M8U eX. Group-terminal-block
  expansion produced 32 endpoint-ID + group + direction composite routes.
- The endpoint watcher reached initial-enumeration-completed and stopped deterministically. Its bounded run
  observed three initial adds and no updates/removals.
- A short MIDI 1 channel-voice UMP and three SysEx7 UMP packets were sent only from diagnostic loopback A
  to B. The SysEx7 sequence was Start, Continue, End and reconstructed 14 data bytes exactly through
  `MidiSystemExclusiveMessageHelper`. The 14-byte test data was a SysEx7 UMP payload and did not contain
  MIDI 1.0 `F0`/`F7` framing bytes. Stage 1 therefore proved byte-exact builder/helper/loopback payload
  handling, not MIDI 1.0 byte-stream <-> UMP SysEx7 framing conversion. Callbacks after close: 0.
- 100 SDK initialize/enumerate/shutdown cycles passed.

Whole-process handle counts rose during bounded WinRT/WMS lazy initialization and then plateaued. The final
run observed 169 handles after cycle 1 and 176 at both cycle 51 and cycle 100. Every sample in the latter
half of the 100-cycle run was 176. Thus the disclosed process-lifetime increase was 7 while steady-state
minimum/maximum were both 176. The spike treats that complete latter-half plateau, successful SDK shutdown
on every cycle, and zero callbacks after close as the resource-lifetime criterion; it does not claim that
shared SDK/WinRT process infrastructure returns to the pre-initialization handle count.

## Route identity and timestamp scope

The exact resolver uses endpoint device ID + group + direction as one composite identity. Tests passed for
reorder, disappearance, changed endpoint ID, changed group/direction, reappearance, missing, and duplicate
ambiguity. Names and list positions are never fallback identities; missing or ambiguous routes fail
visibly.

The WMS endpoint/group model is backend-specific and is not transferable to individual WinMM MIDI 1.0 port
identities. Stage 2 persistence must include the backend and require exact re-resolution or visible user
selection after a backend change. The failing RC4 correlation helpers are not an architecture dependency.

The send test uses `MidiClock::TimestampConstantSendImmediately()`. This establishes only the immediate-send
constant available in the pinned metadata. It does not accept timestamp normalization or scheduling as an
architecture conclusion.

## API-mode observation

The pinned `Microsoft.Windows.Devices.Midi2` `1.0.17-rc.4.25` metadata does not contain `MidiApi`,
`MidiApiMode`, or `GetCurrentlySelectedApiMode`. The spike reports
`unavailable_in_pinned_Microsoft.Windows.Devices.Midi2_1.0.17-rc.4.25`. It did not read the registry or alter
the API mode as a substitute. This is an explicit version-specific limitation, not a simulated result.

## Qt/WMS coexistence and runtime incident

The initial harness unnecessarily linked Qt Widgets and failed to launch because `Qt6Widgets.dll` was not
on the test-process search path. Dependency inspection showed Widgets were unnecessary for the approved
lifecycle proof. The harness was reduced to `Qt6::Core`/`QCoreApplication`; `dumpbin /dependents` then showed
`Qt6Cored.dll` and no `Qt6Widgets.dll`.

CTest prepends `C:/Qt/6.10.3/msvc2022_64/bin` only to each test process through
`ENVIRONMENT_MODIFICATION`. No DLL was copied and no permanent user/system `PATH` was changed. Observed
`Qt6Cored.dll`: version `6.10.3.0`, SHA-256
`FCE55717E43D970451C2E5C14B26336C2B96E96B4AF7E1380507695209115F8C`.

| Scenario | Result | Evidence |
|---|---|---|
| Queued start, deterministic stop, application quit | PASS | Core dispatch 20 us; worker initialization 75 ms; distinct core/worker thread IDs; 83 core ticks; 22 callbacks; 0 callbacks after stop; worker joined |
| Application quit while WMS objects exist | PASS | Core dispatch 22 us; worker initialization 75 ms; distinct threads; 70 core ticks; 27 callbacks; 0 callbacks after stop; worker joined |

The Qt/core thread only queues work. WMS initialization, session, connection, callbacks, and MTA apartment
live on the worker thread. Shutdown closes callback acceptance, revokes events, closes connection/session,
shuts down the SDK runtime, uninitializes the apartment, and joins the worker before application teardown
completes. The safety mechanism is the accepting gate, event-handler revocation, endpoint/session close,
SDK shutdown, synchronous teardown on the worker thread, and completion of worker termination before
dependent objects are destroyed. `QPointer` is only a guarded Qt object reference here; it is not the
cross-thread synchronization or lifetime guarantee and production Stage 2 must not treat it as one.

## Limitations

- No real hardware was sent data and hardware validation has not started.
- No physical endpoint was unplugged or reordered. Watcher operation was observed live; transition and
  ambiguity semantics were exercised by the deterministic exact resolver.
- The pinned RC4 WinMM-correlation helpers fail in the isolated correlation probe; see
  [`STAGE_1_WINMM_REPORT.md`](STAGE_1_WINMM_REPORT.md). They are not relied upon.
- API-mode and timestamp normalization remain explicitly unresolved for a future approved SDK/version
  decision; they are not silently promoted to architecture conclusions.
- Qt/WMS coexistence was proved only for a `Qt6::Core`/`QCoreApplication` host plus a dedicated WMS MTA
  worker. It did not prove coexistence with the production `QApplication` host or an STA GUI main thread.
  Stage 5 must repeat the relevant apartment, close-active, callback, and shutdown tests in the actual host.
- The 100-cycle WMS run and loopback byte-integrity path are outside the normal CTest suite. Stage 2 must
  add an opt-in/labeled local regression path and must not claim hosted-CI WMS integration when the service
  or runtime is unavailable.
- The current close-while-active harness reports a requested scenario; a later regression must assert the
  actual worker-active state at close. Its hard-coded Qt runtime path and configure-time C++/WinRT projection
  are spike-only and must not become the production Stage 2 build/deployment architecture.
