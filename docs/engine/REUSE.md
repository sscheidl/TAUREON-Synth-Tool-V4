# Generic MIDI engine reuse (experimental)

The generic C++20 engine is now a separate CMake target, `taureon_midi_engine`. It
contains `src/core/midi`, `src/core/sysex`, and `src/core/transfer`, and publishes
the transport interface `transports/IMidiTransport.hpp`. It does not contain Qt,
device profiles, the product application, or a native Windows backend.

The package is **not yet a stable SDK**: its independent package version is
0.1.0, with compatibility restricted to the same minor release while the major
version is zero. Public API questions remain open. Keep the source revision
pinned when reusing it in another project. The supported build and package route
is Windows/MSVC only; other platforms have not been validated. A static-library
consumer must use a compatible MSVC/STL toolchain, C++ runtime and configuration
(Debug or Release). Native WinMM/WMS transports are not included in the package.
The current repository source is offered under the MIT license in `LICENSE`;
historical restricted test material removed from the current tree is not relicensed.

## Build and consume on Windows

The following commands use PowerShell and create disposable build output below
the repository's `build/` directory:

```powershell
cmake -S . -B build/engine -G "Visual Studio 17 2022" -A x64 `
  -DTAUREON_BUILD_PRODUCT_APP=OFF -DBUILD_TESTING=ON `
  -DCMAKE_INSTALL_PREFIX="$PWD/build/engine-install"
foreach ($configuration in @('Debug', 'Release')) {
  cmake --build build/engine --config $configuration
  ctest --test-dir build/engine -C $configuration --output-on-failure
  cmake --install build/engine --config $configuration
}
cmake -S tests/consumer -B build/consumer -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="$PWD/build/engine-install"
foreach ($configuration in @('Debug', 'Release')) {
  cmake --build build/consumer --config $configuration
  ctest --test-dir build/consumer -C $configuration --output-on-failure
}
```

In an independent CMake project:

```cmake
find_package(TaureonMidiEngine 0.1 CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE TaureonMidiEngine::MidiEngine)
```

The standalone consumer is deliberately configured from a separate source tree
against the **installed** headers and library. It compiles all installed headers
and runs a transfer with its own transport implementation. CI runs this route,
plus the Stage 2/3 engine tests, without Qt in both configurations.

## Current capability boundary

- MIDI 1.0 parsing includes polyphonic key pressure (Poly AT), channel pressure,
  pitch bend, channel voice, system common/realtime, and SysEx. Raw bytes remain
  authoritative.
- UMP words can be carried without lossy flattening; generic MIDI 1.0 SysEx
  can be converted to/from UMP SysEx7 byte-exactly.
- SysEx capture reports incomplete, malformed, and known-loss-affected frames;
  transfer supports ordered submission, pacing, progress, and cancellation.
- MIDI 2.0 Channel Voice semantics, SysEx8, MIDI-CI, and MPE zone/note-state
  interpretation are **not** implemented by this package.
- The WMS and WinMM native backends remain Windows-specific product targets,
  not part of this installed generic package. Hardware acceptance is separate.

## Next capability work

Do not conflate Poly AT with MPE or MIDI 2.0 per-note expression. Poly AT is a
MIDI 1.0 message addressed by key and channel. MPE uses MIDI 1.0 member channels
for per-note expression; MIDI 2.0 has its own high-resolution Channel Voice and
per-note messages. Extend the generic model with explicit protocol/capability
types and roundtrip/failure tests before exposing conversions or sending new
message classes. No route or protocol should be selected by a guess.

References: [UMP and MIDI 2.0 Protocol v1.1.2](https://midi.org/universal-midi-packet-ump-and-midi-2-0-protocol-specification),
[MPE v1.1](https://midi.org/mpe-midi-polyphonic-expression).
