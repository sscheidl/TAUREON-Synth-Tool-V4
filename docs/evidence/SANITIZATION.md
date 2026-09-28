# Evidence sanitization (2026-09-28, PR #12)

Before PR #12 was merged into the public `main`, raw system-specific identifiers
were removed from the retained evidence. No technical result was changed.
Git history was not rewritten; earlier published commits still contain the raw
captures.

## Removed files (`reconstruction-20260926/`)

Raw device inventories whose findings are already stated in `STATUSBERICHT.md`
(sections 3, 5, 8–9) and `endpoint-cleanup.json`:

- `summit-online-pnp.json` (PnP instance IDs including the Summit USB serial)
- `summit-online-winmm.jsonl`, `winmm-enumeration-before.jsonl`,
  `winmm-enumeration-after.jsonl` (full WinMM port inventory)
- `summit-online-wms.txt`, `wms-endpoints-before.txt`, `wms-endpoints-after.txt`
  (full WMS endpoint inventory with device endpoint IDs)
- `legacy-winrt-before.txt` (full MIDI 1.0 port inventory with device interface IDs)

The before/after equality of the WinMM and WMS enumerations remains recorded as
`WinmmUnchanged`/`WmsUnchanged` in `endpoint-cleanup.json`.

## Placeholders in retained files

| Placeholder | Replaced value |
|---|---|
| `<SUMMIT_USB_SERIAL>` | USB instance serial of the Summit (VID/PID kept) |
| `<SUMMIT_WMS_ENDPOINT_KEY>`, `<KONTROL_S61_MK3_WMS_ENDPOINT_KEY>`, `<M8U_EX_WMS_ENDPOINT_KEY>`, `<AF16RIG_WMS_ENDPOINT_KEY>`, `<MINIFREAK_WMS_ENDPOINT_KEY>`, `<IRIDIUM_WMS_ENDPOINT_KEY>` | Device-specific `midiu_ks[a]_<number>` part of WMS endpoint IDs |
| `<TEMP_LOOPBACK_KEY>` | Random key of temporary test loopback endpoints (names kept) |
| `<LOCAL_PATH>` | Local repository root path |
| `<USER_PROFILE>` | Windows user profile path |
| `<PROCESS_ID>` | Windows process ID in a WMS session listing |
| `<AUTHOR_EMAIL>` | Commit author e-mail in the CI run JSON |
| `(private ChatGPT conversation; link removed)` | Private conversation URLs in `STATUSBERICHT.md` |

Kept on purpose: device and loopback names, VID/PID, the generic WMS interface
GUID, standard tool locations (`C:/Qt/6.10.3/msvc2022_64`,
`C:\Program Files\Windows MIDI Services`), commit SHAs, CI run IDs, test counts,
durations, counters, handle trends, lifecycle and apartment results. The GitHub
hosted-runner CI log (`pr12-ci.log`) contains no local-system data and is unchanged.

`reconstruction-20260926/SHA256SUMS.json` now lists the retained files with their
sanitized hashes. The `EndpointSnapshotSha256` in `p1-closure-summary.json` still
matches the retained, unchanged `p1-soak-endpoints-before.txt` and
`p1-soak-endpoints-after.txt`.
