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

Technical architecture changes belong in ADRs, not this log.
