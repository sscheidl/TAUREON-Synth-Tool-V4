# Stage 3 Report – Generic MIDI, SysEx & Transfer Engine

**Stage:** 3
**Date:** 2026-08-24
**Implementation lead:** Codex
**Status:** PASS / CLOSED after targeted HOLD remediation and User/Project Manager gate
**Mandatory Claude Code re-review:** PASS WITH NON-BLOCKING FOLLOW-UPS; no P0/P1 findings
**Result:** **PASS WITH NON-BLOCKING FOLLOW-UPS**

## Outcome

Stage 3 implements the generic, Qt-independent MIDI/SysEx/transfer foundation and completes the production
realtime boundary for both approved backends. Exact MIDI 1.0 `F0 ... F7` framing ↔ 64-bit UMP SysEx7
segmentation/reassembly is independently tested at boundary sizes and with synthetic data beyond 1 MiB.
No device/profile logic, third backend, physical MIDI traffic, or global system change was introduced.

## Targeted HOLD review and remediation

The independent Claude Code review of commit `8030c20f4de1f621401a86c84994b408fba91817` placed Stage 3 on
HOLD. All three findings were confirmed against the production code:

- **P1:** WinMM diagnosed `MIM_LONGERROR`, `MIM_ERROR`, callback-queue overflow, and input-buffer requeue
  failure only through counters. There was no ordered semantic signal from the transport to capture/parsing, so
  a damaged frame could later appear complete.
- **P2:** a SysEx7 decode failure returned a malformed result but left the affected group's active sequence
  intact, allowing a later End packet to complete stale bytes.
- **P2:** `.syx` I/O failures generally omitted the relevant path and discarded available OS error context.

The remediation adds a backend-neutral, sequenced `MidiStreamEvent` carrying either a copied native message or
a `MidiDataLossEvent`. WMS and WinMM deliver it on their existing worker-consumer path; native callbacks do not
invoke application code. Loss markers are non-droppable relative to the bounded normal-data queues. WinMM emits
ordered markers for `MIM_LONGERROR`, queue overflow, input requeue failure, and shutdown-returned long data.
`MIM_ERROR` is also delivered visibly, but with `affects_sysex=false`: WinMM defines it as invalid short-message
input, not a long/SysEx input error. Diagnostic counters remain in place.

`SysExCaptureSession` is the production consumer that routes ordered MIDI 1.0 messages/loss markers to
`SysExStreamParser` and grouped UMP SysEx7 messages/loss markers to `SysEx7Assembler`. A loss taints the active
frame; if it precedes the next frame it taints that frame; if no more data arrives, `finish()` emits a malformed,
loss-affected result. Once that affected frame is terminated, later independent frames remain clean. Normal
`.syx` save rejects every malformed or loss-affected frame.

SysEx7 decode failure now immediately terminates the active sequence for only the packet's group, returning the
accumulated bytes as malformed. Continue/End after that failure cannot complete the old sequence; concurrent
groups remain independent; a fresh sequence on the affected group succeeds. `.syx` failures now distinguish
input, temporary staging, target, and final replace operations and retain the relevant path, stable error class,
native operation/code, and available OS error message. No architecture deviation was required.

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
- An ordered transport loss while a frame is active, before its next frame, or before capture close taints that
  capture; it cannot become a verified complete frame.
- The parser operates on application-owned byte copies. WinMM `MIDIHDR` ownership never crosses into it, so a
  frame may span buffers and one chunk may contain multiple frames without changing reconstruction.

## WMS SysEx7 conversion and reassembly

`SysEx7` implements the official 64-bit UMP Data Message form: message type, group, complete/start/continue/end
status, 0–6-byte count, and six payload slots. Encoding accepts only a complete, unaffected, 7-bit MIDI 1.0
frame; removes exactly the outer `F0`/`F7`; segments payload at six-byte boundaries; and preserves the selected
group. Reassembly is independent per group and restores exactly one `F0` and `F7`. Invalid counts/types,
non-7-bit payload, continuation/end without start, interrupted sequences, and unfinished sequences are visible.
Decode failure terminates only the affected group's active state and preserves its accumulated bytes as a
malformed frame; later packets cannot resurrect that sequence.

Unit coverage includes payload boundaries 0, 1, 5, 6, 7, 11, 12, 13, 18, and 19; single/multi-packet cases;
Start/Continue/End; consecutive frames; forward and reverse byte-equivalent roundtrips; malformed packets; and
incomplete sequences.
Targeted closure coverage adds Start → invalid → End, Start → invalid → Continue → End, one-group failure while
another group completes, and a fresh valid sequence after the failed group's state was reset.

## `.syx` I/O

`SyxFile` performs binary-safe reads and retains both original bytes and parsed frames. Normal save accepts only
complete, unaffected frames, does not overwrite by default, and writes through a project-local temporary file
with atomic replacement where supported. A separate explicit raw-save path is required for incomplete or
malformed content. Tests prove multiple-frame load/save and byte-identical valid load → parse → save, while an
unterminated file remains incomplete.
Targeted failure tests verify a missing input, a directory used as input, temporary-file creation below a
missing parent, and final replacement onto a directory. Each failure identifies the relevant path and operation
and retains a concrete OS cause/code when one is available.

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
on the worker. Overflow inserts a non-droppable, group-specific ordered loss marker. The receive route group is
enforced. Send validates complete UMP packet boundaries and route
group, then uses immediate send unless a `wms-native-ticks` timestamp was explicitly supplied. Shutdown closes
callback acceptance, discards/diagnoses queued application events, revokes handlers, disconnects, closes the
session, and prevents callback access to destroyed state. Close drains already accepted ordered events before
the application lifetime boundary ends.

### Native WinMM

Short RX is unpacked on the worker with the native millisecond timestamp. Short TX uses `midiOutShortMsg`.
Complete `F0 ... F7` long TX uses a transport-owned `WinmmOutputBuffer` registered before submit; ownership is
released only after `MOM_DONE`, unprepare, and legal close. Long RX continues callback → bounded queue → worker
copy/requeue. Close drains already accepted ordered data/loss events, retains ownership completions, resets,
waits, unprepares, and closes deterministically. Transport-level tests cover short/long success, native send
failure unwind, timestamp delivery, completion-before-unprepare, and the retained Stage-2 input-submit P1
regression.

The targeted production-path regression drives native fake callbacks through
WinMM transport → worker stream handler → `SysExCaptureSession` → parser. It proves normal complete/save,
`MIM_LONGERROR` + later `F7`, injected queue overflow between `F0`/`F7`, input requeue failure, loss followed by
close with no more data, visible non-SysEx `MIM_ERROR`, rejection by normal `.syx` save, and no taint leakage to
later independent frames.

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

# Targeted HOLD closure
cmake --build build/vs2022-x64 --config Debug --parallel
ctest --test-dir build/vs2022-x64 -C Debug -L stage3 --output-on-failure
ctest --test-dir build/vs2022-x64 -C Debug -L ci --output-on-failure

cmake -S . -B build/stage3-hold-final-clean-20260824 -G "Visual Studio 17 2022" -A x64 `
  -DTAUREON_BUILD_STAGE1_SPIKES=OFF `
  -DTAUREON_ENABLE_WMS_TRANSPORT=ON `
  -DTAUREON_ENABLE_LOCAL_MIDI_INTEGRATION_TESTS=OFF
cmake --build build/stage3-hold-final-clean-20260824 --config Debug --parallel
ctest --test-dir build/stage3-hold-final-clean-20260824 -C Debug -L ci --output-on-failure

tools/RunStage3WmsLoopback.ps1 `
  -Executable build/vs2022-x64/tests/integration/Debug/stage3_wms_local.exe -Cycles 500
```

Original Stage-3 completion evidence set for reviewed commit `8030c20`:

- current build and environment-independent CI-labeled suite: 8/8 PASS;
- clean build and clean CI-labeled suite: 8/8 PASS;
- clean configuration local-MIDI registration: 0 tests;
- complete opt-in local suite, including retained Stage-1/2 regressions: 5/5 PASS;
- Stage-3 local WMS/WinMM realtime tests: 2/2 PASS, 200 cycles total;
- final failures/skips: 0 on this machine.

Targeted HOLD closure evidence:

- targeted parser/file/WinMM production-path regressions: 3/3 PASS;
- first complete Stage-3 closure suite: 7/7 PASS, before the final WMS refinement was rebuilt locally;
- final CI-labelled suite: 9/9 PASS, including the retained Stage-2 partial-open regression and synthetic
  handle-growth tests;
- final clean configure/build and clean CI-labelled suite: 9/9 PASS;
- first complete opt-in local MIDI suite: 5/5 PASS in 170.79 s, before the final WMS overflow-marker
  coalescing refinement was rebuilt into the local binaries;
- final rebuilt local WinMM evidence remains PASS from the complete suite (8.12 s);
- pre-investigation final rebuilt WMS rerun: Stage-2 lifecycle and Stage-3 realtime both FAIL only their former
  `steady_span <= 1` process-handle assertion with `steady_span=2`; Stage-3 still reported 100 cycles, 100 TX,
  400 RX callbacks, 0 drops, 0 late callbacks, and exact byte integrity;
- isolated Stage-3 WMS reproduction: same `steady_span=2` failure after 100 cycles;
- instrumented 100-cycle Stage-2 and Stage-3 runs: full sample series recorded; both showed reversible
  `209 → 210 → 211 → 209` step behavior, no new late peak, and no final growth;
- instrumented 500-cycle Stage-3 run: 500 TX, 2,000 RX callbacks, 0 drops, 0 late callbacks, exact bytes;
  samples moved `207/208/209 → 207 → 205 → 203 → 205 → 207 → 209`, span 6 but no peak above the
  warm-up maximum;
- final corrected WMS criterion: Stage-2 and Stage-3 100-cycle runs 2/2 PASS;
- final complete opt-in local MIDI suite: 5/5 PASS in 192.61 s;
- `git diff --check`: PASS; final validation failures/skips: 0.

## WMS handle investigation and corrected test contract

The authorized investigation confirmed that `steady_maximum - steady_minimum` measured whole-process handle
dispersion, not retained per-cycle ownership. `GetProcessHandleCount` includes WMS/WinRT threadpool and RPC
activity, and the complete sample series showed reversible timed plateaus. The 500-cycle run increased the old
span from two to six while returning to the already observed maximum and completing five times the traffic
without cumulative growth. Raising the old numeric limit would therefore preserve the wrong property.

Both WMS harnesses now retain and print every sample and evaluate sustained growth over the same second-half
steady window. A failure requires all three growth signals:

1. a positive least-squares slope across the steady samples;
2. a steady-window peak above every handle count already observed during warm-up;
3. a trailing-window median above the leading-window median.

This tests a directional, newly expanding process envelope rather than its harmless internal range. No numeric
span tolerance, fixed sleep, resampling-until-green, deadline trick, or shifted steady window was introduced.
Synthetic CI tests reject both a one-handle-per-cycle leak and a slower one-handle-per-100-cycle leak, while
accepting flat data, reversible multi-handle jitter, bounded warm-up, and a late transient that returns to its
baseline. The retained Stage-2 and Stage-3 local WMS regressions pass with the corrected contract.

## Files/modules changed

- MIDI: `src/core/midi/MidiMessage.*`, error additions, native diagnostics contract;
- SysEx: `src/core/sysex/SysExFrame.hpp`, `SysExCaptureSession.*`, `SysExStreamParser.*`, `SysEx7.*`,
  `SyxFile.*`;
- transfer: `src/core/transfer/TransferEngine.*`;
- transports: fake, WMS, WinMM native API/transport realtime additions;
- tests: Stage-3 MIDI/SysEx, file, transfer, large-data, handle-growth analysis, WinMM transport, and WMS/WinMM
  local harnesses;
- build/CI: source/test targets, opt-in loopback runner, CI label wording;
- documentation: architecture, Project State, Risk Register, Stage-3 brief/report.

## Architecture and risk impact

No architecture deviation or new ADR was required. The implementation fills the layers and native send/receive
boundary already reserved by the approved Stage-2/Stage-3 architecture. Stage-2 route identity, one-backend
ownership, Qt independence, WMS MTA ownership, WinMM callback/worker split, and `MIDIHDR` invariants remain.

R-003 is again mitigated for the generic software layer by the complete native-loss → ordered marker → capture
taint → no verified-complete result → no normal save chain, plus conversion, malformed/incomplete, large-data,
file, and local native-loopback evidence. Hardware/vendor-driver residual risk remains separate. R-002 is
further mitigated by repeated 10-KiB multi-buffer WinMM traffic and output
completion/failure tests. Physical/vendor-driver behavior remains a later User-owned hardware validation item.

## Deviations, Stop/Ask events, and known limitations

No Stop/Ask condition occurred during the original implementation. The subsequent independent review did identify the P1
ordered-loss gap and two P2 gaps documented above; this report does not erase that history. All three were
corrected without an architecture deviation, byte mismatch, ambiguous lifetime, or third-backend requirement.

Final closure validation then triggered a Stop/Ask condition. After the complete local suite first passed, the
final WMS overflow-marker coalescing refinement was rebuilt into the local binaries. Both WMS lifecycle tests
then reproducibly observed a steady process-handle range of two rather than the established maximum range of
one. The Stage-2 test ended at the same 218 handles sampled after its first cycle (minimum 218, maximum 220), so
this evidence does not by itself prove a leak; the Stage-3 run retained exact traffic and zero drop/late-callback
results. The implementation does not change the test threshold or add sleeps. Determining whether this is a
runtime sampling fluctuation, a test-contract issue, or a new lifecycle defect was outside the then-authorized
three-finding closure scope. The Project Manager approved the bounded investigation without changing the
threshold. Full 100- and 500-cycle series confirmed reversible WMS-runtime plateaus rather than cumulative
ownership growth. The resulting directional growth criterion and its synthetic counterexamples are documented
above; all final suites now pass.

Known later-stage limitations:

- native timestamps are retained but not normalized across backends;
- WMS maximum-transmission/scheduling policy and endpoint watcher subscription remain later work;
- real vendor drivers and physical synthesizers are explicitly unvalidated;
- profile/device protocols, checksums, ACK/NAK, restore semantics, and GUI behavior remain out of Stage 3.

## Mandatory Stage-4 brief inputs from closure review

Claude Code accepted both items below as bounded, non-blocking refinements. Neither changes the accepted
architecture, neither reopens Stage 3, and neither was implemented during this closure task.

### FU-1 (P2) — overflow loss-marker coalescing across a frame boundary

WinMM currently coalesces queued `queue_overflow` markers while one marker remains pending. If the worker is
blocked inside an `invoke` command while input continues to overflow, that one marker can taint the frame open
when it is consumed but under-report later frames completed during the same sustained overflow window.
`dropped_events` remains observable, but frame-level validity can be too optimistic. The Stage-4 brief must
require a bounded refinement that either emits one marker per dropped event or retains episode taint through the
next relevant frame boundary, plus a deterministic blocked-worker/sustained-overflow/multiple-frame regression.

### FU-2 (P3) — short handle-series applicability

For runs shorter than 20 cycles, `HandleGrowth` has no distinct warm-up segment (`start == 0`) and derives the
reference maximum from the leading part of the same steady series. `new_steady_high` is therefore not a useful
independent signal for quick diagnostic runs such as `-Cycles 10`. Production 100/500-cycle evidence is
unaffected. The Stage-4 brief must require either `new_steady_high=false` when no warm-up segment exists or an
explicit not-applicable result for series too short to separate warm-up from steady state, with a short-series
regression.

Stage 4 did not start.

## Gate recommendation

**PASS WITH NON-BLOCKING FOLLOW-UPS — accepted by User/Project Manager.** Claude Code's targeted re-review
accepted the three corrected findings, their regression tests, and the evidence-based WMS handle-growth
contract with no P0/P1 remaining. The historical HOLD, original red `steady_span=2` result, Stop/Ask decision,
100/500-cycle investigation, and reason for replacing dispersion with directional growth remain recorded above.
R-003 remains mitigated rather than closed. The two reviewer follow-ups are mandatory inputs to the Stage-4
brief, but they do not block Stage-3 closure. Stage 4 did not start in this closure task.
