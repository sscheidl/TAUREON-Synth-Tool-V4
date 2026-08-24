# TAUREON V4 – Architecture

**Status:** Active architecture baseline; updated with Stage 0/1 evidence
**Architecture review lead:** Claude Code  
**Maintained by:** Project Manager after accepted decisions

## 1. Architecture objective

TAUREON V4 is a clean native C++20 rebuild.

The architecture is designed to make three concerns independent:

1. Windows MIDI transport
2. generic MIDI/SysEx behavior
3. device-specific knowledge

The previous Python implementation is a behavioral reference, not a source architecture to port.

## 2. Layer model

```text
Qt GUI / Application Shell
          |
          v
Application Services / Controllers
          |
    +-----+--------------------------+
    |                                |
    v                                v
MIDI Monitor                  Librarian / File Workflows
    |                                |
    +---------------+----------------+
                    |
                    v
           Device/Profile Services
           + Protocol Modules
                    |
                    v
              Transfer Engine
                    |
                    v
                MIDI Core
                    |
                    v
              IMidiTransport
               /          \
              v            v
      WMS Direct       Native WinMM
```

Cross-cutting services:

- diagnostics;
- settings;
- logging;
- file I/O;
- provenance/version metadata.

## 3. Non-negotiable invariants

### Transport isolation

- exactly one active transport backend owns a logical connection;
- no hidden parallel/shadow receiver;
- no split architecture where one backend receives SysEx and another receives channel MIDI;
- WMS and WinMM are alternative transports behind a common contract.

### Device isolation

- no synth-specific branches inside WMS/WinMM;
- device-specific behavior lives in profiles/protocol modules above transport;
- generic monitoring and raw SysEx work without any device module.

### Data integrity

- MIDI 1.0 SysEx bytes are preserved exactly;
- incomplete/overflow captures cannot masquerade as valid complete captures;
- no speculative repair;
- WMS UMP information is not irreversibly flattened before the application has extracted what it needs.

### Concurrency/lifetime

- native callbacks do minimal work;
- callbacks never touch Qt widgets;
- callback-owned data is copied/moved into application-owned queues;
- queues are bounded or otherwise explicitly controlled;
- overflow is observable;
- shutdown prevents callbacks/use-after-free after destruction;
- no detached worker threads.
- cross-thread `QPointer` access is not a synchronization or lifetime mechanism;
- production shutdown uses an explicit acceptance gate, callback/event revocation, resource close, worker-
  thread teardown, and worker completion before dependent object destruction.

### Routing

- RX and TX routes may differ;
- WMS group identity is represented where relevant;
- persisted identity is not only a numeric port index;
- no fuzzy silent route substitution.
- route identity is backend-specific and includes the backend discriminator;
- WMS endpoint/group identity and individual WinMM MIDI 1.0 port identity are not transferable;
- changing backend invalidates or requires exact re-resolution of persisted routes;
- missing or ambiguous resolution requires visible user selection; never silently translate/rebind across
  backends.

## 4. MIDI transport contract

The transport layer owns only:

- endpoint enumeration;
- endpoint identity;
- open/close;
- native receive;
- native send;
- transport errors;
- transport-level capabilities;
- timestamp delivery/conversion contract.

It does **not** own:

- device recognition;
- pacing policy;
- bank semantics;
- checksums;
- restore workflows;
- GUI presentation.

## 5. Windows MIDI Services backend

Target properties:

- direct official Windows MIDI Services App SDK integration;
- no `midi.exe` subprocess;
- native endpoint/group information;
- UMP-native receive representation;
- SysEx7 reassembly from UMP packets;
- deterministic initialization and shutdown.

The exact package, namespace, runtime initialization model, API-mode query, timestamp behavior, and maximum-transfer behavior are **Stage 0/1 verification items**, not assumptions frozen by this document.

## 6. Native WinMM backend

Compatibility backend for legacy/vendor environments.

Requirements include:

- native enumeration;
- short-message handling;
- long-message/SysEx buffers;
- explicit `MIDIHDR` ownership;
- prepare → queue → completion → unprepare lifecycle;
- deterministic shutdown;
- no RtMidi wrapper hidden under a native name.

Stage 1's spike requeued input headers with `midiInAddBuffer` inside `midiInProc`. This is not the production
design decision and its virtual-loopback success does not establish vendor-driver safety. Stage 2 must keep
native callbacks minimal and evaluate/prefer callback -> signal/queue -> worker-owned `MIDIHDR` requeue.
Production output-header ownership and failure paths require RAII.

The pinned RC4 WMS/WinMM correlation helpers fail-fast in the isolated Stage 1 probe. They are not an
architecture dependency and must not be used to silently translate route identities across backends.

## 7. MIDI Core

The MIDI Core owns generic representation and parsing of:

- MIDI 1.0 channel voice;
- system common;
- realtime;
- Polyphonic Aftertouch;
- Program Change;
- Pitch Bend;
- Clock/transport;
- Active Sensing;
- SysEx frame boundaries;
- retained UMP representation/metadata where needed.

It has no GUI dependency.

## 8. SysEx engine

Responsibilities:

- `F0 ... F7` framing;
- WMS SysEx7 UMP reassembly;
- WinMM buffer/chunk assembly;
- `.syx` load/save;
- incomplete-frame detection;
- legal realtime interleaving handling;
- exact roundtrip;
- frame/byte accounting.

The SysEx engine treats payloads as bytes unless a device protocol above it supplies semantics.

## 9. Transfer engine

The transfer engine is separate from transport.

It owns:

- frame sequencing;
- inter-frame delay/pacing;
- cancellation;
- timeout;
- progress;
- optional request/response state machines through protocol hooks;
- destructive-operation classification/warnings;
- profile-specific transfer policy.

Transport only moves messages from A to B.

## 10. Device/profile model

### Data-driven profiles

Suitable for:

- manufacturer/model labels;
- port aliases;
- CC/NRPN/RPN names;
- enum/toggle mappings;
- monitor labels;
- default channel;
- bank/slot display metadata;
- default pacing;
- safe warnings.

Preferred format: versioned JSON or equivalent simple data.

### Compiled protocol modules

Required for:

- checksums;
- request/response;
- ACK/NAK;
- non-trivial state machine;
- semantic dump validation;
- staged transfer;
- binary codecs;
- device-specific bank import/export.

Do not create a JSON mini-language for complex protocols.

## 11. Librarian architecture

The librarian is a domain/application layer above generic file/transfer services.

Generic domain objects may include:

- Preset;
- Bank;
- Slot;
- Collection;
- Source File.

Device modules define the actual valid combinations, capacities, dependencies, and codecs.

The librarian reuses the common transfer engine.

## 12. GUI boundary

Qt 6 Widgets provide presentation and interaction.

Rules:

- no native MIDI callback accesses Qt widgets;
- Qt models/views are used for high-volume tabular data;
- GUI updates are batched/throttled;
- business/protocol logic stays outside widgets;
- the clickable HTML mockup is a workflow/design reference only.

## 13. Settings and diagnostics

Settings use a versioned schema.

Persist stable route identity, never numeric index alone.

Corrupt or obsolete settings fall back safely.

Diagnostics must expose enough information to explain:

- selected backend;
- API/runtime state;
- selected route/group;
- connection state;
- RX/TX counts;
- dropped/overflow counts;
- SysEx frame/byte counts;
- last transport error;
- disconnect/reconnect state.

## 14. Architecture decisions still open

Stage 1 resolved the spike package/initializer mechanics and demonstrated backend-specific identity. Stage 2
and later must consume that evidence while resolving these production decisions:

- production WMS SDK/runtime version and deployment policy, including supported API-mode detection;
- serialized backend-discriminated route identities and exact backend-change re-resolution UX;
- timestamp normalization and WMS maximum-transmission constraints;
- MIDI 1.0 `F0 ... F7` byte-stream <-> UMP SysEx7 conversion and segmentation/reassembly;
- WinMM callback-to-worker requeue and complete RAII ownership/error paths;
- WMS integration regression availability/skip policy;
- Qt/WMS lifetime and apartment behavior in the actual `QApplication` product host.

No implementation should guess these where official documentation or spike evidence is required.


## 15. Suggested repository structure

The exact tree may evolve, but architectural boundaries should remain visible.

```text
TAUREON-Synth-Tool-V4/
├─ CMakeLists.txt
├─ CMakePresets.json
├─ README.md
├─ CHANGELOG.md
├─ .github/
│  └─ workflows/
├─ cmake/
├─ docs/
├─ resources/
│  ├─ manufacturers/
│  └─ device_profiles/
├─ spikes/
│  ├─ wms_direct/
│  └─ winmm_native/
├─ src/
│  ├─ app/
│  ├─ core/
│  │  ├─ midi/
│  │  ├─ sysex/
│  │  ├─ transfer/
│  │  ├─ diagnostics/
│  │  └─ settings/
│  ├─ transports/
│  │  ├─ wms/
│  │  └─ winmm/
│  ├─ protocols/
│  └─ gui/
├─ tests/
│  ├─ unit/
│  ├─ integration/
│  ├─ fixtures/
│  └─ fakes/
├─ tools/
└─ packaging/
```

Do not create empty directories merely to satisfy the diagram. Add them when their responsibility exists.
