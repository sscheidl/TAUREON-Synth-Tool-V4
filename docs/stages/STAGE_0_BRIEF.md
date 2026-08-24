# Stage 0 Brief – Bootstrap, Inventory, Provenance & Architecture Verification

**Stage:** 0  
**Status:** READY FOR EXECUTION  
**Implementation/inspection lead:** Codex  
**Coordination:** ChatGPT Classic / Project Manager  
**Mandatory review:** Claude Code  
**Decision authority:** User where required

## Goal

Establish a clean V4 repository, prove the development environment, inventory useful legacy evidence, and freeze enough architecture facts to start native transport spikes safely.

## In scope

### New project/repository

- create/verify local V4 directory:
  `D:\Eigene Dateien\Eigene Dokumente\Playground\TAUREON-Synth-Tool4-V4`
- initialize new Git repository;
- create/verify new GitHub repository;
- add baseline documentation from this package;
- create minimal C++20/CMake project skeleton;
- add sensible `.gitignore`;
- establish initial Windows CI if the verified dependency/toolchain setup supports a hosted runner cleanly;
- create lightweight GitHub issue/milestone tracking for Stage 0 where useful.

### Legacy/reference inventory

Read-only inspection of:

- `D:\Eigene Dateien\Eigene Dokumente\Playground\TAUREON-Synth-Tool`
- relevant `engine3_native` work;
- relevant side projects only when directly useful;
- known-good SysEx captures/fixtures;
- previous hardware-test records;
- MIDI/profile research.

Record:

- source location;
- what is useful;
- whether it is behavioral evidence, data fixture, or code;
- provenance/license;
- migration recommendation.

### Environment verification

Record actual:

- Windows build;
- MSVC version;
- CMake version;
- Windows SDK;
- Qt 6 installation/version;
- dependency manager state if relevant;
- Windows MIDI Services installation/runtime;
- current official WMS SDK/package/namespace/init guidance;
- current supported API-mode detection method;
- WinMM availability.

Do not change the environment merely to make verification easier.

### Architecture verification

Resolve or explicitly mark open:

- WMS runtime initialization;
- endpoint/group identity;
- persistent route identity;
- timestamp model;
- maximum transmission constraints;
- SysEx7 transfer/reassembly API/helper;
- WinMM identity model;
- Qt/WMS initialization coexistence;
- what legacy native work can be reused cleanly.

Create ADRs only for decisions supported by evidence.

## Out of scope

- production WMS transport;
- production WinMM transport;
- GUI implementation;
- device/profile implementation;
- real hardware sends;
- driver/registry/service changes;
- migration of the old application wholesale.

## Deliverables

- new repository/project skeleton;
- `docs/reference/LEGACY_INVENTORY.md`;
- `docs/reference/PROVENANCE.md`;
- environment/toolchain report;
- any evidence-backed ADRs;
- `docs/stages/STAGE_0_REPORT.md`;
- updated `docs/status/PROJECT_STATE.md`.

## Acceptance / gate criteria

PASS requires:

- new repo is separate and legacy work remains intact;
- trivial C++20 CMake build succeeds;
- Qt status is known;
- current official WMS integration route is identified with sources/evidence;
- WinMM availability is confirmed;
- no essential migration depends on unresolved licensing;
- major Stage 1 assumptions are either resolved or explicitly scoped for spikes.

HOLD if any of these fail materially.

## Stop / Ask

Use `QUALITY_POLICY.md`.

Especially stop if:

- existing work could be overwritten;
- required system changes would be needed;
- current WMS SDK route cannot be identified through documented mechanisms;
- essential code reuse has unclear licensing.

## Review handoff

Claude Code receives only:

- Stage 0 brief;
- Stage 0 report;
- new/changed ADRs;
- environment report;
- legacy/provenance summaries;
- relevant diffs.

Review focus:

- WMS assumptions;
- WinMM lifecycle plan;
- provenance;
- architecture boundaries;
- Stage 1 readiness.

Do not begin Stage 1 before the Stage 0 gate is resolved.
