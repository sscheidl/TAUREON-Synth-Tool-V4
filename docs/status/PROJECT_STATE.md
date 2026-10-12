# TAUREON V4 – Project State

**Last updated:** 2026-10-12 (status reconciliation by Codex at Product Owner request)
**Maintained by:** ChatGPT Classic / Project Manager

**Verified `main` baseline (2026-10-12, before this documentation update):**
`69a8a6ce1a9684514818d3066a1b86f282285fb9`, the 2026-10-11 merge of
[PR #25](https://github.com/sscheidl/TAUREON-Synth-Tool-V4/pull/25). Its exact PR
HEAD `fa0eb68059f9e28557eea54ecb094c6528db6313` has the same tracked tree.
Windows CI [run 38137459793](https://github.com/sscheidl/TAUREON-Synth-Tool-V4/actions/runs/38137459793)
was inspected including job steps and logs: whitespace and full software build
passed, software CTest passed 24/24, and engine/consumer CTest passed 9/9, 9/9,
2/2 and 2/2. The merge deliberately used `[skip ci]`; no separate run on the merge
SHA is claimed. Hardware-build steps and the release job were skipped in this PR
run. No PR or issue was open at the start of this reconciliation.

**Merged alpha follow-ups:** PR #17 reconciled earlier status; PRs #18–#20 fixed
port selection/retry and Monitor following, migrated WMS to Preview 9, added
Monitor filters, Auto fallback, manual Protein CC labels and SysEx UI refinements.
PR #20 also hides SysEx Manager navigation while retaining its implementation.
PR #21 prepared alpha.2; PR #22 added tagged pre-release publishing. PR #23 now
pins the WMS client to In-box Preview 10 (`Windows.Devices.Midi2`
`0.99.88-preview.10`) and falls back to the installed Tools API. PR #24 adds the
read-only Diagnostics engine check; PR #25 sanitizes its exported results.
These changes do not establish physical MIDI/SysEx acceptance.

**Latest published alpha:**
[`v0.1.0-alpha.2`](https://github.com/sscheidl/TAUREON-Synth-Tool-V4/releases/tag/v0.1.0-alpha.2),
published 2026-10-10 from `530d7b97d4461cb70590bd216283b56b9fefcab5`, with a
Windows x64 ZIP and SHA-256 sums. The tag's Windows CI
[run 38085408474](https://github.com/sscheidl/TAUREON-Synth-Tool-V4/actions/runs/38085408474)
logs show 24/24 software tests, 9/9, 9/9, 2/2 and 2/2 engine/consumer tests,
and release publication. This release predates PRs #23–#25: Preview-10/Tools
fallback and the new Diagnostics check/export correction are unreleased.

**First public alpha (2026-10-02):** PR #16 set the application version to
`0.1.0-alpha.1`. The Product Owner published the GitHub pre-release
[`v0.1.0-alpha.1`](https://github.com/sscheidl/TAUREON-Synth-Tool-V4/releases/tag/v0.1.0-alpha.1)
from `bab37a5` as a portable, unsigned Windows x64 ZIP with a SHA-256 sums file. It is
an experimental preview: product-path MIDI/SysEx transfer and device restore are not
validated, and it is **not** a Stage-5 gate (D-026).

**Engine package:** PR #14 (merged 2026-09-27) and PR #15 (merged 2026-09-28) separate the generic
MIDI/SysEx engine into an internal CMake package, currently version 0.3.0, with
`include/taureon` public headers, separately reusable Core/WinMM/WMS targets, and a
WinMM callback context that remains safe when native close fails. It is for the
owner's own TAUREON projects, not a public SDK. The repository is public since
2026-09-27 and MIT-licensed (D-024).

**Provenance note:** the former real Summit SysEx fixture is absent from every remote
branch tip since the Product Owner deleted `claude/reference-archive-v1` and
`claude/stage5-gate-cleanup` on 2026-10-06 (local bundle backups exist). It remains in
public Git history, which is not rewritten (D-027); see `docs/reference/PROVENANCE.md`.

## Current stage

```text
Stage 0 – Bootstrap, inventory, provenance, architecture verification
Status: PASS / CLOSED
Gate: PASS

Stage 1 – Native transport spikes (WMS + WinMM)
Status: PASS / CLOSED
Gate: PASS WITH NON-BLOCKING FOLLOW-UPS (mandatory Claude Code review; no P0/P1 findings)

Stage 2 – MIDI Core + transport abstraction
Status: PASS / CLOSED after targeted-review P1 remediation and User gate
Implementation lead: Codex

Stage 3 – Realtime MIDI + SysEx engine
Status: PASS / CLOSED after targeted HOLD remediation and User/Project Manager gate
Gate: PASS WITH NON-BLOCKING FOLLOW-UPS (targeted Claude Code re-review; no P0/P1 findings)
Implementation lead: Codex

Stage 4 – Device/profile isolation proof
Status: PASS / CLOSED at revision 986115d
Gate: PASS WITH NON-BLOCKING FOLLOW-UP (mandatory Claude Code review; FU-3 is a Stage-5 brief input)
Implementation lead: Codex
Target: Generic profile plus exactly one Novation Summit data profile

Stage 5 – Qt 6 Product GUI
Status: HOLD/ACTIVE — Block A merged; B-3 closed (PR #12 threads resolved and merged 2026-09-28); B-5 closed
Gate: HOLD — formal B-4 disposition and product-path hardware evidence remain
Implementation lead: Codex
```

## Stage 5 readiness — Block A and local Block B

Slice 9a was merged at `6b6b4d9191e592826afa1bfa2ea7f0ad5703e562`. Block A was merged at `bd4dd4f26bf19c0bd25ef2f2136e70cb470365b8` from reviewed PR #7 head `a4627d9113fa9191423171616e56b0a267fee763`. Windows CI #220 passed at the merge head with a full Debug build and 24/24 CTest, 0 failed, 0 skipped. The targeted F-1 through F-5 follow-up was merged at `1ede95b8a7c3d8738999bc2f563aec23703968fe` from PR #8 head `cdd72d9a7b9a5fbef2ada50a4d2c16f962cc7e37`; Windows CI #226 passed at that exact head with whitespace PASS, full Debug build PASS, 24/24 CTest PASS, 0 failed, and 0 skipped. F-6 remains a structural retained-evidence mapping with no action.

PR #9 was merged at `0ea82e526e89fddb2d6e8ae10a24b481c0937813`. Windows CI #236 passed at its exact head `434727d729097f844b42df7746e8d7b4b71b4d87` with whitespace PASS, Debug build PASS, capture/artifact upload PASS, and 24/24 CTest PASS, 0 failed, 0 skipped. The `stage5-gui-preview-1920x1080` artifact contains one capture of each actual workspace; the hosted offscreen raster was 2164×1080 at DPR 1.0. It is **VISUAL PREVIEW ONLY**, not Windows desktop, High-DPI, or Block-B evidence.

At the earlier Stage-5 gate, enforced branch protection was not available for
the then-private repository on the GitHub Free plan, so no protection rule or
ruleset was enforced at that time. The repository is now public; the current
branch-protection configuration has not been re-audited in this PR.
The standing control at that gate was manual pre-merge verification,
applied to every Stage-5 pull request: the exact PR
HEAD, a green Windows CI run identified by that `head_sha` rather
than by a badge, and a conflict-free base. The PR whitespace check
remains advisory. This is a plan limitation that has been assessed
and accepted, not an unaddressed gap.

Stage 5 cloud/software work is closed at
`d5f3a0bad140032fb6ef35c3cf5e1bb38c31d824`. On 2026-09-09, local Block-B work was
continued on the Product Owner's Windows machine. PR #12 subsequently received three
P1 findings against the B-3 evidence. Their six-file remediation was initially present only in the
dirty worktree at branch HEAD `7dc09c9bd241cb773b25b41402d1aea892926d3f` and passed
fresh local verification on 2026-09-26. Independent targeted review accepted all three
code corrections. Commit `7af5dcc0df6878b9bf9cb7263770126a7f5ecc8f` then passed
both push and pull-request Windows CI plus a retained new 100-cycle soak per backend.
The review's technical closure conditions are complete. After a main-sync re-verification
(`docs/evidence/pr12-main-sync-20260928`), the Product Owner resolved all three PR-#12
review threads and merged PR #12 on 2026-09-28 as `a932287` (code remediation `693fd52`; verified
main-sync merge `2827560`; final head `5a0d795` only sanitizes evidence).
B-5's minimum-size defect is corrected. B-4 passes at native 125%; process-local 150%
and 200% simulations pass but do not replace native OS display-scaling inspection.
Stage 5 remains HOLD/ACTIVE and Stage 6 has not begun.

Block A closes deterministic cloud/software evidence: N-1, N-2, N-3, N-6, S7-3, S7-4, the fake close-while-in-flight proof, large SysEx malformed/incomplete/tainted rejection, and Stage-5-specific large cancellation. L-3 is unchanged: 256 MiB per document and 512 MiB aggregate raw workspace payload are resource limits, not MIDI/SysEx protocol limits and not total-process-memory bounds. Detailed traceability is in [`STAGE_5_REPORT.md`](../stages/STAGE_5_REPORT.md).

Block-B disposition on the Product Owner's Windows machine:

- B-3 **CLOSED** (formal PR-#12 thread resolution completed 2026-09-28; main-sync
  re-verification passed 24/24 non-local CTest, two 5-cycle runs, and one gated
  100-cycle soak per backend): the regular product-host gate runs
  five cycles per backend, proves real receive activity before receive-active close,
  requires every native close to succeed with final state `closed`, observes GUI
  `MAINSTA` and WMS-worker MTA, and runs WMS and WinMM in separate processes. The short
  run records but does not gate process-handle growth. A retained 100-cycle soak on
  immutable code SHA `7af5dcc0df6878b9bf9cb7263770126a7f5ecc8f` applied the gate
  for both backends and passed with negative slopes, no sustained growth, zero drops and
  zero late callbacks. Both Windows-CI runs on the same SHA are green.
- B-4 **PARTIAL:** all seven 1920x1080 workspaces pass native Windows 125% inspection.
  Effective 150% and 200% Qt simulations also fit and remain usable, but native Windows
  inspection must be distinguished from those simulations. The later
  [alpha feedback](ALPHA_FEEDBACK_20261006.md#display-scaling-disposition) records
  the Product Owner's completed native 150% impression test and deferral of native
  200% for the alpha. This is not a recorded full Stage-5 B-4 closure; no native
  200% pass or Stage-5 gate is claimed.
- B-5 **CLOSED:** the measured minimum changed from 1170x903 to 758x419 logical pixels;
  captures at DPR 1.25, 1.5, and 2.0 are now exactly 1920x1080.

Fresh 2026-09-26 verification built the dirty snapshot, passed 28/28 non-local tests,
and passed the corrected five-cycle product-host test in 34.65 seconds. WMS recorded
receive callbacks/delivery 1/1, WinMM 3/1, both with one transmit, zero drops, zero late
callbacks, successful closes and final `closed`; GUI apartment was `main_sta` and the
WMS worker MTA assertion passed. The other five long local regressions were not rerun,
so no new 6/6 claim is made. Temporary loopback endpoints were removed and confirmed
absent. Detailed current evidence is in
[`docs/evidence/reconstruction-20260926`](../evidence/reconstruction-20260926/STATUSBERICHT.md);
the historical Block-B measurements remain in
[`docs/evidence/stage5-block-b-20260909`](../evidence/stage5-block-b-20260909/README.md).

## Project location

Confirmed local project:

```text
D:\Eigene Dateien\Eigene Dokumente\Playground\TAUREON-Synth-Tool-V4
```

GitHub repository:

```text
https://github.com/sscheidl/TAUREON-Synth-Tool-V4 (public as of 2026-09-27; `main` includes the dedicated Stage-3 completion commit recorded in Git history)
```

Legacy local reference:

```text
D:\Eigene Dateien\Eigene Dokumente\Playground\TAUREON-Synth-Tool
```

Legacy GitHub:

```text
https://github.com/sscheidl/TAUREON-Synth-Tool
```

## Current architecture baseline

- clean native C++20 rebuild;
- Qt 6 Widgets;
- CMake;
- Windows 11 x64 primary target;
- direct Windows MIDI Services backend;
- native WinMM compatibility backend;
- one active backend per logical connection;
- device-specific logic above transport;
- generic SysEx byte integrity is a core invariant.

Stage 2 production implementation established:

- backend-specific, versioned persisted route identity with exact/missing/ambiguous/invalid resolution;
- no WinMM index-only persistence, fuzzy fallback, or WMS↔WinMM translation;
- a Qt-independent `IMidiTransport`, error/result model, lifecycle states, native message representation, and
  fake transport;
- WMS worker/MTA ownership with deterministic session/SDK/apartment teardown;
- WinMM callback→bounded queue→worker requeue and deterministic `MIDIHDR` owners;
- transport-level WinMM submit-failure coverage proving unprepare-before-close on partial-open unwind;
- ordinary CI unit tests plus separately opt-in local WMS/WinMM regressions.

Stage 0 recorded the current WMS C++/WinRT namespace, documented initialization/API-mode route, endpoint/group identity guidance, and SysEx7 helper in `docs/reference/ENVIRONMENT_REPORT.md`.

Stage 1 implementation established evidence for:

- exact pinned WMS package/runtime deployment pairing, and acquisition of any build-time SDK input (none present locally);
- unpackaged desktop initialization and API-mode observation entry points;
- immediate-send `MidiClock` metadata; timestamp normalization remains an explicit future decision;
- actual SysEx7 callback sequence and byte-exact UMP payload handling, but not MIDI 1.0 `F0`/`F7` framing
  conversion;
- backend-specific WMS and WinMM identity behavior;
- 100 WinMM open/close lifecycles, including one complete short/SysEx send/receive/`MOM_DONE` cycle;
- Qt Core/QCoreApplication plus dedicated WMS MTA-worker interaction, not the production QApplication host.

## Governance

- User: Product Owner, Hardware Tester, final decision authority.
- ChatGPT Classic: Project Manager / Supervisor / Context Keeper.
- Codex: Implementation Lead.
- Claude Code: Architecture & Review Lead.

## Documentation status

Stage 0 local evidence is now available:

- README
- product vision
- architecture baseline
- development process
- AI collaboration
- quality policy
- GUI design specification
- clickable GUI mockup
- C++20/CMake bootstrap and initial Windows CI workflow
- environment/toolchain report
- legacy inventory
- provenance record
- Stage 0 report

Earlier TAUREON2 planning documents are reference sources, not active specifications.

## Current blockers

No open review blocker remains. The three PR-#12 P1 threads (measurable receive-active
shutdown, successful close/final `closed`, GUI/WMS apartment observation) were resolved
on 2026-09-28 after immutable-SHA CI, the retained 100-cycle soak on
`7af5dcc0df6878b9bf9cb7263770126a7f5ecc8f`, and the main-sync re-verification; PR #12
is merged. There is no currently reproduced runtime failure.

The Stage-5 gate still requires Product-Owner acceptance that automated or hosted
runs cannot supply: a formal B-4 disposition and product-path physical MIDI/SysEx
evidence. Native 150% impression testing is recorded; native 200% was deferred
for the alpha, not passed. The alpha disposition does not close Stage 5.

No known Stage-4 implementation P0/P1 exists. Stage 4 is PASS/CLOSED at revision `986115d` after the mandatory
Claude Code architecture review, with FU-3 carried forward as a non-blocking Stage-5 GUI/evidence refinement.
No Stage-3 blocker or unresolved P0/P1 remains. The historical review P1 ordered-loss gap and both P2 findings
were corrected without an architecture deviation and accepted by targeted Claude Code re-review. The authorized
WMS investigation recorded complete 100- and 500-cycle handle series and confirmed reversible whole-process
runtime plateaus rather than cumulative ownership growth. The retained harnesses now test directional sustained
growth, not min/max dispersion; synthetic fast/slow leaks fail and all final local and clean-CI suites pass. The
Stage-2 partial-open P1 remains closed by its transport-level regression. Stage-3
ordered-loss, exact conversion, grouped decode-failure, `.syx` I/O-diagnostic, large-data, clean-CI, and local
WMS/WinMM evidence are recorded in [`STAGE_3_REPORT.md`](../stages/STAGE_3_REPORT.md).

Historical RC4 limitations remain in [`STAGE_1_REPORT.md`](../stages/STAGE_1_REPORT.md).
The current client uses Preview 10 as specified in
[`WMS_DEPENDENCIES.md`](../reference/WMS_DEPENDENCIES.md); historical SDK observations
must not be presented as its current runtime status. Route identity remains
backend-specific, without WinMM/WMS correlation.

## Mandatory later-stage inputs

- **Stage 2 PASS / CLOSED:** Backend-specific persistence/resolution, WinMM worker
  requeue/RAII, WMS MTA lifetime, P1 closure regression, and opt-in local regressions are implemented and
  recorded in [`STAGE_2_REPORT.md`](../stages/STAGE_2_REPORT.md).
- **Stage 3 PASS / CLOSED:** Claude Code returned PASS WITH NON-BLOCKING FOLLOW-UPS with no P0/P1 findings;
  the two reviewer follow-ups are mandatory Stage-4-brief inputs and R-003 remains mitigated, not closed.
- **Stage-4 FU-1 implemented:** WinMM overflow markers coalesce only across consecutive dropped callbacks;
  non-droppable long-header events break the run so later drops receive a new ordered marker. The blocked-worker,
  sustained-overflow regression proves two affected frames are tainted and a later clean frame is not.
- **Stage-4 FU-2 implemented:** `new_steady_high=false` when `start == 0`, because no independent warm-up
  envelope exists; the short rising-series regression remains non-applicable rather than self-referential.
- **Stage 4 PASS / CLOSED:** strict profile schema/loader, deterministic registry/matching, Generic fallback, one
  bounded Summit profile, and the User-approved read-only fixture were accepted. FU-3 (P3) requires the future
  Stage-5 GUI to disclose a manual selection overridden by stronger fingerprint evidence and offer saved-binding
  promotion; the matching order itself remains unchanged.
- **Stage 5:** B-3 is closed: targeted review accepted the code remediation for all three
  PR-#12 P1 findings, its immutable-SHA CI and retained 100-cycle-soak conditions pass,
  and the threads were formally resolved on 2026-09-28. Formal B-4 disposition
  and product-path physical-device evidence remain; the alpha's native 150%
  impression and deferred 200% scope are recorded separately.

## Stage 0 evidence correction (2026-08-24)

The recorded WMS integration route was re-derived from the installed `Microsoft.Windows.Devices.Midi2.winmd`.
The earlier entry named a wrong namespace root and three identifiers that do not exist in the SDK. The
Stage 0 report's review record was also corrected: the reviewer's verdict was HOLD with two P1 findings, not
the "PASS WITH NON-BLOCKING FOLLOW-UPS" that had been entered. Stage 0 closure per D-012 stands; only the
evidence and the review record changed. Details in [`STAGE_0_REPORT.md`](../stages/STAGE_0_REPORT.md).

## Exact next action

Keep Stage 5 at **HOLD/ACTIVE**. Reconcile formal B-4 acceptance with the Product
Owner's recorded native 150% impression and alpha-only 200% deferral; do not
represent the deferral as a pass. Handle alpha.2 feedback through issues and
focused pull requests. Preview-10 support and PRs #18–#25 are already merged;
there is no remaining stacked-PR merge or SDK re-pin task. Any product-path
physical MIDI/SysEx work still requires its own safe, explicit scope. A later
release is needed to distribute PRs #23–#25. Do not begin Stage 6.

## Hardware validation

Temporary WMS/WinMM loopback validation is complete. On 2026-09-26 the Product Owner
generated real channel-MIDI input while the Microsoft WMS console passively monitored
three exact USB endpoints: Summit (905 messages), KONTROL S61 MK3 Main (232), and
MiniFreak (789). No output route or request was used. This proves the Windows WMS
receive substrate for those endpoints only; it is not V4 product-path, SysEx, WinMM,
losslessness, latency, or Stage-5 hardware acceptance evidence. Two coordinated Summit
WinMM helper runs received zero bytes and remain a bounded compatibility-path question.

Previous hardware evidence belongs to the legacy/reference inventory until explicitly migrated as fixture/evidence.
