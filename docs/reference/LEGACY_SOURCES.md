# Legacy & Reference Sources

The old TAUREON project remains available for read-only reference.

## Primary legacy project

Local:

```text
D:\Eigene Dateien\Eigene Dokumente\Playground\TAUREON-Synth-Tool
```

GitHub:

```text
https://github.com/sscheidl/TAUREON-Synth-Tool
```

## What may be reused conceptually

- protocol facts;
- observed device behavior;
- known-good SysEx captures;
- fixture sizes/hashes/frame counts;
- pacing observations;
- file-format findings;
- previous failure cases;
- UI/workflow lessons.

## What requires provenance/licensing review

- source code copied into V4;
- third-party code;
- vendor utilities;
- externally sourced fixtures/data redistributed with V4;
- code originating from forks/side projects with unclear licenses.

Prefer reimplementation from documented behavior when code provenance is unclear.

## Known planning/research sources

Examples from the previous project discussions include:

- native WinMM capture work (`engine3_native`);
- MIDI CC/device-profile research;
- validated captures for minilogue xd, Summit, Artemis, Virus A, K5000S, TEO-5, and others;
- K5000 librarian GUI interaction prototype.

Stage 0 creates a concrete `LEGACY_INVENTORY.md` and `PROVENANCE.md` from the actual local repository state. This document is only the starting map.
