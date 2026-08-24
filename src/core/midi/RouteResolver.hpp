#pragma once

#include "MidiTypes.hpp"

#include <optional>
#include <vector>

namespace taureon::midi {

enum class RouteResolutionStatus {
    exact,
    missing,
    ambiguous,
    invalid,
};

struct RouteResolution {
    RouteResolutionStatus status{RouteResolutionStatus::invalid};
    std::optional<MidiEndpointDescriptor> endpoint;
    std::vector<MidiEndpointDescriptor> candidates;
};

[[nodiscard]] RouteResolution resolve_route(
    const MidiRouteIdentity& persisted,
    const std::vector<MidiEndpointDescriptor>& available);

} // namespace taureon::midi
