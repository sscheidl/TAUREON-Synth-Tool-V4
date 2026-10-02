# Contributing to TAUREON Synth Tool V4

Thanks for helping improve the first public alpha. Bug reports, reproducible MIDI/SysEx examples that you are allowed to share, documentation, tests, and focused pull requests are welcome. Please do not commit private preset dumps, device identifiers, or captures without checking their provenance and permissions.

Open an issue before a large feature or architecture change. Include the Windows version, app release and build revision from Diagnostics, selected backend, expected behavior, actual behavior, and safe reproduction steps. Redact personal paths and device identifiers if necessary. Never ask another contributor to run a destructive device operation to reproduce a bug.

The current supported build target is Windows 11 x64 with Visual Studio 2022 (v143), CMake 3.25+, C++20, and Qt 6.10.3 for MSVC 2022 x64. The pinned WMS build dependencies are acquired by `tools/AcquireStage1Dependencies.ps1`; this script downloads and verifies SDK packages but does not install or reconfigure the Windows runtime. For a build without WMS:

```powershell
cmake -S . -B build/contributor -G "Visual Studio 17 2022" -A x64 -T v143 `
  -DCMAKE_PREFIX_PATH="C:\path\to\Qt\6.10.3\msvc2022_64" `
  -DTAUREON_ENABLE_WMS_TRANSPORT=OFF -DBUILD_TESTING=ON
cmake --build build/contributor --config Debug --parallel
ctest --test-dir build/contributor -C Debug -LE local-midi --output-on-failure
```

Keep changes focused and include the test results in your pull request. The project [quality policy](docs/process/QUALITY_POLICY.md) distinguishes software evidence from physical-hardware validation; only the device owner can confirm the latter. Do not send automated test traffic to a real synth or change Windows MIDI drivers, services, registry, or API mode as part of a contribution.
