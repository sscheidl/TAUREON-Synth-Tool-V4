# Stage 3 Brief – Generic MIDI, SysEx & Transfer Engine

**Stage:** 3
**Status:** IMPLEMENTATION COMPLETE / PASS RECOMMENDED / awaiting User gate
**Implementation lead:** Codex
**Coordination:** ChatGPT Classic / Project Manager
**Review:** targeted Claude Code review only if architecture changes materially, a P0/P1 appears, byte/lifetime integrity remains uncertain, or the Project Manager requests it
**Decision authority:** User where required

## 1. Goal

Build the complete **generic MIDI, SysEx, and transfer foundation** on top of the production MIDI Core and transport architecture established in Stage 2.

Stage 3 must turn the native transport boundary into a reliable generic engine capable of:

* receiving and representing realtime MIDI;
* preserving exact MIDI 1.0 data;
* retaining WMS UMP information where required;
* converting MIDI 1.0 `F0 ... F7` SysEx to and from WMS SysEx7 UMP without byte loss;
* assembling SysEx independently of transport chunk boundaries;
* loading and saving `.syx` files exactly;
* detecting malformed and incomplete SysEx;
* sequencing transfers;
* pacing output;
* reporting progress;
* supporting cancellation;
* handling timeout, disconnect, and queue-overflow conditions deterministically.

The generic engine must remain completely independent of synth/device-specific knowledge.

The central Stage-3 correctness requirement is:

> **MIDI 1.0 `F0 ... F7` SysEx ↔ WMS UMP SysEx7 conversion, segmentation, and reassembly must be byte-exact.**

This closes the Stage-1/Stage-2 open item tracked as **R-003**.

---

## 2. Preconditions

Stage 0: **PASS / CLOSED**

Stage 1: **PASS WITH NON-BLOCKING FOLLOW-UPS / CLOSED**

Stage 2: **must be PASS / CLOSED before implementation starts**

In particular, the Stage-2 WinMM partial-open `MIDIHDR` P1 found during targeted review must be fixed, regression-tested, committed, and gated before Stage 3 begins.

Mandatory architecture inherited from Stage 2:

1. Exactly one backend owns a logical transport connection.
2. Supported production backends are:

   * Windows MIDI Services Direct;
   * Native WinMM.
3. No hidden receiver or third backend.
4. WMS and WinMM route identity remain backend-specific.
5. No cross-backend route translation or fuzzy rebinding.
6. RX and TX routes may be independent.
7. Native callbacks perform minimal work.
8. WMS lifetime remains worker/MTA-owned and deterministic.
9. WinMM callback → queue → worker separation remains intact.
10. WinMM `MIDIHDR` ownership remains deterministic.
11. Transport/Core remains Qt-independent.
12. Device-specific logic is forbidden below the future device/profile/protocol layer.

---

## 3. Architecture Boundary

Stage 3 fills the generic engine layers between application-facing functionality and the existing transport contract:

```text
Application / future GUI
          |
          v
Application Services / Controllers
          |
          v
Device/Profile Services        [Stage 4+]
          |
          v
Transfer Engine                [Stage 3]
          |
          v
Generic MIDI / SysEx Core      [Stage 3]
          |
          v
MIDI Core                      [Stage 2]
          |
          v
IMidiTransport                 [Stage 2]
        /       \
       v         v
 WMS Direct   WinMM Native
```

Stage 3 may extend the existing transport implementations only where required to provide the production realtime message send/receive boundary already anticipated by `IMidiTransport`.

Do not move transfer semantics into WMS or WinMM.

---

## 4. Core Principles

### 4.1 Exact data before interpretation

Raw/native data is authoritative.

Do not:

* normalize unknown data silently;
* repair malformed SysEx;
* invent missing framing bytes;
* discard UMP information before the application has extracted what it requires;
* alter MIDI 1.0 payload bytes during conversion or persistence.

### 4.2 Transport chunks are not SysEx frames

A WinMM `MIDIHDR`, callback, WMS packet, UMP message, queue element, or file-read chunk must never be assumed to equal one complete SysEx frame.

Frame assembly belongs to the generic SysEx layer.

### 4.3 Transfer policy is not transport policy

WMS and WinMM move native messages.

The Transfer Engine owns:

* sequencing;
* pacing;
* progress;
* cancellation;
* timeout;
* transfer state;
* generic error handling.

### 4.4 Unknown devices remain generic

No manufacturer/model-specific branch, checksum, ACK/NAK sequence, bank rule, preset rule, or restore protocol may enter Stage-3 transport or generic SysEx code.

---

# Part A – Generic MIDI

## 5. Generic MIDI Message Processing

Implement the generic parsing/representation required for at least:

* Note On;
* Note Off;
* Control Change;
* Program Change;
* Channel Pressure;
* Polyphonic Aftertouch;
* Pitch Bend;
* System Common;
* MIDI Clock / transport;
* Active Sensing;
* System Realtime;
* SysEx boundaries;
* retained/unknown UMP representation where the generic parser does not understand the message semantically.

Requirements:

* exact native/raw representation remains accessible;
* parsing must not make device-specific assumptions;
* unknown messages remain representable;
* malformed input must fail or remain explicitly unknown rather than being guessed;
* parsing must not destroy information needed for diagnostics or later protocol interpretation.

MIDI interpretation may add structured metadata, but raw data remains authoritative.

---

## 6. Realtime MIDI Production Path

Complete the production receive/send boundary required by Stage 3.

### WMS

Implement the required realtime message/event path using the existing worker/MTA ownership model.

Requirements:

* native WMS callbacks perform minimal work;
* callbacks must not invoke application or Qt GUI logic directly;
* complete UMP data remains representable;
* callback acceptance can be stopped deterministically during shutdown;
* no callback can reach destroyed state.

### WinMM

Use the existing callback → bounded queue → worker architecture.

Requirements:

* short-message receive is delivered through the generic message boundary;
* long-message data is delivered without violating `MIDIHDR` ownership;
* output short/long messages use deterministic ownership;
* no Stage-1 callback-side `midiInAddBuffer` regression;
* queue overflow and late callbacks remain observable.

### Send contract

Stage 2 currently permits unsupported send behavior.

Stage 3 must provide the production generic send path required by the transfer engine for both supported backends to the extent safely testable without physical MIDI hardware.

Transport send remains transport-only and must not acquire pacing, SysEx semantics, retries, or device protocol logic.

---

# Part B – Generic SysEx Engine

## 7. MIDI 1.0 SysEx Frame Model

Create a generic frame representation capable of distinguishing at minimum:

```text
Complete
Incomplete / truncated
Malformed / invalid
```

A complete MIDI 1.0 SysEx frame must preserve its exact byte sequence including its framing.

Requirements:

* exact `F0 ... F7` byte preservation;
* multiple frames in one stream;
* one frame across many input chunks;
* many frames across arbitrary chunk boundaries;
* empty/minimal legal frames where applicable;
* truncated input remains incomplete;
* unexpected bytes are not silently repaired;
* legal realtime interleaving must not corrupt SysEx reconstruction;
* frame count and byte count must be deterministic.

Do not infer device semantics.

---

## 8. Stream Parser / Assembler

Provide incremental parsing so the SysEx engine can consume arbitrary byte chunks.

It must not depend on:

* file-read chunk size;
* WinMM buffer size;
* WinMM callback count;
* WMS packet count;
* transport message count.

Required behavior includes:

```text
chunk 1: F0 01
chunk 2: 02 03
chunk 3: 04 F7
```

producing the same complete frame as a single contiguous input.

Likewise, several complete frames may occur inside one input chunk.

Parser state must be deterministic across:

* normal completion;
* explicit reset;
* cancellation;
* malformed data;
* disconnect;
* end-of-file/end-of-stream.

---

## 9. WMS SysEx7 ↔ MIDI 1.0 Conversion

This is the primary Stage-3 integrity task.

Implement and prove:

```text
MIDI 1.0 F0 ... F7 byte stream
              ↕
WMS UMP SysEx7 packets
```

Requirements:

* byte-exact payload preservation;
* correct segmentation when one frame requires multiple SysEx7 UMP messages;
* correct reassembly;
* correct handling of complete/start/continue/end packet sequences;
* no missing bytes;
* no duplicated bytes;
* no spurious `F0` or `F7`;
* no accidental treatment of SysEx7 packet metadata as payload;
* one-packet and multi-packet cases;
* consecutive frames;
* boundary cases around segmentation points;
* malformed/incomplete sequences reported explicitly.

The conversion must be independently testable without hardware.

A roundtrip:

```text
MIDI 1.0 SysEx bytes
    ↓
SysEx7 UMP
    ↓
MIDI 1.0 SysEx bytes
```

must produce byte-identical output.

The reverse direction must receive equivalent coverage.

Do not assume Stage-1 SysEx7 loopback evidence already proves this conversion. It does not.

---

## 10. WinMM Long-Message Assembly

Implement generic assembly above the WinMM transport.

Requirements:

* one frame may span multiple returned `MIDIHDR` buffers;
* one buffer may contain more than one frame;
* arbitrary buffer boundaries do not change the reconstructed byte stream;
* incomplete final buffers remain incomplete;
* reset/close data is not mistaken for valid completed SysEx;
* MIDIHDR ownership remains entirely in the transport layer.

The SysEx engine must consume application-owned bytes, not native header ownership.

---

# Part C – SysEx Files

## 11. `.syx` Load

Implement generic `.syx` file loading.

Requirements:

* binary-safe exact reads;
* no newline/text conversion;
* support multiple frames;
* preserve framing exactly;
* identify truncated/incomplete data;
* do not classify incomplete data as a valid complete capture;
* provide useful frame/byte accounting.

No device identification is required.

---

## 12. `.syx` Save

Implement exact binary persistence.

Requirements:

* complete frames save byte-identically;
* load → parse → save of valid input is byte-identical;
* saving malformed/incomplete data must require an explicit API path/status rather than masquerading as a valid complete capture;
* do not silently normalize or repair payloads;
* prefer safe/atomic file replacement where practical;
* imported originals must not be modified in-place by default.

No GUI Save/Save As workflow is required yet.

---

# Part D – Generic Transfer Engine

## 13. Transfer Engine Responsibility

Create the generic transfer engine above `IMidiTransport`.

It owns:

* ordered frame/message submission;
* inter-message/inter-frame pacing;
* transfer state;
* progress;
* cancellation;
* timeout;
* transport failure propagation;
* disconnect handling;
* generic diagnostics.

It does not own:

* device checksum algorithms;
* bank semantics;
* SysEx Device-ID rewriting;
* ACK/NAK protocol;
* manufacturer-specific handshake;
* device-specific restore validation;
* synth-specific retry policy.

Those belong to later profile/protocol work.

---

## 14. Transfer State Model

Define deterministic transfer states appropriate to the implementation, for example:

```text
Idle
Preparing
Running
Cancelling
Completed
Cancelled
Failed
```

Exact names may differ if justified.

Requirements:

* deterministic transitions;
* one terminal result;
* no success after known failure;
* no continued frame submission after cancellation has taken effect;
* disconnect becomes visible failure/cancellation as defined by the API;
* shutdown cannot leave an active detached transfer.

Do not hide races with arbitrary sleeps.

---

## 15. Pacing

Provide generic pacing independently of transport.

At minimum support:

* immediate/no additional delay where safe for software testing;
* configurable inter-message/inter-frame delay.

Requirements:

* pacing is represented explicitly;
* timing delay represents transfer policy, not synchronization hacks;
* device-specific pacing defaults belong to Stage 4 profiles or later;
* transport backends must not contain synth-specific timing.

Do not interpret the WMS maximum-future-timestamp constant as a SysEx/message-size limit.

If Stage 3 requires a material timestamp/scheduling decision beyond immediate send, document evidence and create an ADR only if architecture-significant.

---

## 16. Progress

Expose deterministic progress suitable for later GUI use.

At minimum make available where meaningful:

* frames/messages total;
* frames/messages accepted/sent;
* bytes total;
* bytes accepted/sent;
* current transfer state.

Do not claim hardware receipt merely because the operating-system transport accepted a message.

Transport acceptance and hardware validation are different concepts.

---

## 17. Cancellation

Cancellation must be deterministic and testable.

Requirements:

* cancellation request is observable;
* no new transfer units are scheduled after cancellation takes effect;
* in-flight native ownership completes/unwinds safely;
* no use-after-free;
* no detached thread;
* repeated cancellation is sensible/idempotent;
* shutdown during cancellation remains deterministic.

No automatic resume of a cancelled destructive transfer.

---

## 18. Timeout and Disconnect

Model failures explicitly.

Cover at least:

* transport closes unexpectedly;
* selected endpoint disappears;
* native send fails;
* transfer cannot make progress;
* cancellation races a completion;
* application shutdown occurs during transfer.

Do not reduce all outcomes to Boolean success/failure.

Preserve useful transport/native error details.

---

## 19. Queue Overflow Diagnostics

Queue/resource pressure must be observable.

Expose appropriate diagnostics such as:

* dropped native events;
* dropped generic events where applicable;
* queue high-water mark;
* overflow condition;
* incomplete SysEx caused by data loss.

A capture affected by known overflow must never be presented as a verified complete SysEx capture.

---

# Part E – Test Architecture

## 20. Pure Unit Tests

Environment-independent tests belong in ordinary CTest/CI.

Cover at minimum:

### MIDI

* representative channel voice messages;
* system common/realtime;
* Polyphonic Aftertouch;
* Program Change;
* Pitch Bend;
* unknown/raw preservation;
* malformed input.

### SysEx byte-stream parser

* tiny frame;
* multiple frames;
* truncated frame;
* byte-by-byte input;
* arbitrary chunk splits;
* many small frames;
* legal realtime interleaving;
* malformed framing;
* explicit parser reset.

### WMS SysEx7 conversion

* single-packet frame;
* multi-packet frame;
* segmentation boundaries;
* Start/Continue/End;
* consecutive frames;
* incomplete packet sequence;
* malformed packet sequence;
* MIDI1 → SysEx7 → MIDI1 byte-exact roundtrip;
* SysEx7 → MIDI1 → SysEx7 equivalent payload roundtrip.

### WinMM stream assembly

* frame split across buffer boundaries;
* multiple frames per chunk;
* truncated final chunk;
* arbitrary chunk patterns.

### `.syx`

* load;
* save;
* multiple frames;
* incomplete file;
* byte-identical load → parse → save.

### Transfer engine

* success;
* multiple frames;
* pacing;
* progress;
* cancellation before start;
* cancellation during transfer;
* timeout;
* send failure;
* disconnect;
* shutdown while active;
* failure propagation.

---

## 21. Large Synthetic SysEx Tests

Run synthetic tests with at least approximately:

* 64 KiB;
* 600 KiB;
* 900 KiB;
* greater than 1 MiB.

Requirements:

* exact byte preservation;
* deterministic frame counts;
* no truncation;
* no hidden fixed-size assumption;
* no unbounded accidental memory growth;
* cancellation remains responsive;
* malformed/truncated variants remain detectable.

Include both:

* one/few large frames where representable by the generic test model;
* many smaller frames totaling large stream sizes.

No physical hardware is required or permitted.

---

## 22. Fake Transport Tests

Use the Stage-2 fake transport to test generic engine behavior without WMS/WinMM.

It should be possible to drive:

* receive;
* send;
* disappearance;
* failure;
* cancellation;
* timeout;
* controlled delays;
* overflow/failure signals

without accessing a MIDI service or real endpoint.

Do not add fake behavior that changes the production contract merely to simplify tests.

---

## 23. Local WMS Integration Tests

Keep WMS integration tests explicitly opt-in/labeled.

Where safely supported by the existing WMS-native loopback environment, verify:

* production realtime receive path;
* production generic send;
* short/realtime messages as appropriate;
* SysEx7 segmentation;
* SysEx7 reassembly;
* byte-exact MIDI1 ↔ UMP SysEx7 roundtrip;
* repeated transfer/cancel/close where useful;
* no callbacks after shutdown;
* cleanup.

No physical MIDI hardware.

Do not change Windows MIDI API mode, service configuration, drivers, or registry.

---

## 24. Local WinMM Integration Tests

Keep WinMM integration tests explicitly opt-in/labeled.

Use only the previously approved uniquely named temporary Windows-native WMS loopback strategy.

Verify where applicable:

* short send/receive;
* long-message send/receive;
* multi-buffer SysEx;
* production callback → queue → worker path;
* byte-exact receive;
* transfer cancellation/close;
* deterministic `MIDIHDR` ownership;
* no callbacks after close;
* cleanup of temporary loopback.

Automated code must reject ambiguous or non-test endpoints.

No ESI or other physical MIDI output may be selected.

---

## 25. CI

Ordinary hosted CI must run environment-independent tests.

Environment-specific MIDI tests remain separate.

CI must not:

* install a virtual MIDI driver;
* fake a WMS PASS;
* claim local WMS/WinMM integration coverage when prerequisites are absent;
* access physical MIDI hardware.

Use clear labels/configuration so:

```text
CI PASS
```

means the software-only suite actually ran and passed.

Local MIDI integration PASS must remain separately reported.

---

# Part F – Diagnostics and Integrity

## 26. Required Generic Diagnostics

Provide primitives useful to later Diagnostics/GUI layers.

At minimum expose where applicable:

* received message count;
* transmitted message count;
* SysEx frame count;
* SysEx byte count;
* incomplete/malformed frame count;
* queue drops;
* queue high-water state;
* active transfer state;
* transfer progress;
* last transfer error;
* transport disconnect/error propagation.

No GUI implementation.

---

## 27. Data Integrity Rules

The following are mandatory invariants:

1. Valid MIDI 1.0 SysEx input survives a generic roundtrip byte-identically.
2. WMS SysEx7 conversion never silently changes payload bytes.
3. Arbitrary WinMM buffer boundaries never change reconstructed bytes.
4. Truncated data is never labeled complete.
5. Overflow-affected capture cannot masquerade as complete.
6. No speculative frame repair.
7. Unknown data is preserved or explicitly rejected, never guessed.
8. Original imported files are not silently overwritten.
9. Transfer success means the defined software transfer boundary succeeded — not that physical hardware accepted or stored the data.

---

# Part G – Out of Scope

## 28. Stage 3 Does NOT Implement

* device profiles;
* device detection;
* manufacturer/model-specific interpretation;
* synth-specific code;
* checksums;
* ACK/NAK protocols;
* request/response device state machines;
* validated restore workflows;
* bank/preset parsing;
* librarian;
* preset conversion;
* Universal MIDI Identity workflow beyond generic message handling;
* production Qt GUI;
* monitor table/UI;
* SysEx Manager UI;
* profile editor;
* automatic backend failover;
* fuzzy endpoint rebinding;
* WMS↔WinMM route translation;
* third MIDI backend;
* real hardware validation.

Stage 4 owns the first device/profile isolation proof.

Stage 5 owns the production Qt GUI.

---

# Part H – Safety

## 29. System Safety

Automated Stage-3 work must not:

* install/remove/change MIDI drivers;
* alter registry MIDI settings;
* alter Windows MIDI API mode;
* modify `MidiSrv`;
* restart/configure MIDI services merely to pass a test;
* install Bome/teVirtualMIDI or another virtual driver;
* use a physical MIDI output as an automated test target;
* automatically resume a failed transfer.

Any required machine-wide change is Stop/Ask.

---

## 30. Hardware Safety

No unattended real-hardware transmission.

Software-only loopback evidence is not hardware validation.

Only the User may later establish hardware behavior.

All Stage-3 reports must explicitly state that physical MIDI hardware was not validated.

---

# Part I – Deliverables

## 31. Required Deliverables

At minimum:

* generic MIDI parser/representation additions;
* generic SysEx frame model;
* incremental MIDI 1.0 SysEx stream parser;
* WMS SysEx7 segmentation/conversion/reassembly;
* WinMM chunk/buffer assembly above transport;
* `.syx` binary load/save support;
* generic Transfer Engine;
* pacing;
* progress;
* cancellation;
* timeout/disconnect/error propagation;
* queue/overflow diagnostics;
* required WMS/WinMM realtime transport completion;
* fake-transport tests;
* comprehensive pure unit tests;
* large synthetic SysEx tests;
* opt-in local WMS integration tests;
* opt-in local WinMM integration tests;
* architecture update where Stage-3 behavior becomes established;
* Risk Register update;
* Project State update;
* `docs/stages/STAGE_3_REPORT.md`.

Create ADRs only for genuinely architecture-significant decisions.

---

# Part J – Acceptance Criteria

## 32. Stage 3 Acceptance

Stage 3 may PASS only when all mandatory applicable criteria are met:

1. Generic MIDI parsing exists and remains device-independent.
2. Raw MIDI 1.0 data remains exact and accessible.
3. Unknown UMP remains representable without forced flattening.
4. Generic SysEx frames distinguish complete, incomplete, and malformed data.
5. SysEx assembly is independent of transport/file chunk boundaries.
6. Multiple frames per stream are supported.
7. Legal realtime interleaving does not corrupt SysEx reconstruction.
8. Truncated SysEx can never be reported as complete.
9. MIDI 1.0 `F0 ... F7` → WMS UMP SysEx7 conversion is implemented.
10. WMS UMP SysEx7 → MIDI 1.0 `F0 ... F7` reconstruction is implemented.
11. Single-packet SysEx7 conversion is byte-exact.
12. Multi-packet SysEx7 segmentation/reassembly is byte-exact.
13. Start/Continue/End sequences are tested.
14. MIDI1 → SysEx7 → MIDI1 roundtrip is byte-identical.
15. WinMM buffer/chunk boundaries do not alter reconstructed SysEx.
16. `.syx` load/save supports multiple frames.
17. Valid `.syx` load → parse → save is byte-identical.
18. Incomplete `.syx` data remains explicitly incomplete.
19. Transfer Engine is separate from transport.
20. Transfer Engine provides sequencing.
21. Generic pacing is implemented above transport.
22. Progress is deterministic and does not imply hardware acceptance.
23. Cancellation works before and during transfer.
24. Timeout/send failure/disconnect are observable explicit outcomes.
25. Shutdown during active/cancelling transfer is deterministic.
26. Queue overflow/data-loss state is observable.
27. Overflow-affected SysEx cannot be presented as complete.
28. Approximately 64 KiB synthetic stream test passes.
29. Approximately 600 KiB synthetic stream test passes.
30. Approximately 900 KiB synthetic stream test passes.
31. Greater-than-1-MiB synthetic stream test passes.
32. Fake-transport transfer/failure tests run in ordinary CI.
33. Environment-independent Stage-3 tests run in ordinary CTest/CI.
34. WMS/WinMM integration tests remain opt-in/labeled.
35. Safe local WMS regression passes where prerequisites exist.
36. Safe local WinMM regression passes where prerequisites exist.
37. No physical MIDI endpoint receives automated test traffic.
38. No device/profile logic has entered transport or generic SysEx code.
39. No hidden third backend exists.
40. No architecture regression from Stage 2 exists.
41. Clean build passes.
42. All applicable Stage-3 tests pass.
43. No unresolved P0/P1 remains.

---

# Part K – Gate Conditions

## 33. PASS

PASS when:

* generic MIDI/SysEx/transfer architecture is complete to Stage-3 scope;
* byte integrity is proven;
* MIDI 1.0 ↔ SysEx7 conversion is proven;
* large-data and failure-path tests pass;
* transport lifetime remains sound;
* no P0/P1 remains.

---

## 34. PASS WITH NON-BLOCKING FOLLOW-UPS

Permitted only if:

* all byte-integrity and lifetime requirements pass;
* all mandatory functionality is complete;
* no P0/P1 remains;
* remaining limitations are genuinely later-stage/device/GUI/hardware concerns.

Do not downgrade an unresolved byte-integrity, lifetime, cancellation, or routing problem to a non-blocking follow-up.

---

## 35. HOLD

Hold Stage 3 if:

* any MIDI 1.0 ↔ SysEx7 byte mismatch remains;
* segmentation/reassembly is ambiguous;
* malformed/incomplete data can masquerade as valid;
* a WinMM buffer boundary changes reconstructed bytes;
* a transfer can continue after cancellation incorrectly;
* shutdown can release native/application data too early;
* a race is hidden with sleeps/timeouts;
* queue overflow can silently corrupt a supposedly valid capture;
* a required send/receive path needs a third backend;
* a system/driver/API-mode change is required;
* physical MIDI hardware would be required to prove generic correctness;
* device-specific logic becomes necessary inside transport/core;
* unresolved P0/P1 remains.

---

# Part L – Stop / Ask

## 36. Stop / Ask Conditions

Stop instead of stacking workarounds if:

* two materially different fixes fail for the same core blocker;
* byte corruption has no identified root cause;
* WMS SysEx7 framing behavior cannot be reconciled with deterministic MIDI 1.0 bytes;
* WinMM ownership/lifetime becomes ambiguous;
* WMS lifetime differs materially from the Stage-2 model;
* transfer cancellation cannot be made deterministic;
* a callback requires documented-unsafe work;
* a race appears solvable only by arbitrary timing delays;
* an undocumented reverse-engineering dependency becomes necessary;
* a system/driver/service/API-mode change appears required;
* endpoint ambiguity could target physical hardware;
* an architecture change conflicts with Stage-2 invariants.

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

---

# Part M – Stage Report

## 37. `STAGE_3_REPORT.md`

The Stage-3 report must document:

* generic MIDI architecture implemented;
* SysEx frame/parser architecture;
* exact MIDI 1.0 framing rules implemented;
* WMS SysEx7 conversion and reassembly;
* WinMM chunk assembly;
* `.syx` I/O behavior;
* transfer engine;
* pacing model;
* progress semantics;
* cancellation semantics;
* timeout/disconnect behavior;
* queue/overflow diagnostics;
* production transport realtime additions;
* files/modules changed;
* commands/builds run;
* CI unit-test results;
* large synthetic-data results;
* local WMS integration results or explicit skip reason;
* local WinMM integration results or explicit skip reason;
* byte-integrity evidence;
* relevant failure-injection evidence;
* architecture changes/ADRs;
* Risk Register changes;
* deviations from Stage 2;
* Stop/Ask events;
* known limitations;
* hardware-dependent behavior explicitly not tested;
* explicit PASS/HOLD recommendation.

Do not claim hardware compatibility.

---

# Part N – Review Handoff

## 38. Claude Code Review Policy

A full Claude Code review is not automatically required at the end of Stage 3.

Request a targeted or full Claude review before gate closure if:

* a P0/P1 appears;
* MIDI/SysEx byte integrity remains uncertain;
* WMS/WinMM lifetime or concurrency changes materially;
* an architecture-changing ADR is created;
* cancellation/shutdown ownership remains questionable;
* Codex and deterministic evidence disagree;
* the Project Manager judges an independent check worthwhile.

If no such condition exists, Project Manager/User may gate Stage 3 directly from deterministic evidence.

Stage 4 retains its own mandatory architecture review.

---

# Part O – Stage Completion

## 39. Completion Rule

Do not begin Stage 4 automatically.

Finish Stage 3 with:

* complete Stage-3 implementation;
* complete Stage-3 report;
* Project State updated;
* Risk Register updated;
* architecture/ADRs updated where applicable;
* clean build;
* all applicable tests passing;
* clean repository diff/worktree;
* dedicated Stage-3 commit;
* current `main` pushed to the private GitHub repository;
* explicit PASS/HOLD recommendation.

Stage 4 begins only after the Stage-3 gate is resolved.
