# Documentation Consistency Audit – V4 Baseline

**Date:** 2026-08-24  
**Result:** PASS after restructuring  
**Performed by:** ChatGPT Classic / Project Manager

## 1. Problems found in the previous document set

### 1.1 Duplicate governance

Roles, review checkpoints, Stop/Ask rules, stage sequencing, and gate logic appeared repeatedly in:

- AI workflow;
- AI roles;
- build process;
- rebuild master specification.

Risk: one file could be updated while another remained stale.

### 1.2 Overloaded master specification

The large rebuild specification acted simultaneously as:

- architecture document;
- implementation prompt;
- product spec;
- GUI spec;
- test plan;
- release plan;
- AI governance.

Risk: unnecessary context loading and unclear ownership of future edits.

### 1.3 Product-domain vs GUI-tab ambiguity

Earlier documents described five product areas while the GUI prototype contained additional workspaces such as Diagnostics and SysEx Manager.

Resolution:

- five **logical product domains** remain;
- GUI workspaces may split domains for usability;
- Diagnostics is cross-cutting;
- SysEx Transfer, SysEx Manager, and Librarian have distinct semantics.

### 1.4 Diagnostics contradiction

One older concept placed diagnostics inside Options; later GUI work made it a main tab.

Resolution:

- Diagnostics is architecturally cross-cutting;
- the GUI exposes it as a dedicated workspace because troubleshooting benefits from visibility.

### 1.5 Old implementation phase conflict

The old Aufgabenbereiche document proposed an early Qt GUI skeleton, while the newer reliability architecture requires transport/core proof before production GUI.

Resolution:

- Stage 5 remains the production Qt GUI stage;
- clickable mockups may be developed earlier because they contain no wiring.

### 1.6 Unclear authority

Earlier process text sometimes implied that Codex could self-gate routine stages, while later discussion established explicit User decision authority and a Project Manager role.

Resolution:

- User = Product Owner, Hardware Tester, final decision authority;
- ChatGPT Classic = Project Manager/Supervisor/Context Keeper;
- Codex = Implementation Lead;
- Claude Code = Architecture & Review Lead;
- Project Manager administers evidence/gates and prepares decisions;
- User retains scope/risk/hardware/final acceptance authority.

### 1.7 DIN profile detection ambiguity

Earlier ideas risked conflating “profile exists” with “DIN port is mapped”.

Resolution:

- clear USB/endpoint identity may suggest a profile;
- generic DIN interface names do not identify the connected synth;
- explicit user port/profile binding is remembered only when deliberately requested;
- unknown DIN remains generic rather than guessed.

## 2. Active source-of-truth structure

The new set separates:

- product;
- architecture;
- process;
- roles;
- quality;
- GUI;
- current status;
- stage-specific scope;
- legacy references.

Each rule has one primary home.

## 3. Cross-document consistency checks

### Product vs architecture

PASS.

Generic-first product intent matches transport/device separation.

### Architecture vs development process

PASS.

Stages prove transport/core before profiles/GUI.

### Development process vs AI roles

PASS.

Process refers to role document rather than redefining detailed responsibilities.

### Quality policy vs stages

PASS.

Stages refer to cross-cutting quality/safety rules rather than duplicating them.

### GUI vs product model

PASS.

Seven GUI workspaces are explicitly mapped to five logical product domains.

### GUI vs architecture

PASS.

Mockup is non-wired; Qt Stage 5 remains after core/profile proof.

### Legacy vs V4

PASS.

Legacy source is preserved as evidence/reference without controlling new architecture.

### Project-management controls

PASS.

The baseline now includes a current-state file, risk register, decision log, stage brief/report artifacts, lightweight Git/issue guidance, and CI expectations appropriate for a one-developer professional project.

## 4. Remaining intentionally open items

These are not documentation inconsistencies; they require Stage 0/1 evidence or later product decisions:

- exact current WMS SDK/runtime integration details;
- exact persistent WMS/WinMM route identity strategy;
- Qt/WMS lifecycle details;
- final V4.0 Librarian depth;
- exact new GitHub repository URL until created;
- licensing decision for the new repository.

## 5. Conclusion

The restructured documentation set is internally consistent and suitable as the baseline for a professional V4 repository.

Earlier planning documents should remain archived/reference-only and should not be copied into the active V4 documentation tree unless a specific historical need arises.
