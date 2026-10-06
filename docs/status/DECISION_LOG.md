# TAUREON V4 – Decision Log

**Maintained by:** ChatGPT Classic / Project Manager

This log records concise product/process decisions that are important but do not require a technical ADR.

| ID | Date | Decision | Authority | Consequence |
|---|---|---|---|---|
| D-001 | 2026-08-24 | Start V4 as a new project/repository instead of continuing the old TAUREON tree | User | Legacy project remains reference-only |
| D-002 | 2026-08-24 | Confirmed local path is `D:\Eigene Dateien\Eigene Dokumente\Playground\TAUREON-Synth-Tool-V4` | User | Stage 0 bootstraps here; documentation was corrected from the earlier `Tool4` spelling |
| D-003 | 2026-08-24 | Legacy TAUREON remains available as reference for facts, fixtures and lessons | User | No wholesale code migration |
| D-004 | 2026-08-24 | User is Product Owner, Hardware Tester and final decision authority | User | Hardware/release decisions stay with User |
| D-005 | 2026-08-24 | ChatGPT Classic is Project Manager/Supervisor/Context Keeper | User | PM coordinates stages, docs, risks and AI handoffs |
| D-006 | 2026-08-24 | Codex is default Implementation Lead | User | Routine implementation/build/test work goes to Codex |
| D-007 | 2026-08-24 | Claude Code is default Architecture & Review Lead | User | Risky architecture and mandatory reviews go to Claude Code |
| D-008 | 2026-08-24 | Product documentation is split by responsibility; old giant master documents are reference-only | Project Manager, accepted by User workflow | Avoid conflicting active instructions |
| D-009 | 2026-08-24 | Five product domains may map to seven GUI workspaces | Project Manager | SysEx Manager and Diagnostics can be separate workspaces without changing product-domain model |
| D-010 | 2026-08-24 | Production Qt GUI remains Stage 5; earlier HTML mockup is design-only | Project Manager | Core/transport evidence precedes real GUI wiring |
| D-011 | 2026-08-24 | Create the V4 GitHub repository as private `sscheidl/TAUREON-Synth-Tool-V4` | User | Remote `origin` is the private V4 repository; Stage 1 remains gated by the Stage 0 review |
| D-012 | 2026-08-24 | Close Stage 0 with PASS after mandatory Claude Code re-review | User | Stage 1 is planned only; its brief requires Project Manager/User approval before any transport work |
| D-013 | 2026-08-24 | Authorize Stage 1 start after correcting the Stage 0 WMS evidence and review record | User | Stage 1 is ACTIVE; Stage 0 closure per D-012 stands, corrected evidence supersedes the earlier route entry |
| D-014 | 2026-08-24 | Approve one temporary software-loopback endpoint for Stage 1, preferring WMS-native loopback facilities and allowing an installed third-party facility only as fallback | User / Project Manager | A uniquely named WMS-native pair may be created, verified through WinMM, tested, and removed without driver/system changes or physical MIDI traffic |
| D-015 | 2026-08-24 | Close Stage 1 after mandatory Claude Code review with PASS WITH NON-BLOCKING FOLLOW-UPS and no P0/P1 findings | User / Project Manager | Stage 1 evidence is accepted with mandatory Stage 2/3/5 inputs; Stage 2 remains planned and not started |
| D-016 | 2026-08-24 | Approve and start Stage 2 under `STAGE_2_BRIEF.md` | User | Codex implements only the MIDI Core/transport architecture; Stage 3 remains forbidden pending a later gate |
| D-017 | 2026-08-24 | Place Stage 2 on HOLD for the targeted Claude Code P1 and authorize narrowly scoped closure remediation | User / Project Manager | Fix WinMM partial-open header unwind, add the exact transport regression, preserve the accepted architecture, and keep Stage 3 unstarted |
| D-018 | 2026-08-24 | Close Stage 2 with PASS after the P1 closure commit was tested and pushed; authorize Stage 3 under its supplied brief | User | Stage 3 becomes ACTIVE; Stage 4 remains forbidden pending its later gate |
| D-019 | 2026-08-24 | Close Stage 3 after targeted Claude Code re-review with PASS WITH NON-BLOCKING FOLLOW-UPS and no P0/P1 findings | User / Project Manager | Historical HOLD and WMS handle-test evidence remain; two reviewer follow-ups become mandatory Stage-4-brief inputs; Stage 4 remains unauthorized |
| D-020 | 2026-08-24 | Approve `STAGE_4_BRIEF.md` and authorize Stage 4 implementation | User | Codex remains Implementation Lead; Stage 5 remains forbidden |
| D-021 | 2026-08-24 | Replace the unavailable K5000S proof fixture/profile target with exactly one Novation Summit profile and approve `Crazy Sine.syx` from the User's Summit for private read-only tests | User | Historical fixture permission; superseded for current tests by D-025. No publication, conversion, repair, checksum work or hardware transmission was authorized. |
| D-022 | 2026-09-12 | Shorten the ordinary Stage-5 product-host lifecycle regression from 20 to 5 cycles per backend and reserve the directional process-handle-growth gate for an explicit 100-cycle soak | User | The short gate must still prove active receive/send shutdown, successful close, final state and apartment evidence; it cannot claim long-term leak stability |
| D-023 | 2026-09-26 | Authorize coordinated passive receive-only diagnostics on explicitly identified USB MIDI endpoints, with no host-to-device MIDI/SysEx traffic | User | WMS substrate evidence may be recorded separately from V4 product acceptance; physical sends, dumps, restore, presets and firmware remain unauthorized |
| D-024 | 2026-09-27 | License the current repository source under MIT with `TAUREON` as the chosen copyright designation | User | Add root `LICENSE`; audit file provenance and preserve separate terms of external dependencies and historical restricted material |
| D-025 | 2026-09-27 | Remove the real Summit `.syx` and its provenance JSON from the current tree; generate artificial test bytes only in the build directory | User | No `.syx` is tracked at PR #14 HEAD; historical commits and other public branch tips require separate remediation |
| D-026 | 2026-10-02 | Publish `v0.1.0-alpha.1` from `bab37a5` as the first public, experimental Windows x64 pre-release before the Stage-5 gate | User | The alpha is explicitly marked as not validated for product-path MIDI/SysEx transfer or restore; it is not a Stage-5 gate, and Stage 5 stays HOLD/ACTIVE |

Technical architecture changes belong in ADRs, not this log.
