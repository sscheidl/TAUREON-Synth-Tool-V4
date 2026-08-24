# TAUREON V4 – GUI Design Specification

**Status:** Active design baseline / pre-Stage-5  
**Implementation:** Stage 5, Qt 6 Widgets  
**Current prototype:** `TAUREON_V4_GUI_MOCKUP.html`

The clickable HTML prototype exists only to test workflow, information hierarchy, wording, and visual density. It contains no MIDI/file/backend implementation and must not be mechanically translated into Qt.

## 1. Design principles

- professional desktop utility, not a decorative synth skin;
- clear functional separation;
- large tables/workspaces instead of many tiny panes;
- readable at 1920×1080;
- High-DPI aware;
- no tiny fixed fonts;
- status never communicated only by color;
- raw MIDI/SysEx data remains accessible;
- no GUI object access from native MIDI callbacks;
- high-volume views use Qt model/view and batched updates.

Visual direction:

- neutral dark/light system-compatible presentation;
- restrained τAUREON gold/amber accent;
- Windows/Qt-like controls;
- minimal decoration.

## 2. Product domains vs GUI workspaces

The product has five logical domains, but the GUI may split them for usability.

Current workspace model:

1. MIDI Monitor
2. SysEx Transfer
3. SysEx Manager
4. Library / Librarian
5. Devices & Profiles
6. Diagnostics
7. Settings

This is intentional:

- **SysEx Transfer** handles one active receive/send workflow.
- **SysEx Manager** handles files/frames/collections independently of a live transfer.
- **Library/Librarian** handles semantic presets/banks/slots rather than raw SysEx frames.
- **Diagnostics** is cross-cutting but receives its own workspace for troubleshooting.

## 3. Persistent connection bar

Visible across all workspaces.

Contains:

- Backend: Auto / Windows MIDI Services / WinMM
- Connection state
- MIDI Input
- MIDI Output
- group/function-block selection when technically required
- Connect / Disconnect
- Panic
- concise backend/API/runtime status where useful

The bar must show enough identity to avoid sending to the wrong route.

## 4. MIDI Monitor

Primary view: one large table.

Suggested columns:

- Time
- Direction
- Route
- Group (when applicable)
- Channel
- Type
- Parameter / Event
- Value
- Raw

Toolbar:

- MIDI Profile
- profile status
- Edit Profile
- channel filter
- event-type filters
- SysEx visibility
- Clock/Active Sensing filters
- Pause
- Clear
- Copy/Export

Rules:

- profile interpretation changes labels/semantic values only;
- raw controller/message identity remains available;
- unknown events remain unknown rather than guessed;
- monitor history is bounded;
- GUI updates are batched/throttled.

## 5. SysEx Transfer

Purpose: active receive/send session.

Show:

- loaded/received filename;
- manufacturer when recognized;
- profile;
- frame count;
- byte count;
- framing/completeness state;
- frame inspector;
- selected output route;
- pacing mode;
- Receive
- Send
- Cancel
- Save received data
- Clear
- progress
- transfer log/status.

Safety:

- no automatic payload repair;
- raw send is explicit;
- destructive known restore is warned/confirmed;
- device validation shown only where protocol knowledge actually exists.

## 6. SysEx Manager

Purpose: manage SysEx files without implying an active hardware transfer.

File table:

- File
- Device/Manufacturer
- Frames
- Size
- Status

Actions may include:

- Inspect Frames
- Open in Transfer
- Split into Frames
- Merge Selected
- Compare
- Duplicate
- Rename
- Export Copy
- Scan / Identify
- Calculate Hashes
- Find Duplicates
- Validate Framing
- Sort/Group
- Create Collection

This workspace remains distinct from the Librarian because it operates on files/messages rather than semantic presets/banks.

## 7. Library / Librarian

V4.0 may begin with a limited foundation; the UI must not fake a full universal librarian before the domain model is ready.

Long-term primary model for bank-oriented devices:

- large slot/preset matrix;
- variable bank/slot capacity;
- multi-selection;
- Ctrl/Shift selection;
- lasso where appropriate;
- Copy/Cut/Paste;
- F2 Rename;
- Delete;
- context menu;
- drag & drop;
- Undo/Redo where supported;
- inspector for current selection.

Device rules determine capacities, object types, compatibility, and valid operations.

## 8. Devices & Profiles

Show configured/generated/user profiles clearly.

Profile detail may show:

- manufacturer;
- model/variant;
- status;
- profile type;
- source/provenance;
- firmware scope;
- port aliases/detection hints;
- supported control/transfer/format capabilities.

Actions:

- Edit JSON/data
- Duplicate to user profile
- Validate
- Open folder
- explicit "bind/use this profile for this port" where appropriate

Rules:

- generated/reference profiles are not silently overwritten;
- edits create/update user-owned profiles;
- invalid data cannot be silently saved;
- manual profile selection does not automatically persist a port binding unless the user explicitly chooses to remember it;
- generic DIN interface names are not used for fuzzy synth guesses.

## 9. Diagnostics

Dedicated technical workspace.

Show/export:

- app version/build;
- OS/process architecture;
- selected backend;
- current Windows MIDI API mode when supported;
- WMS runtime/SDK where detectable;
- endpoint IDs/names/groups;
- connection state;
- RX/TX counters;
- dropped/overflow counters;
- SysEx frames/bytes;
- queue high-water state;
- last transport error;
- disconnect/reconnect transitions.

Provide `Export Diagnostic Bundle`.

Do not include user SysEx dumps in the bundle by default.

## 10. Settings

Use a versioned settings schema.

Groups:

### General
- theme;
- UI density/font scaling where appropriate;
- standard paths;
- session behavior.

### MIDI
- default backend preference;
- preferred routes;
- reconnect behavior;
- monitor defaults.

### SysEx
- generic pacing defaults;
- confirmation policy;
- capture/transfer safety defaults.

### Logging/Diagnostics
- log level;
- log destination/rotation;
- diagnostic export preferences.

Device-specific timing/behavior belongs to the device/profile unless explicitly overridden.

## 11. Stage-5 design freeze

Before Stage 5 implementation begins, the Project Manager and User confirm:

- workspace structure;
- terminology;
- connection-bar fields;
- monitor columns/filters;
- transfer workflow;
- SysEx Manager scope;
- minimum Librarian scope for V4.0;
- profile editing/binding behavior;
- visual density/accessibility.

After design freeze, changes are normal product changes, not silent implementation improvisation.
