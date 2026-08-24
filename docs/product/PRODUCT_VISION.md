# TAUREON V4 – Product Vision

**Status:** Active product definition  
**Owner:** User / Product Owner  
**Maintained by:** Project Manager

## 1. Product intent

TAUREON is a modular Windows utility for reliable MIDI and SysEx work with hardware synthesizers.

It addresses a practical gap: many classic MIDI/SysEx tools are old, device-specific, difficult to use on current Windows systems, or combine monitoring, transfer, and librarian functions poorly.

TAUREON should provide one consistent workflow while keeping generic MIDI/SysEx functionality independent from device-specific knowledge.

Guiding principle:

> **Unknown devices are handled generically; known devices are handled intelligently.**

## 2. Product domains

### 2.1 MIDI Monitor

Purpose:

> Show what is actually transmitted on the selected MIDI routes.

Core capabilities:

- MIDI input monitoring;
- logging of TAUREON-originated output where useful;
- timestamp;
- direction;
- route/endpoint;
- group where applicable;
- channel;
- event type;
- parameter/controller;
- value;
- raw bytes/UMP details.

Message coverage should include:

- Note On/Off;
- Control Change;
- Program Change;
- Channel Pressure;
- Polyphonic Aftertouch;
- Pitch Bend;
- RPN/NRPN where reconstructable;
- MIDI Clock and transport;
- Active Sensing;
- System Common/Realtime;
- SysEx.

Filters:

- event type;
- channel;
- route;
- direction;
- controller/parameter where useful;
- SysEx visibility.

Profile-aware interpretation may add device-specific names and symbolic values. Unknown data stays unknown rather than guessed.

Possible later diagnostics:

- message rate;
- CC flood detection;
- potentiometer/encoder jitter detection;
- feedback-loop warnings;
- timing/latency analysis;
- incomplete-frame detection;
- disconnect/reconnect history.

### 2.2 SysEx / Transfer / Conversion

Purpose:

> Receive, preserve, inspect, transmit, and transform MIDI System Exclusive data.

Receive/capture:

- explicit start/stop;
- clear session boundaries;
- complete-frame accounting;
- byte count;
- incomplete-capture detection;
- recovery save when useful;
- no silent repair.

Raw send:

- explicit output route;
- exact payload;
- pacing;
- progress;
- cancellation;
- no automatic send at startup.

Validated restore:

- distinct from raw send;
- offered only where a device profile/protocol knows enough to validate the workflow;
- may enforce pacing, message order, target rules, handshake, staging, or user confirmation.

SysEx file management:

- load/save `.syx`;
- inspect frames;
- split/merge;
- extract individual frames;
- compare files;
- hash/duplicate checks;
- validate framing;
- batch file/collection operations.

Conversion:

- `.syx` ↔ Standard MIDI File where unambiguous;
- device-specific formats through optional codecs/protocol modules;
- format support is capability-based, not all-or-nothing.

Support levels for a device format should be distinguishable:

```text
Detect → Read → Inspect → Extract → Modify → Serialize → Transfer → Validated Restore
```

A parser being able to read a format does not imply safe rewrite/restore.

### 2.3 Librarian / Preset Management

Purpose:

> Organize sounds, presets, banks, slots, and collections for supported devices.

Generic domain objects:

- Preset / Program / Patch;
- Multi / Performance / Combination;
- Bank;
- Slot;
- Library / Collection;
- Source File;
- optional Category / Tag / Rating / Comment.

Operations may include:

- load/save/save-as;
- create bank;
- merge/split banks;
- append/replace/extract content;
- multi-file import;
- batch export;
- select/multi-select;
- copy/cut/paste;
- move;
- drag & drop;
- delete/empty slot;
- rename;
- sort/search/filter;
- undo/redo where practical;
- duplicate detection.

For bank-oriented devices, a large reusable slot/preset matrix is the preferred interaction model.

Duplicate detection should distinguish:

1. **Exact file duplicate**
2. **Exact preset payload duplicate**
3. **Possible duplicate** — advisory only, never automatic deletion

The librarian does not implement a second MIDI/SysEx engine. Transfer actions call the shared transfer engine.

### 2.4 Devices & Profiles

Purpose:

> Describe which device is connected and what TAUREON safely knows about it.

Per configured device, TAUREON may store:

- user-visible name;
- manufacturer/model/variant;
- preferred MIDI input/output;
- aliases;
- channel;
- SysEx Device ID;
- assigned profile/protocol package;
- device-specific overrides.

Detection/suggestion may use, in decreasing reliability:

1. saved explicit device/port assignment;
2. clear USB/endpoint/port identity;
3. Universal MIDI Identity where supported;
4. known SysEx fingerprints;
5. manual selection;
6. Generic MIDI fallback.

For generic DIN-interface ports, TAUREON must not infer the attached synth from the interface name alone. Explicit user binding is safer.

Device knowledge has two implementation levels:

- **data-driven profile** for names, mappings, aliases, pacing defaults, bank metadata, warnings;
- **compiled protocol module** for state machines, handshakes, checksums, semantic validation, codecs, staged transfer.

### 2.5 Options / System

Global configuration may include:

- application paths;
- session behavior;
- confirmation policy;
- backend preference;
- route preferences;
- reconnect policy;
- generic SysEx pacing defaults;
- logging;
- theme and accessibility;
- monitor-history limits;
- diagnostics/export.

Device-specific settings belong primarily to the device/profile, not global options.

## 3. Cross-cutting diagnostics

Diagnostics is a cross-cutting capability, not a sixth product domain.

The GUI may expose Diagnostics as a dedicated workspace because operational troubleshooting benefits from a single place that shows:

- application/build info;
- Windows/MIDI backend state;
- routes/groups;
- counters;
- errors;
- queue high-water marks;
- disconnect/reconnect transitions;
- diagnostic-bundle export.

## 4. First stable V4 scope

The first production-capable V4 release must prioritize reliability over feature breadth.

Baseline scope:

- native WMS + WinMM transport to agreed release scope;
- generic MIDI Monitor;
- generic SysEx receive/save/load/send;
- file/frame inspection;
- safe transfer engine with pacing/cancel/progress;
- data-driven profile infrastructure;
- at least one real profile/fixture path proving device isolation;
- Qt GUI;
- diagnostics/settings;
- packaging and hardware-validation workflow.

A **full universal librarian/database** is not required for V4.0. The architecture and GUI must, however, provide a clean foundation for it. Limited file/library organization may be included if it is stable and does not delay the reliable core.

## 5. Non-goals for V4.0

Not required:

- macOS/Linux;
- DAW/plugin hosting;
- audio streaming;
- full MIDI-CI;
- full MIDI 2.0 Property Exchange;
- SysEx8 semantic editing;
- firmware flashing;
- automatic driver repair/management;
- cloud accounts;
- universal graphical synth editors.

## 6. Product success

TAUREON succeeds when:

1. an unknown MIDI synth can be monitored and used with generic SysEx without a profile;
2. a known synth can gain understandable names, transfer rules, format support, and librarian behavior without contaminating generic transport code;
3. transport/backend failure does not require rewriting the product architecture;
4. new device support can be added without model-specific branches in central GUI/core classes;
5. preset/bank workflows use modern desktop interactions rather than overloaded legacy-librarian screens;
6. the user can always identify active device, route, profile, and transfer mode;
7. TAUREON never labels a write/restore path as validated unless that path has actually been validated.
