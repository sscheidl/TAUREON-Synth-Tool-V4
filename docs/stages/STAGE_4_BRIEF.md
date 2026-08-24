# Stage 4 Brief – Device/Profile Isolation Proof

**Stage:** 4
**Status:** READY – start only after the Stage 3 closure commit is pushed and the worktree is clean
**Implementation lead:** Codex
**Coordination:** ChatGPT Classic / Project Manager
**Mandatory review:** Claude Code architecture review (Opus, high)
**Decision authority:** User where required

## 1. Goal

Prove that device-specific knowledge can be added above the generic MIDI/SysEx and transfer layers without contaminating either transport backend or duplicating the generic engine.

Stage 4 establishes:

- a neutral Generic profile/fallback;
- a versioned, validated data-driven profile format;
- a deterministic profile registry and match/result model;
- exactly one real data-driven device profile;
- one provenance-approved real-device fixture path;
- capability/support-level metadata that never overclaims safe operations;
- evidence that unknown devices remain fully usable through the generic Stage 3 path.

The preferred real proof target is **Kawai K5000S**, because it is relevant to the Product Owner and exposes the required separation clearly. Stage 4 does not implement K5000 bank-file editing, checksum reverse engineering, or validated restore.

## 2. Preconditions

Before implementation starts, verify and record:

1. Stage 3 is closed as **PASS WITH NON-BLOCKING FOLLOW-UPS**.
2. The Stage 3 closure commit is on `origin/main`.
3. `HEAD == origin/main` and the worktree is clean.
4. No Stage 4 implementation already exists outside the approved scope.
5. R-003 remains worded as **Mitigated in software / targeted re-review pending**, not Closed.
6. The final Stage 3 report and review are read for their exact two non-blocking follow-ups; both must be copied into Stage 4 tracking and resolved or explicitly carried forward.

Do not reconstruct Stages 0–3 or rescan the complete legacy repository. Read only the current project state, this brief, relevant accepted ADRs, the Stage 3 report/review, relevant source/tests, and named provenance material.

## 3. Architecture boundary

```text
Application services (later)
            |
            v
Device/Profile Services + optional Protocol Modules
            |
            v
Generic Stage 3 Transfer / SysEx / MIDI Core
            |
            v
IMidiTransport
      /           \
WMS Direct     WinMM Native
```

Non-negotiable rules:

- no synth/model/manufacturer branch in WMS or WinMM;
- no profile logic inside `IMidiTransport`;
- no second MIDI/SysEx parser, serializer, capture engine, or transfer engine;
- profile interpretation may annotate or constrain operations but must not mutate raw bytes;
- Generic MIDI/SysEx behavior works without a matched profile;
- backend/route identity remains independent from device/profile identity;
- an interface/port name does not prove which DIN-connected synthesizer is attached;
- unknown data remains unknown rather than guessed.

Any required change to these boundaries needs an ADR and architecture review before implementation continues.

## 4. In scope

### 4.1 Versioned data-driven profile schema

Define a strict, documented, versioned profile format. JSON is preferred unless the existing repository already contains a stronger accepted choice.

The schema must support, where applicable:

- stable profile ID;
- schema version and profile version;
- manufacturer, model, variant, aliases and user-facing name;
- profile capability/support levels;
- safe descriptive metadata for channels, CC, RPN/NRPN, bank/slot organization, warnings and generic pacing defaults;
- optional recognition evidence such as Universal Identity fields or known SysEx fingerprints;
- source/provenance/license metadata;
- optional protocol-module identifier without requiring a compiled module;
- explicit profile-specific defaults distinct from global defaults.

Invalid types, invalid ranges, missing required fields, unsupported schema versions, duplicate IDs, contradictory capabilities and malformed fingerprints must fail visibly. Do not silently coerce or ignore safety-relevant invalid data.

### 4.2 Generic profile

Provide a Generic profile that:

- requires no manufacturer/model claim;
- adds no invented symbolic interpretation;
- preserves all generic Stage 3 monitor, capture, load/save and raw-transfer behavior;
- never advertises validated restore;
- remains available when no real profile matches, matching is ambiguous, or a profile fails validation.

Generic fallback is a normal supported result, not an error.

### 4.3 Profile registry

Implement a deterministic registry that can:

- load built-in/repository profiles from an explicit location;
- validate them before registration;
- reject duplicate stable IDs and conflicting versions deterministically;
- enumerate profiles and capabilities;
- resolve an explicitly selected profile;
- return visible outcomes for unavailable or invalid profiles;
- remain testable without Qt or MIDI hardware.

Do not add uncontrolled plugin loading, remote downloads, executable scripts, or arbitrary shared-library discovery.

### 4.4 Matching and binding result model

Represent profile selection explicitly. At minimum distinguish:

```text
Explicit
ConfidentSuggestion
Ambiguous
NoMatch
Invalid
GenericFallback
```

Selection policy follows this confidence order:

1. saved explicit user binding;
2. clear device-native identity evidence;
3. Universal MIDI Identity evidence;
4. known SysEx fingerprint;
5. manual selection;
6. Generic fallback.

Requirements:

- ambiguity never becomes “take the first profile”;
- a port/display name alone must not identify a synth behind a generic DIN interface;
- WMS and WinMM route identities are not translated into device identities;
- matching must expose its evidence and confidence;
- profile selection must not silently change the active transport route;
- profile disappearance or invalidation must not redirect output.

Stage 4 may produce a suggestion result. It does not implement the Stage 5 selection GUI.

### 4.5 One real Kawai K5000S data profile

Create exactly one bounded real profile for the Kawai K5000S, limited to facts supported by approved sources/fixtures.

Permitted examples, only when evidenced:

- manufacturer/model/variant identity;
- aliases;
- channel/controller/NRPN names;
- descriptive bank/slot metadata;
- conservative generic SysEx pacing default;
- warnings and known limitations;
- recognition fingerprint or Universal Identity values;
- capability declarations.

The profile must distinguish the support ladder:

```text
Detect -> Read -> Inspect -> Extract -> Modify -> Serialize -> Transfer -> Validated Restore
```

Do not infer a higher support level from a lower one. In particular, parsing or recognizing a message does not authorize rewrite, checksum regeneration, transfer ordering, or validated restore.

### 4.6 One strong real-device fixture path

Use one provenance-approved K5000S fixture path to prove known-device behavior. Prefer a byte-exact, read-only identity or SysEx sample with a documented origin and hash.

Requirements:

- original bytes remain immutable;
- fixture origin, ownership/license status, acquisition method and hash are recorded;
- expected recognition facts are explicit and independently testable;
- malformed/truncated/tampered derivatives are generated in tests rather than stored as unexplained “real” captures;
- the fixture is never sent automatically or used to claim hardware validation;
- a tainted Stage 3 capture cannot become an accepted known-good fixture.

If no qualifying fixture/source exists, Stop/Ask. Do not copy an unreviewed legacy file, use one of the known incomplete root dumps, invent bytes, or infer a checksum.

### 4.7 Optional compiled protocol hook

A small interface/registration hook may be defined only if required to prove that future compiled protocols can live above the generic engine.

Do not implement a K5000 protocol state machine unless data-only implementation becomes demonstrably impossible and the User approves the scope change. A compiled hook does not justify handshakes, checksum logic, codecs, restore flows, or device I/O in this stage.

## 5. Mandatory Stage 3 carry-forward

### 5.1 R-003 and ordered data-loss integrity

Preserve the complete safety chain established by the Stage 3 correction:

```text
transport data loss
    -> ordered DataLoss signal
    -> affected capture/parser is tainted
    -> no verified-complete result
    -> no save_syx_frames / encode_sysex7 acceptance
```

Profile recognition or interpretation must never clear, hide, replace or downgrade the tainted/malformed state. Add a Stage 4 regression proving that a frame matching the real profile still remains rejected when tainted.

R-003 remains **Mitigated in software / targeted re-review pending** until the mandatory Stage 4 architecture review accepts the integrated path.

### 5.2 WMS resource-growth regression contract

Retain the evidence-based WMS lifecycle criterion introduced during Stage 3:

- measure resource growth/trend, not mere process-wide handle scatter;
- do not restore the obsolete `steady_span <= 1` assumption;
- do not raise a tolerance until a run passes;
- do not mask behavior with sleeps or a shifted observation window;
- preserve the historical red `steady_span=2` evidence and rationale in the Stage 3 report.

Stage 4 changes must not regress the existing local WMS/WinMM lifecycle suites.

## 6. Out of scope

- Qt GUI, widgets, model/view integration or product shell;
- physical MIDI hardware validation or automated sends to a synth;
- driver, registry, service, API-mode or global system changes;
- a universal profile database or multiple real device profiles;
- K5000 KA1/KAA/IMG parsing, editing, conversion or librarian operations;
- K5000 checksum reverse engineering or checksum regeneration;
- device-specific handshake, request/dump state machine or validated restore;
- automatic background identity requests;
- profile download/update service;
- legacy source/profile/fixture bulk migration;
- fuzzy cross-backend route translation;
- Stage 5 GUI work.

## 7. Deliverables

- profile domain types and capability/support-level model;
- versioned profile schema and validation implementation;
- Generic profile;
- deterministic profile registry and match/binding result model;
- exactly one bounded Kawai K5000S data profile;
- one provenance-approved real-device fixture path plus metadata/hash;
- pure unit/integration tests and CI registration;
- any evidence-required ADR;
- updated architecture/provenance/risk documentation;
- `docs/stages/STAGE_4_REPORT.md`;
- updated `docs/status/PROJECT_STATE.md`.

## 8. Acceptance tests / evidence

### Schema and registry

- valid Generic and K5000S profiles load;
- unsupported schema version fails visibly;
- required-field, type, range and contradictory-capability errors fail;
- duplicate IDs/conflicting versions are deterministic errors;
- registry order does not change resolution;
- invalid real profile cannot displace Generic fallback.

### Matching and isolation

- explicit binding resolves exactly;
- known fixture produces the documented match/suggestion evidence;
- unknown fixture/data yields NoMatch/GenericFallback without guessing;
- two equally plausible profiles yield Ambiguous, never first-match selection;
- a generic DIN-interface name alone never identifies the K5000S;
- backend or route changes do not silently rebind the profile;
- profile matching does not mutate MIDI 1.0 bytes or UMP words;
- no K5000/model/manufacturer branch exists in WMS, WinMM, `IMidiTransport`, SysEx parser or transfer engine.

### Capability safety

- support levels are explicit and monotonic claims are not inferred;
- K5000S profile does not advertise unsupported Modify/Serialize/Validated Restore capabilities;
- data-only pacing/warnings are applied above transport;
- Generic behavior remains byte-identical with no profile selected;
- malformed/incomplete/tainted known-device data remains malformed/incomplete/tainted and cannot be saved or encoded as verified complete.

### Regression and build evidence

- all ordinary CI tests pass;
- clean configure/build/test from a separate directory passes;
- all applicable Stage 2/3 tests pass unchanged;
- opt-in local WMS/WinMM lifecycle/realtime tests pass under their approved contracts;
- no physical hardware route is opened or sent to;
- hosted CI does not claim real WMS/hardware coverage;
- `git diff --check` passes;
- final worktree and pushed revision are reported accurately.

## 9. Gate criteria

PASS requires:

1. Generic behavior works fully without a device profile.
2. One real data-driven profile and one approved fixture path prove known-device interpretation.
3. Matching, ambiguity and fallback are deterministic and visible.
4. No device-specific branch exists below the profile/protocol boundary.
5. Raw bytes and taint state remain intact through profile processing.
6. Capability metadata does not overclaim rewrite/restore safety.
7. Provenance is complete.
8. All applicable tests/builds pass.
9. Mandatory Claude Code architecture review finds no unresolved P0/P1.

HOLD if:

- a real fixture lacks provenance;
- device identity could be inferred from an ambiguous DIN/interface name;
- profile logic contaminates WMS/WinMM or duplicates the generic engine;
- malformed/tainted data can be upgraded to valid;
- capability claims exceed evidence;
- a compiled protocol becomes necessary without an approved scope/ADR;
- an unresolved P0/P1 or architecture deviation remains.

## 10. Stop / Ask

Apply `QUALITY_POLICY.md`. Stop and escalate especially when:

- no provenance-approved K5000S fixture/source is available;
- implementing the real profile would require unknown checksum or reverse engineering;
- an existing legacy profile/file would need to be copied without per-item provenance;
- endpoint/profile ambiguity could target the wrong physical device;
- a device-specific requirement appears to require transport changes;
- two materially different fixes fail for the same blocker;
- byte integrity or taint propagation becomes uncertain;
- a driver/system change or hardware-only fact is required.

## 11. Model and effort

Default implementation:

```text
Codex GPT-5.6 Terra – high
```

Escalate only a bounded compiled-protocol, byte-integrity or architecture blocker to Sol high. Do not use Sol Ultra or max for the whole stage.

Mandatory review:

```text
Claude Code Opus – high
```

## 12. Review handoff

Claude Code receives only:

- this brief and `STAGE_4_REPORT.md`;
- Stage 3 report plus targeted re-review outcome;
- changed ADRs and architecture/provenance/risk documents;
- profile schema/registry/matching source;
- Generic and K5000S profile data;
- fixture provenance/hash and relevant tests;
- Stage 4 diff and exact build/test evidence.

Review focuses on:

- device/profile isolation;
- deterministic matching, ambiguity and Generic fallback;
- route identity remaining separate from device identity;
- capability overclaiming;
- fixture provenance;
- preservation of bytes and Stage 3 taint state;
- absence of device branches in transport/core;
- justification for any compiled protocol hook;
- Stage 5 readiness.

Codex verifies every finding before fixing it. No Stage 5 work begins until the Stage 4 gate is closed.
