#include "ProfileRegistry.hpp"

#include "ProfileLoader.hpp"

#include <algorithm>
#include <system_error>

namespace taureon::profiles {
namespace {

midi::MidiError registry_error(std::string message) {
    return {midi::MidiErrorCode::serialization_error, std::move(message), "profile-registry",
            std::nullopt};
}

bool fingerprint_matches(const SysExFingerprint& fingerprint,
                         const std::vector<std::uint8_t>& bytes) {
    if (fingerprint.offset > bytes.size() ||
        fingerprint.bytes.size() > bytes.size() - fingerprint.offset) {
        return false;
    }
    return std::equal(fingerprint.bytes.begin(), fingerprint.bytes.end(),
                      bytes.begin() + static_cast<std::ptrdiff_t>(fingerprint.offset));
}

} // namespace

midi::Result<void> ProfileRegistry::register_profile(DeviceProfile profile) {
    if (const auto validation = validate_profile(profile); !validation) return validation;
    if (const auto existing = profiles_.find(profile.profile_id); existing != profiles_.end()) {
        return midi::Result<void>::failure(registry_error(
            "duplicate profile_id '" + profile.profile_id + "' (registered version " +
            existing->second.profile_version + ", incoming version " + profile.profile_version + ')'));
    }
    profiles_.emplace(profile.profile_id, std::move(profile));
    return midi::Result<void>::success();
}

midi::Result<void> ProfileRegistry::load_directory(const std::filesystem::path& directory,
                                                   std::vector<ProfileLoadIssue>& issues) {
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error)) {
        return midi::Result<void>::failure(
            {midi::MidiErrorCode::io_error,
             "profile directory is unavailable: " + directory.string(), "profile-directory",
             error ? std::optional<std::int64_t>(error.value()) : std::nullopt});
    }
    std::vector<std::filesystem::path> files;
    for (std::filesystem::directory_iterator iterator(directory, error), end;
         !error && iterator != end; iterator.increment(error)) {
        if (!iterator->is_regular_file()) continue;
        const auto name = iterator->path().filename().string();
        if (name.ends_with(".profile.json")) files.push_back(iterator->path());
    }
    if (error) {
        return midi::Result<void>::failure(
            {midi::MidiErrorCode::io_error,
             "profile directory enumeration failed: " + directory.string(),
             "profile-directory-enumerate", static_cast<std::int64_t>(error.value())});
    }
    std::sort(files.begin(), files.end());
    for (const auto& path : files) {
        auto loaded = load_profile_file(path);
        if (!loaded) {
            issues.push_back({path, loaded.error()});
            continue;
        }
        auto registered = register_profile(std::move(loaded.value()));
        if (!registered) issues.push_back({path, registered.error()});
    }
    return midi::Result<void>::success();
}

const DeviceProfile* ProfileRegistry::find(const std::string_view profile_id) const noexcept {
    const auto found = profiles_.find(profile_id);
    return found == profiles_.end() ? nullptr : &found->second;
}

std::vector<DeviceProfile> ProfileRegistry::profiles() const {
    std::vector<DeviceProfile> result;
    result.reserve(profiles_.size());
    for (const auto& [_, profile] : profiles_) result.push_back(profile);
    return result;
}

ProfileMatchResult ProfileRegistry::generic_fallback(std::string message) const {
    for (const auto& [id, profile] : profiles_) {
        if (!profile.generic) continue;
        return {ProfileMatchStatus::GenericFallback, id,
                {{id, ProfileEvidenceKind::generic_fallback,
                  "no deterministic device evidence selected a real profile"}},
                std::move(message)};
    }
    return {ProfileMatchStatus::NoMatch, std::nullopt, {}, std::move(message)};
}

ProfileMatchResult ProfileRegistry::from_candidates(
    const std::vector<std::pair<const DeviceProfile*, ProfileMatchEvidence>>& candidates) const {
    if (candidates.empty()) return generic_fallback("no profile matched the available evidence");
    std::vector<ProfileMatchEvidence> evidence;
    evidence.reserve(candidates.size());
    for (const auto& candidate : candidates) evidence.push_back(candidate.second);
    std::sort(evidence.begin(), evidence.end(), [](const auto& left, const auto& right) {
        return left.profile_id < right.profile_id;
    });
    if (candidates.size() == 1) {
        return {ProfileMatchStatus::ConfidentSuggestion, candidates.front().first->profile_id,
                std::move(evidence), "one profile matched deterministic device evidence"};
    }
    return {ProfileMatchStatus::Ambiguous, std::nullopt, std::move(evidence),
            "multiple profiles matched equally; explicit selection is required"};
}

ProfileMatchResult ProfileRegistry::match(const sysex::SysExFrame& frame,
                                          const ProfileMatchInput& input) const {
    if (frame.status != sysex::SysExFrameStatus::complete || frame.affected_by_data_loss) {
        return {ProfileMatchStatus::Invalid, std::nullopt,
                {{"", ProfileEvidenceKind::data_integrity_rejection,
                  "incomplete, malformed, or data-loss-affected SysEx cannot identify a device"}},
                "profile matching rejected non-verified SysEx data"};
    }

    if (input.saved_profile_id) {
        const auto* selected = find(*input.saved_profile_id);
        if (!selected || selected->generic) {
            return {ProfileMatchStatus::Invalid, std::nullopt, {},
                    "saved explicit profile binding is unavailable or invalid"};
        }
        return {ProfileMatchStatus::Explicit, selected->profile_id,
                {{selected->profile_id, ProfileEvidenceKind::saved_binding,
                  "saved explicit user binding"}},
                "saved explicit profile binding resolved exactly"};
    }

    if (input.native_identity) {
        std::vector<std::pair<const DeviceProfile*, ProfileMatchEvidence>> candidates;
        for (const auto& [id, profile] : profiles_) {
            if (profile.generic || !profile.recognition.native_identity ||
                *profile.recognition.native_identity != *input.native_identity) continue;
            candidates.push_back({&profile,
                                  {id, ProfileEvidenceKind::native_identity,
                                   "exact device-native identity evidence"}});
        }
        if (!candidates.empty()) return from_candidates(candidates);
    }

    if (input.universal_identity) {
        std::vector<std::pair<const DeviceProfile*, ProfileMatchEvidence>> candidates;
        for (const auto& [id, profile] : profiles_) {
            if (profile.generic || !profile.recognition.universal_identity ||
                *profile.recognition.universal_identity != *input.universal_identity) continue;
            candidates.push_back({&profile,
                                  {id, ProfileEvidenceKind::universal_identity,
                                   "exact Universal MIDI Identity evidence"}});
        }
        if (!candidates.empty()) return from_candidates(candidates);
    }

    std::vector<std::pair<const DeviceProfile*, ProfileMatchEvidence>> fingerprint_candidates;
    for (const auto& [id, profile] : profiles_) {
        if (profile.generic) continue;
        for (const auto& fingerprint : profile.recognition.sysex_fingerprints) {
            if (!fingerprint_matches(fingerprint, frame.bytes)) continue;
            fingerprint_candidates.push_back(
                {&profile,
                 {id, ProfileEvidenceKind::sysex_fingerprint,
                  "matched fingerprint '" + fingerprint.id + "'"}});
            break;
        }
    }
    if (!fingerprint_candidates.empty()) return from_candidates(fingerprint_candidates);

    if (input.manual_profile_id) {
        const auto* selected = find(*input.manual_profile_id);
        if (!selected || selected->generic) {
            return {ProfileMatchStatus::Invalid, std::nullopt, {},
                    "manually selected profile is unavailable or invalid"};
        }
        return {ProfileMatchStatus::Explicit, selected->profile_id,
                {{selected->profile_id, ProfileEvidenceKind::manual_selection,
                  "manual user selection"}},
                "manual profile selection resolved exactly"};
    }

    // port_display_name and route_identity are intentionally ignored. A transport route is not a
    // device identity, especially for synthesizers connected behind a generic DIN interface.
    return generic_fallback("unknown device data remains available through Generic fallback");
}

} // namespace taureon::profiles
