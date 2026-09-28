#pragma once

#include <taureon/core/midi/MidiTypes.hpp>
#include <taureon/core/midi/Result.hpp>
#include <taureon/core/midi/TransportState.hpp>

#include <functional>
#include <optional>
#include <vector>

namespace taureon::midi {

struct MidiConnectionRequest {
    std::optional<MidiRouteIdentity> receive_route;
    std::optional<MidiRouteIdentity> transmit_route;
};

struct MidiTransportCapabilities {
    bool supports_input{};
    bool supports_output{};
    bool supports_endpoint_notifications{};
    bool supports_midi1{};
    bool supports_ump{};
};

struct MidiTransportDiagnostics {
    std::uint64_t native_callbacks{};
    std::uint64_t delivered_messages{};
    std::uint64_t transmitted_messages{};
    std::uint64_t dropped_events{};
    std::uint64_t callbacks_after_acceptance_closed{};
    std::uint64_t queue_high_water_mark{};
};

using MidiMessageHandler = std::function<void(const NativeMidiMessage&)>;
using MidiStreamEventHandler = std::function<void(const MidiStreamEvent&)>;
using EndpointChangeHandler = std::function<void(const EndpointChange&)>;

// Implementer/caller contract (the callback thread is intentionally unspecified):
// - The owner serializes enumerate/open/close/send and destruction; none is called
//   from a handler. No method may race with destruction.
// - A handler may run on a transport worker or synchronously on a caller thread.
//   It must not throw, block on a transport control call, or destroy its transport.
// - Handler replacement is thread-safe against dispatch, but an already copied
//   handler may still run after set_* returns. Clearing a handler is not a join.
// - close() may deliver already-queued events while it runs. This experimental
//   interface does not yet promise callback quiescence at close() return; keep
//   handler-captured state alive until the transport has been safely destroyed.
//   A failed close() is never evidence that native callbacks have stopped.
// - Implementations must publish events in stream sequence order and never call
//   user handlers while holding a lock needed by set_* or control methods.
class IMidiTransport {
public:
    virtual ~IMidiTransport() = default;

    [[nodiscard]] virtual MidiBackend backend() const noexcept = 0;
    [[nodiscard]] virtual MidiTransportCapabilities capabilities() const noexcept = 0;
    [[nodiscard]] virtual MidiTransportDiagnostics diagnostics() const noexcept = 0;
    [[nodiscard]] virtual TransportState state() const noexcept = 0;
    [[nodiscard]] virtual Result<std::vector<MidiEndpointDescriptor>> enumerate() = 0;
    [[nodiscard]] virtual Result<void> open(const MidiConnectionRequest& request) = 0;
    [[nodiscard]] virtual Result<void> close() = 0;
    [[nodiscard]] virtual Result<void> send(const NativeMidiMessage& message) = 0;
    virtual void set_message_handler(MidiMessageHandler handler) = 0;
    virtual void set_stream_event_handler(MidiStreamEventHandler handler) = 0;
    virtual void set_endpoint_change_handler(EndpointChangeHandler handler) = 0;
};

} // namespace taureon::midi
