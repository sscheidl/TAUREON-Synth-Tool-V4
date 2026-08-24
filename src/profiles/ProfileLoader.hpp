#pragma once

#include "DeviceProfile.hpp"
#include "core/midi/Result.hpp"

#include <filesystem>
#include <string_view>

namespace taureon::profiles {

inline constexpr std::uint32_t current_profile_schema_version = 1;

[[nodiscard]] midi::Result<DeviceProfile> parse_profile_json(std::string_view json,
                                                              std::string source_name);
[[nodiscard]] midi::Result<DeviceProfile> load_profile_file(const std::filesystem::path& path);
[[nodiscard]] midi::Result<void> validate_profile(const DeviceProfile& profile);

} // namespace taureon::profiles
