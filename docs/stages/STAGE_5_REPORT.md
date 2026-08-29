# Stage 5 Report – Qt 6 Product GUI

**Stage:** 5
**Date opened:** 2026-08-24
**Implementation lead:** Codex
**Baseline revision:** `070cf6be1c4f562d7a3b8455616204e45b3f6d85`
**Status:** ACTIVE

## Start-gate record

Stage 4 is PASS/CLOSED at implementation revision `986115d`; the closure documentation handoff is pushed to
`origin/main`. Immediately before Stage-5 activation, `HEAD == origin/main` and the worktree was clean.

The User authorized Stage 5 and supplied the approved full `STAGE_5_BRIEF.md`. Its requirements are now tracked
in the repository. No Stage-5 product code has been changed at this opening record.

## Carried-forward Stage-4 follow-ups

- FU-1 (P2): implemented in Stage 4; no further GUI action beyond preserving the ordered-taint contract.
- FU-2 (P3): implemented in Stage 4; retain trend-based WMS resource evidence.
- FU-3 (P3): Stage 5 must preserve matching precedence; domain evidence must retain a temporary manual profile
  selection overridden by stronger fingerprint/identity evidence; the GUI must disclose it and offer deliberate
  promotion to a saved explicit binding. Tests must cover precedence, retained evidence, visible presentation, and
  explicit promotion.

## Initial architecture direction

The production target will be a Qt 6 Widgets `QApplication` host. Views will depend on Qt models and presentation
controllers; application services own workflow orchestration; the accepted Stage 2–4 transport, SysEx, transfer,
profile and route-identity core remains Qt-independent. The Stage-1 `taureon_wms_qt_harness` is retained only as
spike evidence and will not become the production host.

## Initial scope check

No conflict has yet been found between the approved brief and the accepted Stage-2–4 contracts. This report will
record composition-root, threading, test, performance, lifecycle, visual, and hardware-limit evidence as each
vertical slice is completed. Stage 6 is not authorized.

## Slice 1 — production host and shell baseline

The new `taureon_app` target is a real Qt 6 Widgets `QApplication` host. It is separate from the retained
Stage-1 `QCoreApplication` spike harness and links the Qt-independent `taureon_midi_core` only at the product
composition boundary. The initial shell provides a non-movable persistent connection bar and navigable pages for
the seven frozen workspaces. No transport is constructed, no route is selected, and no MIDI or hardware operation
can occur in this slice: connection selectors and Connect/Panic are intentionally disabled with an explanation.

`stage5_gui_smoke` constructs the production window offscreen and verifies the expected seven-workspace shell.
The first run exposed a runtime dependency issue: the Debug executable could not resolve `Qt6Cored.dll` when
CTest launched it with the ordinary process PATH. The observed Windows dialog named that missing DLL. The test
now prepends the existing `C:\Qt\6.10.3\msvc2022_64\bin` through CTest's process-local
`ENVIRONMENT_MODIFICATION`; this uses no DLL copying, Qt installation, permanent PATH change, driver, service,
registry, or MIDI configuration change. The resolved smoke test passes in 0.16 seconds.

For an interactive development start, `scripts/run-taureon-app.ps1` derives the configured Qt `bin` directory from
the local CMake cache and prepends it only to the launcher process before starting `taureon_app`. It likewise makes
no system-wide change and is not a release-deployment mechanism.

The host event-loop/lifetime proof is not claimed by this construction-only smoke test. It remains an explicit
later Stage-5 lifecycle slice under section 14 of the brief; no timeout or sleep was introduced to mask it.

### Current validation

```powershell
cmake --preset vs2022-x64
cmake --build --preset vs2022-x64-debug --config Debug --parallel
ctest --test-dir build/vs2022-x64 -C Debug -R '^stage5_gui_smoke$' --output-on-failure
git diff --check
```

Result: configure and Debug build pass; `stage5_gui_smoke` passes (1/1); `git diff --check` passes.

## Slice 2 — exact connection control and bounded monitor boundary

`ConnectionController` is Qt-independent and depends only on `IMidiTransport`. It enumerates the selected
backend, accepts only an exact enumerated route identity with the matching backend/direction, keeps RX and TX
independent, refuses a connection without either route, and exposes selected-route disappearance as a visible
`degraded` state requiring deliberate reselection. It performs no port-name guessing, fuzzy fallback, or
cross-backend translation.

`MonitorEventQueue` is an application-owned, mutex-protected bounded queue. A native callback may copy/move a
message into it without touching Qt. Full-queue events are rejected and counted; after GUI acceptance closes,
later events are separately rejected and counted. `MonitorEventBridge` is a GUI-thread timer adapter that drains
at most 512 events per 16 ms tick into `MidiMonitorModel`; shutdown stops the timer and closes queue acceptance
before presentation children are destroyed. No detached thread, `QPointer`, arbitrary sleep, or timeout is used.

The MIDI Monitor workspace now contains the production `QAbstractTableModel` table with the frozen nine columns.
Its history is capped at 10,000 rows and batch insertion evicts the exact oldest overflow before inserting. The
initial Qt-independent tests prove exact RX/TX connection, explicit missing-route failure, degraded state without
silent rebinding, queue capacity/high-water/drop accounting, bounded model eviction, raw-byte visibility, batch
drain, and rejection after GUI acceptance closes.

Current Debug validation: all 12 CI-labelled tests pass, including both Stage-5 tests and all retained Stage-2–4
unit/WinMM regressions; failures/skips are zero and `git diff --check` passes.

## FU-3 bounded completion

The Stage-4 matching precedence is unchanged. Native identity, Universal Identity, and SysEx fingerprint results
now append a distinct `overridden_manual_selection` evidence item when they displace a different valid temporary
manual choice. The winning `selected_profile_id` remains the stronger match. Saved explicit bindings retain rank
one and return only saved-binding evidence.

`ProfileSelectionService` keeps temporary and saved choices separate. Promotion is possible only from an actual
overridden-manual evidence item in the last result and never occurs merely by presenting the result.
`ProfileMatchPanel` visibly names the discarded choice, explains that stronger evidence won, and enables
**Remember binding** only when a promotion callback is deliberately available. Its offscreen test proves:

1. fingerprint precedence remains unchanged;
2. the discarded manual profile is present in domain evidence;
3. the GUI exposes the override visibly;
4. deliberate promotion produces a saved binding that wins the subsequent match.

The panel is present in the Devices & Profiles workspace. Wiring it to a live/imported SysEx workflow remains part
of the later profile/application composition slice; no route or transport behavior is coupled to profile choice.

## MIDI Monitor filters, pause policy, and 100,000-event evidence

The production monitor uses a `QSortFilterProxyModel` for direction and case-insensitive event-type filters. These
filters affect only visible rows. Clear resets only the bounded presentation model. Pause has an explicit policy:
transport/callback accounting continues, the bounded application queue is drained, and paused presentation events
are counted and discarded rather than creating hidden backlog. Resume affects only later events. The UI exposes
displayed and paused-discarded counts and explains this policy in the control tooltip.

The strict local responsiveness target was fixed at maximum 250 ms between independent 1-ms GUI heartbeat
callbacks before running the 100,000-event test. A producer thread injected exact MIDI 1.0 event copies through
the production `MonitorEventQueue`; the GUI thread used the production 512-event batch bridge and bounded model.
Recorded Debug result:

```json
{"events":100000,"accepted":100000,"dropped":0,"queue_high_water":169,"model_rows":10000,"heartbeats":886,"max_heartbeat_delay_ms":2,"elapsed_ms":908}
```

The accounting identity `accepted + dropped == 100000` holds; current queue size is zero after drain, history never
exceeds 10,000 rows, queue high-water remains below its 8,192 capacity, and displayed count equals accepted count.
No per-event widget allocation, crash, deadlock, or unexplained loss occurred.

## Connection-bar application worker and fake-transport GUI evidence

The product composition root now constructs one `ConnectionWorker`. The worker exclusively owns the selected
native transport and the Qt-independent `ConnectionController`; backend construction, enumeration, open, close,
snapshot access, transport destruction, and orderly backend switching execute on its joined application thread.
The GUI receives only `future<Result<ConnectionSnapshot>>` values and polls readiness without blocking the Qt
thread. No detached thread, arbitrary sleep, native callback access to Qt, cross-backend route translation, or
lower-layer contract change was introduced. Invalid worker construction is rejected before a thread is started.

The WMS transport retains its accepted internal MTA worker boundary. Construction and destruction of that
transport occur from the application worker, while its SDK/session objects remain confined to the transport's
established MTA thread. The composition-root declaration order destroys the window first, then joins and destroys
the connection worker and its transport, and only afterward destroys the callback sequence and bounded monitor
queue. Native receive callbacks copy messages only into `MonitorEventQueue`; they never invoke Qt.

The persistent connection bar now performs real backend enumeration and exact route selection. Auto remains an
inactive pre-connection policy until settings provide an exactly resolvable saved backend/routes. WMS labels show
display name, endpoint device ID, and one-based group; WinMM labels show display name plus manufacturer, product,
and driver identity. RX and TX remain independently optional. Backend changes close the current transport before
constructing the next backend. Selected-route loss is presented as **Degraded** with the original binding retained
and no fuzzy rebinding. Panic remains deliberately disabled with an explanation until its backend-appropriate,
explicit-user-action implementation is completed.

`stage5_connection_worker_unit` proves transport creation occurs off the caller/GUI thread, exact RX/TX connect,
route-loss degradation without rebinding, deterministic disconnect, backend switching, and rejection of an empty
factory without leaking a joinable thread. `stage5_connection_ui_unit` drives the production `MainWindow`, worker,
controller, and bounded monitor boundary with `FakeMidiTransport`. It proves endpoint/group identity is visible,
input-only and output-only connection both work, Connect/Disconnect transitions are explicit, and selected TX
disappearance becomes visibly degraded. The GUI test uses a bounded event-processing guard and no sleep.

No physical MIDI endpoint was selected and no driver, service, registry, API-mode, system PATH, or machine-wide Qt
setting was changed. Product-host native WMS/WinMM lifecycle and close-while-active evidence remains a later
Stage-5 validation slice and is not claimed by these fake-transport tests.

### Paused checkpoint validation

Before this active Stage-5 implementation checkpoint was frozen, the Debug build passed and the complete
CI-labelled, non-local suite passed **17/17** with zero failures. This included all seven current Stage-5 tests,
the monitor stress test, both new connection tests, and the retained Stage-2/3 WinMM partial-open and transport
regressions. `git diff --check` also passed.

The separately invoked local-MIDI suite passed its first two tests
(`stage2_local_winmm_wms_loopback` and `stage3_local_winmm_realtime`) before the User requested a pause; the run
was then deliberately interrupted while the third test was active. This checkpoint therefore does **not** claim a
complete local-MIDI rerun, separate clean build, product-host native lifecycle result, or Stage-5 gate result.
Stage 5 remains **ACTIVE** and Stage 6 remains unstarted.

### Resumed checkpoint validation — 2026-08-25

The deliberately interrupted local run recorded above was resumed without changing its test contract. All five
registered local-MIDI regressions passed: Stage-2 WinMM/WMS loopback, Stage-3 WinMM realtime, Stage-2 WMS
lifecycle, Stage-3 WMS realtime/trend, and retained WinMM byte-integrity evidence. Total local test time was
223.89 seconds. Only the previously approved diagnostic loopback paths were used; no physical MIDI endpoint was
selected.

A first truly fresh configure correctly exposed that Qt was not discoverable from the ordinary process
environment. No system PATH or Qt installation was changed. The failed disposable build directory was removed,
and the clean configure was rerun with the exact existing SDK prefix
`C:/Qt/6.10.3/msvc2022_64` supplied only as that CMake process's `CMAKE_PREFIX_PATH`. The separate build and its
then-current CI suite passed 17/17. This records a clean-build invocation requirement, not a release deployment
solution. The disposable clean-build directory was removed after verification.

## SysEx Transfer receive/send slice

`SysExTransferSession` is a Qt-independent application service over the accepted Stage-3 parser, capture session,
SysEx7 encoder, `.syx` loader/writer, and transfer engine. It retains exact source bytes and frames, counts complete,
incomplete, malformed, and data-loss-tainted frames separately, and allows verified received-data save only for a
finished receive capture whose frames are all complete and unaffected. Saving uses the accepted atomic,
no-replace-by-default writer. Imported sources remain read-only. No payload repair, normalization, checksum,
conversion, retry, or device-specific codec was added.

Raw-send preparation rejects empty, incomplete, malformed, or tainted documents. WinMM receives exact MIDI 1.0
`F0...F7` frames. WMS receives the accepted SysEx7 representation built for the group carried by the exact selected
TX route; one document frame remains one paced transfer-engine message. The UI labels the operation **Raw Send**,
shows the actual backend-specific TX identity/group, and starts only from a direct button click. It never sends on
startup, file load, profile match, route connection, or capture completion. **Validated Restore** remains disabled
with an explanation because no profile/protocol declares and implements that capability.

The `ConnectionWorker` owns the session and each active `TransferEngine` alongside its native transport. Backend
switch, disconnect, and worker destruction request cancellation, join the transfer, finish capture, then close and
destroy the transport in deterministic order. A second active transfer is rejected. Receive events enter through
the native stream callback, which never touches Qt and copies only into a bounded 8,192-event application queue.
When that queue is full, every rejected event increments diagnostics and is represented by a backend/group-aware
SysEx-affecting loss marker; the Stage-3/4 taint contract therefore reaches the capture session rather than becoming
silent GUI loss. Commands that finish capture or close a connection drain already accepted stream events before
the terminal transition.

The product SysEx Transfer workspace now shows source name, deterministic device/profile evidence, match status,
frame/byte and integrity counts, application drops, exact TX route/group, explicit pacing, progress, bounded log,
frame model, and selected-frame raw bytes. Receive/Stop Receive, Raw Send, Cancel, Save received data, Clear, and
Open `.syx` are functional or state-disabled with an explanation. The accepted Stage-4 profile files are deployed
beside the development executable. The approved Summit fixture is recognised by its deterministic fingerprint as
a confident `Novation Summit` suggestion. Its `transfer=false` claim remains visible; the generic Raw Send control
does not present that profile as supporting transfer or validated restore. Invalid/tainted evidence produces an
invalid match with no selected device profile, so recognition cannot clear or hide taint.

### SysEx Transfer evidence

- `stage5_sysex_transfer_session_unit` proves byte-exact fixture import, WinMM bytes, WMS group encoding, complete
  receive, explicit DataLoss/Taint propagation, Raw-Send/save rejection for tainted data, verified received-data
  atomic save, Summit fingerprint recognition, and preservation of its negative capability claims.
- `stage5_connection_worker_unit` proves fake WMS raw-send completion, exact one-message accounting, receive and
  tainted receive through the production worker boundary, and deterministic application-queue overflow. With the
  worker held inside enumeration, 9,000 injected events produce the exact bounded result: capacity 8,192 and 808
  reported drops.
- `stage5_connection_ui_unit` proves the production widget/worker/controller/fake-transport chain for Receive/Stop,
  frame presentation, file load with zero automatic sends, visible WMS endpoint/group, visible Summit match and
  negative transfer capability, explicit one-click Raw Send, and disabled Validated Restore.

Current Debug build and the complete CI-labelled non-local suite pass **18/18**, including all eight current
Stage-5 tests and the retained Stage-2 partial-open/WinMM and Stage-3 regressions. A second separate clean configure,
build, and CI run over this completed SysEx slice also passes **18/18**. `git diff --check` passes. The disposable
clean-build output was removed. Product-host native WMS/WinMM close-while-active evidence and the remaining Stage-5
workspaces are still pending; Stage 5 remains **ACTIVE** and Stage 6 remains unstarted.

### Targeted P3 follow-up — application loss-marker sequence

A targeted review found that synthetic loss markers created when the bounded Stage-5 application stream queue
overflowed used `sequence = 0`, while transport-originated events carry their native callback sequence. The
current FIFO consumer does not inspect or reorder by sequence, so this was not an observed capture defect, but a
later sorting or deduplication layer could have moved the synthetic marker ahead of the affected stream range.

The queue now retains the exact sequence of the most recently rejected transport event and assigns that value to
each still-pending synthetic marker. Backend, group, drop accounting, bounded capacity, callback ownership, FIFO
delivery, and DataLoss/Taint behavior are unchanged. Marker state is consumed only after its queue insertion has
succeeded, preserving the existing exception-safe pending-loss accounting. The transport-level worker regression
still injects 9,000 events into the blocked 8,192-event boundary, proves exactly 808 drops, and now additionally
proves `application_last_loss_sequence == 8999`. No architecture deviation or Stage-6 work was required. Stage 5
remains **ACTIVE**.

## SysEx Manager slice — targeted review follow-up pending verification

PR #2 adds the deliberately generic, read-only SysEx Manager workspace: user-selected `.syx` files are
inspected as file/frame data, with deterministic FNV-1a 64 identity aids and exact file, frame, and payload
duplicate evidence only. The Manager reuses the accepted Stage-3 parser and atomic writer, exports or merges only
verified complete unaffected frames, and never mutates source files. Profile recognition is annotation only; no
device-specific codec, librarian semantics, automatic cleanup, or automatic transmission is introduced.

The original independently reviewed PR revision was
`6706ef930b18f0fcb0f769a46e89662bbbd4812e`. Its historical Windows CI evidence is
[Windows CI #29](https://github.com/sscheidl/TAUREON-Synth-Tool-V4/actions/runs/33227162834):
`windows-2022`, MSVC v143 / `cl.exe 19.44.35228.0`, CMake 3.31.6, Qt 6.10.3 `msvc2022_64`,
Configure PASS, full Debug build PASS, and 21/21 cloud-capable CTest tests PASS. That is historical evidence for
the original slice revision only.

Three internal Codex review findings on the earlier implementation were corrected before that review: Manager
handoff now supplies the inspected retained document rather than rereading a path; multi-file selection retains
actual item/frame references for Merge; and duplicate analysis hashes/buckets candidates and compares bytes only
within matching buckets. Claude Code reviewed the exact original revision with
**PASS WITH NON-BLOCKING FOLLOW-UPS**.

### Targeted follow-up status

The repair implementation revision
`e870611b7d854a23e5464c12ce2526c7a2bc9a17` passed
[Windows CI #67](https://github.com/sscheidl/TAUREON-Synth-Tool-V4/actions/runs/33235022070):
`windows-2022`, MSVC v143 toolset 14.44.35207 / `cl.exe 19.44.35228.0`, CMake 3.31.6, and
Qt 6.10.3 `msvc2022_64`. Configure and the complete Debug build passed; all 21 registered
cloud-capable CTest tests passed with zero failures.

- **M-1 — verified:** the Transfer panel explicitly accepts or rejects a Manager handoff, reports its eventual
  worker result, and navigation occurs only after successful document load. A rejected request preserves the active
  capture/document state and does not send.
- **M-2 — verified:** targeted negative evidence covers non-duplicates, tainted Manager documents, malformed
  versus incomplete input, invalid selection/error codes, failed-output cleanup, immutable source bytes, merge
  ordering, and no automatic fake-transport send.
- **L-1 — verified:** refresh snapshots contain summary metadata rather than raw/frame byte vectors; selected
  inspection copies only one frame. The raw inspector is explicitly limited to 256 displayed bytes and names both
  shown and total byte counts; stored/exported/transferred data remains complete.
- **L-2 — verified:** native Save dialog confirmation is propagated as `replace_existing` to the existing
  `save_syx_frames` / atomic-write path. Unconfirmed targets retain the no-replace default.
- **L-3 — OPEN Product Owner decision:** the Manager workspace is user-initiated and currently has no fixed total
  file/frame/byte capacity limit. No ad-hoc cap was introduced because large SysEx support remains required. A
  concrete capacity/memory policy must be decided before the final Stage-5 gate.
- **L-4 — verified:** Manager item absence uses `not_found`; empty or invalid frame selection uses
  `invalid_argument`, with direct code assertions.
- **L-5 — completed:** this report records both the original reviewed revision and its historical CI, plus the exact
  successful follow-up implementation revision and CI evidence above.

This remains automated Windows software evidence only. Offscreen GUI tests do not establish a visual Windows GUI
review. Real MIDI/SysEx device, port, WMS lifecycle, timing, and hardware validation remain pending after
09.09.2026. The five local hardware/loopback tests remain unregistered in cloud CI; none has been simulated or
enabled here. Stage 5 remains **ACTIVE**; no Stage-5 gate or merge is implied.
