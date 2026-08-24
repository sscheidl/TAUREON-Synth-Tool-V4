#include "RouteResolver.hpp"

namespace taureon::midi {

RouteResolution resolve_route(const MidiRouteIdentity& persisted,
                              const std::vector<MidiEndpointDescriptor>& available) {
    if (!is_valid(persisted)) return {RouteResolutionStatus::invalid, std::nullopt, {}};

    std::vector<MidiEndpointDescriptor> matches;
    for (const auto& endpoint : available) {
        if (endpoint.identity == persisted) matches.push_back(endpoint);
    }
    if (matches.empty()) return {RouteResolutionStatus::missing, std::nullopt, {}};
    if (matches.size() > 1) return {RouteResolutionStatus::ambiguous, std::nullopt, std::move(matches)};
    return {RouteResolutionStatus::exact, matches.front(), std::move(matches)};
}

} // namespace taureon::midi
