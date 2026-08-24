#pragma once

#include "transports/IMidiTransport.hpp"

#include <mutex>

namespace taureon::midi {

class FakeMidiTransport final : public IMidiTransport {
public:
    explicit FakeMidiTransport(MidiBackend backend,
                               std::vector<MidiEndpointDescriptor> endpoints = {});

    [[nodiscard]] MidiBackend backend() const noexcept override;
    [[nodiscard]] MidiTransportCapabilities capabilities() const noexcept override;
    [[nodiscard]] MidiTransportDiagnostics diagnostics() const noexcept override;
    [[nodiscard]] TransportState state() const noexcept override;
    [[nodiscard]] Result<std::vector<MidiEndpointDescriptor>> enumerate() override;
    [[nodiscard]] Result<void> open(const MidiConnectionRequest& request) override;
    [[nodiscard]] Result<void> close() override;
    [[nodiscard]] Result<void> send(const NativeMidiMessage& message) override;
    void set_message_handler(MidiMessageHandler handler) override;
    void set_endpoint_change_handler(EndpointChangeHandler handler) override;

    void set_endpoints(std::vector<MidiEndpointDescriptor> endpoints);
    void remove_endpoint(const MidiRouteIdentity& identity);

private:
    [[nodiscard]] Result<void> validate_route(
        const MidiRouteIdentity& route, MidiDirection direction,
        const std::vector<MidiEndpointDescriptor>& endpoints) const;

    MidiBackend backend_;
    TransportStateMachine lifecycle_;
    mutable std::mutex mutex_;
    std::vector<MidiEndpointDescriptor> endpoints_;
    MidiConnectionRequest connection_;
    MidiMessageHandler message_handler_;
    EndpointChangeHandler endpoint_handler_;
    MidiTransportDiagnostics diagnostics_;
};

} // namespace taureon::midi
