# ADR-0001 – Backend-specific persisted route identity

**Status:** Accepted
**Date:** 2026-08-24
**Owner:** Codex / Implementation Lead
**Review:** User-approved Stage 2 brief

## Context

WMS exposes endpoint device IDs and groups, while WinMM exposes individual MIDI 1.0 ports with names,
manufacturer/product IDs, driver versions, and unstable runtime indices. Stage 1 did not establish a safe
cross-backend identity correlation. A persisted route must never silently select a different endpoint after
enumeration order or backend changes.

## Decision

Persist a versioned, backend-discriminated route identity:

- common fields: schema version, backend, and direction;
- WMS fields: endpoint device ID and group;
- WinMM fields: port name, `wMid`, `wPid`, and driver version;
- never persist a WinMM runtime index as identity.

Resolution compares the complete backend-specific composite identity and returns exactly one of `Exact`,
`Missing`, `Ambiguous`, or `Invalid`. It never uses display-name-only fallback, first-candidate selection,
fuzzy matching, or WMS↔WinMM translation. Backend changes require deliberate route selection.

## Alternatives considered

1. Persist display name or WinMM index only: rejected because either may change or collide.
2. Guess WMS↔WinMM correlation: rejected because Stage 1 correlation helpers failed fast and correlation is
   not required for correctness.
3. Persist a universal hardware identity: rejected because neither backend provides one reliably.

## Consequences

### Positive

- Missing and ambiguous routes fail visibly.
- Enumeration order cannot silently rebind a WinMM route.
- WMS groups and RX/TX direction remain part of identity.
- The persistence format can be migrated by schema version.

### Negative / risks

- Backend changes and some driver/metadata changes require user reselection.
- Two genuinely identical WinMM descriptors remain ambiguous by design.

## Validation

- Unit roundtrips cover both identities, escaping, malformed/unknown fields, schema versions, and group
  bounds.
- Resolver tests cover exact, missing, ambiguous, invalid, reordered indices, same-name cross-backend
  candidates, and duplicate candidates.
- The local WMS-native loopback regression verifies exact selection and removal without physical MIDI use.

## Supersedes / superseded by

- None
