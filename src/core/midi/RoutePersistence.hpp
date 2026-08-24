#pragma once

#include "MidiTypes.hpp"
#include "Result.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace taureon::midi {

inline constexpr std::uint32_t current_route_schema_version = 1;

struct PersistedMidiRoute {
    std::uint32_t schema_version{current_route_schema_version};
    MidiRouteIdentity identity;

    bool operator==(const PersistedMidiRoute&) const = default;
};

[[nodiscard]] Result<std::string> serialize_route(const PersistedMidiRoute& route);
[[nodiscard]] Result<PersistedMidiRoute> deserialize_route(std::string_view text);

} // namespace taureon::midi
