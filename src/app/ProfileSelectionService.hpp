#pragma once

#include "profiles/ProfileRegistry.hpp"

#include <optional>
#include <string>

namespace taureon::app {

class ProfileSelectionService {
public:
    explicit ProfileSelectionService(const profiles::ProfileRegistry& registry);

    [[nodiscard]] midi::Result<void> select_temporary(std::string profile_id);
    [[nodiscard]] profiles::ProfileMatchResult match(const sysex::SysExFrame& frame);
    [[nodiscard]] midi::Result<void> remember_overridden_manual();
    void forget_binding() noexcept;

    [[nodiscard]] const std::optional<std::string>& manual_profile_id() const noexcept;
    [[nodiscard]] const std::optional<std::string>& saved_profile_id() const noexcept;

private:
    const profiles::ProfileRegistry& registry_;
    std::optional<std::string> manual_profile_id_;
    std::optional<std::string> saved_profile_id_;
    std::optional<profiles::ProfileMatchResult> last_result_;
};

} // namespace taureon::app
