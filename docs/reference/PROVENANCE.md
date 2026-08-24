# Provenance

**Stage 0 status:** No legacy code, data, fixture, profile, or third-party asset has been migrated into V4.

| Component | Source | Author/owner | License | V4 use | Redistribution allowed? | Evidence/notes |
|---|---|---|---|---|---|---|
| V4 bootstrap source and CMake files | This repository | TAUREON V4 project | License not selected | Minimal C++20 build/test only | Not decided | Created new in Stage 0; contains no borrowed implementation |
| Legacy WinMM/Python sources | `TAUREON-Synth-Tool` | Local legacy project; individual source origin not audited | No root license file found | None | Not established | Behavioral reference only; no copy or translation performed |
| Legacy SysEx dumps and generated profiles | `TAUREON-Synth-Tool` | Mixed local/device-research sources | Per-item provenance not audited | None | Not established | No data migrated; incomplete dumps are explicitly excluded from fixtures |
| Windows MIDI Services SDK/runtime | Microsoft installation / official distribution | Microsoft | Governed by its distributed terms | Local development/runtime evidence only | Not bundled by Stage 0 | Integration/deployment terms must be captured with the pinned Stage 1 SDK package |
| Qt 6.10.3 | Local Qt installation | The Qt Company/contributors | Not yet selected/recorded for V4 distribution | Availability verified only | Not bundled by Stage 0 | Qt packaging and license obligations are Stage 5/7 work |

Rules:

- Protocol facts and observed behavior are not reusable source code.
- Do not copy third-party code merely because it is locally available.
- If reuse is non-essential and licensing is unclear, reimplement from documented behavior.
- If reuse becomes essential and licensing remains unclear, stop and ask the User.
