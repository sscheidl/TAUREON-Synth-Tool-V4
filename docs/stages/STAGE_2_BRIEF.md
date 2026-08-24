# Stage 2 Brief – MIDI Core and Transport Interface

**Stage:** 2
**Status:** PASS / CLOSED after targeted-review P1 closure and User gate
**Implementation lead:** Codex
**Coordination:** ChatGPT Classic / Project Manager
**Review:** targeted Claude Code review only if architecture changes materially, a P0/P1 appears, or the Project Manager requests it
**Decision authority:** User where required

## 1. Goal

Turn the evidence from the native WMS and WinMM Stage 1 spikes into the first production-grade TAUREON V4 MIDI architecture.

Stage 2 establishes:
- backend-independent MIDI core types;
- the production transport interface;
- backend-specific route identity;
- deterministic endpoint resolution;
- backend ownership and lifecycle rules;
- thread/callback boundaries;
- production lifetime primitives for WMS and WinMM;
- testable connection/enumeration infrastructure.

Stage 2 does **not** yet implement the complete realtime MIDI path or the generic SysEx transfer engine.

## 2. Preconditions

Stage 0: **PASS / CLOSED**

Stage 1: **PASS WITH NON-BLOCKING FOLLOW-UPS / CLOSED**

No unresolved P0/P1 findings remain.

Mandatory Stage 1 inputs:
1. Backend is part of persisted route identity.
2. WMS and WinMM route identities are not assumed transferable.
3. No silent WMS↔WinMM route translation.
4. No fuzzy automatic rebinding.
5. Missing or ambiguous routes require deliberate user action.
6. WinMM native callbacks perform minimal work.
7. Prefer callback → queue/signal → worker-owned processing/requeue.
8. Production `MIDIHDR` handling requires deterministic RAII-quality ownership on success and failure paths.
9. `QPointer` is not a synchronization primitive.
10. Local WMS/WinMM integration tests are opt-in and honestly separated from hosted CI.
11. RC4 WMS/WinMM correlation helpers are **not** an architecture dependency.
12. R-002 remains Medium / Open.
13. R-003 remains Open and is a mandatory Stage 3 concern.
14. R-005 reflects backend-specific route identity.

## 3. Architecture Boundary

```text
Application / GUI
        │
        ▼
Application Services / Controllers
        │
        ▼
Transfer Engine / MIDI Monitor / Librarian
        │
        ▼
MIDI Core
        │
        ▼
IMidiTransport
        │
        ├── WMS Direct
        └── WinMM Native
```

Stage 2 owns only the lower part required to establish the Core/Transport contract.

No synth-specific logic is permitted below the future device/profile layer.

## 4. Core Design Principles

### 4.1 One active transport backend

Exactly one backend is active for a logical transport connection:
- WMS Direct
- WinMM Native

No hidden secondary receiver. No hot automatic failover while connected. Backend switching is explicit.

### 4.2 RX and TX are independent routes

A logical connection may use one RX route, one TX route, only RX, or only TX. RX and TX may point to different physical ports/groups.

### 4.3 Backend identity is part of route identity

```text
MidiRouteIdentity
    backend
    direction
    backend-specific identity
```

A WMS route and a WinMM route are not interchangeable even when they appear to represent the same hardware.

## 5. Core Types

Create production-grade backend-independent core types for at least:

```text
MidiBackend
    WindowsMidiServices
    WinMM
```

```text
MidiDirection
    Input
    Output
```

Endpoint/route descriptors must retain backend-specific information without pretending both backends expose the same identity model.

Possible fields:
- backend;
- direction;
- backend-native identifier where available;
- display name;
- manufacturer/product metadata where available;
- protocol;
- current-session index where applicable;
- group;
- function-block metadata where applicable;
- capability flags.

## 6. WMS Route Identity

At minimum consider:

```text
backend = WMS
EndpointDeviceId
group
direction
```

Do not claim universal hardware identity. Resolution must represent:
- Exact
- Missing
- Ambiguous
- Invalid

No fuzzy substitution.

## 7. WinMM Route Identity

Numeric port index must never be the sole persisted identity.

Candidate metadata:
- direction;
- port/display name;
- `wMid`;
- `wPid`;
- supported capability information.

Runtime index may be a secondary hint only.

Resolution:

```text
one confident match -> Exact
zero matches -> Missing
multiple plausible matches -> Ambiguous
```

Never select the first matching port merely to continue.

A DIN interface name identifies the interface/port, not the synthesizer behind the cable.

## 8. No Cross-Backend Route Translation

Do not implement WMS↔WinMM route guessing.

The RC4 correlation-helper experiment fail-fast result means those APIs are not an architecture dependency.

Backend change may require deliberate user reselection.

## 9. Route Persistence

Define a versioned persistence representation containing at minimum:

```text
schema version
backend
direction
backend-specific identity
```

Do not bind persisted route identity to profile name, synth model, UI label alone, or current WinMM numeric index.

## 10. Resolution API

Use an explicit result model:

```text
ResolveResult
    Exact
    Missing
    Ambiguous
    Invalid
```

Ambiguity must never become “use first candidate”.

## 11. IMidiTransport

Design and implement the production transport interface.

It should cover:
- enumerate;
- open/connect;
- disconnect/close;
- connection state;
- RX route;
- TX route;
- backend identity;
- capabilities;
- future message delivery/send boundary.

Do not add Stage 3 transfer logic such as SysEx pacing, retries, handshakes, or protocol state machines.

## 12. Error Model

Create explicit errors for meaningful classes such as:
- backend unavailable;
- endpoint missing;
- endpoint ambiguous;
- endpoint disappeared;
- open failure;
- close failure;
- invalid route;
- unsupported capability;
- native API error;
- shutdown/cancellation.

Preserve useful native error information for diagnostics. Do not reduce everything to Boolean success/failure.

## 13. Lifecycle Model

Define deterministic states such as:

```text
Closed
Opening
Open
Closing
Failed
```

Requirements:
- deterministic transitions;
- no use-after-close;
- no callbacks into destroyed state;
- sensible idempotency;
- complete unwind on partial-open failure;
- observable endpoint disappearance;
- deterministic app shutdown.

No race may be solved by arbitrary sleeps.

## 14. WMS Production Lifetime Boundary

Carry forward Stage 1:
- WMS objects owned on intended worker/MTA context;
- explicit apartment init/shutdown;
- deterministic session/connection ownership;
- minimal native callback work;
- disable callback acceptance before destruction;
- revoke event handlers before teardown;
- close session/SDK before worker destruction.

Do not use cross-thread `QPointer` as a lifetime guarantee.

Core/transport code should preferably remain Qt-independent.

## 15. WinMM Production Callback Rule

Do not copy the Stage 1 `midiInAddBuffer`-inside-`midiInProc` pattern into production.

Preferred design:

```text
WinMM callback
      ↓
minimal event capture
      ↓
thread-safe queue/signal
      ↓
transport-owned worker
      ↓
processing / MIDIHDR requeue
```

## 16. MIDIHDR Ownership

Production ownership must explicitly cover:
- allocation;
- prepare;
- submitted ownership;
- completion;
- reset;
- unprepare;
- release;
- partial-open failure;
- send timeout/failure;
- shutdown with active buffers.

Use RAII or equivalent deterministic ownership.

It must be impossible for error-path unwinding to destroy a header/buffer still owned by WinMM.

R-002 remains Open until evidence supports lowering it.

## 17. Message Representation

Stage 2 may define the message/event representation required by the transport contract.

Requirements:
- lossless native data representation;
- preserve backend/timing metadata where needed;
- do not prematurely flatten WMS UMP;
- keep WinMM MIDI 1.0 bytes exact;
- unknown UMP remains representable.

Do not yet implement the Stage 3 SysEx transfer engine.

## 18. Timestamp Model

Separate:
1. timestamp representation;
2. timestamp conversion;
3. future scheduling horizon;
4. transfer/message-size constraints.

Do not treat WMS maximum-future-ticks as a SysEx/message-size limit.

## 19. Endpoint Change Handling

Represent:
- endpoint appeared;
- endpoint disappeared;
- metadata changed;
- selected route became unavailable.

Never silently switch to another route.

## 20. Test Architecture

### A. Pure unit tests

No MIDI service required.

Cover:
- route serialization/deserialization;
- resolver exact/missing/ambiguous cases;
- backend mismatch;
- state transitions;
- error propagation;
- ownership primitives;
- fake/mock transport contract.

These belong in ordinary CTest/CI.

### B. Local WMS integration tests

Opt-in/labeled:
- real initialization;
- enumeration;
- route identity;
- lifecycle;
- repeated open/close;
- endpoint-change behavior where safely reproducible.

### C. Local WinMM integration tests

Opt-in/labeled.

May use the approved temporary Windows-native WMS loopback strategy.

No physical hardware output.

Any temporary loopback must be uniquely named, verified before use, guarded against physical selection, and removed even on failure.

## 21. Regression Harness

Provide reusable local regression entry points for:
- repeated WMS lifecycle;
- repeated WinMM lifecycle;
- route-resolution tests.

Do not blindly reuse spike code. Stage 1 spikes remain evidence/reference, not hidden production libraries.

## 22. CI

Ordinary CI covers environment-independent core tests.

Environment-dependent integration tests must be clearly opt-in/labeled.

CI must distinguish actual PASS from SKIPPED because the local MIDI environment is unavailable.

## 23. Stage 1 Spike Reuse

Production code may reuse concepts only after reviewing:
- ownership;
- error paths;
- threading;
- API lifetime;
- provenance;
- architecture boundaries.

Do not bulk-copy spikes into production `src/`.

## 24. Out of Scope

Stage 2 does NOT implement:
- complete realtime MIDI monitoring;
- generic MIDI routing features;
- SysEx transfer engine;
- SysEx file parsing/pacing;
- request/response or ACK/NAK protocols;
- device profiles;
- synth-specific code;
- librarian;
- bank parsing;
- preset conversion;
- production Qt GUI;
- automatic backend failover;
- hardware validation.

Stage 3 owns realtime MIDI + SysEx engine work.

## 25. Safety

Automated tests must never send to physical synthesizers.

Do not:
- alter MIDI drivers;
- alter registry MIDI settings;
- alter MidiSrv configuration;
- alter Windows MIDI API mode;
- install a virtual MIDI driver;
- use ESI hardware ports as automated targets;
- introduce Bome/teVirtualMIDI as a production dependency;
- add a third transport backend.

Temporary Windows-native loopback use remains allowed only for isolated local integration tests with strict verification and cleanup.

## 26. Required Deliverables

At minimum:
- production MIDI Core source tree;
- `IMidiTransport`;
- WMS route/endpoint model;
- WinMM route/endpoint model;
- versioned route persistence;
- route resolver;
- explicit error/result model;
- deterministic lifecycle/state model;
- worker/callback boundary;
- WinMM `MIDIHDR` RAII/lifetime infrastructure;
- fake/mock transport;
- unit tests;
- opt-in local integration tests;
- architecture updates;
- Risk Register update;
- Project State update;
- `docs/stages/STAGE_2_REPORT.md`.

Create ADRs only for genuinely architecture-significant decisions.

## 27. Architecture Decisions Expected

### Backend-specific persisted route identity

Persisted route identity carries backend identity.

No automatic cross-backend translation.

Backend change may require deliberate reselection.

### WinMM callback and MIDIHDR ownership model

If the worker-owned requeue model proves viable, document it as the production rule.

## 28. Acceptance Criteria

Stage 2 may PASS only when:

1. `IMidiTransport` exists as production code.
2. Core types contain no synth-specific logic.
3. WMS and WinMM identity models remain backend-specific.
4. Persisted route backend is explicit.
5. Numeric WinMM index is never sufficient persisted identity.
6. Exact/missing/ambiguous resolution is tested.
7. No silent fuzzy substitution exists.
8. Backend switching cannot silently reuse an incompatible route.
9. WMS lifetime remains worker/MTA-owned and deterministic.
10. WinMM callbacks perform minimal work.
11. `MIDIHDR` ownership is deterministic on success and error paths.
12. No cross-thread `QPointer` synchronization assumption exists.
13. Environment-independent unit tests run in ordinary CTest.
14. Environment-dependent MIDI integration tests are opt-in/labeled.
15. No physical hardware is used by automated tests.
16. Stage 1 spikes remain isolated from production architecture.
17. No hidden third backend exists.
18. No Stage 3 transfer engine is implemented prematurely.
19. Clean build passes.
20. All applicable Stage 2 tests pass.
21. No unresolved P0/P1 remains.

## 29. Gate Conditions

### PASS

All mandatory Stage 2 architecture and lifecycle requirements are met and tested.

### PASS WITH NON-BLOCKING FOLLOW-UPS

Core architecture is sound and no P0/P1 remains, but bounded later-stage work remains documented.

### HOLD

Hold if:
- route ambiguity can silently select another endpoint;
- WMS lifetime cannot be deterministic;
- WinMM header/buffer ownership remains ambiguous;
- callback design requires unsafe multimedia calls from native callbacks;
- error-path teardown may release active MIDIHDR memory;
- backend abstraction requires WMS↔WinMM guessing;
- a third backend/workaround becomes necessary;
- a race is hidden with sleeps/timeouts;
- system MIDI configuration changes become necessary;
- Stage 2 depends on unreviewed spike behavior;
- unresolved P0/P1 remains.

## 30. Stop / Ask

Stop instead of stacking workarounds if:
- two materially different fixes fail for the same architecture blocker;
- WMS production lifetime differs materially from Stage 1 evidence;
- endpoint/group semantics cannot fit the route model without losing identity;
- WinMM ownership cannot be deterministic;
- a native callback requires a documented-unsafe API;
- cross-backend correlation becomes necessary for correctness;
- system/driver/API-mode modification appears required;
- byte/message integrity becomes uncertain;
- an architecture-changing decision conflicts with approved Stage 1 conclusions.

Use:

```text
Blocker:
Expected:
Actual:
Evidence:
Attempts:
Likely causes:
2–3 viable options:
Recommendation:
Decision required from User:
```

Then stop.

## 31. Stage Report

`STAGE_2_REPORT.md` must document:
- architecture implemented;
- production files added/changed;
- route identity model;
- persistence format;
- resolver behavior;
- WMS lifetime implementation;
- WinMM callback/worker implementation;
- MIDIHDR ownership model;
- tests executed;
- local integration tests executed/skipped;
- reasons for skipped environment-specific tests;
- risk changes;
- ADRs;
- deviations from Stage 1;
- Stop/Ask events;
- known limitations;
- explicit PASS/HOLD recommendation.

## 32. Review Handoff

A full Claude Code review is not automatically required if Stage 2 finishes without:
- P0/P1;
- architecture deviation;
- unresolved lifetime issue;
- material change to the approved transport model.

Any architecture-changing ADR, unresolved P0/P1, or material concurrency/lifetime uncertainty requires Claude Code review before gate closure.

## 33. Stage Completion

Do not begin Stage 3.

Finish Stage 2 with:
- complete Stage 2 report;
- updated Project State;
- updated Risk Register;
- architecture/ADR updates where applicable;
- clean repository diff;
- dedicated Stage 2 commit;
- explicit PASS/HOLD recommendation.

Stage 3 begins only after Project Manager/User gate approval.
