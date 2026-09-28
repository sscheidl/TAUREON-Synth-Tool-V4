# Stage 5 Block B — targeted P1 review handoff

**Prepared:** 2026-09-26

**Repository:** `sscheidl/TAUREON-Synth-Tool-V4`

**Branch:** `codex/stage5-block-b-windows-evidence`

**Published branch HEAD:** `7dc09c9bd241cb773b25b41402d1aea892926d3f`

**Review subject:** uncommitted six-file remediation plus its documentation, not PR #13

**Requested reviewer role:** independent architecture/lifetime review; do not implement fixes

## Decision requested

Determine whether the dirty-worktree delta fully resolves the three open PR-#12 P1
findings without weakening accepted lifetime, route-identity, test-evidence or safety
contracts. Return actionable P0–P3 findings in the repository format:

```text
Severity: P0 / P1 / P2 / P3
Claim:
Evidence:
Why it matters:
Minimal recommended action:
Confidence: high / medium / low
```

Also answer explicitly:

1. May B-3 move from `P1 REMEDIATED LOCALLY / TARGETED REVIEW PENDING` to PASS?
2. Is the five-cycle ordinary gate plus `cycles >= 100` handle-growth gate structurally
   sound?
3. Because no raw log for the earlier 100-cycle product-host soak is retained, must a
   fresh 100-cycle run be captured before B-3 can pass?
4. Does adding `worker_mta_apartment_observed` to the public diagnostics struct create
   an unacceptable contract/ABI impact for the current static/source-built boundary?

## Required context

Read only this bounded context:

- [`STAGE_5_BRIEF.md`](../../stages/STAGE_5_BRIEF.md), especially §§14, 15.4, 19, 20, 23;
- [`STAGE_5_REPORT.md`](../../stages/STAGE_5_REPORT.md), especially the final
  2026-09-26 remediation section;
- [`ADR-0001`](../../architecture/adr/ADR-0001-backend-specific-route-identity.md);
- [`ADR-0002`](../../architecture/adr/ADR-0002-winmm-callback-and-header-ownership.md);
- [`QUALITY_POLICY.md`](../../process/QUALITY_POLICY.md), especially §§3, 5, 9, 10;
- the exact six-file worktree diff listed below;
- `product-host-loopback.log`, `software-tests.log`, `verification-summary.json`,
  `endpoint-cleanup.json`, and `local-source-hashes-before.json` in this folder.

Do not use PR #13 as evidence for this decision. Its independent P2 concerns exact PR
HEAD attribution in the hardware bundle and remains outside this six-file remediation.

## Original P1 findings and local disposition

### P1-1 — prove native receive activity before closing

Original problem: receive mode was entered and immediately closed without waiting for a
native callback/delivery; a later counter belonged to the separate send host.

Local remediation:

- `send_receive_probe()` opens only the temporary loopback output and sends one complete
  five-byte test SysEx frame using the selected backend representation;
- the receive host was already opened on the paired temporary input;
- `process_until()` requires callback sequence advancement plus a receiving snapshot
  with at least five bytes and one complete frame before close;
- post-close evidence requires `receive_active_callbacks > 0` and
  `receive_active_delivered > 0`.

Review focus: prove that the callback and delivery measured belong to the active receive
host and that the separate probe transport cannot invalidate lifetime/ordering evidence.

### P1-2 — reject shutdowns that do not reach `closed`

Original problem: the instrumented wrapper counted close calls but did not retain the
close result or final native state.

Local remediation:

- every wrapper `close()` records success/failure;
- evidence records the wrapped transport state after close;
- every lifecycle assertion requires all close calls successful, zero failed closes and
  final `TransportState::closed`;
- destruction, drops and late-callback assertions remain.

Review focus: ensure the last recorded state cannot be overwritten or missed along any
ordinary-cycle, receive-active or send-active teardown path.

### P1-3 — capture GUI and WMS apartment evidence

Original problem: B-3 was marked PASS without observing the GUI thread apartment or WMS
worker initialization apartment.

Local remediation:

- after `QApplication` construction, the product-host process calls
  `CoGetApartmentType` and requires `APTTYPE_STA` or `APTTYPE_MAINSTA`;
- after WMS `init_apartment(multi_threaded)`, the WMS worker calls
  `CoGetApartmentType`, requires `APTTYPE_MTA`, and records the observed state;
- WMS evidence requires `worker_mta_apartment_observed == true`;
- WMS and WinMM are executed in separate child processes to keep backend apartment and
  runtime state isolated;
- WMS links `ole32.lib` for the apartment query.

Review focus: validate call placement, thread identity, failure propagation, teardown
ordering and whether `APTTYPE_NA`/qualifier behavior needs additional handling.

## Exact implementation/test scope

```text
src/transports/IMidiTransport.hpp
src/transports/wms/CMakeLists.txt
src/transports/wms/WmsTransport.cpp
tests/integration/CMakeLists.txt
tests/integration/Stage5ProductHostLocal.cpp
tools/RunStage5ProductHostLoopback.ps1
```

Current diff size: 121 insertions, 17 deletions. The pre-verification patch is retained
as `local-changes-before.patch`; all six files matched the SHA-256 values in
`local-source-hashes-before.json` after verification and after the physical diagnostics.

## Cycle and timeout policy to review

- Ordinary registered product-host regression: five cycles per backend.
- Short-run handle samples are diagnostic only; `handle_growth_gate_applied=false`.
- Directional handle-growth assertion applies only when `cycles >= 100`.
- Product-host registered timeout: 120 seconds.
- Stage-2 100-cycle WMS lifecycle timeout: 180 seconds, increased from 120 after the
  prior combined run timed out at the external CTest boundary.
- No production sleep or timeout-only lifetime fix was introduced.

The Product Owner authorized shortening the ordinary test. An earlier task history says
the separate 100-cycle product-host soak passed, but no raw soak log was recovered into
this evidence folder. Do not convert that history into deterministic evidence without an
explicit reviewer decision or a fresh retained run.

## Fresh deterministic evidence

Environment: Windows 11 Pro build 26200, MSVC 19.44 for the verified build, Qt 6.10.3,
Windows SDK 10.0.26100.0, WMS SDK/Console 1.0.17-rc.4.25.

```powershell
cmake -S . -B .work/reconstruction-20260926 -G 'Visual Studio 17 2022' -A x64 '-DCMAKE_PREFIX_PATH=C:/Qt/6.10.3/msvc2022_64' -DTAUREON_BUILD_STAGE1_SPIKES=ON -DTAUREON_ENABLE_LOCAL_MIDI_INTEGRATION_TESTS=ON -DTAUREON_ENABLE_WMS_TRANSPORT=ON
cmake --build .work/reconstruction-20260926 --config Debug --parallel 6
ctest --test-dir .work/reconstruction-20260926 -C Debug -LE local-midi --output-on-failure
ctest --test-dir .work/reconstruction-20260926 -C Debug -R '^stage5_local_product_host_lifecycle$' -V
```

Results:

- configure/build PASS;
- 28/28 non-local tests PASS in 10.29 s;
- corrected product-host test 1/1 PASS in 34.65 s;
- WMS: five cycles, active RX callbacks/delivered 1/1, TX 1, drops/late 0/0,
  max shutdown 63 ms, final closes/state asserted, worker MTA asserted;
- WinMM: five cycles, active RX callbacks/delivered 3/1, TX 1, drops/late 0/0,
  max shutdown 37 ms, final closes/state asserted;
- GUI apartment: `main_sta` in both backend processes;
- temporary loopback pair removed; WMS and WinMM enumerations matched before/after;
- `git diff --check` PASS apart from line-ending conversion warnings.

The other five long local MIDI regressions were not rerun on 2026-09-26. Do not claim a
new combined 6/6 result.

## Explicit non-claims

- no final B-3 PASS before independent review;
- no long-term handle/leak claim from the five-cycle run;
- no physical send, SysEx dump, restore, preset or firmware validation;
- passive WMS console reception from Summit/KONTROL/MiniFreak is substrate evidence,
  not V4 product-path evidence;
- no native Windows 150%/200% visual pass;
- no Stage-5 gate closure, Stage-6 readiness, commit, push or merge.

## Expected output

Provide findings only for this bounded delta and evidence. End with one recommendation:

- `PASS — B-3 may close`, or
- `PASS WITH NON-BLOCKING FOLLOW-UPS — B-3 may close`, or
- `HOLD — list the minimum blocking corrections/evidence`.

Do not edit code, resolve GitHub threads, push, merge, or broaden into PR #13/reference
archive work.
