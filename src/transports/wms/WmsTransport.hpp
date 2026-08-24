#pragma once

#include "transports/IMidiTransport.hpp"

#include <memory>

namespace taureon::midi::wms {

class WmsTransport final : public IMidiTransport {
public:
    WmsTransport();
    ~WmsTransport() override;

    WmsTransport(const WmsTransport&) = delete;
    WmsTransport& operator=(const WmsTransport&) = delete;

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

} // namespace taureon::midi::wms
