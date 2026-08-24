# Stage 1 WinMM spike evidence

**Recorded:** 2026-08-24
**Status:** PASS
**Safety substrate:** temporary Windows MIDI Services native loopback; no physical or third-party MIDI endpoint received traffic

## Test substrate and safety boundary

The installed WMS diagnostic loopback endpoints A/B were enumerated by the direct WMS API but were not
visible through native WinMM enumeration. The already-installed official WMS console was therefore used to
create one uniquely named temporary loopback pair. A representative 100-cycle run used:

```text
output: TAUREON S1 WMS A 342FBF
input:  TAUREON S1 WMS B 342FBF
```

Before creation WinMM exposed 16 ESI inputs and 17 outputs (Microsoft GS plus 16 ESI outputs). After
creation it exposed 18 inputs and 19 outputs, including both exact temporary names. The runner verified
that output A and input B each resolved exactly once before opening either handle. It rejected missing or
ambiguous names and accepted only names with the `TAUREON S1 WMS` safety prefix.

No driver was installed or updated. No registry, service, API-mode, `UseLegacyMidi`, reboot, or permanent
`PATH` change occurred. Bome/teVirtualMIDI was not loaded or used. No ESI or other physical output was
opened.

## Commands

```powershell
cmake --build build/vs2022-x64 --config Debug --target taureon_winmm_spike
build/vs2022-x64/spikes/winmm_direct/Debug/taureon_winmm_spike.exe --identity-only
powershell -NoProfile -ExecutionPolicy Bypass -File tools/RunStage1WinmmWmsLoopback.ps1 -Cycles 100
```

The PowerShell runner uses the installed official `midi.exe loopback create/remove` commands solely to
provide and remove the endpoint pair. The transport under test is a separate native WinMM executable linked
only to `winmm.lib`; it contains no WMS SDK or third-party transport dependency.

## Message and lifetime evidence

- Native enumeration used `midiInGetNumDevs`/`midiInGetDevCapsW` and
  `midiOutGetNumDevs`/`midiOutGetDevCapsW`.
- One short Note On message returned byte-exactly through the WMS-native crosswire.
- The long-message test transmitted the 10-byte frame `F0 7D 01 02 03 04 05 06 07 F7` and received the
  same bytes.
- Input owned two 1024-byte `MIDIHDR` buffers through prepare -> queue -> callback/requeue -> stop/reset ->
  unprepare -> close.
- Output prepare -> `midiOutLongMsg` -> `MOM_DONE` -> unprepare completed successfully; exactly one long
  completion was observed.
- All 100 open/close lifecycle cycles passed. Cycle 1 additionally executed the complete short-message and
  SysEx send/receive/`MOM_DONE` evidence path; cycles 2-100 exercised open/close and buffer teardown without
  sending. Each reset returned the two queued input buffers during teardown; those two expected callbacks
  occurred while ownership was still alive. Callbacks after close: 0. Requeue failures: 0.
- Process handles were 157 after cycle 1 and 157 after cycle 100: growth 0.

## Persistent route identity

The tested identity is device name + `wMid` + `wPid` + driver version, with numeric index retained only as
a volatile open/correlation hint. Deterministic resolver tests cover reorder, missing, changed product or
driver data, reappearance, and duplicate ambiguity. There is no fuzzy name fallback or silent rebind.

The live temporary pair resolved as:

| Direction | Name | `wMid` | `wPid` | Driver | Index hint |
|---|---|---:|---:|---:|---:|
| input | `TAUREON S1 WMS B 342FBF` | 1 | 25 | 256 | 17 |
| output | `TAUREON S1 WMS A 342FBF` | 1 | 26 | 256 | 17 |

After the first pair was removed, both names visibly failed resolution. The runner recreated the same names
with a new WMS association, resolved the same composite fields again without using the index as identity,
then removed the pair a second time. Final WinMM absence was confirmed. This proves the bounded
disappearance/re-enumeration behavior without unplugging hardware.

This composite identifies an individual WinMM MIDI 1.0 port and is backend-specific. It is not transferable
to a WMS endpoint/group identity. Stage 2 persistence must include the backend and require exact
re-resolution or visible user selection after a backend change; it must never silently translate or
fuzzy-rebind across backends.

## WMS correlation helper hypothesis

Correlation was isolated in `taureon_wms_winmm_correlation.exe` so a WMS SDK failure could not compromise
the WinMM transport process. With pinned RC4 (`1.0.17-rc.4.25`):

- apartment, SDK runtime, version, and service initialization passed;
- `FindEndpointDeviceIdForAssociatedMidi1PortNumber` for the WinMM input returned an empty endpoint ID;
- the subsequent output-number helper terminated the isolated process with `0xC0000005`;
- earlier isolated probing of `FindAllEndpointDeviceIdsForAssociatedMidi1PortName` terminated with
  `0xC0000409`.

These reproducible fail-fast results make the RC4 helpers unsuitable for route identity. They are negative
Stage 1 hypothesis evidence, not a conclusion about newer WMS SDKs and not a hidden part of the WinMM
transport. Exact WinMM identity and visible failure remain the demonstrated safe path.

## Cleanup and limitations

The temporary pair was removed in a `finally` path after every run and its absence was verified through
WinMM. No temporary endpoint remains.

No real hardware send, physical unplug/replug, large SysEx stream, or hardware validation was performed.
Stage 1 proves the native WinMM buffer/lifecycle mechanism only; broader SysEx engine coverage belongs to a
later approved stage.

The spike calls `midiInAddBuffer` directly from `midiInProc`. This is observed Stage 1 spike behavior, not
the intended production design and not evidence that multimedia operations inside callbacks are safe for
all vendor drivers. Stage 2 must keep native callbacks minimal and evaluate/prefer callback -> signal/queue
-> worker-owned `MIDIHDR` requeue.

The output `MIDIHDR` error path is also spike evidence rather than the production ownership model. Production
code requires RAII that keeps the payload and prepared header alive and guarantees reset/completion and
unprepare across every failure path.

The full temporary-loopback suite and byte-integrity path are not registered as normal CTests. Stage 2 must
provide an opt-in/labeled local regression path where the required Windows MIDI Services runtime and safe
loopback environment exist. Hosted CI must skip/report unavailable integration prerequisites instead of
claiming WMS/WinMM integration coverage it cannot execute.
