#include "TestSupport.hpp"

#include "core/sysex/SyxFile.hpp"
#include "profiles/ProfileLoader.hpp"
#include "profiles/ProfileRegistry.hpp"

#include <filesystem>

using namespace taureon;

int main() {
    return test::run([] {
        const std::filesystem::path root{TAUREON_SOURCE_DIR};
        profiles::ProfileRegistry registry;
        std::vector<profiles::ProfileLoadIssue> issues;
        TAUREON_REQUIRE(registry.load_directory(root / "resources" / "device_profiles", issues));
        TAUREON_REQUIRE(issues.empty());

        const auto* summit = registry.find("novation.summit");
        TAUREON_REQUIRE(summit != nullptr);
        auto manual = *summit;
        manual.profile_id = "manual.other";
        manual.display_name = "Manual Other Profile";
        manual.recognition.sysex_fingerprints = {
            {"other-header", 0, {0xF0, 0x7D, 0x55, 0x66}}};
        TAUREON_REQUIRE(registry.register_profile(std::move(manual)));

        const auto fixture = sysex::load_syx_file(
            root / "tests" / "fixtures" / "novation_summit_crazy_sine.syx");
        TAUREON_REQUIRE(fixture);
        TAUREON_REQUIRE(fixture.value().frames.size() == 1);

        profiles::ProfileMatchInput input;
        input.manual_profile_id = "manual.other";
        const auto result = registry.match(fixture.value().frames.front(), input);
        TAUREON_REQUIRE(result.status == profiles::ProfileMatchStatus::ConfidentSuggestion);
        TAUREON_REQUIRE(result.selected_profile_id == std::optional<std::string>{"novation.summit"});
        TAUREON_REQUIRE(result.evidence.size() == 2);
        TAUREON_REQUIRE(result.evidence.front().kind == profiles::ProfileEvidenceKind::sysex_fingerprint);
        TAUREON_REQUIRE(result.evidence.back().profile_id == "manual.other");
        TAUREON_REQUIRE(result.evidence.back().kind ==
                        profiles::ProfileEvidenceKind::overridden_manual_selection);

        input.saved_profile_id = "manual.other";
        const auto explicit_result = registry.match(fixture.value().frames.front(), input);
        TAUREON_REQUIRE(explicit_result.status == profiles::ProfileMatchStatus::Explicit);
        TAUREON_REQUIRE(explicit_result.selected_profile_id ==
                        std::optional<std::string>{"manual.other"});
        TAUREON_REQUIRE(explicit_result.evidence.size() == 1);
        TAUREON_REQUIRE(explicit_result.evidence.front().kind ==
                        profiles::ProfileEvidenceKind::saved_binding);
    });
}
