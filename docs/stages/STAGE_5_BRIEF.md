# Stage 5 Brief – Qt 6 Product GUI

**Stage:** 5
**Status:** ACTIVE – User-authorized 2026-08-24; Stage-4 closure revision `986115d` is pushed
**Implementation lead:** Codex
**Coordination:** ChatGPT Classic / Project Manager
**Review:** Project Manager gate review; targeted Claude Code review only when the triggers in section 16 apply
**Decision authority:** User / Product Owner where required

## 1. Goal

Build the first real TAUREON V4 desktop application in **C++20 with Qt 6 Widgets** on top of the proven Stage 2–4 core.

Stage 5 must turn the existing transport, generic MIDI/SysEx/transfer, and profile capabilities into one coherent product without moving business logic into widgets or weakening any earlier safety invariant.

The stage establishes:

- the production `QApplication` host and application composition root;
- a persistent application shell and connection bar;
- explicit backend, RX-route, TX-route, group, and profile selection UX;
- functional MIDI Monitor, SysEx Transfer, SysEx Manager, Devices & Profiles, Diagnostics, and Settings workspaces;
- a deliberately bounded Library/Librarian foundation;
- application services/controllers and Qt model/view adapters above the existing core;
- responsive progress, cancellation, error, ambiguity, and degraded-state presentation;
- measured Qt/WMS/WinMM lifetime evidence in the actual product host.

This is a product-integration stage, not an excuse to redesign the proven core or to implement a universal synthesizer librarian.

## 2. Start gate and required reading

Do not begin implementation until all of the following are true and recorded:

1. Stage 4 is closed as **PASS** or **PASS WITH NON-BLOCKING FOLLOW-UPS**.
2. The Stage 4 closure revision is on `origin/main`.
3. `HEAD == origin/main` and the worktree is clean.
4. The final `STAGE_4_REPORT.md`, Claude review, review disposition, and exact carried-forward follow-ups have been read.
5. No unresolved Stage 4 P0/P1 or architecture deviation remains.
6. The User has authorized Stage 5. Approval of this brief also confirms the design freeze in section 4 unless the User records a change.

Read in this order and keep the session narrow:

1. `docs/status/PROJECT_STATE.md`
2. `docs/process/AI_COLLABORATION.md`
3. this brief
4. final Stage 4 report/review and their follow-up disposition
5. relevant accepted ADRs
6. `docs/design/GUI_DESIGN_SPEC.md`
7. `docs/design/TAUREON_V4_GUI_MOCKUP.html`
8. the current application/core interfaces and relevant tests
9. only the necessary sections of `ARCHITECTURE.md`, `PRODUCT_VISION.md`, `QUALITY_POLICY.md`, and `AI_MODEL_POLICY.md`

Do not reconstruct Stages 0–4, scan the entire legacy repository, or mechanically port the HTML mockup. The mockup defines workflow, hierarchy, wording, and density; it is not source code or a pixel-exact implementation specification.

Before changing code, report:

- the verified Stage 4 closure revision and gate result;
- the exact Stage 4 follow-ups entering Stage 5;
- the existing Qt targets/harnesses and whether they are production or spike-only;
- the proposed production target, composition root, controller/service boundaries, and Qt model boundaries;
- any conflict between this brief, an accepted ADR, and the actual repository.

If a conflict would change architecture or product scope, Stop/Ask before implementation.

### Recorded start-gate intake (2026-08-24)

- Stage 4 is **PASS / CLOSED** at implementation revision `986115d`; the closure state and its documentation
  handoff are on `origin/main`.
- `HEAD` matched `origin/main` and the worktree was clean before Stage-5 activation.
- No Stage-4 P0/P1 or architecture deviation is open.
- FU-1 and FU-2 were implemented during Stage 4. FU-3 (P3) is the only carried-forward item: preserve matching
  precedence; retain a manual selection overridden by stronger fingerprint evidence in domain evidence; show it in
  the GUI; and offer an explicit promotion to a saved binding with tests.
- The previous `taureon_wms_qt_harness` is a Stage-1 `QCoreApplication` spike, not a production GUI target.

## 3. Architecture boundary

```text
Qt Widgets / Views
        |
        v
Qt Models + Presentation Controllers
        |
        v
Application Services / Use Cases
        |
        +-------------------------------+
        |                               |
        v                               v
Device/Profile Services       Generic File/Transfer Workflows
        |                               |
        +---------------+---------------+
                        v
        Stage 3 MIDI / SysEx / Transfer Core
                        |
                        v
                 IMidiTransport
                  /          \
                 v            v
          WMS Direct      WinMM Native
```

Non-negotiable rules:

- native MIDI callbacks never access `QObject`, Qt models, or widgets;
- widgets do not own transport, parser, transfer, profile, file, or persistence logic;
- business logic is testable without rendering a GUI;
- high-volume data reaches Qt models through an explicit queued/batched boundary;
- no detached worker threads and no `QPointer` used as cross-thread synchronization;
- exactly one selected backend owns a logical connection;
- RX and TX routes remain independently selectable;
- WMS and WinMM route identities remain backend-specific and are never silently translated;
- profile/device identity remains separate from backend/route identity;
- changing a profile never changes the active transport route;
- profile interpretation may add labels, warnings, or constraints but never mutate raw MIDI/SysEx bytes or clear malformed/tainted state;
- the existing generic parser, serializer, capture, and transfer engine are reused; no GUI-local duplicate is permitted;
- unknown events/devices remain unknown and Generic fallback remains a supported state;
- capability metadata controls availability and wording; the GUI never advertises unsupported Modify, Serialize, or Validated Restore behavior.

Any necessary change to these rules requires an ADR and architecture review before implementation continues.

## 4. Stage-5 design freeze

Approval of this brief freezes the following Stage 5 baseline.

### 4.1 Workspaces and terminology

Use these user-facing workspaces:

1. **MIDI Monitor**
2. **SysEx Transfer**
3. **SysEx Manager**
4. **Library / Librarian**
5. **Devices & Profiles**
6. **Diagnostics**
7. **Settings**

Keep **SysEx Transfer**, **SysEx Manager**, and **Library / Librarian** separate:

- Transfer is one active receive/send session.
- Manager works with raw SysEx files, frames, and collections.
- Librarian works only with semantic device objects supplied by a capable profile/protocol.

### 4.2 Visual baseline

- professional desktop utility, not a decorative synth skin;
- Qt/Windows-like controls and restrained τAUREON gold/amber accent;
- system-compatible dark and light presentation;
- large primary workspaces rather than many tiny panes;
- readable at 1920x1080 and 125%, 150%, and 200% scaling;
- no tiny fixed fonts or fixed pixel layouts that break under scaling/localization;
- status is never communicated by color alone;
- raw MIDI/SysEx remains reachable from interpreted views;
- normal keyboard navigation, visible focus, sensible tab order, and accessible names are required.

Use the HTML mockup as a workflow reference. Do not embed a browser, ship HTML/CSS as the UI, or translate its DOM mechanically into widgets.

### 4.3 Minimum Librarian scope for V4.0

The Stage 5 Librarian is a foundation, not a fake universal editor.

It must provide:

- domain-neutral collection/bank/slot interfaces separated from Qt presentation;
- a reusable model/view matrix or table capable of variable bank/slot sizes;
- keyboard and multi-selection behavior suitable for later Copy/Cut/Paste, rename, delete, drag/drop, and undo/redo;
- capability-aware empty/unavailable states;
- fake/in-memory provider coverage proving the GUI contract.

It must not:

- treat raw SysEx frames as presets merely to populate the view;
- claim Novation Summit preset/bank parsing or semantic editing support beyond the capabilities actually proved in Stage 4;
- enable semantic import/edit/export when no profile/protocol codec exists;
- invent bank capacity, object type, compatibility, or write safety.

For the Generic and bounded Stage 4 **Novation Summit** profiles, show the real declared capability state. Unsupported semantic actions remain disabled with a concise explanation.

### 4.4 Profile selection and binding behavior

- a manual profile choice is session-scoped unless the User explicitly chooses **Remember binding**;
- a saved explicit binding and a temporary manual choice are visually distinct;
- match confidence and evidence are inspectable;
- an ambiguous or invalid result is visible and never auto-resolved by list order;
- a generic DIN-interface name is never used as proof of the attached synth;
- a profile suggestion does not silently become a saved binding;
- a profile disappearance/invalidation does not redirect output;
- any manual selection overridden by stronger evidence is shown explicitly, including the selected profile, discarded manual choice, winning evidence, and confidence.

Do not change the Stage 4 selection priority merely to simplify the UI.

If the final Stage 4 result model does not expose enough information to render an overridden manual choice, verify the final Stage 4 follow-up disposition. Do not reconstruct the decision in widgets. Escalate the missing domain evidence as a bounded prerequisite correction.

## 5. Production application shell

Create the real Qt 6 Widgets application target and composition root.

The shell must provide:

- one main window;
- persistent connection bar;
- workspace navigation;
- central status/error presentation;
- non-modal progress where the user may continue safely;
- explicit modal confirmation only for genuinely destructive or safety-critical actions;
- deterministic startup and shutdown;
- version/build identity suitable for Diagnostics.

The Stage 1 minimal Qt harness is spike evidence, not the product shell. Supersede it without linking spike-only architecture into production. Do not delete user-owned or historical evidence merely to tidy the tree.

Every visible Stage 5 action must be functional, intentionally disabled with an explanation, or clearly marked as unavailable because the current capability is absent. Do not ship inert buttons or simulated success.

## 6. Persistent connection bar

The connection bar remains visible across all workspaces and contains:

- Backend: **Auto / Windows MIDI Services / WinMM**;
- connection state;
- MIDI Input;
- MIDI Output;
- WMS group/function-block selection only where technically required;
- **Connect / Disconnect**;
- **Panic**;
- concise backend/API/runtime state where useful.

Behavioral requirements:

- **Auto** is a pre-connection selection policy, never a hidden parallel backend;
- Auto may restore only an exactly resolvable saved backend/route choice;
- missing, ambiguous, stale, or cross-backend route identity requires visible user selection;
- runtime indices and display labels alone are not persisted identities;
- input-only and output-only connections remain possible where the transport contract permits;
- selectors show enough identity to distinguish similarly named routes;
- route disappearance produces an explicit degraded/disconnected state and never fuzzy rebinding;
- reconnect behavior follows explicit settings and exact identity only;
- Panic sends only after a direct user action, only to the selected TX route, and is logged visibly;
- changing backend while active requires an orderly disconnect before a new backend is opened.

## 7. MIDI Monitor

Implement one large Qt model/view table with at least:

- Time;
- Direction;
- Route;
- Group where applicable;
- Channel;
- Type;
- Parameter / Event;
- Value;
- Raw.

Provide:

- active MIDI Profile and profile status;
- access to profile details/editing;
- channel, direction, route, event-type, SysEx, Clock, and Active Sensing filters;
- Pause/Resume;
- Clear;
- Copy and Export;
- bounded history with a user-visible/configurable limit;
- observable dropped/overflow accounting.

Rules:

- raw identity is always available even when a profile supplies a semantic label;
- unknown controller/parameter values remain unknown;
- filtering and pausing do not silently change transport ownership or transfer behavior;
- Pause stops presentation growth; its exact capture/accounting behavior must be explicit in the UI;
- updates are queued, batched, and throttled;
- insertion/removal does not cause unbounded relayout or per-event widget allocation;
- any loss between transport and model is bounded, counted, and visible.

## 8. SysEx Transfer

Implement the active receive/send workspace using the existing Stage 3 engine.

Show:

- loaded/received filename;
- manufacturer/device only when supported by real evidence;
- active profile and match status;
- frame count and byte count;
- framing, completeness, malformed, and tainted state;
- frame inspector and raw bytes;
- selected TX route;
- pacing source/mode;
- Receive, Send, Cancel, Save received data, and Clear;
- progress and transfer log/status.

Safety requirements:

- no automatic send on startup, file open, profile change, or connection;
- raw send is labelled **Raw Send** and is not presented as Validated Restore;
- Validated Restore is absent/disabled unless an actually validated protocol capability exists;
- destructive classification/warnings come from application/profile/protocol policy above transport;
- malformed, incomplete, or tainted data cannot be saved/encoded as verified complete;
- profile recognition cannot clear or hide taint;
- no automatic payload repair, checksum invention, normalization, or retry of a failed destructive operation;
- Send always displays the actual output route and requires explicit user initiation;
- cancellation reaches a stable observable terminal state without leaving a second active transfer;
- original imported files are not modified in place by default; use Save As/atomic write where applicable.

Use fake transport for automated send/receive tests. Local product-host lifecycle tests may use only previously approved diagnostic loopback paths; never select or send to an arbitrary physical output.

## 9. SysEx Manager

Implement a generic file/frame workspace independent of a live connection.

Minimum functional scope:

- add/open one or more `.syx` files;
- display File, recognized Device/Manufacturer where evidenced, Frames, Size, and Status;
- inspect individual frames and raw bytes;
- validate framing and display incomplete/malformed/tainted state;
- calculate deterministic file/frame hashes;
- detect exact file duplicates and exact frame/payload duplicates;
- split/export selected frames through explicit destination selection;
- merge selected complete frames and save to a new file;
- open a selected valid item in SysEx Transfer;
- remove an item from the current workspace without deleting the source file.

Requirements:

- operations reuse the Stage 3 parser/serializer and preserve byte order exactly;
- source files are not overwritten by default;
- profile recognition annotates but does not mutate content;
- “possible duplicate” semantic guesses are not implemented or auto-deleted;
- malformed/incomplete/tainted content cannot be relabelled complete through split/merge;
- no device-specific Novation Summit dump parser or codec is added here.

Other mockup actions may remain unavailable if they are not required for this minimum and their absence is presented honestly.

## 10. Library / Librarian foundation

Implement the frozen minimum from section 4.3.

Required evidence:

- pure domain/application tests for variable bank/slot capacity and selection-safe operations;
- Qt model tests for indexing, reset/insert/remove notification correctness, multi-selection, and bounded updates;
- fake provider populates a representative matrix without device-specific branches in the view/controller;
- Generic/Novation Summit production state shows only capabilities actually declared by the active profile/protocol;
- unavailable semantic operations are disabled and explained;
- no raw SysEx frame is silently promoted to Preset/Program/Patch.

Do not spend Stage 5 on a persistent database, tag/rating system, Novation Summit preset/bank editor, universal drag/drop codec, or complete undo stack.

## 11. Devices & Profiles

Implement:

- deterministic enumeration of Generic, built-in, generated/reference, and user-owned profiles;
- profile details: ID, manufacturer, model/variant, version, source/provenance, firmware scope, capability/support ladder, warnings, and recognition evidence;
- active match result, confidence, ambiguity, invalid state, and Generic fallback;
- temporary **Use profile** action;
- separate explicit **Remember binding** and **Forget binding** actions;
- profile validation with visible field/path diagnostics;
- duplicate-to-user-profile before editing protected/built-in data;
- JSON/data editing for user-owned profiles with validation before atomic save;
- safe reload that cannot displace Generic fallback with an invalid profile;
- open the user-profile folder where supported.

Requirements:

- built-in/generated/reference profiles are never silently overwritten;
- invalid data cannot be saved as active;
- editing or binding a profile never opens, closes, or changes a MIDI route;
- profile load/reload errors appear in Diagnostics and in the relevant profile UI;
- evidence for a stronger match and an overridden manual choice remains inspectable;
- no uncontrolled plugin loading, remote profile download, executable script, or arbitrary shared-library discovery.

## 12. Diagnostics and logging

Provide a dedicated Diagnostics workspace showing/exporting at least:

- application version/build/revision;
- OS and process architecture;
- selected/active backend;
- current Windows MIDI API mode when supported by existing safe observation APIs;
- detectable WMS runtime/SDK information;
- endpoint IDs/names/groups and selected RX/TX identity;
- connection and transfer state;
- RX/TX counters;
- dropped/overflow/late-callback counters;
- queue current/high-water state where available;
- SysEx frame/byte counts;
- last transport/application error;
- disconnect/reconnect transitions;
- active profile/match outcome and validation errors.

Provide **Export Diagnostic Bundle**.

The default bundle must not contain user SysEx payloads, preset dumps, or arbitrary source files. If a future explicit opt-in payload export is considered, it is outside this stage unless separately approved.

Logging must be thread-safe, bounded/rotated according to settings, and must not become the synchronization mechanism for correctness.

## 13. Settings

Implement a versioned settings schema and UI for the minimum active fields:

### General

- theme;
- UI density/font scaling where appropriate;
- standard paths;
- safe session behavior.

### MIDI

- default backend preference;
- exact preferred RX/TX routes using the accepted Stage 2 identity schema;
- explicit reconnect policy;
- monitor defaults/history bound.

### SysEx

- global generic pacing defaults;
- confirmation policy;
- capture/transfer safety defaults.

### Logging/Diagnostics

- log level;
- log destination/rotation;
- diagnostic export preferences.

Rules:

- do not duplicate or weaken the accepted route-persistence/resolution contract;
- corrupt, unsupported, or partial settings fall back safely and visibly;
- unknown fields may be preserved only if the chosen schema explicitly supports forward compatibility;
- device-specific timing/behavior belongs to a profile/device override, not silently to global settings;
- settings writes are atomic where practical;
- no setting changes drivers, services, registry, or Windows MIDI API mode.

Deep corrupted-settings/failure-injection coverage remains Stage 6, but Stage 5 must prove basic version rejection/migration and safe fallback.

## 14. Threading, lifetime, and shutdown

Repeat the relevant WMS/WinMM coexistence evidence in the actual `QApplication` product host.

Required properties:

- explicitly document the observed GUI-main-thread apartment model; do not assume it from the spike;
- WMS objects remain owned by their established MTA worker boundary;
- Qt receives application events only through a safe queued/batched bridge;
- native callbacks never invoke widgets/models, direct Qt event delivery, modal UI, or file I/O;
- close disables GUI acceptance of new transport events before dependent presentation objects are destroyed;
- active transfer/capture is cancelled or closed through the established engine contract;
- transport callbacks/events are revoked, resources are closed, workers finish, and owned threads are joined before dependency destruction;
- stale queued GUI events are rejected by explicit generation/lifetime state rather than pointer luck;
- no detached thread and no sleep/timeout-only race fix;
- repeated open/close and app construction/shutdown do not show unexplained resource growth.

Close-while-active tests must prove that the worker/callback/transfer was measurably active. A requested scenario flag is not sufficient evidence.

If Qt/WMS apartment, shutdown order, or callback ownership requires changing an accepted lower-layer contract, Stop/Ask and prepare an ADR before proceeding.

## 15. Performance and test evidence

### 15.1 Fake-transport GUI integration

The fake transport must drive the production controllers/models for deterministic tests of:

- endpoint enumeration and independent RX/TX selection;
- exact reconnect and route disappearance;
- connection state/error transitions;
- channel/realtime/SysEx monitor events;
- profile match, ambiguity, invalid, Generic fallback, and overridden-manual evidence;
- capture, raw send, pacing progress, cancellation, and failure;
- diagnostics/counter updates;
- application close while activity is proven in flight.

Do not create a separate fake-only GUI architecture.

### 15.2 Monitor stress

Inject at least **100,000 synthetic MIDI events** through the production application boundary.

Prove and report:

- the configured history bound is never exceeded;
- queue/model high-water marks remain bounded;
- displayed, filtered, paused, and dropped counts reconcile under the documented policy;
- no per-event widget allocation or unbounded row/layout work occurs;
- the GUI event loop continues servicing an independent heartbeat during ingestion;
- maximum observed heartbeat delay, elapsed time, peak model rows, and drop counters;
- no crash, deadlock, use-after-free, or unexplained loss.

Use a documented local responsiveness target rather than weakening the test after seeing the result. Any hosted-CI timing threshold must tolerate runner variance while the strict local measurement remains recorded.

### 15.3 Large SysEx workflow

Exercise synthetic valid and malformed/tainted inputs at the sizes required by `QUALITY_POLICY.md`, including at least approximately 64 KiB, 600 KiB, 900 KiB, and greater than 1 MiB.

Prove:

- open/inspect/transfer-progress/cancel UI remains responsive;
- byte/frame counts are exact;
- valid save/merge output is byte-exact;
- malformed/incomplete/tainted state remains visible and rejected by verified-complete save/encode paths;
- cancellation reaches a stable state;
- memory/model growth is bounded and reported.

### 15.4 Product-host native lifecycle

Using only approved local diagnostic endpoints/loopbacks:

- repeat applicable WMS and WinMM open/close lifecycle tests in the actual `QApplication` host;
- test close while receive/send/worker activity is measurably active;
- report apartment initialization, callback counts, callbacks after acceptance closed, queue/drop counters, final state, and resource-growth trend;
- retain the approved Stage 3 WMS trend-based resource criterion; do not restore obsolete process-handle scatter assumptions or raise limits after a failure;
- do not claim hosted CI or physical-hardware coverage where none exists.

### 15.5 Model/view and visual verification

- test model indices, roles, row insertion/removal/reset, filtering, sorting, and selection preservation;
- test shutdown with queued model batches;
- capture/inspect the main workspaces at 1920x1080 and 125%, 150%, and 200% scaling where the environment permits;
- verify no clipped critical controls, unreadable fonts, color-only state, broken focus order, or unusable high-density table;
- record environment limitations instead of fabricating a pass.

### 15.6 Build and regression

- clean configure/build/test from a separate build directory;
- all ordinary CI tests pass;
- all applicable Stage 2–4 tests pass unchanged;
- relevant opt-in local WMS/WinMM suites pass under their approved contracts;
- GUI tests run headless/offscreen where valid and native-host tests remain honestly local-only;
- `git diff --check` passes;
- final revision, push state, and worktree state are reported accurately.

## 16. Stage 4 carry-forward handling

Copy every final Stage 4 non-blocking follow-up into Stage 5 tracking with its exact identifier and disposition. Do not infer missing identifiers from an earlier draft.

At minimum preserve the reviewed profile-selection UX requirement:

- the match priority remains unchanged;
- when stronger fingerprint/identity evidence overrides a temporary manual profile choice, the discarded choice is retained in domain evidence and shown by the Stage 5 UI;
- the user can understand why it was overridden and can deliberately create an explicit saved binding through a separate action;
- the GUI does not silently reinterpret the discarded choice as an explicit binding.

If Stage 4 closes this item in the domain layer, Stage 5 consumes and tests it. If it is explicitly carried forward, implement only the approved bounded completion and keep it outside widgets. If the final review materially changes this requirement, the final Stage 4 disposition wins and this brief must be updated before Stage 5 starts.

Preserve all earlier accepted integrity/lifetime contracts, including:

- ordered data loss taints the affected capture;
- profile recognition never upgrades tainted/malformed data;
- R-003 wording follows the final Stage 4 review disposition;
- WMS resource evidence measures growth/trend, not incidental handle scatter;
- backend-specific route identity and deliberate reselection UX remain intact.

## 17. Out of scope

- QML, Electron, browser-embedded UI, or mechanically ported HTML/CSS;
- full universal librarian/database implementation;
- Novation Summit dump parsing, preset/bank editing, device-specific conversion, checksum reverse engineering, or validated restore;
- new device-specific protocol state machine, codec, checksum, handshake, or restore flow;
- multiple new real device profiles or bulk legacy-profile migration;
- automatic background identity requests;
- remote profile download/update service;
- uncontrolled plugin/shared-library/script loading;
- third MIDI backend or cross-backend route correlation;
- MIDI-CI, Property Exchange, SysEx8 semantic editing, firmware flashing, or synth editor panels;
- exhaustive failure injection and adversarial corrupted-state campaign reserved for Stage 6;
- release packaging/installer/portable ZIP reserved for Stage 7;
- physical-synth validation or automated real-hardware writes;
- driver, registry, service, `MidiSrv`, API-mode, or global system changes;
- cloud accounts, telemetry, or upload of user MIDI/SysEx data.

## 18. Deliverables

- production Qt 6 Widgets application target and composition root;
- application services/controllers and explicit Qt event/model bridge;
- shell, connection bar, and seven frozen workspaces;
- functional minimum MIDI Monitor, SysEx Transfer, and SysEx Manager;
- bounded capability-aware Library/Librarian foundation;
- Devices & Profiles selection/binding/editing UX;
- Diagnostics and versioned Settings UI/services;
- fake-transport GUI integration tests;
- monitor, large-SysEx, model/view, high-DPI, and product-host lifecycle evidence;
- any evidence-required ADR;
- updated architecture, GUI design, settings, risk, provenance, and decision documentation where changed;
- `docs/stages/STAGE_5_REPORT.md` using the stage-report template;
- updated `docs/status/PROJECT_STATE.md` with exact gate/readiness state.

Do not create empty directories, placeholder subsystems, or documentation-only claims to satisfy this list.

## 19. Gate criteria

### PASS requires

1. The production Qt application is built on the proven core through explicit application/controller/model boundaries.
2. No native callback touches Qt and no unresolved cross-thread ownership/lifetime ambiguity remains.
3. The connection bar preserves exact backend-specific route identity, independent RX/TX selection, and deliberate reselection.
4. Monitor history/queues are bounded and the 100,000-event evidence is reproducible.
5. Generic SysEx receive/load/inspect/raw-send/progress/cancel/save workflows operate through the shared Stage 3 engine and preserve byte/taint integrity.
6. SysEx Manager minimum file/frame operations are functional and non-destructive by default.
7. Profile match confidence/evidence, ambiguity, Generic fallback, temporary manual choice, and explicit binding are distinct and visible.
8. An overridden manual profile choice is visible without changing the approved priority order.
9. Librarian UI does not overclaim semantic/device capabilities.
10. Diagnostics expose the required state/counters and the default bundle excludes user payloads.
11. Settings are versioned and preserve the accepted route identity contract.
12. Actual `QApplication` WMS/WinMM apartment/lifetime/close-active/shutdown evidence passes using approved diagnostic paths.
13. Large-SysEx responsiveness and high-DPI/readability evidence pass or any environmental limitation is explicitly reported without replacing a required software test.
14. All applicable builds/tests/regressions pass, `git diff --check` passes, and no unresolved P0/P1 remains.
15. No real hardware validation, validated-restore support, or unsupported capability is claimed.

### HOLD if

- Stage 4 is not closed or its final follow-ups are unresolved/unknown;
- a widget/controller duplicates or bypasses the generic MIDI/SysEx/transfer/profile engine;
- a native callback accesses Qt or lifetime depends on timing luck, sleeps, `QPointer`, or an unjoined thread;
- the GUI can silently rebind a missing/ambiguous route or translate identity across backends;
- a profile selection silently changes the output route;
- profile interpretation can hide or clear malformed/tainted state;
- a manual profile choice is silently discarded without evidence/presentation;
- an unbounded monitor/model/queue or reproducible GUI freeze remains;
- the app labels raw transfer as validated restore;
- the Librarian presents unsupported raw data as semantic presets/banks;
- an unresolved P0/P1, architecture deviation, byte-integrity uncertainty, or unsafe file/write path remains.

## 20. Stop / Ask

Apply `QUALITY_POLICY.md`. Stop and escalate especially when:

- the final Stage 4 report/review contradicts this draft;
- a Stage 4 follow-up lacks a final disposition needed by the GUI;
- Qt integration appears to require changing `IMidiTransport`, parser, transfer, profile, or route-identity contracts;
- GUI-main-thread/WMS apartment behavior is uncertain and cannot be established from code plus deterministic evidence;
- endpoint ambiguity could target the wrong physical device;
- a physical output would have to be used for an unattended test;
- byte corruption or taint loss has no identified root cause;
- two materially different fixes fail for the same blocker;
- a race/deadlock is being treated only with sleeps/timeouts;
- implementation would overwrite user files, protected profiles, or existing work;
- a driver, registry, service, API-mode, dependency installation, or global system change is required;
- a device-specific format/protocol capability is required to make a GUI feature honest.

Use the required escalation format:

```text
Blocker:
Expected:
Actual:
Evidence:
Attempts:
Likely causes:
Options:
Recommendation:
Decision required from User:
```

## 21. Model and effort

Default implementation:

```text
Codex GPT-5.6 Terra – high
```

Terra is appropriate for the routine Qt/CMake/controller/model work. Use medium only for a separately bounded cosmetic or mechanical task with strong tests.

Escalate only the affected bounded task to:

```text
Codex GPT-5.6 Sol – high
```

when a real Qt/WMS lifetime, shutdown race, model/view concurrency, byte-integrity, P0/P1, or architecture-sensitive blocker appears. Do not move the whole stage to Sol Ultra/max.

Claude Code review is not automatically required for ordinary Stage 5 GUI work. Request a targeted review when:

- an accepted ADR or lower-layer contract must change;
- Qt/WMS apartment or shutdown ownership remains questionable;
- a verified P0/P1 remains;
- implementer and Project Manager disagree on material evidence.

Use Sonnet medium/high for a bounded ordinary review and Opus high for lifetime/architecture/P0/P1 review.

## 22. Implementation/reporting discipline

- Work in coherent vertical slices that remain buildable/testable.
- Preserve unrelated user changes in a dirty worktree; do not use destructive Git cleanup.
- Verify every review finding against actual code before fixing it.
- Prefer deterministic tests and measured evidence over screenshots or prose claims.
- Do not weaken a threshold, history bound, safety state, or test after observing a failure without evidence and approval where required.
- Do not install dependencies or alter the machine merely to make a test pass.
- Keep hardware-dependent behavior explicitly unvalidated.
- Update the Stage 5 report throughout implementation rather than reconstructing evidence at the end.

Recommended slice order:

1. production host/composition root and fake-driven shell;
2. application event bridge plus connection bar and lifecycle;
3. bounded MIDI Monitor;
4. SysEx Transfer;
5. SysEx Manager;
6. Devices & Profiles including match/binding evidence;
7. Diagnostics and Settings;
8. bounded Librarian foundation;
9. stress, native product-host lifecycle, visual/high-DPI verification, documentation, and gate evidence.

Do not start Stage 6 work simply because its failure scenarios are nearby. Record Stage 6 candidates separately.

## 23. Review handoff and final report

The Stage 5 report must include:

- closure revision and exact Stage 4 follow-ups consumed/carried;
- production target and architecture boundary summary;
- files/modules changed;
- commands and build configurations;
- automated test counts with failures/skips and reasons;
- 100,000-event monitor measurements;
- large-SysEx responsiveness/integrity measurements;
- model/view and high-DPI/readability evidence;
- actual `QApplication` WMS/WinMM apartment/lifetime/close-active/shutdown evidence;
- route/profile ambiguity and overridden-manual-choice evidence;
- diagnostics-bundle payload-exclusion evidence;
- architecture/ADR impact;
- known issues and Stage 6 candidates;
- hardware-dependent items not tested;
- exact commit, push, and worktree state;
- PASS/HOLD recommendation and Stage 6 readiness.

If targeted Claude review is triggered, provide only:

- this brief and current Stage 5 report;
- relevant accepted ADRs;
- the bounded source/tests/diff for the issue;
- exact logs and deterministic reproduction evidence;
- the specific decision question.

Codex verifies every finding before changing code. No Stage 6 work begins until the Stage 5 gate is explicitly closed.
