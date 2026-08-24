# TAUREON V4 – AI Model, Reasoning & Quota Policy

**Status:** Active living policy  
**Last verified:** 2026-08-24  
**Maintained by:** ChatGPT Classic / Project Manager  
**Budget model:** Subscription quota only unless the User explicitly approves additional paid usage

## 1. Objective

TAUREON should use the strongest reasoning where mistakes are expensive, but should not consume premium model quota for routine work.

Principle:

> **Use the lowest-cost model/effort combination that is reliably adequate for the task; escalate only the difficult part, not the whole stage.**

Model availability changes over time. The Project Manager therefore maintains this policy and verifies available models before major stages.

The model picker in the actual Codex/Claude Code account is the operational source of truth.

---

## 2. Subscription assumptions

Current project planning assumes:

- Codex is used through the User's ChatGPT Plus/Pro-class subscription access.
- Claude Code is used through subscription access rather than an Anthropic API key.
- No pay-as-you-go API budget is assumed.
- No extra usage credits are enabled for project work unless the User explicitly approves them.

### Claude extended context

On Claude Pro, 1M-context variants may require additional usage credits. Therefore:

> **Do not select `opus[1m]` or `sonnet[1m]` for TAUREON by default.**

The Project Manager should instead keep specialist sessions focused and provide compact handoff context.

---

## 3. Current available Codex family

Confirmed from the User's Codex model picker:

- GPT-5.6 Sol Ultra
- GPT-5.6 Sol
- GPT-5.6 Terra
- GPT-5.6 Luna

Fable is **not currently available** and is not part of this policy.

### Codex model roles

#### Luna

Use only for very low-risk/mechanical work if ChatGPT Classic does not already handle it.

Examples:

- trivial edits;
- simple searches;
- repetitive transformations;
- basic documentation mechanics.

Not for production core architecture or concurrency-sensitive code.

#### Terra

**Default economical implementation model.**

Use for:

- ordinary C++ implementation;
- CMake/build work;
- test scaffolding;
- profiles/data;
- routine Qt GUI implementation;
- packaging;
- straightforward fixes.

Default reasoning:

```text
medium
```

Raise to:

```text
high
```

when the task spans several files or requires non-trivial reasoning.

#### Sol

**Correctness-critical implementation model.**

Use for:

- WMS/WinMM native API work;
- transport lifetime;
- endpoint/group identity;
- MIDI/UMP representation;
- SysEx reassembly/integrity;
- cancellation/concurrency;
- difficult debugging;
- architecture-sensitive refactors.

Default reasoning:

```text
high
```

Use `xhigh` only for a bounded genuinely difficult problem.

Use `max` only as an explicit escalation for a blocker/P0/P1-level problem.

#### Sol Ultra

**Exceptional escalation only.**

Ultra coordinates parallel agent work and is therefore not quota-efficient as a default.

Use only when the Project Manager explicitly decides that parallel investigation is useful, for example:

- several plausible root causes must be investigated independently;
- a complex P0/P1 spans transport, concurrency and protocol layers;
- a release-critical blocker benefits from parallel falsification.

Do not use Ultra for an entire normal stage.

---

## 4. Claude Code model strategy

Fable is currently unavailable on the User's account and is excluded.

Claude Code's own `/model` command is the source of truth for the models actually available to the account.

### Sonnet

**Default economical Claude Code model.**

Use for:

- focused code review;
- ordinary research;
- bounded design questions;
- reviewing well-specified diffs;
- non-critical test-gap analysis.

Default effort:

```text
medium
```

Use `high` for non-trivial reviews.

### Opus

**Architecture and difficult-review model.**

Use for:

- WMS/WinMM architecture/lifecycle;
- concurrency/resource lifetime;
- architecture-changing ADRs;
- difficult P0/P1 analysis;
- final independent release review.

Default effort:

```text
high
```

Use `xhigh` if the selected Opus version offers it and the task is genuinely difficult.

Use `max` only for a bounded release blocker or final-review question where marginal reasoning quality is worth substantial quota consumption.

### Hybrid planning

If Claude Code is temporarily asked to both plan and implement a difficult task, `opusplan` can be efficient: Opus plans, Sonnet executes.

This is **not the normal TAUREON workflow**, because Codex is the implementation lead.

---

## 5. Reasoning-effort rules

Reasoning effort is chosen per task, not as a badge of quality.

### Low

Only:

- mechanical;
- easily verified;
- low-risk.

### Medium

Default for normal engineering work.

Use when:

- requirements are clear;
- architecture is already decided;
- deterministic tests provide strong feedback.

### High

Default for critical engineering/review.

Use when:

- several constraints interact;
- resource ownership matters;
- native APIs are involved;
- an error may corrupt SysEx or misroute hardware.

### XHigh

Use for a bounded task when high effort did not provide enough confidence or the problem is inherently long-horizon.

### Max

Emergency/frontier setting.

Use only for:

- unresolved P0/P1;
- unexplained byte corruption;
- subtle concurrency/lifetime blocker;
- final review dispute with material release consequences.

Do not run whole stages at `max`.

---

## 6. Stage-by-stage default matrix

| Stage | Codex default | Codex effort | Claude Code default | Claude effort | Notes |
|---|---|---|---|---|---|
| **0 – Bootstrap / inventory / API verification** | **Terra** | high | **Opus** for mandatory gate review | high | Escalate WMS-specific blocker to Sol high/xhigh |
| **1 – WMS + WinMM native spikes** | **Sol** | high | **Opus** mandatory review | high | xhigh only for lifecycle/API ambiguity |
| **2 – MIDI Core / transport abstraction** | **Sol** | high | Sonnet targeted; Opus if architecture changes | medium/high | Core interfaces/lifetime justify Sol |
| **3 – MIDI / SysEx / Transfer Engine** | **Sol** | high | Sonnet targeted; Opus for integrity/concurrency blockers | medium/high | Use xhigh only for difficult reassembly/race work |
| **4 – Device/Profile isolation proof** | **Terra** | high | **Opus** mandatory architecture review | high | Use Sol if compiled protocol logic becomes non-trivial |
| **5 – Qt GUI** | **Terra** | medium/high | Sonnet only if targeted review needed | medium | Escalate thread/model-view issue to Sol/Opus |
| **6 – Robustness / failure injection** | **Sol** | high | Opus for concurrency/P0/P1; otherwise Sonnet | high when used | Highest bug-finding risk after Stage 1/3 |
| **7 – Packaging / RC** | **Terra** | medium | Sonnet on demand | medium | ChatGPT Classic handles much of docs/release coordination |
| **8 – Independent final review** | Terra/Sol only for verified fixes | task-specific | **Opus** | **xhigh if available; otherwise high** | `max` only for unresolved release-critical findings |
| **9 – User hardware validation** | Terra for support/log triage | medium | normally none | — | Escalate actual blocker to Sol/Opus as needed |

---

## 7. Stage 0 recommendation

For the Stage 0 handoff now:

### Codex

Start with:

```text
GPT-5.6 Terra
reasoning: high
```

Why:

- much of Stage 0 is repository inspection, inventory, environment detection, documentation, and deterministic tool use;
- using Sol for the entire inventory would waste quota;
- the high-risk WMS part is bounded and can be escalated separately.

Escalate only the WMS/API or architecture-sensitive subtask to:

```text
GPT-5.6 Sol
reasoning: high
```

If a genuinely difficult documented-API ambiguity remains:

```text
GPT-5.6 Sol
reasoning: xhigh
```

Do **not** use Sol Ultra or `max` for the normal Stage 0 pass.

### Claude Code – Stage 0 review

Use the strongest available Opus from `/model`:

```text
model: opus
effort: high
```

Claude receives only:

- Stage 0 brief;
- Stage 0 report;
- environment report;
- created/changed ADRs;
- legacy/provenance summaries;
- relevant diff.

Do not ask Claude to reread the complete old TAUREON repository.

If Opus is unavailable or quota is exhausted:

```text
model: sonnet
effort: high
```

is the fallback.

---

## 8. Quota-saving workflow

### Project Manager absorbs low-value context work

ChatGPT Classic should handle, where practical:

- prompt preparation;
- document maintenance;
- report consolidation;
- risk/decision logs;
- research summaries;
- comparing Codex/Claude findings;
- stage handoff packages;
- routine planning.

### Specialist sessions stay narrow

Codex/Claude Code receive:

- current state;
- current stage brief;
- relevant ADRs;
- relevant files/diff.

Avoid:

- rereading the full repo every stage;
- pasting the giant historical rebuild specification;
- loading binary/SysEx fixtures as text;
- keeping multi-stage sessions alive indefinitely.

### Escalate the problem, not the stage

Example:

```text
Stage 5 GUI on Terra medium
        ↓
one difficult shutdown race discovered
        ↓
switch that task to Sol high
        ↓
if architecture/lifetime remains questionable:
Claude Opus high targeted review
        ↓
return to Terra for ordinary GUI work
```

---

## 9. Automatic escalation triggers

The Project Manager should consider a model/effort upgrade when:

- the same verified defect survives one competent fix;
- native ownership/lifetime is unclear;
- tests contradict the current mental model;
- byte integrity is uncertain;
- an ADR must change;
- a P0/P1 appears;
- reviewer and implementer disagree on material evidence;
- a release gate depends on the answer.

A stronger model is **not** a substitute for Stop/Ask when the project's Stop/Ask criteria are reached.

---

## 10. Model-policy maintenance

Before each mandatory review stage (0, 1, 4, 8), the Project Manager should verify:

- which Codex models appear in the User's picker;
- which Claude Code models appear under `/model`;
- available effort levels;
- whether subscription rules/limits changed.

Only this file should contain the detailed stage/model matrix.

Other project documents should link to it rather than duplicate model recommendations.
