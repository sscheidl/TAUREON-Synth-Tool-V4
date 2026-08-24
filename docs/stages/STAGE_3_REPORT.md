# Stage 3 Report – Generic MIDI, SysEx & Transfer Engine

**Stage:** 3
**Date:** 2026-08-24
**Implementation lead:** Codex
**Status:** Implementation complete; awaiting User gate
**Recommendation:** **PASS**

## Outcome

Stage 3 implements the generic, Qt-independent MIDI/SysEx/transfer foundation and completes the production
realtime boundary for both approved backends. Exact MIDI 1.0 `F0 ... F7` framing ↔ 64-bit UMP SysEx7
segmentation/reassembly is independently tested at boundary sizes and with synthetic data beyond 1 MiB.
No device/profile logic, third backend, physical MIDI traffic, or global system change was introduced.

## Generic MIDI architecture

`core/midi/MidiMessage` provides a raw-authoritative MIDI 1.0 parsed view for channel voice, Polyphonic
Aftertouch, Control/Program Change, Channel Pressure, Pitch Bend, System Common, timing clock/transport,
Active Sensing, reset/realtime, SysEx, and unknown legal statuses. Exact input bytes remain accessible.
Malformed and incomplete messages return distinct typed errors. `NativeMidiMessage` continues to retain either
exact MIDI 1.0 bytes or complete UMP words plus optional backend-native timestamp metadata.

## SysEx architecture and exact framing rules

- `SysExFrame` distinguishes `complete`, `incomplete`, and `malformed`, preserves exact bytes, records an issue,
  optional UMP group, and whether known data loss affected the capture.
- `SysExStreamParser` consumes arbitrary byte chunks. `F0` starts a frame; `F7` completes it; legal realtime
  bytes are reported separately; nested start, status inside a frame, unexpected end, and bytes outside a
  frame are explicit malformed results. End-of-stream leaves an unterminated frame incomplete.
- A notified overflow while a frame is active taints that capture; it cannot become a verified complete frame.
- The parser operates on application-owned byte copies. WinMM `MIDIHDR` ownership never crosses into it, so a
  frame may span buffers and one chunk may contain multiple frames without changing reconstruction.

## WMS SysEx7 conversion and reassembly

`SysEx7` implements the official 64-bit UMP Data Message form: message type, group, complete/start/continue/end
status, 0–6-byte count, and six payload slots. Encoding accepts only a complete, unaffected, 7-bit MIDI 1.0
frame; removes exactly the outer `F0`/`F7`; segments payload at six-byte boundaries; and preserves the selected
group. Reassembly is independent per group and restores exactly one `F0` and `F7`. Invalid counts/types,
non-7-bit payload, continuation/end without start, interrupted sequences, and unfinished sequences are visible.

Unit coverage includes payload boundaries 0, 1, 5, 6, 7, 11, 12, 13, 18, and 19; single/multi-packet cases;
Start/Continue/End; consecutive frames; forward and reverse byte-equivalent roundtrips; malformed packets; and
incomplete sequences.

## `.syx` I/O

`SyxFile` performs binary-safe reads and retains both original bytes and parsed frames. Normal save accepts only
complete, unaffected frames, does not overwrite by default, and writes through a project-local temporary file
with atomic replacement where supported. A separate explicit raw-save path is required for incomplete or
malformed content. Tests prove multiple-frame load/save and byte-identical valid load → parse → save, while an
unterminated file remains incomplete.

## Transfer engine

The generic `TransferEngine` remains above `IMidiTransport` and owns ordered submission, configurable
inter-message pacing, explicit state, progress, cancellation, timeout, and propagation of disconnect/native
send failures. Its owned thread is never detached; destruction requests cancellation and joins. Pacing uses an
interruptible condition-variable deadline, not an arbitrary synchronization sleep.

Progress reports messages/bytes total and accepted plus applied pacing intervals. “Accepted” means the
software transport send boundary succeeded; it does not claim hardware receipt, storage, or validation.
Cancellation is idempotent before start and stops new submission during transfer. Tests cover success,
multi-message order, pacing, progress, pre/during cancellation, timeout, injected native error preservation,
endpoint disappearance/disconnect, active shutdown, and large-transfer cancellation.

## Production realtime transports

### WMS Direct

The existing MTA worker continues to own SDK initialization, session, connections, and teardown. Native event
callbacks copy the complete UMP packet and native timestamp into a bounded queue only; application handlers run
on the worker. The receive route group is enforced. Send validates complete UMP packet boundaries and route
group, then uses immediate send unless a `wms-native-ticks` timestamp was explicitly supplied. Shutdown closes
callback acceptance, discards/diagnoses queued application events, revokes handlers, disconnects, closes the
session, and prevents callback access to destroyed state.

### Native WinMM

Short RX is unpacked on the worker with the native millisecond timestamp. Short TX uses `midiOutShortMsg`.
Complete `F0 ... F7` long TX uses a transport-owned `WinmmOutputBuffer` registered before submit; ownership is
released only after `MOM_DONE`, unprepare, and legal close. Long RX continues callback → bounded queue → worker
copy/requeue. Close drops/diagnoses queued short application events, retains ownership completions, resets,
waits, unprepares, and closes deterministically. Transport-level tests cover short/long success, native send
failure unwind, timestamp delivery, completion-before-unprepare, and the retained Stage-2 input-submit P1
regression.

Both backends expose native callback, delivered, transmitted, drop, late-callback, and queue-high-water
diagnostics where applicable. No pacing, retry, or SysEx semantic policy entered either transport.

## Large synthetic data evidence

The ordinary Stage-3 suite verifies exact arbitrary-chunk parsing plus MIDI1 → SysEx7 → MIDI1 reconstruction
for single frames of:

- 64 KiB;
- 600 KiB;
- 900 KiB;
- 1 MiB + 257 bytes.

It also verifies 2,048 × 600-byte frames (1,228,800 bytes total), exact frame/byte accounting, large truncated
and malformed variants, and responsive cancellation after one accepted 64-KiB transfer unit. No hidden native
or file buffer size is used by the generic engine.

## Local WMS/WinMM evidence

The tests are registered only with `TAUREON_ENABLE_LOCAL_MIDI_INTEGRATION_TESTS=ON`, labeled `local-midi`, and
serialized by a resource lock. Each run creates a unique `TAUREON S3 WMS ...` pair through the already-installed
Windows MIDI Services console, resolves exactly one named input/output through the backend under test, and
removes the pair in `finally` with an absence check.

- WinMM: 100 open/send/receive/close cycles; 200 transmitted messages; 400 received callbacks; exact short MIDI
  plus a 10,257-byte SysEx spanning multiple 4,096-byte input buffers every cycle; 0 drops; 0 late callbacks;
  steady handle span 0; PASS.
- WMS: 100 open/send/receive/close cycles; 100 accepted multi-message sends; 400 receive callbacks; exact MIDI 1
  UMP, timing-clock UMP, and multi-packet SysEx7 sequence/reassembly every cycle; 0 drops; 0 late callbacks;
  steady handle span 1; PASS.

No physical endpoint was selected. The tests did not install/update a driver, modify registry/API mode,
configure/restart MidiSrv, change service settings, reboot, or change system PATH.

## Test/build commands and results

```powershell
cmake --build --preset vs2022-x64-debug --config Debug --parallel
ctest --preset vs2022-x64-debug -L "stage3|ci" -LE local-midi --output-on-failure
ctest --preset vs2022-x64-debug -R "stage3_local_(winmm|wms)_realtime" --output-on-failure

cmake -S . -B build/stage3-final-clean-20260824 -G "Visual Studio 17 2022" -A x64 `
  -DTAUREON_BUILD_STAGE1_SPIKES=OFF `
  -DTAUREON_ENABLE_WMS_TRANSPORT=ON `
  -DTAUREON_ENABLE_LOCAL_MIDI_INTEGRATION_TESTS=OFF
cmake --build build/stage3-final-clean-20260824 --config Debug --parallel
ctest --test-dir build/stage3-final-clean-20260824 -C Debug -L ci --output-on-failure
ctest --test-dir build/stage3-final-clean-20260824 -C Debug -N -L local-midi

ctest --preset vs2022-x64-debug -L local-midi --output-on-failure
git diff --check
```

Final evidence set:

- current build and environment-independent CI-labeled suite: 8/8 PASS;
- clean build and clean CI-labeled suite: 8/8 PASS;
- clean configuration local-MIDI registration: 0 tests;
- complete opt-in local suite, including retained Stage-1/2 regressions: 5/5 PASS;
- Stage-3 local WMS/WinMM realtime tests: 2/2 PASS, 200 cycles total;
- final failures/skips: 0 on this machine.

## Files/modules changed

- MIDI: `src/core/midi/MidiMessage.*`, error additions, native diagnostics contract;
- SysEx: `src/core/sysex/SysExFrame.hpp`, `SysExStreamParser.*`, `SysEx7.*`, `SyxFile.*`;
- transfer: `src/core/transfer/TransferEngine.*`;
- transports: fake, WMS, WinMM native API/transport realtime additions;
- tests: Stage-3 MIDI/SysEx, file, transfer, large-data, WinMM transport, WMS/WinMM local harnesses;
- build/CI: source/test targets, opt-in loopback runner, CI label wording;
- documentation: architecture, Project State, Risk Register, Stage-3 brief/report.

## Architecture and risk impact

No architecture deviation or new ADR was required. The implementation fills the layers and native send/receive
boundary already reserved by the approved Stage-2/Stage-3 architecture. Stage-2 route identity, one-backend
ownership, Qt independence, WMS MTA ownership, WinMM callback/worker split, and `MIDIHDR` invariants remain.

R-003 is closed for the generic software layer by conversion, malformed/incomplete, large-data, file, and local
native-loopback evidence. R-002 is further mitigated by repeated 10-KiB multi-buffer WinMM traffic and output
completion/failure tests. Physical/vendor-driver behavior remains a later User-owned hardware validation item.

## Deviations, Stop/Ask events, and known limitations

No Stop/Ask condition occurred. There was no byte mismatch, ambiguous lifetime, third-backend requirement,
architecture change, or P0/P1. Development-time compile/test corrections were local implementation fixes and
did not change the approved design.

Known later-stage limitations:

- native timestamps are retained but not normalized across backends;
- WMS maximum-transmission/scheduling policy and endpoint watcher subscription remain later work;
- real vendor drivers and physical synthesizers are explicitly unvalidated;
- profile/device protocols, checksums, ACK/NAK, restore semantics, and GUI behavior remain out of Stage 3.

Stage 4 did not start.

## Gate recommendation

**PASS.** All 43 mandatory applicable acceptance criteria are met by deterministic unit, large-data,
failure-injection, clean-build, and isolated native-loopback evidence. No unresolved P0/P1 is known. This is a
software/generic-engine gate only and does not claim physical MIDI hardware compatibility.
