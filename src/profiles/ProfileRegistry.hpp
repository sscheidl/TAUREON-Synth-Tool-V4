#pragma once

#include "DeviceProfile.hpp"
#include "core/midi/Result.hpp"
#include "core/sysex/SysExFrame.hpp"

#include <filesystem>
#include <map>
#include <vector>

namespace taureon::profiles {

struct ProfileLoadIssue {
    std::filesystem::path path;
    midi::MidiError error;
};

class ProfileRegistry {
public:
    [[nodiscard]] midi::Result<void> register_profile(DeviceProfile profile);
    [[nodiscard]] midi::Result<void> load_directory(const std::filesystem::path& directory,
                                                    std::vector<ProfileLoadIssue>& issues);

    [[nodiscard]] const DeviceProfile* find(std::string_view profile_id) const noexcept;
    [[nodiscard]] std::vector<DeviceProfile> profiles() const;
    [[nodiscard]] ProfileMatchResult match(const sysex::SysExFrame& frame,
                                           const ProfileMatchInput& input = {}) const;

private:
    [[nodiscard]] ProfileMatchResult from_candidates(
        const std::vector<std::pair<const DeviceProfile*, ProfileMatchEvidence>>& candidates) const;
    [[nodiscard]] ProfileMatchResult generic_fallback(std::string message) const;

    std::map<std::string, DeviceProfile, std::less<>> profiles_;
};

} // namespace taureon::profiles
