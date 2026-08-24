# Stage 5 Brief – GUI Profile-Selection Follow-up Handoff

**Stage:** 5  
**Status:** DRAFT / DOCUMENTATION HANDOFF ONLY — implementation is not authorized by this file  
**Lead:** Codex  
**Coordination:** ChatGPT Classic / Project Manager  
**Review:** to be determined by the approved full Stage-5 scope  
**Decision authority:** User / Product Owner

## Purpose of this handoff

This draft records the one mandatory, non-blocking Stage-4 review follow-up. It does not define the complete
Stage-5 GUI scope and must be completed and approved before any Stage-5 implementation begins.

## Normative carried-forward requirement: FU-3 (P3)

Stage 5 shall preserve the accepted Stage-4 matching precedence:

```text
saved explicit binding -> native identity -> Universal MIDI Identity -> SysEx fingerprint
    -> manual profile selection -> Generic fallback
```

When stronger SysEx fingerprint evidence overrides `input.manual_profile_id`, the result shall include the
discarded manual selection as an additional `ProfileMatchResult::evidence` entry. It must not become
`selected_profile_id`, must not lower the precedence of the winning fingerprint evidence, and must not be silently
dropped.

The Stage-5 GUI shall:

- show that the user's manual selection was overridden by stronger fingerprint evidence;
- identify both the discarded manual profile and the selected fingerprint-matched profile;
- offer an intentional promotion of that manual selection to a saved explicit binding;
- never create or modify a saved binding merely by showing the evidence.

The saved explicit binding, once intentionally created through the approved Stage-5 interaction, shall retain its
existing rank-one behavior on a subsequent match. No port name, route identity, fuzzy matching, or background
device request may be used to replace this precedence.

## Required tests

Stage-5 evidence must prove all of the following:

1. The existing precedence is unchanged: a matching SysEx fingerprint wins over a conflicting manual selection.
2. The result evidence contains the discarded manual selection and identifies that it was overridden by stronger
   fingerprint evidence.
3. The GUI presents the override visibly rather than displaying the fingerprint result as though no manual choice
   had been made.
4. An explicit user promotion creates a saved binding only through the approved interaction, and that binding wins
   a subsequent match over fingerprint evidence.

Tests must also prove that merely viewing a result is side-effect free and that no manual selection or binding can
clear malformed/incomplete/tainted SysEx state.

## Out of scope for this handoff

- implementation of Stage-5 GUI/application-host functionality;
- a change to Stage-4 matching precedence;
- automatic binding creation, fuzzy rebinding, or device detection from DIN port names;
- real-hardware communication, drivers, registry, service, API-mode, or global-system changes.

## Stop / Ask

Stop and ask the User/Project Manager before implementing if the complete Stage-5 scope requires a different
selection order, makes a saved binding persistent in a form not covered by the accepted route/profile identity
model, or would weaken the Stage-4 SysEx taint/integrity chain.
