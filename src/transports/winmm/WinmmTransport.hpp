#pragma once

#include "transports/IMidiTransport.hpp"
#include "WinmmNativeApi.hpp"

#include <memory>

namespace taureon::midi::winmm {

class WinmmTransport final : public IMidiTransport {
public:
    WinmmTransport();
    explicit WinmmTransport(WinmmTransportApiPtr native_api);
    ~WinmmTransport() override;

    WinmmTransport(const WinmmTransport&) = delete;
    WinmmTransport& operator=(const WinmmTransport&) = delete;

    [[nodiscard]] MidiBackend backend() const noexcept override;
    [[nodiscard]] MidiTransportCapabilities capabilities() const noexcept override;
    [[nodiscard]] MidiTransportDiagnostics diagnostics() const noexcept override;
    [[nodiscard]] TransportState state() const noexcept override;
    [[nodiscard]] Result<std::vector<MidiEndpointDescriptor>> enumerate() override;
    [[nodiscard]] Result<void> open(const MidiConnectionRequest& request) override;
    [[nodiscard]] Result<void> close() override;
    [[nodiscard]] Result<void> send(const NativeMidiMessage& message) override;
    void set_message_handler(MidiMessageHandler handler) override;
    void set_stream_event_handler(MidiStreamEventHandler handler) override;
    void set_endpoint_change_handler(EndpointChangeHandler handler) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    TransportStateMachine lifecycle_;
};

} // namespace taureon::midi::winmm
