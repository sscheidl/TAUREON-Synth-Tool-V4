# Stage 1 Report – Native WMS and WinMM Transport Spikes

**Stage:** 1
**Baseline revision:** `eb63c4a9bf77da05c6b053a853151105615b88ef` on `main` / `origin/main`
**Status:** PASS / CLOSED
**Mandatory Claude Code review:** PASS WITH NON-BLOCKING FOLLOW-UPS; no P0/P1 findings
**Result recommendation:** PASS WITH NON-BLOCKING FOLLOW-UPS

## Completed work

- Acquired and pinned the official WMS RC4 SDK and matching C++/WinRT package project-locally before
  writing WMS client code; recorded exact versions and SHA-256 hashes.
- Built a direct C++/WinRT WMS spike for initialization, endpoint/group enumeration, watcher lifecycle,
  diagnostic-loopback short/SysEx7 receive, exact route identity, and deterministic shutdown.
- Built a minimal Qt Core/QCoreApplication coexistence harness with an explicit MTA worker boundary, normal
  stop, application-close-while-active, callback gating, and thread join.
- Built a native WinMM-only spike for enumeration, exact composite identity, short messages, explicit
  input/output `MIDIHDR` ownership, byte-exact SysEx, repeated teardown, and handle tracking.
- Used the already-installed WMS tools to create and remove one unique temporary WMS-native loopback pair
  for the safe WinMM test. Bome/teVirtualMIDI and physical MIDI ports were not used.
- Isolated the RC4 WinMM/WMS correlation helpers in a separate reproducer and recorded their fail-fast
  behavior as negative hypothesis evidence, not as an architecture decision.

No production transport abstraction, MIDI core, product GUI, legacy migration, or Stage 2 implementation
was created.

## Deliverables

- `tools/AcquireStage1Dependencies.ps1`
- `tools/RunStage1WinmmWmsLoopback.ps1`
- `spikes/route_identity.hpp`
- `spikes/wms_direct/`
- `spikes/winmm_direct/`
- [`STAGE_1_WMS_DEPENDENCIES.md`](../reference/STAGE_1_WMS_DEPENDENCIES.md)
- [`STAGE_1_WMS_REPORT.md`](../reference/STAGE_1_WMS_REPORT.md)
- [`STAGE_1_WINMM_REPORT.md`](../reference/STAGE_1_WINMM_REPORT.md)
- this report, `PROJECT_STATE.md`, `RISK_REGISTER.md`, and `DECISION_LOG.md`

## Reproducible commands

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/AcquireStage1Dependencies.ps1
cmake --preset vs2022-x64 -DCMAKE_PREFIX_PATH=C:/Qt/6.10.3/msvc2022_64
cmake --build --preset vs2022-x64-debug --parallel
ctest --preset vs2022-x64-debug --output-on-failure
build/vs2022-x64/spikes/wms_direct/Debug/taureon_wms_spike.exe --cycles 100
powershell -NoProfile -ExecutionPolicy Bypass -File tools/RunStage1WinmmWmsLoopback.ps1 -Cycles 100
```

A separate fresh `build/stage1-clean` configure/build/test completed with all five CTests passing; that
disposable build directory was removed afterward. The normal ignored build tree is not a repository input.

## Acceptance summary

| Requirement | Result | Evidence |
|---|---|---|
| Pinned official WMS dependency/runtime pairing | PASS | Exact packages/hashes; packaged and installed WinMD identical |
| Direct WMS initialization/enumeration/watcher | PASS | 3 endpoints, 32 composite routes, watcher complete/stopped |
| WMS receive and SysEx7 helper | PASS (bounded) | Short UMP plus Start/Continue/End, 14 payload bytes exact, no `F0`/`F7` byte-stream framing tested, 0 callbacks after close |
| WMS repeated lifecycle | PASS | 100/100 cycles; identical handle count in every sample over cycles 51-100 |
| WMS route identity | PASS | Endpoint ID + group + direction; missing/ambiguous visible; no fallback |
| Qt/WMS coexistence | PASS (bounded) | `QCoreApplication` host + worker MTA, nonblocking core thread, normal and close-active shutdown, 0 late callbacks; production `QApplication`/STA host not tested |
| Native WinMM enumeration | PASS | Direct WinMM API; no wrapper or hidden backend |
| WinMM short and long messages | PASS | Short exact; 10-byte SysEx exact; `MOM_DONE`; headers unprepared |
| WinMM repeated lifecycle | PASS | 100 open/close cycles, including one complete short/SysEx send/receive/`MOM_DONE` cycle; 0 callbacks after close; 0 requeue failures; 0 handle growth |
| WinMM route persistence/change | PASS | Exact name+`wMid`+`wPid`+driver; live remove/recreate; no silent fallback |
| Temporary endpoint cleanup | PASS | Removed in `finally`; final WinMM absence confirmed |
| Hardware validation | NOT STARTED | Explicitly outside unattended Stage 1 evidence |

## Findings and bounded limitations

1. The pinned RC4 metadata cannot perform the newer documented API-mode query. The result is recorded as
   unavailable; no registry or system-mode workaround was used.
2. The pinned RC4 WinMM correlation helpers are not safe enough to depend on: input-number lookup returned
   empty, output-number lookup failed with `0xC0000005`, and the name helper failed with `0xC0000409` in
   isolated processes. The native transports remain independent and exact identity does not rely on them.
3. WMS whole-process handles increase by a bounded amount during initial WinRT/SDK activity, then remain
   flat through cycle 100. The report discloses both the process-lifetime increase and zero steady-state
   growth rather than claiming return to the pre-initialization count.
4. `MidiClock::TimestampConstantSendImmediately()` was exercised only as immediate-send metadata. Timestamp
   normalization/scheduling is not accepted as an architecture conclusion.
5. Physical unplug/replug and hardware transmission were not performed. Temporary endpoint
   disappearance/re-enumeration and exact resolver transitions provide the Stage 1 software evidence.
6. The WMS test proved byte-exact SysEx7 UMP payload handling through the SDK builder/helper/loopback path.
   Its payload did not include MIDI 1.0 `F0`/`F7`; MIDI 1.0 byte-stream <-> UMP SysEx7 framing conversion
   and segmentation/reassembly remain unproved.

None of these findings creates an unsafe endpoint selection, demonstrated payload corruption, hidden
backend, or required machine-wide change. The unproved conversion and production lifetime/design items
remain mandatory follow-ups below.

## Mandatory inputs for later stages

### Stage 2 – architecture and regression

- Persisted route identity includes the backend. WMS endpoint/group identities and WinMM individual MIDI
  1.0 port identities are not transferable. Changing backend invalidates the saved route or requires exact
  re-resolution; missing/ambiguous resolution requires visible user selection. Never silently translate or
  fuzzy-rebind across backends.
- RC4 WMS/WinMM correlation helpers are not an architecture dependency.
- Native WinMM callbacks perform minimal work. Evaluate and prefer callback -> signal/queue -> worker-owned
  `MIDIHDR` requeue rather than calling `midiInAddBuffer` directly from `midiInProc`.
- Production lifetime safety uses explicit shutdown ordering and synchronization. Cross-thread `QPointer`
  access is not a synchronization mechanism.
- Add an opt-in/labeled local regression path for the 100-cycle WMS run, full WinMM loopback suite, and
  byte-integrity checks. Hosted CI must skip/report unavailable WMS prerequisites, not simulate success.
- The output `MIDIHDR` error path requires production RAII. The hard-coded Qt runtime path and
  configure-time projection used by the spike must not become production build architecture.

### Stage 3 – SysEx conversion

- Prove byte-exact MIDI 1.0 `F0 ... F7` SysEx <-> UMP SysEx7 conversion, including segmentation and
  reassembly. R-003 remains open until broader byte-integrity acceptance is met.

### Stage 5 – product-host coexistence

- Repeat the relevant apartment, lifetime, close-active, callback, and shutdown tests with the actual
  `QApplication`-based product host, including its GUI-main-thread apartment model.
- Make close-while-active assertions depend on measured actual active state rather than only the requested
  scenario.

## Stop/Ask disposition

The earlier endpoint-safety HOLD was resolved by explicit User/Project Manager approval for a temporary
software loopback, subsequently refined to prefer WMS-native facilities. The installed WMS console produced
a WinMM-visible pair without installing a driver or changing global MIDI configuration. The pair was
uniquely verified before sends and removed afterward. No active Stop/Ask condition remains.

## Gate disposition

**PASS WITH NON-BLOCKING FOLLOW-UPS.** Claude Code completed the mandatory review with no P0/P1 findings.
Stage 1 is closed with the bounded evidence and mandatory later-stage inputs above. Stage 2 remains planned
and has not started.
