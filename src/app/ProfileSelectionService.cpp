#include "app/ProfileSelectionService.hpp"

#include <algorithm>

namespace taureon::app {
namespace {

midi::Result<void> invalid(std::string detail) {
    return midi::Result<void>::failure(
        {midi::MidiErrorCode::invalid_state, std::move(detail), "profile-selection", std::nullopt});
}

} // namespace

ProfileSelectionService::ProfileSelectionService(const profiles::ProfileRegistry& registry)
    : registry_(registry) {}

midi::Result<void> ProfileSelectionService::select_temporary(std::string profile_id) {
    const auto* profile = registry_.find(profile_id);
    if (!profile || profile->generic) return invalid("temporary profile selection is unavailable or Generic");
    manual_profile_id_ = std::move(profile_id);
    return midi::Result<void>::success();
}

profiles::ProfileMatchResult ProfileSelectionService::match(const sysex::SysExFrame& frame) {
    profiles::ProfileMatchInput input;
    input.saved_profile_id = saved_profile_id_;
    input.manual_profile_id = manual_profile_id_;
    last_result_ = registry_.match(frame, input);
    return *last_result_;
}

midi::Result<void> ProfileSelectionService::remember_overridden_manual() {
    if (!last_result_) return invalid("no profile match result is available");
    const auto evidence = std::find_if(last_result_->evidence.begin(), last_result_->evidence.end(),
                                       [](const auto& item) {
                                           return item.kind == profiles::ProfileEvidenceKind::overridden_manual_selection;
                                       });
    if (evidence == last_result_->evidence.end()) return invalid("no overridden manual selection is available");
    const auto* profile = registry_.find(evidence->profile_id);
    if (!profile || profile->generic) return invalid("overridden manual profile is no longer available");
    saved_profile_id_ = profile->profile_id;
    return midi::Result<void>::success();
}

void ProfileSelectionService::forget_binding() noexcept { saved_profile_id_.reset(); }

const std::optional<std::string>& ProfileSelectionService::manual_profile_id() const noexcept {
    return manual_profile_id_;
}

const std::optional<std::string>& ProfileSelectionService::saved_profile_id() const noexcept {
    return saved_profile_id_;
}

} // namespace taureon::app
