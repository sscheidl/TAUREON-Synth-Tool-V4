#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace taureon::spike {

enum class RouteDirection {
    input,
    output,
    bidirectional,
};

struct WmsRouteIdentity {
    std::string endpoint_id;
    std::uint8_t group{};
    RouteDirection direction{};

    bool operator==(const WmsRouteIdentity&) const = default;
};

enum class ResolveStatus {
    resolved,
    missing,
    ambiguous,
};

struct ResolveResult {
    ResolveStatus status{ResolveStatus::missing};
    std::size_t index{};
};

inline ResolveResult resolve_exact(const WmsRouteIdentity& persisted,
                                   const std::vector<WmsRouteIdentity>& available) {
    std::size_t matches = 0;
    std::size_t matched_index = 0;
    for (std::size_t index = 0; index < available.size(); ++index) {
        if (available[index] == persisted) {
            ++matches;
            matched_index = index;
        }
    }
    if (matches == 1) {
        return {ResolveStatus::resolved, matched_index};
    }
    return {matches == 0 ? ResolveStatus::missing : ResolveStatus::ambiguous, 0};
}

inline bool run_wms_route_identity_acceptance_tests() {
    const WmsRouteIdentity persisted{"endpoint-A", 3, RouteDirection::output};

    const std::vector<WmsRouteIdentity> initial{
        {"endpoint-A", 2, RouteDirection::output},
        persisted,
        {"endpoint-B", 3, RouteDirection::output},
    };
    if (resolve_exact(persisted, initial).status != ResolveStatus::resolved) return false;

    const std::vector<WmsRouteIdentity> reordered{
        {"endpoint-B", 3, RouteDirection::output},
        persisted,
        {"endpoint-A", 2, RouteDirection::output},
    };
    if (resolve_exact(persisted, reordered).status != ResolveStatus::resolved) return false;

    const std::vector<WmsRouteIdentity> disappeared{
        {"endpoint-A", 2, RouteDirection::output},
        {"endpoint-B", 3, RouteDirection::output},
    };
    if (resolve_exact(persisted, disappeared).status != ResolveStatus::missing) return false;

    const std::vector<WmsRouteIdentity> changed_id{
        {"endpoint-A-reenumerated", 3, RouteDirection::output},
    };
    if (resolve_exact(persisted, changed_id).status != ResolveStatus::missing) return false;

    const std::vector<WmsRouteIdentity> wrong_group_or_direction{
        {"endpoint-A", 4, RouteDirection::output},
        {"endpoint-A", 3, RouteDirection::input},
    };
    if (resolve_exact(persisted, wrong_group_or_direction).status != ResolveStatus::missing) return false;

    const std::vector<WmsRouteIdentity> reappeared{persisted};
    if (resolve_exact(persisted, reappeared).status != ResolveStatus::resolved) return false;

    const std::vector<WmsRouteIdentity> ambiguous{persisted, persisted};
    return resolve_exact(persisted, ambiguous).status == ResolveStatus::ambiguous;
}

} // namespace taureon::spike
