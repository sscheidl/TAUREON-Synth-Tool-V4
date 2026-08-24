# ADR-0002 – WinMM callback and MIDIHDR ownership

**Status:** Accepted
**Date:** 2026-08-24
**Owner:** Codex / Implementation Lead
**Review:** User-approved Stage 2 brief

## Context

WinMM owns prepared input/output `MIDIHDR` memory while it is submitted. Requeueing input buffers directly
inside `midiInProc`, as the Stage 1 spike did, would mix driver callback execution with mutable transport
ownership and teardown. Error-path unwinding must never release a buffer still owned by WinMM.

## Decision

The native callback performs bounded event capture and signals a transport-owned worker. It does not call
`midiInAddBuffer`, application code, or Qt. The worker delivers copied data and requeues returned input
headers. Header completion events take priority over droppable short-message events when the queue is full,
while worker control commands take priority over queued traffic so a continuous MIDI flood cannot starve
shutdown. Overflow remains observable.

Input and output headers use deterministic owners with explicit states for allocation, preparation,
submission, return/completion, unprepare, and release. A submitted header cannot be unprepared or destroyed.
Shutdown closes callback acceptance, stops/resets input, drains ownership completions on the worker,
unprepares returned headers, closes handles, and joins the worker. Production code uses no arbitrary sleeps.

## Alternatives considered

1. Requeue inside the native callback: rejected because it expands callback work and complicates teardown.
2. Let raw `MIDIHDR` storage follow ordinary container lifetime: rejected because error unwinding could free
   native-owned memory.
3. Poll or sleep during shutdown: rejected because timing guesses do not establish ownership.

## Consequences

### Positive

- Native callback work is minimal and independent of Qt/application lifetime.
- Header ownership and failure paths are explicit and unit-testable.
- Reset completions can be drained without accepting new application delivery.
- Drops and late callbacks are diagnosable.

### Negative / risks

- The bounded queue needs observable overflow policy.
- Vendor-driver behavior remains a hardware-dependent regression item; the safe local test uses a WMS-native
  virtual loopback only.
- Stage 3 must add production send and full message processing without weakening these ownership rules.

## Validation

- Fake native-API unit tests cover prepare, submit, completion/return, unprepare, retryable failures, and
  rejection of unprepare while submitted.
- A transport-level injected-native-API regression drives `open()` through input submit failure and proves
  unprepare occurs before the native handle closes; this closes the targeted-review P1 without changing the
  ownership model.
- The production WinMM transport completed 100 open/close cycles over a uniquely named temporary WMS
  loopback with stable handles, zero dropped callback events, and zero callbacks after acceptance closed.
- Stage 1 byte-integrity evidence remains an isolated opt-in regression and is not linked into production.

## Supersedes / superseded by

- None
