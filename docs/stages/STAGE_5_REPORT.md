# Stage 5 Report – Qt 6 Product GUI

**Stage:** 5  
**Status:** ACTIVE — Slice 9a ready for targeted review  
**Closure revision:** pending independent review; no Stage-5 closure is claimed.

## Consolidated implementation record

Stage 5 delivers the Qt 6 Widgets production shell and seven bounded workspaces. Qt presentation depends on Qt-independent application/core services; the accepted Stage-2–4 transport, route identity, parser, serializer, SysEx7, taint/DataLoss, profile-priority, and transfer-safety contracts remain unchanged.

Implemented Stage-5 modules include the application host/connection worker, bounded Monitor queue/model, SysEx Transfer/Manager, Devices & Profiles evidence UI, versioned Settings, bounded Diagnostics and payload-free Diagnostic Bundle, and the generic Librarian foundation. Generic and Novation Summit have no semantic Librarian provider: recognition of raw SysEx is not semantic preset decoding and never manufactures collection, bank, slot, or capability state.

The local test-only InMemory Librarian provider remains confined to `Stage5LibrarianTests.cpp`; it is not product-registered, selectable, or instantiated by `taureon_app`. Known provider capacity bounds rows; unknown capacity renders only observed slots. This is Librarian model/presentation boundedness, not the SysEx Manager L-3 policy.

## Stage-4 follow-ups consumed

FU-3 is implemented: profile matching preserves stronger evidence over a temporary manual choice, presents the override, and allows deliberate saved-binding promotion. Existing Stage-4 byte/taint and profile contracts are preserved.

## Slice 9a software evidence

### Large SysEx

Existing Stage-3 large-SysEx infrastructure and the Manager test exercise synthetic 64 KiB, 600 KiB, 900 KiB, and 1 MiB + 257 byte frames. Complete data preserves exact bytes/frame counts through parser and Manager import/export. Existing malformed/incomplete/tainted tests retain visible integrity state and reject verified-complete save/merge/encode paths. The Stage-3 transfer cancellation regression reaches the stable cancelled state after the first of sixteen 64 KiB messages.

The bounded presentation limits are measured/defined as: Monitor model 10,000 rows, Monitor queue 8,192 events, and GUI bridge batches of at most 512 events per update cycle. This is offscreen bounded-work evidence, not a wall-clock responsiveness claim for an unobserved machine.

### Model/view and lifetime

Automated Qt tests cover model indices, roles, headers, insertion/removal/reset, filtering/sorting where models expose them, keyboard and extended selection, and provider/bank switching. The Librarian now proves repeated refresh with the same provider does not accumulate table rows or selector entries. Its operation-availability fields are explicitly reserved semantic-provider metadata until a later slice; no unimplemented action is enabled.

Manager-to-Transfer queued handoff now also completes while the Manager panel is alive and then destroys that panel before queued delivery runs. MainWindow clears its synchronous Transfer snapshot observer before sibling-child teardown. Target-local `AUTOMOC` remains intentional because only the Qt targets need generated moc sources; core targets remain Qt-independent.

Diagnostics starts its four-Hz refresh timer only while visible and stops it on hide. Its UI test explicitly shows the panel and observes a refreshed status, so it does not pass merely because hidden-panel polling ceased.

### L-3 product resource policy

`SysExManager.hpp` defines product constants of **256 MiB per raw SysEx document** and **512 MiB aggregate loaded raw payload**. They are safety/resource limits, not MIDI/SysEx protocol limits and are not Settings-configurable. Aggregate accounting is the sum of workspace `raw_bytes`, not total process memory: parser structures, per-frame payload copies, and Qt model copies consume additional memory.

`add_file` checks filesystem size before reading, `add_document` checks before ownership, add operations reject before mutation, and removal returns payload budget. Export and merge project the output before assembling or creating an output/temp file. Rejections use the appended `MidiErrorCode::resource_limit_exceeded` and identify actual size, applicable limit, and limit type. The Manager summary line now states the configured limits. Tests prove pre-read document-limit rejection, aggregate rejection with earlier items preserved, budget release after removal, and normal loading/export above 1 MiB.

### Finding closures

- S8-4: availability fields documented as reserved semantic-provider metadata; no inactive semantic action was enabled.
- S8-5: malformed provider snapshot and selected-provider/no-bank paths now state their actual causes.
- S8-6: repeated same-provider refresh produces no accumulating rows/widgets.
- S8-7: corrected attribution — `4e40e25` changed the Access label; `0e154d5` added the S8-1–S8-3 evidence.
- S7-2: Diagnostics timer is visibility-coupled with a non-vacuous shown-panel test.
- S6-2: second queued-delivery/destroy ordering regression is covered.
- S6-3: synchronous observer is cleared during deterministic MainWindow teardown.
- S6-4: target-local AUTOMOC is retained to preserve Qt independence of core targets.

## Validation and limits

Windows CI runs during Slice 9a, including the complete Debug configure/build and registered CTest suite, passed on the current implementation history. The final exact run and test totals are recorded on PR #6 after the final documentation commit.

No actual `QApplication` WMS/WinMM apartment, close-active, or shutdown-lifetime evidence is claimed here (**B-3 remains outstanding for Slice 9b on the Product Owner's Windows hardware after 09.09.2026**). No 1920x1080 visual inspection at 125%, 150%, or 200% scaling is claimed (**the visual half of B-4 remains outstanding for Slice 9b**). The five hardware/loopback tests are **NOT REGISTERED — NOT SKIPPED — NOT SIMULATED**.

## Architecture/ADR impact and known issues

No new persistent database, device semantic codec, transport path, route identity behavior, or asynchronous provider was introduced. No ADR change is required. Stage 6 has not begun.

Open Stage-5 gate candidates are B-3 and the visual half of B-4 for Slice 9b. Hardware/real MIDI, WMS/WinMM runtime timing/lifetime, Windows visual quality, and High-DPI inspection remain untested. Later-slice candidates include complete Librarian semantic actions and provider implementations; they are not enabled by this foundation.

## Exact state

- Starting Slice-9a main: `2ca2c246f15ab9ee5be6e9b04fb38e102d361032`
- Branch: `codex/stage5-software-completion`
- Draft PR: [#6](https://github.com/sscheidl/TAUREON-Synth-Tool-V4/pull/6)
- This report is software completion evidence only. Stage 5 remains **ACTIVE**.
