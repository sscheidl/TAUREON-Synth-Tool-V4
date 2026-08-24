# Stage 4 Report – Device/Profile Isolation Proof

**Stage:** 4
**Date:** 2026-08-24
**Implementation lead:** Codex
**Baseline revision:** `5994ea6b92d6431c6988045f0e841fa1e14bacfe`
**Status:** PASS / CLOSED
**Result recommendation:** **PASS WITH NON-BLOCKING FOLLOW-UP** — the mandatory Claude Code review accepted
the Stage-4 architecture; FU-3 is a bounded Stage-5 GUI/evidence handoff.

## Outcome

Stage 4 implements a Qt-independent, data-driven device/profile layer above the accepted generic MIDI, SysEx,
transfer and transport layers. It provides a strict versioned JSON schema, neutral Generic fallback,
deterministic profile registry, explicit matching/binding outcomes and exactly one bounded real profile.

The approved real target changed from Kawai K5000S to Novation Summit after the required fixture Stop/Ask. The
User approved `Crazy Sine.syx`, a byte-exact 527-byte single-patch dump from the User's Summit, for private
repository read-only tests only. No K5000 data was migrated. No device I/O, checksum work, conversion, repair,
restore behavior, GUI work or Stage-5 implementation was introduced.

## Profile architecture

The new `profiles` module contains only domain data, schema loading/validation, deterministic registration and
matching. It has no Qt, WMS or WinMM dependency. Profile interpretation does not enter `IMidiTransport`, the
generic SysEx parser/assembler, `.syx` persistence or transfer engine.

Schema v1 requires stable ID, schema/profile version, Generic/device identity, aliases, all explicit support
claims, safe descriptive metadata, optional profile defaults, warnings, recognition evidence and complete
provenance metadata. Unknown fields, duplicate JSON keys, unsupported schema, missing fields, wrong types,
non-integer numbers, invalid ranges, duplicate IDs/numbers/fingerprints, contradictory support ladders and
malformed recognition evidence fail visibly.

The support ladder remains explicit:

```text
Detect -> Read -> Inspect -> Extract -> Modify -> Serialize -> Transfer -> Validated Restore
```

No higher claim is inferred. The Summit profile claims only `Detect`; every later level is explicitly false.
No compiled protocol hook or module was needed.

## Matching, ambiguity and route isolation

`ProfileMatchStatus` distinguishes `Explicit`, `ConfidentSuggestion`, `Ambiguous`, `NoMatch`, `Invalid` and
`GenericFallback`. Evidence follows the approved order: saved explicit binding, exact device-native identity,
Universal MIDI Identity, exact SysEx fingerprint, manual selection, then Generic fallback.

The Summit suggestion uses only the fixture-observed prefix
`F0 00 20 29 01 11 01 33`. Port/display names and WMS/WinMM route identities are deliberately ignored as
device evidence. Missing explicit bindings fail visibly. Equal fingerprints produce sorted ambiguity evidence,
never first-registration selection. Reordered registration and backend/route changes do not alter resolution or
redirect output. The matcher receives an immutable `SysExFrame` and does not accept or mutate UMP storage.

## Generic behavior and capability safety

The Generic profile has no manufacturer/model, recognition fingerprint, symbolic controller interpretation or
support claim. Unknown data returns Generic fallback when present and `NoMatch` when absent. Generic Stage-3
monitor/capture/file/raw-transfer bytes remain authoritative and unchanged.

The Summit profile carries warnings but no pacing value that was not evidenced by the approved fixture. It does
not claim Read, Inspect, Extract, Modify, Serialize, Transfer or Validated Restore. A profile suggestion never
authorizes sending or changes the active transport route.

## Real fixture and provenance

Repository fixture:

```text
tests/fixtures/novation_summit_crazy_sine.syx
```

- source: User backup `Single Presets.zip`, entry `Single Presets/Crazy Sine.syx`;
- source device: User-owned Novation Summit, confirmed by User;
- permission: private TAUREON repository, internal read-only tests only;
- redistribution: forbidden;
- size: 527 bytes;
- framing: one `F0 ... F7` frame, offsets 0 through 526;
- embedded non-realtime status bytes: none;
- Summit prefix: `F0 00 20 29 01 11 01 33`;
- SHA-256: `9FAECB1B98BC1926369528263B7CF7E9C9BFC7291A822F5E3BC38280D5BB7409`.

The repository copy is byte-identical to the approved backup candidate. Tests generate tainted/malformed state
in memory rather than modifying or storing derivatives. Nothing sends the fixture to hardware.

## Stage-3 carry-forward closure

### FU-1 (P2) – overflow marker coalescing

The finding was confirmed. WinMM previously searched the complete pending callback queue and suppressed every
later overflow marker while any earlier marker remained queued. Coalescing now applies only to consecutive
dropped callbacks. A non-droppable callback, including a returned long-input header, ends that run; a later drop
therefore inserts a new ordered marker before later frame data.

The transport-level fake regression blocks the worker inside `send_on_worker`, saturates the callback queue,
places two distinct drop runs around non-droppable long callbacks and proves that both affected frames are
malformed/data-loss-tainted. After the episode drains, a later independent frame is complete and clean. Native
callback ownership and the two-buffer production invariant remain unchanged.

### FU-2 (P3) – short handle series

When `steady_start_index == 0`, no independent warm-up envelope exists. `new_steady_high` is now explicitly
false while slope/median diagnostics remain available. A monotonically increasing ten-sample regression proves
that the self-referential peak criterion is not applied. The 100/500-cycle production contract and historical
Stage-3 `steady_span=2` evidence are unchanged.

## R-003 integrated integrity chain

The real Summit fixture regression proves:

```text
ordered DataLoss / malformed capture
    -> fixture bytes remain unchanged
    -> profile match result is Invalid
    -> no selected real profile
    -> encode_sysex7 rejects
    -> save_syx_frames rejects
```

Profile matching cannot clear or downgrade frame status/taint. R-003 remains exactly
**Mitigated in software / targeted re-review pending**; the Stage-4 review accepted this integrated path, while
the retained risk status continues to require its separately scoped targeted re-review.

## Files/modules changed

- `src/profiles/DeviceProfile.hpp`: profile, support, evidence and result domain types;
- `src/profiles/ProfileLoader.*`: strict JSON parser, schema decoder and validation;
- `src/profiles/ProfileRegistry.*`: deterministic registration, loading, explicit resolution and matching;
- `resources/device_profiles/`: Generic and exactly one Summit profile;
- `tests/fixtures/`: immutable Summit fixture and per-item provenance metadata;
- `tests/unit/Stage4ProfileTests.cpp`: schema, registry, matching, capability, byte and taint coverage;
- WinMM overflow coalescing regression and short-series handle analysis refinement;
- schema, architecture, provenance, project state, risk, decision and stage documentation.

## Commands/builds run

```powershell
cmake --preset vs2022-x64
cmake --build --preset vs2022-x64-debug --config Debug --parallel
ctest --test-dir build/vs2022-x64 -C Debug -L ci -LE local-midi --output-on-failure

cmake -S . -B build/stage4-final-clean-20260824 -G "Visual Studio 17 2022" -A x64 `
  -DTAUREON_BUILD_STAGE1_SPIKES=OFF `
  -DTAUREON_ENABLE_WMS_TRANSPORT=ON `
  -DTAUREON_ENABLE_LOCAL_MIDI_INTEGRATION_TESTS=OFF
cmake --build build/stage4-final-clean-20260824 --config Debug --parallel
ctest --test-dir build/stage4-final-clean-20260824 -C Debug -L ci --output-on-failure
ctest --test-dir build/stage4-final-clean-20260824 -C Debug -N -L local-midi

ctest --test-dir build/vs2022-x64 -C Debug -L local-midi --output-on-failure
Get-FileHash tests/fixtures/novation_summit_crazy_sine.syx -Algorithm SHA256
rg -n -i "summit|novation" src/core src/transports
git diff --check
```

## Automated test summary

- current CI-labelled suite: 10/10 PASS;
- separate clean configure/build CI suite: 10/10 PASS;
- clean configuration local-MIDI registration: 0 tests, as required;
- complete opt-in local WMS/WinMM suite: 5/5 PASS in 206.43 seconds;
- local Stage-2 WMS lifecycle: PASS in 86.96 seconds;
- local Stage-3 WMS realtime: PASS in 87.00 seconds;
- local Stage-2/3 WinMM regressions and retained Stage-1 byte-integrity evidence: PASS;
- targeted Stage-4 profile, Stage-3 handle-growth and WinMM overflow tests: 3/3 PASS;
- failures/skips: 0.

The final no-device-branch search returned no Summit/Novation occurrence below `src/profiles`. Exactly two
repository profile files exist and exactly one is real. `git diff --check` passes.

## Stop/Ask events and diagnostic correction

1. The original K5000S target had no available provenance-approved fixture. Work stopped before implementation.
   The User changed the bounded target to Summit and approved the real single-patch fixture; no workaround or
   incomplete legacy dump was used.
2. `Choir Pad.syx` was rejected because its product header was `01 10`, unlike the Summit `01 11` evidence.
3. The first diagnostic version of the blocked-worker overflow test attempted three long callbacks while the
   production-compatible fake owns two input headers. The third wait failed and exception unwinding reached the
   deliberately fatal submitted-header destructor invariant, producing a Debug Runtime `abort()` dialog. This
   was a test-setup error, not a production defect. The final regression uses the available two headers to prove
   two affected frame boundaries and passes without weakening ownership or adding sleeps.

No architecture deviation, P0/P1 finding, driver/service/API-mode change, physical MIDI route or hardware send
occurred.

## Hardware-dependent items not tested

- live Summit identity request/response;
- real Summit read, transfer, checksum, write or restore behavior;
- vendor-driver behavior and physical cable/interface routing;
- all profile selection GUI behavior.

These are not claimed by the Stage-4 profile.

## Gate disposition

**PASS / CLOSED** at revision `986115d`. Claude Code's mandatory review accepted the Stage-4 architecture with
one non-blocking P3 follow-up. No Stage-4 code change is required for that finding. R-003 remains
**Mitigated in software / targeted re-review pending** as recorded in the risk register.

## Carried-forward Stage 4 review follow-ups

### FU-3 (P3) — Discarded manual profile selection is invisible in the match result

`ProfileRegistry::match` evaluates SysEx fingerprints (`ProfileRegistry.cpp:159-171`) before
`input.manual_profile_id` (`:173-183`). When a fingerprint matches, the function returns
`ConfidentSuggestion` for the recognised profile and the user's manual selection appears neither in
`selected_profile_id` nor in the evidence list.

This matches the approved selection order in `STAGE_4_BRIEF.md:132-139`, where manual selection ranks below
fingerprint evidence, and is therefore not a deviation. The consequence only becomes visible in the GUI: a user
who deliberately picks profile X while the data fingerprints as Y receives Y with no indication that their choice
was not used. A saved explicit binding (rank 1) behaves differently and does win.

Action: keep the selection order unchanged. Carry the discarded manual selection into
`ProfileMatchResult::evidence` as an additional entry so the Stage 5 UI can show that an explicit choice was
overridden by stronger evidence, and offer the user a way to promote it to a saved binding.

Confidence: high.

## Next action

Use FU-3 as a normative Stage-5 brief input. Do not change the accepted matching precedence or begin Stage-5
implementation until the Stage-5 scope is separately authorized.
