# TAUREON V4 – Risk Register

**Maintained by:** ChatGPT Classic / Project Manager  
**Review cadence:** at every stage gate and whenever a new P0/P1 issue appears

| ID | Risk | Probability | Impact | Current mitigation | Owner | Status |
|---|---|---:|---:|---|---|---|
| R-001 | Current WMS SDK/runtime integration differs from assumptions | Low/Medium | High | Pinned RC4 SDK/runtime pairing and direct lifecycle proven; API-mode and correlation limitations explicitly excluded from architecture assumptions | Claude Code / Codex | Mitigated / non-blocking follow-up |
| R-002 | WinMM long-message teardown/lifetime defect | Low/Medium | High | Production callback→bounded queue→worker requeue and deterministic input/output `MIDIHDR` owners; partial-open P1 fixed; transport/header failure tests; 100 Stage-3 cycles include 10-KiB multi-buffer SysEx TX/RX with zero drops/late callbacks; vendor-driver validation remains | Codex + Claude Code review | Mitigated in software loopback / hardware follow-up |
| R-003 | SysEx byte corruption or incomplete capture marked valid | Low/Medium | Critical | Production WinMM/WMS streams carry non-droppable ordered loss markers into grouped capture/parsing; tests prove native loss → capture taint → no verified-complete/save, loss at close, no later false taint, group-local SysEx7 decode termination, and contextual `.syx` I/O errors; WMS 100/500-cycle handle series and synthetic fast/slow leak tests validate directional ownership stability; physical/vendor-driver behavior remains separate | Codex + Claude Code review | Mitigated in software / targeted re-review pending |
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
