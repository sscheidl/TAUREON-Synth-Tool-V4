# TAUREON MIDI engine reuse (internal)

The generic C++20 engine and its Windows transports are reusable CMake targets.
The core contains MIDI, SysEx, transfer, and `IMidiTransport`; it does not contain
Qt, device profiles, or the product application. WinMM and WMS remain separate,
optional Windows components so a consumer links only what it uses.

This is a shared internal library for the owner's own TAUREON/Synth projects,
not a public SDK or general-purpose MIDI framework. Version `0.3.0` identifies
the installed package; CMake requires an exact package version and consumers
should also pin a source revision. No source or binary compatibility promise is
made across revisions. The supported build and package route is Windows/MSVC;
other platforms have not been validated. A static-library consumer must use a
compatible MSVC/STL toolchain, C++ runtime and configuration (Debug or Release).
The current repository source is offered under the MIT license in `LICENSE`; historical
restricted test material removed from the current tree is not relicensed.

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
find_package(TaureonMidiEngine 0.3.0 EXACT CONFIG REQUIRED COMPONENTS Core)
target_link_libraries(my_app PRIVATE TaureonMidiEngine::MidiEngine)
```

On Windows, request and link one native transport only when required:

```cmake
find_package(TaureonMidiEngine 0.3.0 EXACT CONFIG REQUIRED COMPONENTS Core WinMM)
target_link_libraries(my_app PRIVATE TaureonMidiEngine::WinmmTransport)

# For a package built with TAUREON_ENABLE_WMS_TRANSPORT=ON:
find_package(TaureonMidiEngine 0.3.0 EXACT CONFIG REQUIRED COMPONENTS Core WMS)
target_link_libraries(my_app PRIVATE TaureonMidiEngine::WmsTransport)
```

`WinMM` is built by default on Windows and can be disabled with
`TAUREON_BUILD_WINMM_TRANSPORT=OFF`. `WMS` is built only when the pinned SDK and
C++/WinRT dependencies have been acquired and `TAUREON_ENABLE_WMS_TRANSPORT` is
enabled. It now targets the [In-box Preview 10 API](../reference/WMS_DEPENDENCIES.md).
The Windows MIDI service must be available; at runtime the transport uses a
Windows-provided API, permitted app-local API files beside the executable, or the
API installed with the Windows MIDI Services Tools.
The static engine package does not install or redistribute those API files.

Installed headers are under `include/taureon/`, for example
`#include <taureon/core/midi/MidiTypes.hpp>` and
`#include <taureon/transports/IMidiTransport.hpp>`. These are the same physical
public headers used by the repository build; installation no longer generates or
rewrites a second copy. Backend implementation headers and the WinMM test seam
remain private under `src/` and are not installed. Consumers receive the include
path transitively from the linked CMake target.

The standalone consumer is deliberately configured from a separate source tree
against the **installed** package. It compiles all core headers, runs a transfer
with its own transport, and constructs the installed WinMM component without
opening hardware. The WMS consumer is opt-in even when the installed package
contains WMS. Configure it explicitly with
`-DTAUREON_CONSUMER_WITH_WMS=ON`; this builds and links a separate executable to
prove the installed dependency closure without claiming a runtime or hardware
acceptance test:

```powershell
# AcquireWmsDependencies.ps1 verifies the pinned Preview 10 package and WINMD hashes.
.\tools\AcquireWmsDependencies.ps1
cmake -S . -B build/engine-wms -G "Visual Studio 17 2022" -A x64 `
  -DTAUREON_BUILD_PRODUCT_APP=OFF -DBUILD_TESTING=ON `
  -DTAUREON_ENABLE_WMS_TRANSPORT=ON `
  -DCMAKE_INSTALL_PREFIX="$PWD/build/engine-wms-install"
foreach ($configuration in @('Debug', 'Release')) {
  cmake --build build/engine-wms --config $configuration
  cmake --install build/engine-wms --config $configuration
}
cmake -S tests/consumer -B build/consumer-wms -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="$PWD/build/engine-wms-install" `
  -DTAUREON_CONSUMER_WITH_WMS=ON
foreach ($configuration in @('Debug', 'Release')) {
  cmake --build build/consumer-wms --config $configuration `
    --target taureon_wms_consumer
}
```

`TransferEngine` is one-shot: create a new instance for each transfer attempt.
Its transport must remain alive until `wait()` and destruction complete. The
owner serializes `start()`, `wait()`, and destruction; cancellation and progress
queries may be made from other threads while the object is alive, and must finish
before destruction. Progress handlers run on the worker
thread and may request cancellation, but may not call `wait()`, destroy the engine,
or throw. A transport `send()` that blocks can also block `wait()` beyond the
configured timeout, which is checked between sends.

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
- WMS and WinMM are Windows-specific optional package components. Hardware
  acceptance and WMS runtime deployment remain separate.
- The standalone consumer's custom transport is a compile/link example. It uses
  an explicit external backend and a provider-scoped stable endpoint ID. Route
  resolution remains exact and ambiguity fails closed; it never falls back to
  a display name or an enumeration index. Existing WMS/WinMM serialized strings
  are unchanged. Our own external transports must choose a stable provider ID
  and endpoint ID; no registry or global provider-ID governance is needed.

`IMidiTransport` handlers may run on the transport worker or synchronously on a
caller thread. Never call a control method, destroy the transport, throw, or wait
for its worker from a handler. Replacement/clearing is not a callback join: an
already-copied handler can still run.

WinMM has the stronger reusable-backend contract documented in its public header:
queued callbacks may be delivered while `close()` is running, but no application
handler is called after `close()` returns, including on a close error. Successful
native close is the WinMM callback-quiescence boundary. If WinMM refuses to close
a handle, destruction first detaches and drains the callback context; storage that
the still-live driver handle may reference is retained until process exit. A late
driver callback then observes a detached context and becomes a no-op instead of
touching the destroyed transport. If long-message submission and its immediate
unprepare both fail, the prepared output header likewise remains transport-owned
for retry during `close()` rather than being destroyed. The owner still serializes
control methods and destruction.

## Next capability work

Do not conflate Poly AT with MPE or MIDI 2.0 per-note expression. Poly AT is a
MIDI 1.0 message addressed by key and channel. MPE uses MIDI 1.0 member channels
for per-note expression; MIDI 2.0 has its own high-resolution Channel Voice and
per-note messages. Extend the generic model with explicit protocol/capability
types and roundtrip/failure tests before exposing conversions or sending new
message classes. No route or protocol should be selected by a guess.

References: [UMP and MIDI 2.0 Protocol v1.1.2](https://midi.org/universal-midi-packet-ump-and-midi-2-0-protocol-specification),
[MPE v1.1](https://midi.org/mpe-midi-polyphonic-expression).
