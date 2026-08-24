# Device Profile Schema v1

**Status:** Stage 4 active schema
**Format:** UTF-8 JSON
**Schema version:** `1`

Profiles are descriptive data above the generic MIDI/SysEx/transfer core. They do not select a transport route,
mutate raw bytes, authorize an operation, or provide executable behavior.

## Required root fields

Every `*.profile.json` file contains exactly these fields; unknown fields fail validation:

| Field | Type | Rule |
|---|---|---|
| `schema_version` | integer | Must equal `1` |
| `profile_id` | string | Stable lowercase dotted/dashed identifier |
| `profile_version` | string | `major.minor.patch` |
| `generic` | boolean | Generic must make no device/support claim |
| `manufacturer`, `model`, `variant` | string or null | Manufacturer/model required for real profiles and forbidden for Generic |
| `display_name` | string | Non-empty user-facing name |
| `aliases` | string array | Exact, duplicate-free descriptive aliases |
| `support` | object | All eight support claims are required booleans |
| `metadata` | object | Channels, CC, RPN, NRPN and optional bank description |
| `defaults` | object | Optional channel and SysEx inter-message delay |
| `warnings` | string array | Non-empty safety/descriptive warnings |
| `recognition` | object | Optional native/Universal identity and deterministic SysEx fingerprints |
| `provenance` | object | Required source, owner, license, redistribution and acquisition strings |
| `protocol_module_id` | string or null | Identifier only; it does not load code |

## Support ladder

All claims are explicit:

```text
Detect -> Read -> Inspect -> Extract -> Modify -> Serialize -> Transfer -> Validated Restore
```

A `true` claim requires every preceding level to be explicitly `true`; gaps are rejected. A lower level never
implies any higher level. The Stage-4 Summit profile claims only `Detect`.

## Metadata ranges

- channels: `1..16`, no duplicates;
- CC numbers: `0..127`, no duplicate number;
- RPN/NRPN numbers: `0..16383`, no duplicate number;
- bank/slot counts: `1..65535` when present;
- default channel: `1..16` when present;
- profile pacing: `0..10000` ms when present.

Profile pacing is application/profile policy above `IMidiTransport`; it is not a transport default.

## Recognition

Recognition evidence is evaluated in this order after the capture integrity gate:

1. saved explicit user binding;
2. exact device-native identity;
3. exact Universal MIDI Identity;
4. exact SysEx fingerprint;
5. manual selection;
6. Generic fallback.

Fingerprints contain a stable ID, byte offset and an array of byte values `0..255`. Equal candidates produce
`Ambiguous`; registration order is irrelevant. Port/display names and backend route identities are never device
identity evidence.

## Validation behavior

Invalid JSON, duplicate object keys, non-integer numbers, unsupported schema versions, unknown fields, wrong
types, missing fields, invalid ranges, duplicate IDs/numbers/fingerprints, contradictory support and malformed
recognition data fail visibly. Directory loading reports invalid files individually and does not let them replace
an already valid Generic fallback.
