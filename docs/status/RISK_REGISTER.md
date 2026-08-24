# TAUREON V4 – Risk Register

**Maintained by:** ChatGPT Classic / Project Manager  
**Review cadence:** at every stage gate and whenever a new P0/P1 issue appears

| ID | Risk | Probability | Impact | Current mitigation | Owner | Status |
|---|---|---:|---:|---|---|---|
| R-001 | Current WMS SDK/runtime integration differs from assumptions | Low/Medium | High | Pinned RC4 SDK/runtime pairing and direct lifecycle proven; API-mode and correlation limitations explicitly excluded from architecture assumptions | Claude Code / Codex | Mitigated / non-blocking follow-up |
| R-002 | WinMM long-message teardown/lifetime defect | Medium | High | Production callback→bounded queue→worker requeue and deterministic `MIDIHDR` owners; targeted-review partial-open P1 fixed; exact transport-level submit-failure ordering plus header error tests; 100 production open/close cycles with zero drops/late callbacks; Stage 1 short/SysEx byte-integrity regression retained; vendor-driver validation remains | Codex + Claude Code review | Open / P1 remediated; hardware follow-up |
| R-003 | SysEx byte corruption or incomplete capture marked valid | Low/Medium | Critical | Stage 1 proved byte-exact UMP payload handling only; Stage 3 must prove `F0...F7` byte-stream <-> SysEx7 segmentation/reassembly plus broader fixtures | Codex | Open |
| R-004 | Windows MIDI stack instability complicates testing | Medium | High | WMS diagnostic and temporary native loopbacks; isolated RC4 correlation fail-fast; no physical send or system change | Project Manager / User | Active / bounded |
| R-005 | Wrong route/device selected for send | Low | Critical | Stage 2 schema and resolver enforce backend-specific composite identity, reject missing/ambiguous/invalid routes, omit WinMM indices, and prohibit fuzzy/cross-backend substitution; deliberate-selection GUI remains later work | Architecture / GUI | Mitigated in core / UI follow-up |
| R-006 | Scope creep into universal librarian delays reliable core | High | Medium/High | V4.0 baseline explicitly limits librarian depth; stage scope control | Project Manager / User | Open |
| R-007 | Legacy/third-party reuse has unclear provenance/license | Medium | High | Stage 0 provenance inventory; prefer clean reimplementation | Project Manager / Claude Code | Open |
| R-008 | GUI cannot keep up with Clock/Active Sense/event floods | Medium | Medium/High | Bounded model/view, batched updates, 100k-event stress | Codex | Open |
| R-009 | Specialist AI token/context budget exhausted during risky task | Medium | Medium | Project Manager prepares focused context and absorbs bounded support work | Project Manager | Open |
| R-010 | Documentation/source-of-truth drift | Low/Medium | High | Source-of-truth map, audit at gates, PM owns consistency | Project Manager | Open |

## Risk rules

- P0/P1 defects are not merely risks; they become active blockers/issues.
- A risk may be closed only with evidence.
- If probability/impact changes materially, update the row rather than appending a diary entry.
- User decisions about accepted residual risk are recorded in `DECISION_LOG.md`.
