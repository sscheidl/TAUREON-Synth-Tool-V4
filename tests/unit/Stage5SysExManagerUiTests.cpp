#include "TestSupport.hpp"

#include "gui/SysExManagerPanel.hpp"

#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QTableView>

#include <filesystem>
#include <memory>
#include <optional>

using namespace taureon;

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    return test::run([&] {
        auto registry = std::make_shared<profiles::ProfileRegistry>();
        std::vector<profiles::ProfileLoadIssue> issues;
        TAUREON_REQUIRE(registry->load_directory(
            std::filesystem::path{TAUREON_SOURCE_DIR} / "resources" / "device_profiles", issues));
        TAUREON_REQUIRE(issues.empty());

        std::optional<std::filesystem::path> opened;
        gui::SysExManagerPanel panel(registry, [&opened](const std::filesystem::path& path) {
            opened = path;
        });
        const auto fixture = std::filesystem::path{TAUREON_SOURCE_DIR} / "tests" / "fixtures" /
                             "novation_summit_crazy_sine.syx";
        panel.add_file(fixture);

        auto* files = panel.findChild<QTableView*>("sysExManagerFileTable");
        auto* frames = panel.findChild<QTableView*>("sysExManagerFrameTable");
        auto* summary = panel.findChild<QLabel*>("sysExManagerSummary");
        auto* open = panel.findChild<QPushButton*>("sysExManagerOpenTransfer");
        TAUREON_REQUIRE(panel.has_required_controls());
        TAUREON_REQUIRE(files != nullptr && files->model()->rowCount() == 1);
        TAUREON_REQUIRE(frames != nullptr && frames->model()->rowCount() == 1);
        TAUREON_REQUIRE(summary != nullptr && summary->text().contains("Novation"));
        TAUREON_REQUIRE(open != nullptr && open->isEnabled());
        open->click();
        TAUREON_REQUIRE(opened == fixture);
    });
}
