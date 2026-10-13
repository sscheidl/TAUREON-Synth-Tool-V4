# Preview 11 app-local deployment — 2026-10-13

Base: `origin/main` at `bb05a24e5c97ecf0cd1f48bdf6c2093fb56f9fc7`, including
PR #27's Preview-11 pin/cache remediation. This follow-up implements Pete Brown's
[PR-23 recommendation](https://github.com/sscheidl/TAUREON-Synth-Tool-V4/pull/23)
to replace the Tools-directory lookup with regular app-local activation.
No routing, SysEx protocol, driver/service configuration or synthesizer logic changes.

## Local software evidence

Windows x64 build 26300, VS 2022 v143/MSVC 19.44, Windows SDK 10.0.26100.0,
Qt 6.10.3. Commands ran against the implementation snapshot in this branch;
the PR records the immutable commit and exact Actions evidence separately.

| Check | Result |
| --- | --- |
| Fresh official package downloads and repeat acquisition | PASS: NuGet, C++/WinRT, WINMD, DLL and PRI hashes match the pins in `WMS_DEPENDENCIES.md`. |
| Deliberately modified cached NuGet | PASS: SHA-256 rejection before extraction. |
| WMS ON full Debug build | PASS; 26/26 non-local CTest, none failed/skipped, 17.23 s. |
| WMS OFF full Debug build | PASS; 26/26 non-local CTest, none failed/skipped, 16.77 s. |
| WMS ON RelWithDebInfo application | PASS. |
| Installed engine + separate WMS consumer | PASS build/link; 2/2 registered fake-engine/non-opening WinMM consumer tests. |
| Consumer with no API beside it | PASS `--expect-unavailable`: no loaded API, `backend_unavailable`, retained HRESULT `-2147221164` (class not registered), despite installed Tools. |
| Consumer with official app-local DLL/PRI | PASS `--probe-api`: actual module equals the expected app-local file; `IsSystemProvided=false`; 17 RX / 17 TX routes, no port opened. |
| Fresh portable RelWithDebInfo folder | PASS after deploying Qt, CRT and the hash-checked API pair. Offscreen shell smoke plus `--list-midi wms` (17 RX / 17 TX) and `--list-midi winmm` (17 RX / 18 TX). |
| Portable SystemOnly cleanup | PASS: WMS returns activation failure and WinMM enumeration still succeeds. |
| Same CMake output switched Preview ON → OFF | PASS: rebuild removes the previously deployed API pair. |
| Portable cleanup regression | PASS: idempotence, raw user payload/executable preservation, refusal of directory targets before mutation and failed Preview preflight without overwriting the old DLL. Included in both 26-test suites. |
| Existing Auto fallback/routing/SysEx software tests | PASS in both suites; unavailable/empty WMS selection and exact route identity are covered by existing injected/fake tests. |

The first portable smoke attempt was stopped after incomplete dependency staging.
The completed folder includes app-local VC143 CRT and a matching Qt offscreen
plugin for this local software check; the repeated smoke and enumeration passed.
The CI packaging path already stages CRT and uses its normal shell smoke.
The builds retain the existing C++20 `u8path` deprecation warnings in Settings code.
Raw local enumeration logs contain device names and remain untracked; only counts
are recorded here. No endpoint was opened and no MIDI/SysEx data was sent.

Representative commands (paths are relative to the checkout):

```powershell
.\tools\AcquireWmsDependencies.ps1
cmake -S . -B build/app-local -G "Visual Studio 17 2022" -A x64 -T v143 `
  -DCMAKE_PREFIX_PATH='<Qt-MSVC-prefix>' -DBUILD_TESTING=ON `
  -DTAUREON_ENABLE_WMS_TRANSPORT=ON -DTAUREON_ENABLE_LOCAL_MIDI_INTEGRATION_TESTS=OFF
cmake --build build/app-local --config Debug --parallel 6
ctest --test-dir build/app-local -C Debug -LE local-midi --output-on-failure
# Repeat in build/no-wms with TAUREON_ENABLE_WMS_TRANSPORT=OFF.
cmake --build build/app-local --config RelWithDebInfo --target taureon_app --parallel 6
cmake --install build/app-local --config RelWithDebInfo --prefix build/app-local-install
cmake -S tests/consumer -B build/app-local-consumer -A x64 `
  -DCMAKE_PREFIX_PATH='<absolute-build/app-local-install>' -DTAUREON_CONSUMER_WITH_WMS=ON
cmake --build build/app-local-consumer --config RelWithDebInfo
ctest --test-dir build/app-local-consumer -C RelWithDebInfo --output-on-failure
.\tools\SetWmsAppLocalDeployment.ps1 -Destination '<portable-folder>' -Mode Preview
# In the portable folder, with QT_QPA_PLATFORM=offscreen for this software-only check:
.\TAUREON-Synth-Tool-V4.exe --smoke-test
.\TAUREON-Synth-Tool-V4.exe --list-midi wms
.\TAUREON-Synth-Tool-V4.exe --list-midi winmm
```

## CI and remaining acceptance

The changed workflow runs the complete software suite with WMS OFF and ON,
checks installed-consumer linkage, verifies the unsupported Windows Server 2022
API failure without loading even a staged DLL, checks the deployed hashes and
removes preview files before upload. Public artifact and release exclusions remain.
The exact run, jobs and logs must pass at the PR HEAD; the PR description records
that evidence once available. Offscreen captures are software previews only.

Not performed: actual Windows-provided 1.0 API activation/precedence (no such
API on this local PC), native Windows 24H2/25H2 OS matrix, native 150%/200% GUI
inspection, physical MIDI/SysEx transfer/restore, device disappearance or physical
hardware lifecycle tests. Those remain with the Product Owner; Stage 5 is HOLD/ACTIVE.
Independent Claude Code review is not claimed by this implementation report.

See [deployment and distribution conditions](../reference/WMS_DEPENDENCIES.md)
for all six current Go-live obligations, Preview-10 redistribution exclusions and
the reusable-library boundary. Public preview-file inclusion remains gated by
Product Owner acceptance, an effective supported-OS delivery restriction and an
update/removal, support and customer notification process. No merge, tag or public
release is authorized by this implementation.
