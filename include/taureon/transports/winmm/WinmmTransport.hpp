#pragma once

#include <taureon/transports/IMidiTransport.hpp>

#include <memory>

namespace taureon::midi::winmm {

class IWinmmTransportApi;
class WinmmTransportTestAccess;

class WinmmTransport final : public IMidiTransport {
public:
    WinmmTransport();
    ~WinmmTransport() override;

    WinmmTransport(const WinmmTransport&) = delete;
    WinmmTransport& operator=(const WinmmTransport&) = delete;

    [[nodiscard]] MidiBackend backend() const noexcept override;
    [[nodiscard]] MidiTransportCapabilities capabilities() const noexcept override;
    [[nodiscard]] MidiTransportDiagnostics diagnostics() const noexcept override;
    [[nodiscard]] TransportState state() const noexcept override;
    [[nodiscard]] Result<std::vector<MidiEndpointDescriptor>> enumerate() override;
    [[nodiscard]] Result<void> open(const MidiConnectionRequest& request) override;
    // After close() returns, no application handler will be invoked. A successful
    // close also quiesces and releases the native callback context. If WinMM refuses
    // to close a handle, destruction detaches the context and retains any memory that
    // the driver may still reference, making later native callbacks safe no-ops.
    [[nodiscard]] Result<void> close() override;
    [[nodiscard]] Result<void> send(const NativeMidiMessage& message) override;
    void set_message_handler(MidiMessageHandler handler) override;
    void set_stream_event_handler(MidiStreamEventHandler handler) override;
    void set_endpoint_change_handler(EndpointChangeHandler handler) override;

private:
    explicit WinmmTransport(std::shared_ptr<IWinmmTransportApi> native_api);
    friend class WinmmTransportTestAccess;

    struct Impl;
    std::unique_ptr<Impl> impl_;
    TransportStateMachine lifecycle_;
};

} // namespace taureon::midi::winmm
