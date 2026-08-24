# TAUREON V4 – Quality, Safety & Evidence Policy

**Status:** Active cross-cutting policy  
**Maintained by:** Project Manager

## 1. Evidence hierarchy

Prefer deterministic evidence:

```text
build/compiler
    ↓
unit tests
    ↓
integration / fake transport
    ↓
stress / failure injection
    ↓
packaged clean-folder test
    ↓
independent review
    ↓
real user hardware validation
```

AI reasoning does not replace deterministic test evidence.

Automated software-only tests should run in CI where the hosted Windows environment can support them accurately. Local-only platform evidence must be reported explicitly rather than simulated by CI.

## 2. Software validation vs hardware validation

Software tests may establish:

- parser correctness;
- lifecycle behavior under simulation;
- byte-exact roundtrip;
- queue/cancellation logic;
- GUI responsiveness;
- packaging independence.

Only the User may validate real hardware behavior.

Reports must explicitly list hardware-dependent behavior that has not yet been tested.

## 3. System safety

Unattended development/test work must not:

- install/remove/change MIDI drivers;
- run vendor driver-cleanup utilities;
- change `UseLegacyMidi`;
- change Windows MIDI API mode;
- modify MIDI registry/service configuration;
- restart/configure `MidiSrv`;
- install a virtual MIDI driver merely to make a test pass;
- send automated test traffic to a real synth output;
- automatically resume a failed destructive transfer.

The application adapts to the environment rather than rewriting it.

Any exception requires explicit User approval.

## 4. Data safety

- raw SysEx payloads remain exact;
- incomplete capture is never marked complete;
- no silent normalization/repair;
- original imported files are not modified in place by default;
- Save As / atomic write is preferred where practical;
- malformed capture must not silently overwrite known-good data;
- destructive restore requires explicit confirmation and rollback/backup guidance.

## 5. Concurrency/lifetime quality

Forbidden shortcut:

> fixing an unexplained race/deadlock by only increasing sleeps/timeouts.

A timing delay is acceptable only when it represents documented external protocol timing.

Test lifecycle paths such as:

- close while callback is active;
- reconnect with stale events queued;
- cancel while completion is pending;
- app close during transfer;
- device disappearance during SysEx.

## 6. SysEx coverage

Automated tests should cover:

- tiny frames;
- multiple frames;
- truncated frames;
- realtime interleaving;
- many small frames;
- large frames;
- at least ~64 KiB, ~600 KiB, ~900 KiB, and >1 MiB synthetic streams;
- WMS Start/Continue/End combinations;
- WinMM buffer-boundary cases;
- byte-identical load → parse → save.

Known-good legacy captures may be reused when provenance permits.

## 7. GUI performance

Test at minimum:

- bursty MIDI;
- Clock/Active Sensing load;
- bounded monitor history;
- 100,000+ synthetic monitor events;
- large SysEx transfer/capture progress;
- cancellation;
- route disappearance;
- GUI shutdown during worker activity;
- 1920×1080 readability;
- 125/150/200% scaling.

## 8. Static analysis and runtime diagnostics

Use:

- project warning policy;
- static analysis;
- sanitizer-compatible tests where practical;
- leak/resource-lifetime checks;
- stress loops.

If a tool cannot cover a specific WMS/Qt integration, document the limitation rather than claiming coverage.

## 9. Failure severity

```text
P0 – critical blocker / unsafe / severe data corruption risk
P1 – release blocker
P2 – should fix
P3 – optional / low impact
```

No release candidate proceeds to real hardware validation with unresolved P0/P1 software findings.

## 10. Stop / Ask conditions

Stop and escalate when:

- two materially different fixes failed for the same architecture blocker;
- documented WMS integration cannot be made to work;
- an undocumented reverse-engineering dependency would be required for the core;
- a driver/registry/service/API-mode change is required;
- essential reuse has unclear licensing;
- existing work may be overwritten;
- endpoint ambiguity could target the wrong physical device;
- byte corruption has no identified root cause;
- a race is being treated only with sleeps/timeouts;
- a hardware-only fact blocks safe design;
- an unresolved P0/P1 architecture issue remains.

Escalation format:

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
