# Stage 2 Report – MIDI Core and Transport Interface

**Stage:** 2
**Baseline revision:** `fb543a5689d5e5e9ef0903cf42c4f1e3d13879bf` on `main`
**Completion revision:** `9bcbf0d825627461a285529c18484e5eae77eb7d` on `main` / `origin/main`
**P1 closure revision:** dedicated closure commit containing this updated report
**Status:** HOLD remediation complete / targeted re-review and Project Manager/User gate disposition pending
**Result recommendation:** PASS after P1 closure

## Completed work

- Added Qt-independent C++20 MIDI core types, native message/timestamp representation, explicit result/error
  classes, deterministic transport states, endpoint-change events, and transport diagnostics.
- Added `IMidiTransport` with independent optional RX/TX routes, enumeration, open/close, capabilities,
  backend identity, message boundaries, and endpoint-change boundaries.
- Added backend-specific WMS and WinMM route descriptors, versioned persistence, and an exact resolver with
  visible `Exact`, `Missing`, `Ambiguous`, and `Invalid` outcomes.
- Added a fake transport for environment-independent lifecycle, routing, delivery, and disappearance tests.
- Added the production WMS lifetime boundary: project-local pinned SDK input, build-time C++/WinRT
  projection, MTA worker ownership, direct endpoint/group enumeration, exact route open, and deterministic
  session/SDK/apartment teardown.
- Added the production WinMM lifetime boundary: direct enumeration, bounded callback queue, callback-to-worker
  header return/requeue, observable drops/late callbacks, deterministic close, and RAII-quality input/output
  `MIDIHDR` ownership primitives.
- Added ordinary CI unit tests and separately opt-in/labeled local WMS/WinMM regressions.
- Closed the targeted-review P1 in the WinMM partial-open submit-failure path and added an exact
  transport-level regression for the native-handle/unprepare ordering.
- Added architecture ADRs, CI policy, risk updates, and this gate evidence.

No Stage 3 SysEx engine, realtime monitor, transfer/pacing logic, product GUI, device profile, legacy
migration, third backend, or physical-hardware path was implemented.

## Production files/modules added or changed

- `src/core/midi/`: backend/direction types, native message model, errors/results, persistence, resolver,
  lifecycle state machine.
- `src/transports/IMidiTransport.hpp`: production transport contract.
- `src/transports/fake/`: deterministic fake transport.
- `src/transports/wms/`: production WMS enumeration/open/close and worker/MTA lifetime.
- `src/transports/winmm/`: production WinMM transport, injectable native API boundary, callback/worker queue,
  and `MIDIHDR` owners.
- `cmake/TaureonWmsSdk.cmake`: build-time pinned C++/WinRT projection.
- `tests/unit/`: environment-independent core, WinMM header-owner, and WinMM transport error-path tests.
- `tests/integration/` and `tools/RunStage2WinmmWmsLoopback.ps1`: opt-in local regressions with guarded
  temporary endpoint creation/removal.
- `.github/workflows/windows-ci.yml`: explicit environment-independent Stage 2 configuration and `ci` label.

## Route identity, persistence, and resolution

Persistence schema version 1 is a strict semicolon-delimited key/value representation with percent escaping:

```text
version=1;backend=wms;direction=input;endpoint=<EndpointDeviceId>;group=<0..15>
version=1;backend=winmm;direction=output;name=<port>;wmid=<n>;wpid=<n>;driver=<n>
```

The WinMM runtime index is never serialized. Unknown/duplicate fields, malformed escaping/numbers, unsupported
versions, invalid groups/directions, empty native identifiers, and mismatched backend variants are rejected.
Resolution compares the complete composite identity only. Reordered runtime indices remain resolvable; zero
matches are missing and duplicate exact matches are ambiguous. Display names never trigger fuzzy fallback and
WMS identities are never translated to WinMM identities.

## WMS production lifetime

`WmsTransport` creates one owned worker, initializes C++/WinRT as MTA there, initializes the pinned WMS SDK,
and creates/enumerates/opens all WMS objects on that context. RX and TX routes resolve independently; distinct
endpoint IDs receive distinct connections. Close disconnects and closes the session synchronously. Destruction
closes again idempotently, requests worker termination, shuts down the SDK, uninitializes the apartment, and
joins the worker. No Qt object or `QPointer` participates.

The local production test completed 100 open/close cycles against a uniquely named temporary WMS-native
loopback. WMS/WinRT lazily added process handles during warm-up; cycles 51–100 remained in a bounded 216–217
handle plateau (span 1) with the final transport state `Closed`. This is reported as bounded runtime warm-up,
not as return to the pre-initialization handle count.

Stage 2 does not yet subscribe to WMS realtime message or endpoint-change callbacks. The interface and core
event model exist; those native callbacks belong to the Stage 3 realtime path. Capabilities report this
honestly rather than simulating coverage.

## WinMM callback and MIDIHDR ownership

`midiInProc` captures native event metadata into a bounded queue and signals the worker. It does not call
`midiInAddBuffer`, application handlers, or Qt. The worker copies long-message bytes, marks header ownership
returned, delivers accepted data, and requeues input buffers. Completion events needed for ownership recovery
are prioritized over droppable short events, and worker control commands outrank traffic so a continuous event
flood cannot starve `close()`. Drop and post-acceptance callback counters are exposed.

Input/output buffer owners retain payload and `MIDIHDR` together and model prepare, submit, return/completion,
unprepare, and release. Submitted storage cannot be unprepared or destroyed. Close disables application
acceptance, stops/resets input, drains reset completions, unprepares returned headers, closes handles, and joins
the worker. Production code contains no sleep-based synchronization.

### Targeted review P1 and closure

The targeted Claude Code review placed Stage 2 on HOLD after finding one P1 in the input partial-open path.
When `midiInAddBuffer` failed, the prepared buffer was still a local `unique_ptr`; `close_on_worker()` closed
and nulled the transport handle before the local buffer destructor called `midiInUnprepareHeader` with its
stale handle. `MMSYSERR_INVALHANDLE` then caused the defensive destructor to call `std::terminate()`.

The fix registers the successfully prepared buffer in transport-owned `input_buffers` before calling
`submit()`. On failure, the existing close path therefore owns the buffer and performs stop/reset, legal
unprepare on the still-open handle, buffer release, and handle close in that order. Submitted memory is never
released early, the success path and callback ownership are unchanged, and no architecture deviation or
special destructor exception was introduced.

`stage2_winmm_transport_unit` drives `WinmmTransport::open()` through the exact injected
`midiInAddBuffer` failure. It asserts the explicit returned error, `Failed` state, successful subsequent close
to `Closed`, zero prepared headers, no live handle, no abort, and the exact native ordering
`prepare → submit failure → stop/reset → unprepare → close`.

The production WinMM transport completed 100 open/close cycles against the verified temporary WMS loopback
with identical sampled first/final process handle counts, 200 native header callbacks, zero dropped events, and
zero callbacks after acceptance closed. The Stage 1 opt-in short/SysEx test again proved exact short data,
10-byte long-message integrity, `MOM_DONE`, unprepare, 100 lifecycles, and no handle growth.

## Commands/builds run

```powershell
cmake --preset vs2022-x64 `
  -DTAUREON_ENABLE_LOCAL_MIDI_INTEGRATION_TESTS=ON `
  -DCMAKE_PREFIX_PATH=C:/Qt/6.10.3/msvc2022_64
cmake --build --preset vs2022-x64-debug --parallel
ctest --preset vs2022-x64-debug -L "stage2|ci" --output-on-failure
ctest --preset vs2022-x64-debug -L local-midi --output-on-failure

cmake -S . -B build/stage2-clean-20260824 -A x64 `
  -DTAUREON_BUILD_STAGE1_SPIKES=OFF `
  -DTAUREON_ENABLE_WMS_TRANSPORT=ON `
  -DTAUREON_ENABLE_LOCAL_MIDI_INTEGRATION_TESTS=OFF
cmake --build build/stage2-clean-20260824 --config Debug --parallel
ctest --test-dir build/stage2-clean-20260824 -C Debug -L ci --output-on-failure
ctest --test-dir build/stage2-clean-20260824 -C Debug -N -L local-midi
git diff --check

# Targeted P1 closure
cmake --build --preset vs2022-x64-debug --parallel
ctest --preset vs2022-x64-debug -R stage2_winmm_transport_unit -V
ctest --preset vs2022-x64-debug -L unit --output-on-failure
cmake -S . -B build/stage2-p1-clean-20260824 -A x64 `
  -DTAUREON_BUILD_STAGE1_SPIKES=OFF `
  -DTAUREON_ENABLE_WMS_TRANSPORT=ON `
  -DTAUREON_ENABLE_LOCAL_MIDI_INTEGRATION_TESTS=OFF
cmake --build build/stage2-p1-clean-20260824 --config Debug --parallel
ctest --test-dir build/stage2-p1-clean-20260824 -C Debug -L ci --output-on-failure
ctest --preset vs2022-x64-debug -L local-midi --output-on-failure
```

## Automated test summary

Final results:

- Clean build: PASS; MIDI core, WMS transport, WinMM transport, and unit executables compiled from scratch.
- Ordinary CI suite: 3/3 PASS (`stage2_core_unit`, `stage2_winmm_header_unit`,
  `stage2_winmm_transport_unit`).
- Opt-in local suite: 3/3 PASS (`stage2_local_winmm_wms_loopback`, `stage2_local_wms_lifecycle`, isolated
  `stage2_local_stage1_winmm_byte_integrity`).
- Clean build local-MIDI registration: 0 tests, as required with the option OFF.
- Final failures: 0.
- Final skips: 0 on this local machine.

During harness development, two WMS runs completed all 100 lifecycles but failed an initially over-strict
handle assertion (first required min=max; second compared the cold first sample to the warmed final sample).
The production transport did not fail. The final criterion records cold/final counts and requires a closed
transport plus a bounded second-half plateau; the repeated final run passed. No sleep was added to mask the
observation.

## Architecture impact

- [ADR-0001](../architecture/adr/ADR-0001-backend-specific-route-identity.md): accepted backend-specific,
  versioned route identity and exact resolver behavior.
- [ADR-0002](../architecture/adr/ADR-0002-winmm-callback-and-header-ownership.md): accepted minimal callback,
  bounded queue, worker requeue, and deterministic `MIDIHDR` ownership.

Both ADRs formalize decisions explicitly expected by the approved Stage 2 brief; they do not deviate from the
approved transport model. The later targeted Claude Code review found one P1 implementation defect in
partial-open ownership ordering. The fix uses the existing transport-owned buffer model and required no ADR or
architecture change. No P0/P1 remains in the closure candidate.

## Risk changes

- R-002 remains open because vendor-driver/hardware behavior is not tested, but production ownership,
  callback/worker separation, exact transport-level submit-failure coverage, header-level error coverage, and
  safe 100-cycle evidence now mitigate the architecture portion.
- R-005 probability is reduced to Low for the core layer because persistence and resolution now reject
  backend mismatch, ambiguity, and fuzzy rebinding. Later GUI reselection UX remains open.
- R-003 remains open and unchanged for Stage 3 byte-stream↔SysEx7 conversion/reassembly.

## Deviations from Stage 1

- The Stage 1 callback-side `midiInAddBuffer` pattern was not reused; production requeue occurs on the worker.
- Stage 1 spike code remains isolated and is not linked as a production library.
- WMS projection occurs at build time from the pinned project-local dependency rather than at configure time.
- RC4 correlation helpers are not used.

These are required production hardening outcomes, not scope deviations.

## Stop / Ask events

The targeted Claude Code review placed the gate on HOLD for the verified WinMM partial-open P1. The defect was
confirmed and fixed without expanding scope or changing architecture. No system-change requirement, identity
loss, byte uncertainty, additional P0/P1, or Stop/Ask condition arose during closure.

## Known limitations and hardware-dependent items not tested

- Production realtime WMS/WinMM receive/send, SysEx framing/conversion, transfer/pacing, and endpoint watcher
  subscription remain Stage 3 or later work. `send()` currently returns explicit `UnsupportedCapability`.
- WMS timestamp conversion and maximum-transfer behavior remain later-stage decisions.
- The actual Qt Widgets/`QApplication` host lifetime remains the accepted Stage 5 validation item.
- No ESI port, synthesizer, or other physical MIDI hardware was opened or sent to.
- No driver, registry, MidiSrv, Windows MIDI API mode, machine PATH, or global system setting was changed.

## Gate recommendation

**PASS after P1 closure.** All 21 Stage 2 acceptance criteria are met, all final applicable tests pass, and no
unresolved P0/P1 remains. Targeted re-review and Project Manager/User gate disposition may close Stage 2.
Stage 3 remained unstarted throughout remediation and must remain so until a separate authorization.

## Next action if approved

Project Manager/User records the Stage 2 gate disposition and separately prepares/approves Stage 3. Do not
begin Stage 3 from this report alone.
