#pragma once

#include "transports/IMidiTransport.hpp"

#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace taureon::app {

enum class ConnectionPresentationState { disconnected, ready, connected, degraded, error };

struct ConnectionSnapshot {
    ConnectionPresentationState state{ConnectionPresentationState::disconnected};
    std::vector<midi::MidiEndpointDescriptor> endpoints;
    std::optional<midi::MidiRouteIdentity> receive_route;
    std::optional<midi::MidiRouteIdentity> transmit_route;
    std::string detail;
};

class ConnectionController {
public:
    explicit ConnectionController(midi::IMidiTransport& transport);
    ~ConnectionController();

    ConnectionController(const ConnectionController&) = delete;
    ConnectionController& operator=(const ConnectionController&) = delete;

    [[nodiscard]] midi::Result<void> refresh();
    [[nodiscard]] midi::Result<void> select_route(const midi::MidiRouteIdentity& identity);
    void clear_route(midi::MidiDirection direction);
    [[nodiscard]] midi::Result<void> connect_selected();
    [[nodiscard]] midi::Result<void> disconnect();
    [[nodiscard]] ConnectionSnapshot snapshot() const;

private:
    void endpoint_changed(const midi::EndpointChange& change);

    midi::IMidiTransport& transport_;
    mutable std::mutex mutex_;
    ConnectionSnapshot snapshot_;
};

} // namespace taureon::app
