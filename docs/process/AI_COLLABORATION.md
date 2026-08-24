# TAUREON V4 – AI Collaboration & Responsibilities

**Status:** Active living governance  
**Owner:** User / Product Owner  
**Maintained by:** ChatGPT Classic / Project Manager

This document defines the default division of work. It may change when the User decides that another allocation is more effective.

## 1. Roles

```text
User
Product Owner / Hardware Tester / Final Decision Authority
                         |
                         v
ChatGPT Classic
Project Manager / Supervisor / Context Keeper
            /                            \
           v                              v
Codex                                Claude Code
Implementation Lead           Architecture & Review Lead
```

## 2. User – Product Owner, Hardware Tester, Decision Authority

The User owns:

- product vision and priorities;
- scope decisions;
- acceptance/rejection of material product changes;
- approval of destructive operations;
- approval of driver/registry/service/API-mode changes;
- real hardware validation;
- final release acceptance;
- changes to the role model.

The User is the only authority for claims about real hardware behavior.

The User should receive concise decision-ready information rather than routine implementation detail.

## 3. ChatGPT Classic – Project Manager / Supervisor / Context Keeper

ChatGPT Classic owns project coordination.

Responsibilities:

- maintain the overall roadmap;
- keep documentation source-of-truth consistent;
- prepare/refine stage briefs;
- track current stage, gate, risks, blockers, and next action;
- decide which AI is best suited to a task;
- prepare handoffs/prompts for Codex and Claude Code;
- summarize outputs and resolve documentation drift;
- verify that stage evidence matches the defined process;
- coordinate review/fix loops;
- maintain continuity across chats and long pauses;
- challenge scope creep and duplicated work;
- prepare decision options for the User;
- keep the legacy/reference boundary clear.

### Generalist / token-pressure role

When Codex or Claude Code token budgets are limited, ChatGPT Classic may take over bounded supporting work, for example:

- documentation;
- planning;
- research synthesis;
- test-case drafting;
- fixture inventories;
- issue triage;
- prompt preparation;
- change summaries;
- lightweight code review;
- comparing reviewer findings;
- release notes;
- GUI/workflow planning.

This does **not** silently replace the specialist role for high-risk work. Architecture-critical native API/lifetime decisions still go to Claude Code; main implementation remains with Codex unless the User explicitly reassigns it.

### Project Manager authority

The Project Manager may administer routine process gates and recommend PASS/HOLD, but does not override the User's authority over scope, destructive/system changes, hardware acceptance, or final release.

When the process requires a User decision, the Project Manager presents:

- decision required;
- evidence;
- options;
- recommendation;
- consequences.

## 4. Codex – Implementation Lead

Codex owns routine implementation:

- C++20/Qt/CMake code;
- repository-local inspection for the current task;
- automated tests;
- builds;
- diagnostics implementation;
- packaging automation;
- verified fixes;
- stage technical reports.

Codex must:

- work only on the current approved scope;
- use deterministic tools to establish build/test facts;
- verify review findings against actual code before changing it;
- stop on defined Stop/Ask conditions;
- avoid unapproved architecture changes;
- avoid hardware guessing;
- never claim hardware validation.

Codex should not routinely re-scan the whole legacy project or master history.

## 5. Claude Code – Architecture & Review Lead

Claude Code owns specialist architecture/review work:

- architecture review;
- risky WMS/WinMM API and lifecycle analysis;
- concurrency/resource lifetime;
- endpoint/group identity;
- SysEx integrity risks;
- architecture-changing ADR review;
- targeted high-severity bug analysis;
- mandatory stage reviews;
- independent release review.

Claude Code reviews evidence rather than rewriting the implementation by default.

Findings should be actionable:

```text
Severity: P0 / P1 / P2 / P3
Claim:
Evidence:
Why it matters:
Minimal recommended action:
Confidence: high / medium / low
```

Codex verifies each finding before fixing it.

## 6. Handoff model

Normal implementation flow:

```text
User/Product direction
        |
        v
Project Manager prepares scope/stage brief
        |
        v
Codex implements + tests + reports
        |
        v
Project Manager checks evidence/process
        |
        +--> mandatory/sensible architecture review? --> Claude Code
        |                                                |
        |                                                v
        |                                        findings/recommendation
        |                                                |
        +------------------- fix/verify loop <------------+
        |
        v
Project Manager prepares gate decision
        |
        v
User decides where user authority is required
        |
        v
Next stage/task
```

## 7. Mandatory Claude Code review points

Default mandatory reviews:

- end of Stage 0;
- end of Stage 1;
- end of Stage 4;
- any architecture-changing ADR;
- unresolved P0/P1 issue;
- Stage 8 final independent review.

The Project Manager may request additional focused review when risk justifies it.

Do not run a second full-AI pass on every routine change.

## 8. Temporary role changes

Roles may be changed for a bounded task.

Record:

```text
Task:
Temporary owner:
Reason:
Scope:
Expected handoff:
Ends when:
```

Examples:

- Claude Code designs a difficult protocol state machine, then Codex implements it.
- ChatGPT Classic performs documentation/research work to preserve specialist tokens.
- Codex prepares an ADR draft from spike evidence.
- Another reviewer is introduced for a specialized release concern.

After the task, default roles resume unless the User changes this document.

## 9. Context discipline

Default reading order for Codex/Claude Code:

```text
1. docs/status/PROJECT_STATE.md
2. docs/process/AI_COLLABORATION.md
3. current docs/stages/STAGE_N_BRIEF.md
4. relevant accepted ADRs
5. relevant source/tests
6. relevant architecture/product/design section as needed
7. legacy/reference material only for a named question
```

Do not load all documentation or the full repository simply because a new session started.

The Project Manager is responsible for preparing enough context that specialists do not need to reconstruct the whole project history.

## 10. Disputed findings

If Codex disagrees with a Claude Code finding:

```text
Finding:
Reviewer claim:
Implementation evidence:
Decision:
```

The Project Manager evaluates the disagreement.

If it remains P0/P1 or changes architecture, present it to the User with options and recommendation.
