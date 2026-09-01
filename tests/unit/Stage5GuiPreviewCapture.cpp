#include "app/ConnectionWorker.hpp"
#include "app/MonitorEventQueue.hpp"
#include "gui/MainWindow.hpp"
#include "profiles/ProfileRegistry.hpp"
#include "transports/fake/FakeMidiTransport.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QImage>
#include <QListWidget>
#include <QStackedWidget>

#include <array>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace {

struct Preview {
    int workspace_index;
    const char* file_name;
};

constexpr std::array kPreviews{
    Preview{0, "01-midi-monitor.png"},
    Preview{1, "02-sysex-transfer.png"},
    Preview{2, "03-sysex-manager.png"},
    Preview{3, "04-librarian.png"},
    Preview{4, "05-devices-and-profiles.png"},
    Preview{5, "06-diagnostics.png"},
    Preview{6, "07-settings.png"},
};

void process_pending_events() {
    QCoreApplication::sendPostedEvents(nullptr, 0);
    QApplication::processEvents(QEventLoop::AllEvents);
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    if (argc != 2) {
        std::cerr << "Expected exactly one output-directory argument.\n";
        return 2;
    }

    const std::filesystem::path output_directory{argv[1]};
    std::error_code error;
    std::filesystem::create_directories(output_directory, error);
    if (error) {
        std::cerr << "Could not create preview directory: " << error.message() << "\n";
        return 3;
    }

    auto profiles = std::make_shared<taureon::profiles::ProfileRegistry>();
    std::vector<taureon::profiles::ProfileLoadIssue> profile_issues;
    if (!profiles->load_directory(
            std::filesystem::path{TAUREON_SOURCE_DIR} / "resources" / "device_profiles",
            profile_issues) ||
        !profile_issues.empty()) {
        std::cerr << "Could not load the actual product profile registry.\n";
        return 4;
    }

    taureon::app::MonitorEventQueue monitor_queue(32);
    taureon::app::ConnectionWorker worker(
        [](const taureon::midi::MidiBackend backend) {
            return std::make_unique<taureon::midi::FakeMidiTransport>(backend);
        });
    taureon::gui::MainWindow window(monitor_queue, worker, profiles);
    window.resize(1920, 1080);
    window.show();
    process_pending_events();

    auto* navigation = window.findChild<QListWidget*>("workspaceNavigation");
    auto* workspace_stack = window.findChild<QStackedWidget*>("workspaceStack");
    if (!navigation || !workspace_stack || navigation->count() != static_cast<int>(kPreviews.size())) {
        std::cerr << "The actual MainWindow workspace shell is incomplete.\n";
        return 5;
    }

    for (const auto& preview : kPreviews) {
        navigation->setCurrentRow(preview.workspace_index);
        process_pending_events();
        if (workspace_stack->currentIndex() != preview.workspace_index) {
            std::cerr << "Workspace activation did not reach index " << preview.workspace_index << ".\n";
            return 6;
        }

        const QImage image = window.grab().toImage();
        if (image.isNull()) {
            std::cerr << "Preview capture returned a null image.\n";
            return 7;
        }
        std::cout << "Captured " << preview.file_name << " at " << image.width() << 'x'
                  << image.height() << " (DPR " << image.devicePixelRatio() << ").\n";
        if (!image.save(QString::fromStdString((output_directory / preview.file_name).string()), "PNG")) {
            std::cerr << "Could not write " << preview.file_name << ".\n";
            return 8;
        }
    }

    return 0;
}
