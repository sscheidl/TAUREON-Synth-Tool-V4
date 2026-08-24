#include "TestSupport.hpp"

#include "app/ProfileSelectionService.hpp"
#include "core/sysex/SyxFile.hpp"
#include "gui/ProfileMatchPanel.hpp"

#include <QApplication>

#include <filesystem>

using namespace taureon;

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    return test::run([&] {
        const std::filesystem::path root{TAUREON_SOURCE_DIR};
        profiles::ProfileRegistry registry;
        std::vector<profiles::ProfileLoadIssue> issues;
        TAUREON_REQUIRE(registry.load_directory(root / "resources" / "device_profiles", issues));
        TAUREON_REQUIRE(issues.empty());

        auto manual = *registry.find("novation.summit");
        manual.profile_id = "manual.other";
        manual.display_name = "Manual Other Profile";
        manual.recognition.sysex_fingerprints = {
            {"other-header", 0, {0xF0, 0x7D, 0x55, 0x66}}};
        TAUREON_REQUIRE(registry.register_profile(std::move(manual)));
        const auto fixture = sysex::load_syx_file(
            root / "tests" / "fixtures" / "novation_summit_crazy_sine.syx");
        TAUREON_REQUIRE(fixture);

        app::ProfileSelectionService service(registry);
        TAUREON_REQUIRE(service.select_temporary("manual.other"));
        const auto result = service.match(fixture.value().frames.front());
        TAUREON_REQUIRE(result.selected_profile_id == std::optional<std::string>{"novation.summit"});

        gui::ProfileMatchPanel panel;
        bool promotion_invoked = false;
        panel.set_remember_binding_action([&] {
            promotion_invoked = true;
            TAUREON_REQUIRE(service.remember_overridden_manual());
        });
        panel.show();
        panel.present(result);
        TAUREON_REQUIRE(panel.override_is_visible());
        TAUREON_REQUIRE(panel.remember_binding_is_enabled());
        panel.trigger_remember_binding();
        TAUREON_REQUIRE(promotion_invoked);

        const auto rebound = service.match(fixture.value().frames.front());
        TAUREON_REQUIRE(rebound.status == profiles::ProfileMatchStatus::Explicit);
        TAUREON_REQUIRE(rebound.selected_profile_id == std::optional<std::string>{"manual.other"});
        TAUREON_REQUIRE(rebound.evidence.front().kind == profiles::ProfileEvidenceKind::saved_binding);
    });
}
