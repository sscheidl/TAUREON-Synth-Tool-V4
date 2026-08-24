# Architecture Decision Records

Use one ADR per material architecture decision.

Naming:

```text
ADR-0001-short-title.md
ADR-0002-short-title.md
```

States:

```text
Proposed
Accepted
Superseded
Rejected
```

An ADR is required when a decision:

- changes layer boundaries;
- changes a public/core interface used by multiple stages;
- changes backend strategy;
- introduces a major dependency;
- changes threading/lifetime policy;
- changes persisted identity/data format;
- changes a previously accepted safety invariant.

Routine implementation details do not need ADRs.

Accepted ADRs are authoritative for the decision they cover. `ARCHITECTURE.md` should be updated soon afterward so the architecture baseline does not drift.
