# Generic MIDI engine reuse (experimental)

The generic C++20 engine is now a separate CMake target, `taureon_midi_engine`. It
contains `src/core/midi`, `src/core/sysex`, and `src/core/transfer`, and publishes
the transport interface `transports/IMidiTransport.hpp`. It does not contain Qt,
device profiles, the product application, or a native Windows backend.

The package is **not yet a stable SDK**: the project version remains 0.0.0, the
public API has not had an independent architecture review, and no license for
distribution to unrelated parties has been selected. Keep the source revision
pinned when reusing it in another project.

## Build and consume on Windows

The following commands use PowerShell and create disposable build output below
the repository's `build/` directory:

```powershell
cmake -S . -B build/engine -G "Visual Studio 17 2022" -A x64 `
  -DTAUREON_BUILD_PRODUCT_APP=OFF -DBUILD_TESTING=OFF `
  -DCMAKE_INSTALL_PREFIX="$PWD/build/engine-install"
cmake --build build/engine --config Release --target taureon_midi_engine
cmake --install build/engine --config Release
cmake -S tests/consumer -B build/consumer -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="$PWD/build/engine-install"
cmake --build build/consumer --config Release
ctest --test-dir build/consumer -C Release --output-on-failure
```

In an independent CMake project:

```cmake
find_package(TaureonMidiEngine CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE TaureonMidiEngine::MidiEngine)
```

The standalone consumer is deliberately configured from a separate source tree
against the **installed** headers and library. CI runs this route without Qt.

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
