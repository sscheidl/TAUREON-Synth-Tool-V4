# Legacy Inventory

**Stage 0 inspection date:** 2026-08-24  
**Boundary:** The legacy tree was inspected read-only. No legacy file was copied, edited, or executed for V4.

## Reference baseline

- Local source: `D:\Eigene Dateien\Eigene Dokumente\Playground\TAUREON-Synth-Tool`
- Remote: `https://github.com/sscheidl/TAUREON-Synth-Tool`
- Inspected HEAD: `756b477e6d6de733f4ad5df12dd08b24ae449ae4` (`2026-06-24`, `Fix SysEx receive drain and restructure GUI`)
- The legacy worktree has user-owned untracked files. They were not touched and are not V4 input.

| Source | Type | Relevant behavior/data | Evidence | Reuse recommendation | Provenance status |
|---|---|---|---|---|---|
| `engine3_native/` | C++17 WinMM capture PoC | Native receive, raw `.syx` write, large capture buffers, diagnostic accounting | `README.md`, `CMakeLists.txt`, source SHA-256 `C8E26C1B0737354FD491408FEA32A69B58C8F3924C032C413F0E7539FF83E610` | Treat as Stage 1 behavioral/lifecycle research only; do not copy source | No root license file found; code reuse prohibited pending explicit provenance/license decision |
| `taureon_synth_tool/sysex_utils.py`, `capture_core.py`, and tests | Python behavioral reference | Exact-byte save, incomplete-frame warnings, and device-free generic core constraints | `test_raw_capture_integrity.py` SHA-256 `0D8ED4A430265DA145490CB3D7FE59CFE585510A3BCF940BE2987775F3BDAC15` | Recreate independent C++ tests from observed requirements, without code translation/copying | No migration; Python source remains reference only |
| Root `dump_2026-06-23_*_INCOMPLETE.syx` | SysEx data | Large capture failure examples: 856,030 bytes and 842,886 bytes | SHA-256 `4C051F645C1A0ED538AD8346CFD4814AA98AFDC7391742BF49DEC4E4F861F503` and `E073F7F6070B60E0884C8E6C26D82BE46BD497D81EA12D7B82F513BAAD510381` | Do not migrate as known-good fixtures; retain only as evidence of incomplete-capture cases | User/local capture provenance not yet release-cleared |
| `docs/*_CAPTURE_NOTES.md` and `docs/NATIVE_CAPTURE_VALIDATION_INDEX.md` | Hardware/test records | Device-specific observations for Artemis, Kawai K5000S, Moog Subsequent 37, Summit, and TEO-5 | Legacy README marks the application experimental; records are not V4 validation | Use as historical questions/checklist input only | Hardware observations are not transferable V4 acceptance evidence |
| `resources/midi_profiles/generated/*.json` | Device/profile research | 19 generated profiles and their source metadata | `resources/midi_profiles/README.md` says official documentation first, other sources as fallback | No profile migration in Stage 0; later assess each profile's embedded sources and redistribution rights | Individually unreviewed; no essential dependency |

No verified known-good binary SysEx fixture was found in `captures/`; that directory contained no regular files during this inventory. The only root-level `.syx` candidates are explicitly named `INCOMPLETE` and must never be presented as valid restore data.
