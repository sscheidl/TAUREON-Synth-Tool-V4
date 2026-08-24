# TAUREON V4 – Risk Register

**Maintained by:** ChatGPT Classic / Project Manager  
**Review cadence:** at every stage gate and whenever a new P0/P1 issue appears

| ID | Risk | Probability | Impact | Current mitigation | Owner | Status |
|---|---|---:|---:|---|---|---|
| R-001 | Current WMS SDK/runtime integration differs from assumptions | Medium | High | Verify official current API in Stage 0; prove with Stage 1 spike | Claude Code / Codex | Open |
| R-002 | WinMM long-message teardown/lifetime defect | Medium | High | Isolated spike; explicit `MIDIHDR` lifecycle; stress open/close | Codex + Claude Code review | Open |
| R-003 | SysEx byte corruption or incomplete capture marked valid | Low/Medium | Critical | Byte-exact tests, overflow invalidation, large fixture corpus | Codex | Open |
| R-004 | Windows MIDI stack instability complicates testing | Medium | High | Never auto-modify system MIDI environment; diagnostics; software-first tests | Project Manager / User | Open |
| R-005 | Wrong route/device selected for send | Low/Medium | Critical | Stable identities, explicit route display, no fuzzy substitution, staged hardware tests | Architecture / GUI | Open |
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
