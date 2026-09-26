# TAUREON V4 – Risk Register

**Maintained by:** ChatGPT Classic / Project Manager  
**Review cadence:** at every stage gate and whenever a new P0/P1 issue appears

| ID | Risk | Probability | Impact | Current mitigation | Owner | Status |
|---|---|---:|---:|---|---|---|
| R-001 | Current WMS SDK/runtime integration differs from assumptions | Low/Medium | High | Pinned RC4 SDK/runtime pairing and direct lifecycle proven; API-mode and correlation limitations explicitly excluded from architecture assumptions | Claude Code / Codex | Mitigated / non-blocking follow-up |
| R-002 | WinMM long-message teardown/lifetime defect | Low/Medium | High | Production callback→bounded queue→worker requeue and deterministic input/output `MIDIHDR` owners; partial-open P1 fixed; transport/header failure tests; 100 Stage-3 cycles include 10-KiB multi-buffer SysEx TX/RX with zero drops/late callbacks; vendor-driver validation remains | Codex + Claude Code review | Mitigated in software loopback / hardware follow-up |
| R-003 | SysEx byte corruption or incomplete capture marked valid | Low/Medium | Critical | Production WinMM/WMS streams carry non-droppable ordered loss markers into grouped capture/parsing; adjacent-only WinMM overflow coalescing preserves ordered loss across later frame boundaries; tests prove native loss → capture taint → no verified-complete/profile acceptance/save, loss at close, no later false taint, group-local SysEx7 decode termination, and contextual `.syx` I/O errors; the real Summit fixture remains rejected when tainted; WMS 100/500-cycle handle series and synthetic fast/slow leak tests validate directional ownership stability; targeted review accepts the Stage-5 B-3 active-receive/close corrections but requires a fresh retained 100-cycle soak on the immutable pushed SHA; physical/vendor-driver behavior remains separate | Codex + independent targeted review | Mitigated in software / Stage-5 soak evidence pending |
| R-004 | Windows MIDI stack instability complicates testing | Medium | High | WMS diagnostic and temporary native loopbacks; isolated RC4 correlation fail-fast; passive receive-only WMS diagnostics succeeded on Summit, KONTROL S61 MK3 Main and MiniFreak without physical sends or system changes; Summit WinMM helper RX remains unconfirmed | Project Manager / User | Active / bounded |
| R-005 | Wrong route/device selected for send | Low | Critical | Stage 2 schema/resolver and Stage-5 GUI enforce backend-specific composite identity, reject missing/ambiguous/invalid routes, omit WinMM indices, prohibit fuzzy/cross-backend substitution, and keep RX/TX independently explicit; no physical send has been authorized or performed | Architecture / GUI | Mitigated in core/UI; physical send gate retained |
| R-006 | Scope creep into universal librarian delays reliable core | High | Medium/High | V4.0 baseline explicitly limits librarian depth; stage scope control | Project Manager / User | Open |
| R-007 | Legacy/third-party reuse has unclear provenance/license | Medium | High | Stage 0 provenance inventory; prefer clean reimplementation; Stage-4 Summit fixture has per-item User permission, immutable hash and no-redistribution restriction | Project Manager / Claude Code | Open |
| R-008 | GUI cannot keep up with Clock/Active Sense/event floods | Medium | Medium/High | Bounded model/view, batched updates, and retained 100,000-event stress with zero drops and 2 ms maximum heartbeat delay; native sustained-device flood behavior remains separate | Codex | Mitigated in software / hardware follow-up |
| R-009 | Specialist AI token/context budget exhausted during risky task | Medium | Medium | Project Manager prepares focused context and absorbs bounded support work | Project Manager | Open |
| R-010 | Documentation/source-of-truth drift | Low/Medium | High | 2026-09-26 reconciliation replaced stale 20-cycle/current-PASS wording with the five-cycle remediation state, retained evidence limits, focused review handoff and independent HOLD result; the next audit must bind CI and soak evidence to the pushed commit SHA | Project Manager | Active / bounded |

## Risk rules

- P0/P1 defects are not merely risks; they become active blockers/issues.
- A risk may be closed only with evidence.
- If probability/impact changes materially, update the row rather than appending a diary entry.
- User decisions about accepted residual risk are recorded in `DECISION_LOG.md`.
