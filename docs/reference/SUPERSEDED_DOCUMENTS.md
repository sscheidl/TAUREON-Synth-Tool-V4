# Superseded / Reference-Only Documents

The following earlier documents remain useful as historical source material but are **not active V4 specifications**.

| Earlier document | Status | Active replacement |
|---|---|---|
| `README(2).md` | Superseded legacy README | root `README.md` |
| `TAUREON_SYNTH_TOOL_AUFGABENBEREICHE_V0.1.md` | Product/architecture source; old phase plan retired | `product/PRODUCT_VISION.md`, `architecture/ARCHITECTURE.md` |
| `TAUREON2_REBUILD_SPEC_CLAUDE_CODEX_V1.08_2025.md` | Comprehensive source brief; no longer daily master/control file | split across Product Vision, Architecture, Development Process, Quality Policy, AI Collaboration, GUI Design |
| `TAUREON2_AI_WORKFLOW.md` | Superseded compact workflow | `process/AI_COLLABORATION.md` + `process/DEVELOPMENT_PROCESS.md` |
| `TAUREON2_GUI_CONCEPT_STATUS.md` | Superseded/normalized GUI planning source | `design/GUI_DESIGN_SPEC.md` |
| `TAUREON_Synth_Tool_V2_Status_Empfang_Send_Neuer_Chat.md` | Legacy evidence/status source | Stage 0 legacy inventory/provenance |
| `MIDI_CC_RESEARCH_2026-06-03.md` | Device-profile research source | Stage 4 profile work / reference inventory |

## Why the old rebuild specification was split

The old large rebuild document mixed:

- product scope;
- architecture;
- stage process;
- AI roles;
- test policy;
- GUI design;
- release acceptance;
- legacy history.

That made it comprehensive but poor as a daily source of truth: the same rules appeared in several places and future changes could easily create contradictions.

V4 therefore uses bounded documents by responsibility, with `docs/README.md` as the map.

The old documents should remain accessible in the legacy project/archive when historical detail is needed.
