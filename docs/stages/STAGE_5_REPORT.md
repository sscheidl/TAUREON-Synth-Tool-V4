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


## Devices & Profiles composition slice — FU-3 consumed

PR #3 connects the existing Qt-independent profile-selection policy to the existing, retained SysEx
Transfer session. It does not introduce port binding, route persistence, connection changes, or transmission.

The functional implementation revision
`7548ea725655abe46cb4b8ecbad68babab772a2d` passed
[Windows CI #75](https://github.com/sscheidl/TAUREON-Synth-Tool-V4/actions/runs/33241434863):
Windows Server 2022 / `windows-2022`, MSVC v143 toolset 14.44.35207 /
`cl.exe 19.44.35228.0`, CMake 3.31.6, Qt 6.10.3 `msvc2022_64`.
Configure and the complete Debug build passed; all 21 registered cloud-capable CTest tests passed with zero
failures or skips.

- `SysExTransferSession` owns the session-scoped `ProfileSelectionService` and re-evaluates only its
  retained, verified frame data. The registry is read-only for this purpose.
- `ConnectionWorker` queues temporary selection and deliberate promotion through the existing application worker
  and returns a presentation snapshot only. Neither command opens/closes a route nor starts Raw Send.
- Devices & Profiles now lists real non-Generic profiles, permits an explicit temporary choice, presents the
  matching evidence for imported or captured transfer data, and enables “Remember binding” only for the explicit
  FU-3 override case.
- `stage5_connection_ui_unit` adds the end-to-end negative proof: a deliberately different temporary manual
  profile is overridden by the Summit fingerprint; the override/evidence is retained while the workspace is not
  selected; deliberate promotion resolves the saved binding; the Fake Transport reports zero transmitted messages
  throughout both actions. The existing explicit Raw Send remains the sole send trigger.

This is automated Windows software evidence only. The offscreen test does not establish visual GUI quality.
Real MIDI/SysEx devices, ports, WMS lifecycle, timing, and hardware validation, plus a visual Windows GUI review,
remain pending after 09.09.2026. The five local hardware/loopback tests remain unregistered in cloud CI; none has
been enabled or simulated. Stage 5 remains **ACTIVE**; no Stage-5 gate or merge is implied.


## Slice 6 closure evidence repair (S6-1)

The Devices & Profiles slice’s final evidence chain is recorded here without reopening that
slice:

- G-1 deterministic completion-lifetime correction: 28dc6dcaa5cefcdd3442e17619bb3d95ac92923b;
- final Claude-reviewed Slice-6 HEAD: c90fdb700ca9dea134a9d810c3abbe87454334c2;
- Windows CI #81: PASS;
- Claude verdict: **PASS – STAGE 5 SLICE 6 READY TO MERGE**;
- merge commit: e2719ee5f73bac79da781c539efc3cb9fdd733f6;
- post-merge Windows CI #82: PASS.

This closes S6-1 as a documentation/evidence-chain correction only. S6-2 (optional second
queued-delivery destroy regression), S6-3 (synchronous cross-panel snapshot observer), and
S6-4 (target-specific AUTOMOC) remain non-blocking observations for the final Stage-5 gate.

## Slice 7 — Diagnostics + Settings (implementation evidence)

Slice 7 replaces the Diagnostics and Settings placeholders with a bounded product minimum while
preserving Stage-2 route semantics and the existing worker/transport boundary.

- app::DiagnosticBundle builds a metadata-only snapshot from safe connection, transfer, and
  monitor-queue snapshots. It never serializes raw MIDI/SysEx, source names, transfer logs,
  manager contents, or user files. Values unavailable from a safe existing contract are
  explicitly labeled **not observed**, not estimated.
- app::SettingsStore is Qt-independent, versioned (schema v1), atomically written, and
  handles missing, corrupt, partial, v0-migrated, and future settings with explicit safe
  fallback. Persisted routes use the existing PersistedMidiRoute schema; no runtime index,
  display-name matching, cross-backend matching, or reconnect action is introduced.
- The Settings workspace can deliberately capture only currently observed exact routes as
  preferences. Saving or opening Settings does not open, close, select, rebind, or send on a
  route. Saved preferences are not automatically applied to an active connection.
- BoundedLog provides a mutex-protected, capacity-bounded log with a configurable minimum
  level. It has no network/telemetry path and is not used for synchronization.
- stage5_settings_diagnostics_unit proves Settings save/reload semantic equality, v0 migration,
  future/corrupt/partial fallback, invalid-route rejection without fuzzy binding, bounded logging,
  and a diagnostic-bundle marker exclusion against a non-empty Transfer-session SysEx document.
  The marker and raw frame representation are absent while allowed version/counter metadata is
  present.
- stage5_settings_diagnostics_ui_unit proves the real panels and controls exist, Settings can
  deliberately capture and round-trip exact WMS RX/TX identities, and opening the panels does not
  instantiate a backend transport, connect a route, or send MIDI.

Functional implementation revision
d5f10b3ddb90a9d4348f94ccf4757e63a7dfeba7 passed
[Windows CI #86](https://github.com/sscheidl/TAUREON-Synth-Tool-V4/actions/runs/33267531579):
Windows Server 2022 / windows-2022, MSVC v143 toolset 14.44.35207 /
cl.exe 19.44.35228.0, CMake 3.31.6, Qt 6.10.3 msvc2022_64.
Configure and the complete Debug build passed; **23/23** registered cloud-capable CTest tests
passed with zero failures or skips, including stage5_gui_smoke,
stage5_settings_diagnostics_unit, and stage5_settings_diagnostics_ui_unit.

This is automated Windows software evidence only. Real MIDI/SysEx devices, port hotplug, WMS and
WinMM runtime/API behavior, timing, hardware lifetime, visual Windows GUI quality, and High-DPI
125/150/200% inspection remain pending after 09.09.2026. The five local hardware/loopback tests
remain unregistered, unskipped, and unsimulated. Stage 5 remains **ACTIVE**; no Stage-5 gate is
implied.

### Remaining non-blocking follow-ups

- L-3: Product Owner Capacity/Memory Policy decision before the final Stage-5 gate.
- N-1: append future MidiErrorCode values as a compatibility convention.
- N-2: duplicate is_valid_for_transfer() rule.
- N-3: justify or remove currently test-only add_document() if still unused at gate.
- S6-2, S6-3, S6-4 as recorded above.


S7-1 targeted repair evidence

S7-1 uses the selected variant: an explicit DiagnosticExportPolicy reaches DiagnosticBundle::make_snapshot. With include_route_identity_in_bundle=true, the bundle contains the observed RX/TX route metadata; with false, receive_route and transmit_route remain structurally present but contain exactly `omitted by settings`, distinct from `not observed`. No other endpoint identity is serialized; selected_backend remains backend-only.

Repair revision b061dda4f0f649f6622c8eda8b45712f368d534c passed Windows CI run 33293921972 (pull_request): full Windows Server 2022 configure and Debug build PASS; 23/23 registered cloud-capable CTest tests PASS, 0 failed, 0 skipped. The five local hardware/loopback tests remain unregistered, unskipped, and unsimulated. Real MIDI/SysEx hardware, WMS/WinMM runtime and timing, visual Windows GUI, and High-DPI validation remain pending after 09.09.2026.


## Slice 8 — Bounded Library / Librarian foundation (review pending)

Slice 8 introduces a Qt-independent, domain-neutral Librarian contract:
`ILibrarianProvider → Collection → Bank → Slot → optional semantic object`.
Stable identities, occupancy, per-slot read-only state, known/unknown/unavailable capacity,
and per-bank operation availability are modeled without fixed bank labels, program numbering,
or capacity assumptions. Known capacity bounds presentation; unknown capacity displays only
provider-observed slots. This is **Librarian model/presentation boundedness**, not closure of
**L-3 SysEx Manager Capacity / Memory Policy**.

The production `LibrarianPanel` replaces the former placeholder with a reusable
`QAbstractTableModel/QTableView` workspace. It uses one model and one table, supports
extended selection and keyboard navigation, clean provider/bank replacement via model reset,
and labels unsupported semantic actions as disabled with a textual reason. State is never
expressed by color alone.

No production semantic provider is registered. Consequently both Generic and Novation Summit
correctly display: **Semantic Librarian support is unavailable for this profile.** The only
`InMemoryLibrarianProvider` is local to `Stage5LibrarianTests.cpp`; it is not linked,
instantiated, selectable, or registered by `taureon_app`.

The Librarian has no dependency on the SysEx Manager, transfer session, transport, routes, or
profiles. It does not inspect a SysEx frame, infer a program name/slot/bank from bytes or
filenames, or turn a recognized Summit frame into a semantic object. Thus a complete or
recognized SysEx frame remains raw data and does not promote a semantic Librarian capability.
Changing a Librarian provider, collection, bank, slot, or selection is presentation state only:
there is no connect/disconnect, route rebinding, Raw Send, Restore, or MIDI/SysEx send path.

`stage5_librarian_unit` keeps its test-only semantic provider local to the test target.
At the **domain** level it proves a valid snapshot with multiple banks, different known capacities,
a zero-slot bank, an unknown-capacity bank, occupied/empty/read-only slots, and unavailable
operations. It also rejects a provider marked unavailable while carrying collections, duplicate bank
identity, and entries exceeding known capacity.

At the **model/view** level it renders and selects Alpha (known capacity 3), then actually switches
the production panel's bank selector to Empty (known capacity 0, zero rows) and Unknown (two
observed rows, unknown capacity), before returning to Alpha (three rows). It verifies
row/column/role/invalid-index contracts, reset behavior, explicit Writable versus Read-only slot
access labels, real `QKeyEvent` keyboard navigation, and Shift range selection.

**S8-1 repair:** revision `0e154d5f9e2f7e0737aca405ff1d0a7423989b6d` fixes the false
Read-only access label for writable slots and adds the targeted S8-1 through S8-3 evidence above.
[Windows CI #130](https://github.com/sscheidl/TAUREON-Synth-Tool-V4/actions/runs/33333659405)
passed its complete Windows Debug build and **24/24** registered CTest tests (0 failed, 0 skipped).
Starting main and starting Slice-8 branch were both remotely verified at
`d0de6306782797455779e70f63b2d7c90d550a31`. The working branch is
`codex/stage5-librarian-foundation`; [Draft PR #5](https://github.com/sscheidl/TAUREON-Synth-Tool-V4/pull/5)
remains unmerged.

[Windows CI #124](https://github.com/sscheidl/TAUREON-Synth-Tool-V4/actions/runs/33331596362)
passed on implementation/test head `a033b8a86472b5df336fc2fb6ba6ffc2939251c3`: full Windows
Debug configure/build PASS; **24/24** registered CTest tests PASS, **0** failed, **0** skipped.
The additional registered test is stage5_librarian_unit. This is automated Windows software
evidence only. The five hardware/loopback tests remain **NOT REGISTERED — NOT SKIPPED — NOT
SIMULATED**; real hardware, WMS/WinMM product-host lifetime/timing, visual Windows quality, and
High-DPI inspection remain outside this slice.

The separate `claude/stage5-gate-cleanup` branch (N-1, N-2, N-3, S7-3, S7-4) is untouched.
L-3, S6-2, S6-3, S6-4, and S7-2 remain open. Stage 5 remains **ACTIVE**.

## Slice 9a — Software completion and gate evidence (targeted repair)

**Starting Slice-9a main:** `2ca2c246f15ab9ee5be6e9b04fb38e102d361032`
**Branch:** `codex/stage5-software-completion`
**Draft PR:** [#6](https://github.com/sscheidl/TAUREON-Synth-Tool-V4/pull/6)
**Exact repair HEAD:** `cea1a42c7134b7c505e946692ac9358d86443e71`

Slice 9a is a software-only completion/review slice. It retains every earlier Stage-5 record above; this section adds no Stage-5 closure claim.

### Retained evidence required by §23

The existing 100,000-event Monitor measurement remains recorded above: 100,000 accepted events, zero drops, queue high-water 169, bounded 10,000 model rows, 886 GUI heartbeats, 2 ms maximum observed heartbeat delay, and 908 ms elapsed in that Debug test. It is retained prior Slice-5 evidence, not newly created Slice-9a evidence.

The existing Diagnostics bundle test remains recorded above: a non-empty transfer SysEx document contributes permitted version/counter metadata but no raw frame bytes, source name, Manager content, transfer log, or user-file payload to the exported bundle. This payload-exclusion proof is retained Slice-7 evidence.

The existing Devices & Profiles evidence remains recorded above: a stronger Summit fingerprint can override a distinct temporary manual selection; the discarded choice/evidence remains visible and a deliberate promotion creates the saved binding. This is retained Slice-6/FU-3 route/profile ambiguity evidence, not a route change or automatic send path.

### L-3 product resource policy

`SysExManager.hpp` defines fixed product safety limits of 256 MiB per raw SysEx document and 512 MiB aggregate loaded raw SysEx payload. They are not protocol-size claims and are not Settings-configurable. Aggregate accounting is the sum of workspace `raw_bytes`, not total process memory: parser structures, frame payload copies, and Qt model copies consume additional memory.

`add_file` now obtains `std::filesystem::file_size` before loading. An error while determining the size is rejected as `io_error`; an oversized **document file** is rejected before `load_syx_file`. The distinction is asserted by the unit test. `add_document` independently rejects an oversized **individual document** before ownership, and aggregate rejection leaves existing items unchanged. `remove_item` returns the consumed budget. Export/merge calculate their projected document size before any output or `.taureon.tmp` file is created. `MidiErrorCode::resource_limit_exceeded` remains appended at the end of the enum and identifies actual bytes, applicable limit, and subject.

The aggregate-budget test intentionally allocates roughly 512 MiB of real raw-byte storage in its process (three approximately 171 MiB documents; two retained while the third is offered). This is test-memory cost, not a claim that the 512 MiB raw-payload limit bounds total process memory.

### Large SysEx test observations

The Manager test imports and byte-exactly exports complete synthetic frames of 64 KiB, 600 KiB, 900 KiB, and 1 MiB + 257 bytes. At the observed peak after those four imports, alongside two 527-byte fixture documents, it asserts **6 workspace items, 6 frames, and 2,651,423 loaded raw bytes**. This is the measured Manager-workspace growth figure for the test, not a Monitor constant.

This retained Slice-9a statement is superseded by the Block-A evidence below. Offscreen tests prove bounded work/state transitions, not machine wall-clock responsiveness.

### Model/view scope stated honestly

This Slice-9a scope statement is superseded by the Block-A model/view evidence below. Earlier Slice-5–8 tests remain retained evidence for the other listed model, filtering, keyboard, and extended-selection behavior.

### Accepted targeted corrections

- S6-2: Manager-to-Transfer queued handoff now includes completion followed by Manager-panel destruction before delivery.
- S6-3: MainWindow clears the synchronous Transfer snapshot observer before sibling teardown.
- S6-4: target-local AUTOMOC is retained because only Qt targets require moc; core targets remain Qt-independent.
- S7-2: Diagnostics polling is visibility-coupled; its UI test shows the panel and observes a real refresh.
- S8-4: Librarian operation-availability fields are documented as reserved semantic-provider metadata; no unimplemented action is enabled.
- S8-5: malformed snapshot and selected-provider/no-bank messages name their actual cause.
- S8-6: same-provider refresh does not accumulate Librarian rows or selector entries.
- S8-7 correction: `4e40e25` changed the Access label; `0e154d5` added its test evidence.

### Final Slice-9a CI evidence

[Windows CI #166](https://github.com/sscheidl/TAUREON-Synth-Tool-V4/actions/runs/33362823357) ran on the prior documentation head `5d8f20279553cf6d9a90d25345b101af681953ba`: full Windows Debug configure/build passed and **24/24 CTest tests passed, 0 failed, 0 skipped**.

[Windows CI #176](https://github.com/sscheidl/TAUREON-Synth-Tool-V4/actions/runs/33367640795) ran on targeted-repair head `e42d6e6b68b38e1dd875f7b827891f525dbd04c1`: full Windows Debug configure/build passed and **24/24 CTest tests passed, 0 failed, 0 skipped**.

B-3 remains **OUTSTANDING**: actual `QApplication` WMS/WinMM apartment, close-active, and shutdown-lifetime evidence requires Slice 9b on the Product Owner's Windows hardware after 09.09.2026. The visual half of B-4 remains **OUTSTANDING**: 1920×1080 inspection at 125%, 150%, and 200% requires the same Slice 9b environment. The five hardware/loopback tests remain **NOT REGISTERED — NOT SKIPPED — NOT SIMULATED**. Stage 5 remains **ACTIVE**; Stage 6 has not begun.


## Block A — complete software finalization (PR #7)

**Starting main:** `6b6b4d9191e592826afa1bfa2ea7f0ad5703e562`  
**Branch:** `codex/stage5-software-finalization`  
**Draft PR:** [#7](https://github.com/sscheidl/TAUREON-Synth-Tool-V4/pull/7)

Block A closes only deterministic, cloud-verifiable software work. It neither changes the accepted Stage-2/3/4 contracts nor claims native WMS/WinMM, physical-device, timing, or visual Windows evidence.

### Block-A implementation and retained-evidence disposition

- **N-1 — error-code ordering:** established ordinal positions remain stable; the later L-3 `resource_limit_exceeded` code remains the newest enum value. `stage5_sysex_manager_unit` asserts the relevant values.
- **N-2 — transfer eligibility:** stored and presentation Manager items use one shared verified-complete/unaffected eligibility rule. `stage5_sysex_manager_unit` covers Manager transfer admission and rejection.
- **N-3 — tainted capture document:** the bounded `add_document` path admits an already captured document without mutable-workspace exposure; taint remains visible and rejects normal export, merge, and transfer. This is not a claim that file loading manufactures DataLoss.
- **N-6 / §15.5 software half:** `stage5_application_unit` uses a real `QTableView`/selection model across insertion, removal, and reset, and destroys the monitored model before a queued bridge drain is processed. The bridge closes presentation acceptance synchronously, drains pending presentation work, and cannot call the destroyed model. The test uses neither `QPointer`, sleep, nor timeout-based synchronization.
- **§15.1 fake close while in flight:** `stage5_application_unit` blocks the fake transport's first raw SysEx send, observes the running transfer state, closes the presentation acceptance gate, proves a later callback is rejected, requests cancellation, releases the gate, and lets `ConnectionWorker` join. It is fake/software evidence only, not Product-host WMS/WinMM evidence.
- **§15.3 large SysEx:** `stage5_sysex_manager_unit` covers 64 KiB, 600 KiB, 900 KiB, and 1 MiB + 257 B valid input with byte-exact output, plus malformed, incomplete, and tainted variants at every size. The malformed variant keeps one large malformed frame by placing its invalid status at the terminal byte; it therefore tests large malformed data rather than manufacturing thousands of unrelated out-of-frame fragments. Verified-complete export/merge remains rejected for all invalid variants and raw bytes are not repaired, normalized, or silently truncated.
- **§15.3 cancellation:** `stage5_sysex_transfer_session_unit` adds a Stage-5-specific in-flight cancellation of a >1 MiB first frame followed by a second frame. The fake send gate proves the transfer is running before cancellation; the terminal state is cancelled and only the accepted first message/bytes are counted.
- **S7-3:** Settings state exactly which preferences are persisted only and which settings take effect in this foundation. `stage5_settings_diagnostics_ui_unit` asserts the disclosure.
- **S7-4:** Diagnostics uses documented default export policy when the optional shared policy is absent. `stage5_settings_diagnostics_ui_unit` exercises that path.
- **L-3:** unchanged: 256 MiB maximum raw bytes per document and 512 MiB aggregate loaded raw bytes are product resource limits, not protocol limits.

### Gate and §23 traceability

| Brief | Requirement | Test / evidence | Status |
|---|---|---|---|
| Gate 1–3 | Qt production host; Qt boundary; exact RX/TX routes | retained `stage5_gui_smoke`, `stage5_application_unit`, `stage5_connection_worker_unit`, `stage5_connection_ui_unit` | Block A fulfilled |
| Gate 4 / §15.2 | bounded Monitor and 100,000-event evidence | retained `stage5_monitor_stress`; measured record above | Block A fulfilled |
| Gate 5–6 / §15.3 | raw SysEx integrity; non-destructive Manager | retained `stage5_sysex_transfer_session_unit`, `stage5_sysex_manager_unit`; Block-A large invalid/cancellation evidence above | Block A fulfilled |
| Gate 7–8 / §16 | profile evidence, override, explicit binding | retained `stage5_profile_followup_unit`, `stage5_profile_ui_unit` | Block A fulfilled |
| Gate 9 | bounded Librarian without semantic overclaim | retained `stage5_librarian_unit` | Block A fulfilled |
| Gate 10–11 | payload-free diagnostics; versioned settings | retained `stage5_settings_diagnostics_unit`, `stage5_settings_diagnostics_ui_unit` | Block A fulfilled |
| §15.1 | close while activity is proven in flight | `stage5_application_unit` fake-transport gate/cancellation evidence | Block A fulfilled; not native evidence |
| §15.5 software half | selection and queued Model destruction | `stage5_application_unit` real Qt model/event paths | Block A fulfilled |
| §15.4 / Gate 12 | actual QApplication WMS/WinMM lifecycle and active close | Product Owner Windows runtime after 09.09.2026 | Block B outstanding |
| §15.5 visual half / Gate 13 | 1920×1080 visual inspection at 125%, 150%, 200% | Product Owner Windows inspection after 09.09.2026 | Block B outstanding |
| §15.6 / Gate 14 | cloud software build/regression | [Windows CI #215](https://github.com/sscheidl/TAUREON-Synth-Tool-V4/actions/runs/33472607926) at `5a5e495bca30babb00b1a62119b8f247878d33b8`: whitespace PASS, Debug build PASS, 24/24 CTest PASS, 0 failed, 0 skipped | Code evidence fulfilled; final documentation revision must pass its own CI |
| Gate 15 / §23 | honest limits and unresolved work | this report and `PROJECT_STATE.md` | Block A fulfilled |

The five hardware/loopback tests remain **NOT REGISTERED — NOT SKIPPED — NOT SIMULATED**. They are not cloud failures and are not represented as skipped coverage.

### Block B — explicitly remaining

- actual `QApplication` WMS/WinMM apartment, receive/send-active close, callback, queue, join, and resource-trend evidence;
- actual WMS/WinMM runtime, port, device, loopback, and timing evidence;
- physical MIDI/SysEx hardware evidence;
- Windows 1920×1080 visual inspection at 125%, 150%, and 200% scaling.

**Stage 5 remains HOLD/ACTIVE.** Block A is a review candidate only after the CI run attached to this documentation revision is green. No Stage 6 work has begun.
