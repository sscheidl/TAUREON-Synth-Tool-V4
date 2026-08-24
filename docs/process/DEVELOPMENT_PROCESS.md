# TAUREON V4 – Development Process & Release Gates

**Status:** Active process  
**Process owner:** ChatGPT Classic / Project Manager  
**Final decision authority:** User / Product Owner

This document defines the stage sequence. Role details are in `AI_COLLABORATION.md`; cross-cutting test/safety rules are in `QUALITY_POLICY.md`.

## 1. Stage lifecycle

Every stage follows:

```text
PLANNED
  ↓
BRIEF READY
  ↓
IN PROGRESS
  ↓
LOCAL VALIDATION
  ↓
STAGE REPORT
  ↓
REVIEW (when required)
  ↓
GATE: PASS or HOLD
```

A stage on HOLD remains the current stage.

Each stage produces:

- code/config/docs as applicable;
- required tests/evidence;
- `STAGE_N_REPORT.md`;
- known issues;
- gate recommendation;
- updated `PROJECT_STATE.md`.

The Project Manager checks completeness and prepares the gate decision. The User remains the final authority wherever product scope, risk acceptance, hardware, destructive/system changes, or final release are involved.

## 2. Work tracking and CI

Use lightweight professional controls appropriate for a one-developer project.

### Work tracking

- GitHub Issues (or equivalent) may track stages, bugs, and material feature work.
- Do not create an issue for every trivial edit.
- A stage should have one clear tracking item/milestone when useful.
- P0/P1 defects receive explicit tracked items until closed.

### Continuous integration

Establish Windows CI as early as practical.

Stage 0 target:
- configure/build a trivial/native project on a Windows runner if the toolchain/dependencies are supportable there.

From Stage 2 onward, CI should run software-only tests that do not require real MIDI hardware.

CI must never:
- install/change physical MIDI drivers;
- write to real synth hardware;
- claim WMS integration coverage when the hosted runner lacks the required runtime/environment.

Platform-specific tests that cannot run in hosted CI remain local evidence and are recorded in stage reports.

## 3. Scope discipline

Before a stage starts, `STAGE_N_BRIEF.md` defines:

- goal;
- in scope;
- out of scope;
- acceptance tests;
- deliverables;
- review requirement;
- Stop/Ask conditions.

Codex does not start Stage N+1 automatically.

Architecture changes require an ADR and architecture review before implementation continues.

## 4. Stage 0 – Bootstrap, inventory, provenance, architecture verification

### Goal

Create the clean V4 repository and verify the real toolchain/API environment before production implementation.

### Main work

- create new V4 repo/project;
- preserve old TAUREON unchanged;
- establish documentation/control files;
- inventory relevant legacy code, fixtures, captures, hardware evidence;
- record provenance/licensing;
- verify Windows/MSVC/CMake/SDK/Qt;
- verify current official Windows MIDI Services package/API/runtime requirements;
- record current MIDI environment without changing it;
- identify architecture questions requiring Stage 1 spikes.

### Deliverables

- project skeleton;
- environment report;
- legacy inventory;
- provenance record;
- initial architecture baseline/ADRs;
- Stage 0 report.

### Gate

HOLD if:

- current WMS API cannot be identified confidently;
- trivial C++20 toolchain build fails;
- essential reuse has unclear licensing;
- new repo risks overwriting existing work;
- a blocking architecture assumption remains unresolved.

### Review

Mandatory Claude Code architecture review.

## 5. Stage 1 – Native transport spikes

### Goal

Prove both transport APIs independently before building abstractions or GUI.

### WMS spike

Must demonstrate:

- SDK/runtime initialization;
- endpoint enumeration;
- supported API-mode visibility where available;
- receive callback;
- safe loopback/test send if available;
- deterministic close/shutdown.

No `midi.exe`.

### WinMM spike

Must demonstrate:

- enumeration;
- short-message path;
- long-message/SysEx buffer lifecycle;
- explicit `MIDIHDR` ownership;
- deterministic shutdown/no leaked handles.

### Gate

HOLD on unexplained undocumented hacks, unsafe teardown, ambiguous ownership, or pressure to add a hidden third backend.

Mandatory Claude Code review.

## 6. Stage 2 – MIDI Core + transport abstraction

### Goal

Build the reusable product core around verified Stage 1 behavior.

### Scope

- endpoint/route model;
- stable identity model;
- internal MIDI/UMP event model;
- transport interface;
- fake/loopback transport;
- WMS transport;
- WinMM transport;
- structured errors;
- counters/diagnostics primitives;
- deterministic lifetime.

### Acceptance

- one backend per logical connection;
- no shadow receiver;
- repeated lifecycle test (target: 100 open/close cycles);
- clean resource closure;
- deterministic route identity;
- failures observable.

No GUI yet.

## 7. Stage 3 – Generic MIDI + SysEx + transfer engine

### Goal

Make the generic engine correct before device-specific features or GUI.

### Scope

- MIDI 1.0 parsing and retained UMP representation;
- SysEx frame model;
- WMS SysEx7 reassembly;
- WinMM chunk/buffer assembly;
- `.syx` load/save;
- incomplete-frame detection;
- transfer sequencing;
- pacing;
- progress;
- cancellation;
- timeout/error/disconnect behavior;
- queue overflow diagnostics.

### Acceptance

- byte-exact generic SysEx roundtrip;
- malformed/incomplete data cannot be marked valid;
- large-data tests pass to `QUALITY_POLICY.md`;
- cancellation/failure paths pass;
- no GUI dependency.

## 8. Stage 4 – Device/profile isolation proof

### Goal

Prove that device knowledge lives above transport.

### Scope

- Generic profile;
- one real data-driven profile;
- one strong real-device fixture path;
- profile registry/validation/source metadata;
- optional first compiled protocol hook if justified.

### Acceptance

- generic behavior works without profile;
- known/unknown profile behavior is deterministic;
- no device branch in WMS/WinMM;
- provenance recorded.

Mandatory Claude Code architecture review.

## 9. Stage 5 – Qt 6 product GUI

### Goal

Build the real user application on top of the proven core.

### Required design inputs

- `docs/design/GUI_DESIGN_SPEC.md`
- `docs/design/TAUREON_V4_GUI_MOCKUP.html`

The mockup is a workflow reference, not source code to port.

### Scope

- application shell/connection bar;
- route/backend selection;
- MIDI Monitor;
- SysEx Transfer;
- SysEx Manager;
- Librarian/Library foundation;
- Devices & Profiles;
- Diagnostics;
- Settings;
- progress/cancel/error presentation.

### Acceptance

- fake transport can drive GUI;
- no callback touches GUI;
- bounded model/view monitor;
- 100k-event synthetic stress;
- large SysEx synthetic workflow remains responsive;
- high-DPI/readability tests pass.

No unattended real-hardware writes.

## 10. Stage 6 – Robustness & failure injection

### Goal

Attack failure modes deliberately.

### Scope

- disconnect/port disappearance;
- busy/open failure;
- queue overflow;
- malformed/incomplete SysEx;
- cancellation races;
- shutdown during transfer;
- repeated reconnect;
- corrupted settings;
- filesystem failure/partial write;
- heavy realtime traffic;
- static analysis/sanitizer-compatible tests.

### Acceptance

- no known P0/P1 reliability blocker;
- failure states observable;
- no tested GUI freeze;
- no unexplained lifetime blocker;
- limitations documented.

## 11. Stage 7 – Release-candidate packaging & software validation

### Goal

Produce a runnable release candidate independent of the build tree.

### Deliverables

- Release x64 build;
- documented reproducible build commands;
- portable deployment folder;
- portable ZIP;
- required Qt runtime/plugins;
- WMS dependency/runtime detection;
- version metadata;
- README/changelog/license notices as applicable;
- diagnostics/known-issues documentation;
- `SOFTWARE_VALIDATION_REPORT.md`.

### Acceptance

Packaged app runs from a clean folder without accidental dependency on:

- source tree;
- Python;
- IDE;
- developer-only PATH;
- debug DLLs;
- unbundled Qt plugins.

Not yet hardware-validated or stable.

## 12. Stage 8 – Independent final software review

### Goal

Attempt to falsify the release candidate before hardware handoff.

Default reviewer: Claude Code.

Review:

- architecture/lifetime;
- concurrency;
- SysEx integrity;
- backend isolation;
- routing/identity;
- error handling;
- file safety;
- packaging;
- unsupported success claims.

P0/P1 must be resolved before Stage 9.

Any fix invalidating packaged evidence requires affected tests/package/report to be rerun.

## 13. Stage 9 – User hardware validation

### Goal

Validate the exact release candidate on real hardware.

Create `HARDWARE_VALIDATION_CHECKLIST.md` with exact binary/tag/commit, expected results, log capture, STOP points, and backup/rollback requirements.

Default progression:

1. enumeration only;
2. connect/disconnect/reconnect;
3. realtime receive;
4. harmless realtime send where safe;
5. small SysEx receive;
6. known large/bank dump receive;
7. raw SysEx send to safe/noncritical destination;
8. device-specific validated restore last.

A hardware failure returns to the owning development stage, followed by affected tests, rebuilt RC, and renewed review where risk requires it.

## 14. Stable release

A stable `v4.0.0` may be created only after:

- agreed product MUST criteria pass;
- Stage 8 passes;
- Stage 9 passes to agreed hardware scope;
- no unresolved P0/P1 remains;
- known lower-severity issues are documented/accepted;
- User explicitly accepts the release.

Do not generalize hardware compatibility beyond devices/workflows actually tested.

## 15. Git discipline

Use a new repository; old TAUREON remains untouched.

Prefer small coherent commits.

Suggested simple model:

```text
main       = approved/passed work
feature/*  = optional bounded work
```

Milestones may use:

```text
v4.0.0-alpha*
v4.0.0-beta*
v4.0.0-rc*
v4.0.0
```

No stable tag before User acceptance.
