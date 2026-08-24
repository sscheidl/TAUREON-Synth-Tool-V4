#include "app/ConnectionController.hpp"

#include <algorithm>

namespace taureon::app {
namespace {

midi::Result<void> failure(midi::MidiErrorCode code, std::string detail) {
    return midi::Result<void>::failure({code, std::move(detail), {}, std::nullopt});
}

} // namespace

ConnectionController::ConnectionController(midi::IMidiTransport& transport) : transport_(transport) {
    transport_.set_endpoint_change_handler(
        [this](const midi::EndpointChange& change) { endpoint_changed(change); });
}

ConnectionController::~ConnectionController() { transport_.set_endpoint_change_handler({}); }

midi::Result<void> ConnectionController::refresh() {
    auto result = transport_.enumerate();
    if (!result) {
        std::scoped_lock lock(mutex_);
        snapshot_.state = ConnectionPresentationState::error;
        snapshot_.detail = result.error().message;
        return midi::Result<void>::failure(result.error());
    }
    std::scoped_lock lock(mutex_);
    snapshot_.endpoints = std::move(result.value());
    snapshot_.state = snapshot_.receive_route || snapshot_.transmit_route ?
        ConnectionPresentationState::ready : ConnectionPresentationState::disconnected;
    snapshot_.detail.clear();
    return midi::Result<void>::success();
}

midi::Result<void> ConnectionController::select_route(const midi::MidiRouteIdentity& identity) {
    std::scoped_lock lock(mutex_);
    if (identity.backend != transport_.backend()) {
        return failure(midi::MidiErrorCode::invalid_route, "route backend does not match selected backend");
    }
    const auto count = std::count_if(snapshot_.endpoints.begin(), snapshot_.endpoints.end(),
                                     [&](const auto& endpoint) { return endpoint.identity == identity; });
    if (count == 0) return failure(midi::MidiErrorCode::endpoint_missing, "route is not enumerated");
    if (count > 1) return failure(midi::MidiErrorCode::endpoint_ambiguous, "route identity is ambiguous");
    if (identity.direction == midi::MidiDirection::input) snapshot_.receive_route = identity;
    else snapshot_.transmit_route = identity;
    snapshot_.state = ConnectionPresentationState::ready;
    snapshot_.detail.clear();
    return midi::Result<void>::success();
}

void ConnectionController::clear_route(const midi::MidiDirection direction) {
    std::scoped_lock lock(mutex_);
    if (direction == midi::MidiDirection::input) snapshot_.receive_route.reset();
    else snapshot_.transmit_route.reset();
    snapshot_.state = snapshot_.receive_route || snapshot_.transmit_route ?
        ConnectionPresentationState::ready : ConnectionPresentationState::disconnected;
}

midi::Result<void> ConnectionController::connect_selected() {
    midi::MidiConnectionRequest request;
    {
        std::scoped_lock lock(mutex_);
        request = {snapshot_.receive_route, snapshot_.transmit_route};
    }
    if (!request.receive_route && !request.transmit_route) {
        return failure(midi::MidiErrorCode::invalid_route, "select an exact RX or TX route before connecting");
    }
    auto result = transport_.open(request);
    std::scoped_lock lock(mutex_);
    snapshot_.state = result ? ConnectionPresentationState::connected : ConnectionPresentationState::error;
    snapshot_.detail = result ? std::string{} : result.error().message;
    return result;
}

midi::Result<void> ConnectionController::disconnect() {
    auto result = transport_.close();
    std::scoped_lock lock(mutex_);
    snapshot_.state = result ? ConnectionPresentationState::disconnected : ConnectionPresentationState::error;
    snapshot_.detail = result ? std::string{} : result.error().message;
    return result;
}

ConnectionSnapshot ConnectionController::snapshot() const {
    std::scoped_lock lock(mutex_);
    return snapshot_;
}

void ConnectionController::endpoint_changed(const midi::EndpointChange& change) {
    if (change.kind != midi::EndpointChangeKind::selected_route_unavailable) return;
    std::scoped_lock lock(mutex_);
    snapshot_.state = ConnectionPresentationState::degraded;
    snapshot_.detail = "selected route is no longer available; deliberate reselection is required";
}

} // namespace taureon::app
