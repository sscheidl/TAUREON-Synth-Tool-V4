# Stage 1 Brief – Native WMS and WinMM Transport Spikes

**Stage:** 1  
**Status:** DRAFT / PENDING PROJECT MANAGER AND USER APPROVAL  
**Implementation lead:** Codex  
**Coordination:** ChatGPT Classic / Project Manager  
**Mandatory review:** Claude Code  
**Decision authority:** User where required

## Goal

Independently prove the native Windows MIDI Services (WMS) and WinMM transport paths before creating the production transport abstraction, MIDI core, or GUI.

## Preconditions

- Stage 0 is PASS / CLOSED.
- Use the private `sscheidl/TAUREON-Synth-Tool-V4` repository.
- Pin and record the exact WMS SDK package, C++/WinRT package, `.winmd`, runtime, and deployment-file versions used by the WMS spike. Do not assume the locally installed preview/RC components are a sufficient reproducible dependency declaration.
- No driver, registry, service, Windows MIDI API-mode, or other global MIDI configuration change is permitted.

## In scope

### WMS spike

- C++20/CMake consumption of the pinned official WMS App SDK route using C++/WinRT; no `midi.exe` subprocess.
- Explicit WinRT apartment initialization and a documented MTA/worker-thread boundary.
- Read-only API-mode observation using the documented WMS API; report `LegacyMode` or unavailable-service states without changing them.
- Endpoint enumeration, device-watcher change visibility, session/connection setup, receive callback, and safe diagnostic-loopback/test send only where the installed service exposes it.
- SysEx7 UMP helper use and receive-sequence evidence sufficient to identify Start/Continue/End behavior, byte counts, and transfer constraints.
- Deterministic close/shutdown and clean deployment evidence for the pinned WMS runtime.

### Mandatory route-identity acceptance requirements

- Test endpoint ID + group + direction as one composite route identity.
- Verify persisted-route resolution behavior.
- Do not silently fuzzy-fallback or rebind a route.
- A missing or ambiguous persisted route must fail visibly and require deliberate user action.
- Test endpoint change, disappearance, and re-enumeration behavior.

### Mandatory Qt/WMS coexistence acceptance requirements

- WMS initialization must not block the Qt UI thread.
- Establish and document the intended MTA/worker-thread boundary.
- Test deterministic startup and shutdown.
- Test application close while WMS worker/session objects exist.
- Prove no callback or use-after-free crosses the Qt/application lifetime boundary.

### WinMM spike

- Native input/output enumeration without RtMidi or another hidden transport wrapper.
- Short-message send/receive path.
- Long-message/SysEx input buffering with explicit `MIDIHDR` prepare → queue → completion → unprepare ownership.
- Persistent-identity research/evidence; do not treat the numeric port index as sufficient persisted identity.
- Repeated deterministic close/shutdown with no leaked handles or callback access after destruction.

## Out of scope

- Production `IMidiTransport` abstraction or MIDI core.
- Generic SysEx transfer engine, profiles, librarian, or Qt product GUI.
- Real hardware sends or destructive transfers.
- Driver, registry, service, API-mode, or virtual-driver changes.
- Legacy code/profile/fixture migration.
- Windows CI claims for WMS behavior that a hosted runner cannot provide.

## Deliverables

- Isolated WMS and WinMM spike sources/tests under clearly separate directories.
- Pinned WMS dependency/deployment record with hashes or immutable package identifiers where available.
- WMS and WinMM environment/test reports, including commands, observed API mode, endpoint identity evidence, logs, and known limitations.
- Any evidence-supported ADRs required by a transport, identity, timestamp, deployment, or lifetime decision.
- `docs/stages/STAGE_1_REPORT.md` and updated `docs/status/PROJECT_STATE.md`.

## Acceptance tests / evidence

- Build the WMS and WinMM spikes with the recorded toolchain.
- Demonstrate the WMS initialization/API-mode/enumeration/receive/shutdown lifecycle without a `midi.exe` subprocess.
- Demonstrate WinMM enumeration, short messages, long-message buffer lifecycle, and deterministic teardown.
- Exercise every mandatory route-identity and Qt/WMS coexistence requirement above.
- Record any environment-specific limitation instead of simulating it or changing the system.
- Do not claim real-hardware validation; only the User provides that evidence.

## Gate criteria

PASS requires both spikes to provide documented, reproducible lifecycle evidence; no hidden third backend; no ambiguous ownership; no unexplained callback/lifetime defect; and all mandatory route-identity and Qt/WMS coexistence checks satisfied or explicitly escalated as a Stop/Ask issue.

HOLD if the documented WMS route cannot be made to work, a system change is required, endpoint ambiguity could target the wrong device, byte integrity is uncertain, or deterministic shutdown cannot be demonstrated.

## Stop / Ask

Apply `docs/process/QUALITY_POLICY.md`, especially for required system changes, undocumented core dependencies, endpoint ambiguity, byte corruption, or unresolved P0/P1 lifetime/architecture issues.

## Review handoff

Claude Code receives only the Stage 1 brief/report, changed ADRs, pinned-dependency/deployment record, WMS/WinMM test evidence, relevant logs, and the Stage 1 diff. Review focuses on WMS assumptions, WinMM buffer lifetime, route identity, Qt/WMS boundaries, SysEx integrity, and Stage 2 readiness.
