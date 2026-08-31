#include "TestSupport.hpp"

#include "gui/SysExManagerPanel.hpp"

#include <QApplication>
#include <QItemSelectionModel>
#include <QLabel>
#include <QPlainTextEdit>
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

        std::optional<app::SysExManagerTransferItem> opened;
        gui::SysExManagerPanel panel(
            registry, [&opened](app::SysExManagerTransferItem item,
                                gui::SysExManagerPanel::TransferCompletion completion) {
                opened = std::move(item);
                completion(true);
                return true;
            });
        const auto fixture = std::filesystem::path{TAUREON_SOURCE_DIR} / "tests" / "fixtures" /
                             "novation_summit_crazy_sine.syx";
        const auto total_bytes = std::filesystem::file_size(fixture);
        panel.add_file(fixture);

        auto* files = panel.findChild<QTableView*>("sysExManagerFileTable");
        auto* frames = panel.findChild<QTableView*>("sysExManagerFrameTable");
        auto* summary = panel.findChild<QLabel*>("sysExManagerSummary");
        auto* raw = panel.findChild<QPlainTextEdit*>("sysExManagerRawBytes");
        auto* open = panel.findChild<QPushButton*>("sysExManagerOpenTransfer");
        auto* status = panel.findChild<QLabel*>("sysExManagerStatus");
        TAUREON_REQUIRE(panel.has_required_controls());
        TAUREON_REQUIRE(files != nullptr && files->model()->rowCount() == 1);
        TAUREON_REQUIRE(files->model()->index(0, 1).data().toString().contains("Novation"));
        TAUREON_REQUIRE(frames != nullptr && frames->model()->rowCount() == 1);
        const auto first_frame = frames->model()->index(0, 0);
        frames->selectionModel()->setCurrentIndex(
            first_frame, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        QApplication::processEvents();
        const auto frame_bytes = frames->model()->index(0, 2).data().toULongLong();
        TAUREON_REQUIRE(summary != nullptr && summary->text().contains("workspace file"));
        TAUREON_REQUIRE(raw != nullptr);
        TAUREON_REQUIRE(raw->toPlainText().contains(
            QStringLiteral("Showing 256 of %1 bytes").arg(frame_bytes)));
        TAUREON_REQUIRE(raw->toPlainText().contains("remaining bytes are not displayed"));
        TAUREON_REQUIRE(open != nullptr && open->isEnabled());
        open->click();
        QApplication::processEvents();
        TAUREON_REQUIRE(status != nullptr &&
                        status->text().contains("Opened inspected Manager bytes in SysEx Transfer"));
        TAUREON_REQUIRE(opened.has_value());
        TAUREON_REQUIRE(opened->source_name == "novation_summit_crazy_sine.syx");
        TAUREON_REQUIRE(opened->document.raw_bytes.size() == total_bytes);
        TAUREON_REQUIRE(opened->document.raw_bytes.size() > 256);

        // Complete first so the queued receiver delivery exists, then destroy the
        // receiving Manager panel before processing that queued delivery.
        std::optional<gui::SysExManagerPanel::TransferCompletion> deferred_completion;
        {
            auto shutting_down_panel = std::make_unique<gui::SysExManagerPanel>(
                registry, [&deferred_completion](app::SysExManagerTransferItem,
                                                  gui::SysExManagerPanel::TransferCompletion completion) {
                    deferred_completion = std::move(completion);
                    return true;
                });
            shutting_down_panel->add_file(fixture);
            auto* shutdown_open =
                shutting_down_panel->findChild<QPushButton*>("sysExManagerOpenTransfer");
            TAUREON_REQUIRE(shutdown_open != nullptr && shutdown_open->isEnabled());
            shutdown_open->click();
            TAUREON_REQUIRE(deferred_completion.has_value());
            (*deferred_completion)(true);
        }
        QApplication::processEvents();
    });
}
