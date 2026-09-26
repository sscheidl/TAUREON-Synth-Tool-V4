# Stage 5 Block B — targeted P1 review result

**Received:** 2026-09-26

**Reviewed baseline:** branch `codex/stage5-block-b-windows-evidence`, published HEAD
`7dc09c9bd241cb773b25b41402d1aea892926d3f`, plus the uncommitted six-file remediation

**Reviewer action:** read-only review; no files changed and no PR threads resolved

**Scope:** the three open PR-#12 P1 findings and the bounded six-file remediation

## Recommendation

**HOLD — minimum blocking corrections/evidence:**

1. Commit and push the six-file remediation plus its documentation. Require the
   non-local Windows CI suite to pass on that immutable commit SHA. Bind later evidence
   to the commit SHA or Git blob identities rather than working-tree byte hashes.
2. On exactly that commit, run `RunStage5ProductHostLoopback.ps1 -Cycles 100` for WMS
   and WinMM and retain the complete raw log. Both backends must report
   `handle_growth_gate_applied:true` and `sustained_handle_growth:false`, with command,
   SHA, tool versions, runtime and endpoint-cleanup evidence.

The reviewer accepted all three P1 code corrections and requested no further blocking
code change. B-3 must nevertheless remain HOLD until both items above are satisfied.

## Accepted P1 dispositions

### Native receive activity before close

Accepted. The callback sequence, worker snapshot and active-receive diagnostics all
belong to the receive host. The uninstrumented temporary probe transport is destroyed
before the receive host closes and cannot supply the counters being asserted. The WMS
SDK initializer's reference-counted lifetime means shutting down the probe does not
tear down the concurrently open receive host runtime.

### Successful close and final `closed`

Accepted. Every wrapper `close()` records success/failure and the observed native state.
The shutdown `disconnect()` path goes through the wrapper. Assertions require all close
calls to succeed, zero failed closes and final `TransportState::closed`.

### GUI and WMS apartments

Accepted. The GUI apartment check runs on the main thread after `QApplication`
construction. The WMS check runs in `initialize_runtime()` on the implementation worker
after explicit MTA initialization; failures propagate through startup error handling and
the worker performs apartment uninitialization.

## Reviewer answers

1. **May B-3 close now?** No. The code corrections are accepted, but immutable-SHA CI
   evidence and a retained current-code 100-cycle soak are still required.
2. **Is five cycles plus a gate at 100 structurally sound?** Yes in principle. Five
   samples are too unstable for the directional handle criterion, so disabling that
   gate in the short regression is correct. The separate soak must actually run and be
   retained.
3. **Is a new 100-cycle soak required?** Yes. The historical task statement is not
   deterministic evidence and predates the changed measurement context.
4. **Is `worker_mta_apartment_observed` acceptable?** Yes for the current static,
   source-built boundary. It should be documented as WMS-specific before any future
   DLL/SDK boundary.

## Non-blocking follow-ups

- Add a dedicated opt-in `stage5_local_product_host_soak` CTest with a measured timeout;
  the current manual path accepts at most 100 cycles, so the gate applies only at exactly
  100.
- Measure and retain the actual Stage-2 WMS runtime before claiming a new combined 6/6
  result after its timeout increase from 120 to 180 seconds.
- Treat `worker_mta_apartment_observed == false` as either unobserved or not applicable
  outside WMS; a future ABI boundary needs versioning or a three-state representation.
- Consider per-transport-instance evidence or assert exactly one factory instance.
- Optionally compare the probe frame contents and add a close-during-partial-frame case.
- Carry a mixed WMS-to-WinMM same-process smoke test into Stage 6.
- Restore the lost CMake indentation before committing.

The final two purely editorial follow-ups—the WMS-only diagnostic comment and CMake
indentation—were applied after receipt of the review. They do not change runtime behavior
or the accepted P1 reasoning.

## Explicit boundaries

The review does not establish native Windows 150%/200% visual acceptance, V4 physical
MIDI/SysEx product-path validation, a new combined 6/6 local result, Stage-5 closure or
Stage-6 readiness. Passive Summit, KONTROL S61 MK3 and MiniFreak WMS reception remains
substrate evidence only.
