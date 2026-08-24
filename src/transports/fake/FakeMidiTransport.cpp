#include "FakeMidiTransport.hpp"

#include "core/midi/RouteResolver.hpp"

#include <algorithm>

namespace taureon::midi {
namespace {

MidiError route_error(const RouteResolutionStatus status) {
    if (status == RouteResolutionStatus::missing) {
        return {MidiErrorCode::endpoint_missing, "route is missing", {}, std::nullopt};
    }
    if (status == RouteResolutionStatus::ambiguous) {
        return {MidiErrorCode::endpoint_ambiguous, "route is ambiguous", {}, std::nullopt};
    }
    return {MidiErrorCode::invalid_route, "route is invalid", {}, std::nullopt};
}

} // namespace

FakeMidiTransport::FakeMidiTransport(const MidiBackend backend,
                                     std::vector<MidiEndpointDescriptor> endpoints)
    : backend_(backend), endpoints_(std::move(endpoints)) {}

MidiBackend FakeMidiTransport::backend() const noexcept { return backend_; }

MidiTransportCapabilities FakeMidiTransport::capabilities() const noexcept {
    return {true, true, true, true, backend_ == MidiBackend::windows_midi_services};
}

MidiTransportDiagnostics FakeMidiTransport::diagnostics() const noexcept {
    std::scoped_lock lock(mutex_);
    return diagnostics_;
}

TransportState FakeMidiTransport::state() const noexcept { return lifecycle_.state(); }

Result<std::vector<MidiEndpointDescriptor>> FakeMidiTransport::enumerate() {
    std::scoped_lock lock(mutex_);
    return Result<std::vector<MidiEndpointDescriptor>>::success(endpoints_);
}

Result<void> FakeMidiTransport::validate_route(
    const MidiRouteIdentity& route, const MidiDirection direction,
    const std::vector<MidiEndpointDescriptor>& endpoints) const {
    if (route.backend != backend_ || route.direction != direction) {
        return Result<void>::failure(
            {MidiErrorCode::invalid_route, "route backend or direction mismatch", {}, std::nullopt});
    }
    const auto resolution = resolve_route(route, endpoints);
    if (resolution.status != RouteResolutionStatus::exact) {
        return Result<void>::failure(route_error(resolution.status));
    }
    return Result<void>::success();
}

Result<void> FakeMidiTransport::open(const MidiConnectionRequest& request) {
    if (!request.receive_route && !request.transmit_route) {
        return Result<void>::failure(
            {MidiErrorCode::invalid_route, "connection requires an RX or TX route", {}, std::nullopt});
    }
    const auto begun = lifecycle_.begin_open();
    if (!begun) return begun;

    std::vector<MidiEndpointDescriptor> endpoints;
    {
        std::scoped_lock lock(mutex_);
        endpoints = endpoints_;
    }
    if (request.receive_route) {
        const auto result = validate_route(*request.receive_route, MidiDirection::input, endpoints);
        if (!result) {
            lifecycle_.fail();
            return result;
        }
    }
    if (request.transmit_route) {
        const auto result = validate_route(*request.transmit_route, MidiDirection::output, endpoints);
        if (!result) {
            lifecycle_.fail();
            return result;
        }
    }
    {
        std::scoped_lock lock(mutex_);
        connection_ = request;
    }
    const auto completed = lifecycle_.complete_open();
    if (!completed) lifecycle_.fail();
    return completed;
}

Result<void> FakeMidiTransport::close() {
    const auto begun = lifecycle_.begin_close();
    if (!begun) return begun;
    if (lifecycle_.state() == TransportState::closed) return Result<void>::success();
    {
        std::scoped_lock lock(mutex_);
        connection_ = {};
    }
    return lifecycle_.complete_close();
}

Result<void> FakeMidiTransport::send(const NativeMidiMessage& message) {
    MidiMessageHandler handler;
    {
        std::scoped_lock lock(mutex_);
        if (lifecycle_.state() != TransportState::open) {
            return Result<void>::failure(
                {MidiErrorCode::invalid_state, "transport is not open", {}, std::nullopt});
        }
        if (message.backend != backend_) {
            return Result<void>::failure(
                {MidiErrorCode::unsupported_capability, "message backend mismatch", {}, std::nullopt});
        }
        handler = message_handler_;
        ++diagnostics_.delivered_messages;
    }
    if (handler) handler(message);
    return Result<void>::success();
}

void FakeMidiTransport::set_message_handler(MidiMessageHandler handler) {
    std::scoped_lock lock(mutex_);
    message_handler_ = std::move(handler);
}

void FakeMidiTransport::set_endpoint_change_handler(EndpointChangeHandler handler) {
    std::scoped_lock lock(mutex_);
    endpoint_handler_ = std::move(handler);
}

void FakeMidiTransport::set_endpoints(std::vector<MidiEndpointDescriptor> endpoints) {
    std::scoped_lock lock(mutex_);
    endpoints_ = std::move(endpoints);
}

void FakeMidiTransport::remove_endpoint(const MidiRouteIdentity& identity) {
    EndpointChangeHandler handler;
    bool selected = false;
    {
        std::scoped_lock lock(mutex_);
        endpoints_.erase(std::remove_if(endpoints_.begin(), endpoints_.end(),
                                        [&](const auto& endpoint) {
                                            return endpoint.identity == identity;
                                        }),
                         endpoints_.end());
        selected = (connection_.receive_route && *connection_.receive_route == identity) ||
                   (connection_.transmit_route && *connection_.transmit_route == identity);
        handler = endpoint_handler_;
    }
    if (selected) lifecycle_.fail();
    if (handler) {
        handler({selected ? EndpointChangeKind::selected_route_unavailable :
                            EndpointChangeKind::disappeared,
                 identity, std::nullopt});
    }
}

} // namespace taureon::midi
