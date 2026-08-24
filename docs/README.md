# TAUREON V4 Documentation

The documentation is deliberately split by responsibility. There is **no active giant master document** that mixes product scope, architecture, development process, AI roles, test policy, and GUI design.

## Source-of-truth map

| Question | Authoritative document |
|---|---|
| What is TAUREON and why does it exist? | `../README.md` |
| What should the product eventually be able to do? | `product/PRODUCT_VISION.md` |
| How is the software structured technically? | `architecture/ARCHITECTURE.md` + accepted ADRs |
| How is the project executed from Stage 0 to release? | `process/DEVELOPMENT_PROCESS.md` |
| Who coordinates, implements, reviews, and decides? | `process/AI_COLLABORATION.md` |
| What are the safety/test/evidence rules? | `process/QUALITY_POLICY.md` |
| How should the GUI behave and look? | `design/GUI_DESIGN_SPEC.md` + clickable mockup |
| What is true right now? | `status/PROJECT_STATE.md` |
| What risks are being tracked? | `status/RISK_REGISTER.md` |
| What product/process decisions were made? | `status/DECISION_LOG.md` |
| What exactly is in scope for the current stage? | `stages/STAGE_N_BRIEF.md` |
| What evidence was produced by a completed stage? | `stages/STAGE_N_REPORT.md` |
| Why was an architecture choice made? | `architecture/adr/ADR-XXXX-*.md` |

## Authority and conflict rules

1. The **User/Product Owner** owns product scope, destructive/system-changing approvals, real hardware validation, and final release acceptance.
2. **ChatGPT Classic / Project Manager** maintains the project-control documents, sequencing, handoffs, risks, and gate administration.
3. `PRODUCT_VISION.md` defines product intent, but not implementation details.
4. `ARCHITECTURE.md` defines current technical invariants.
5. An **accepted ADR** may supersede part of `ARCHITECTURE.md` until the architecture document is updated.
6. A stage brief may **narrow** work for a stage, but may not silently contradict product scope or architecture.
7. `PROJECT_STATE.md` contains current state only; it does not override architecture.
8. GUI design may express product workflows, but it cannot redefine transport/core architecture.
9. Legacy documents and the old TAUREON repository are reference material only.

If active documents conflict, the Project Manager must resolve the inconsistency explicitly and update the affected files. Do not rely on an undocumented precedence rule.

## Documentation language

Active technical/project documentation is kept in English for consistency with code, toolchains, Codex, and Claude Code. User-facing discussion may remain German.

## Retired source documents

Earlier planning documents are intentionally not active specifications in V4. See:

```text
reference/SUPERSEDED_DOCUMENTS.md
```
