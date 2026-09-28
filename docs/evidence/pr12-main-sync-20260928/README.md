# PR #12 main-sync verification (2026-09-28)

Scope: local software and loopback evidence after merging `main` (PR #15, engine
0.3.0) into PR #12. No physical MIDI device, SysEx send, or visual GUI acceptance
is claimed.

Code under test: merge commit `28275605b4479d43544b29ba02663697444ed283`. The
only later change is documentation (DECISION_LOG D-021 cross-reference and this
evidence folder); no source, test, or build file differs.

Build: Visual Studio 2022 x64 Debug, Qt 6.10.3 msvc2022_64,
`TAUREON_ENABLE_WMS_TRANSPORT=ON`, `TAUREON_ENABLE_LOCAL_MIDI_INTEGRATION_TESTS=ON`.

| Check | Result | Log |
|---|---|---|
| Non-local CTest (`-LE local-midi`) | 24/24 passed | `software-tests.log` |
| Product-host gate, 5 cycles per backend | passed (twice) | `product-host-5cycles-run1.log`, `product-host-5cycles-run2.log` |
| Product-host soak, 100 cycles per backend, handle-growth gate applied | passed | `product-host-soak-100cycles.log` |

Every run proved receive-active close, successful closes with final state
`closed`, 0 dropped events, 0 late callbacks, GUI apartment `main_sta`, and
`worker_mta_apartment_observed == true` for the WMS worker.

The two non-gating 5-cycle WMS handle records showed a rise of two handles over
the first samples (463→465, 467→469). Because this differed from the
2026-09-26 reference, the 100-cycle soak was run: WMS slope -0.104 (453–457
steady), WinMM slope -0.038 (481–483 steady), no sustained growth for either
backend. The short-run rise is a start-up plateau, not cumulative growth.

Each run created one uniquely named temporary WMS loopback pair and removed it;
the script asserted absence afterwards and a final `midi.exe endpoint list`
showed no remaining `TAUREON S5` endpoints.
