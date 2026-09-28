#include "app/ConnectionWorker.hpp"
#include "app/MonitorEventQueue.hpp"
#include "gui/MainWindow.hpp"
#include "profiles/ProfileRegistry.hpp"
#include "transports/fake/FakeMidiTransport.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QGuiApplication>
#include <QImage>
#include <QListWidget>
#include <QScreen>
#include <QStackedWidget>

#include <array>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
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
    if (argc < 2 || argc > 3) {
        std::cerr << "Expected an output directory and optional --physical-1920x1080.\n";
        return 2;
    }
    const bool physical_1920x1080 = argc == 3 &&
        std::string_view{argv[2]} == "--physical-1920x1080";
    if (argc == 3 && !physical_1920x1080) {
        std::cerr << "Unknown preview mode.\n";
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
    const auto* screen = QGuiApplication::primaryScreen();
    if (!screen) {
        std::cerr << "No primary screen is available.\n";
        return 5;
    }
    const qreal dpr = screen->devicePixelRatio();
    const QSize requested_size = physical_1920x1080 ?
        QSize{qRound(1920.0 / dpr), qRound(1080.0 / dpr)} : QSize{1920, 1080};
    const QSize minimum_hint = window.minimumSizeHint();
    const QSize size_hint = window.sizeHint();
    const QRect screen_geometry = screen->geometry();
    const QRect available_geometry = screen->availableGeometry();
    std::cout << "Qt platform " << QGuiApplication::platformName().toStdString()
              << "; screen logical " << screen_geometry.width() << 'x'
              << screen_geometry.height() << "; available logical "
              << available_geometry.width() << 'x' << available_geometry.height()
              << "; DPR " << dpr << "; logical DPI " << screen->logicalDotsPerInch()
              << "; physical DPI " << screen->physicalDotsPerInch() << ".\n";
    std::cout << "MainWindow minimumSizeHint " << minimum_hint.width() << 'x'
              << minimum_hint.height() << "; sizeHint " << size_hint.width() << 'x'
              << size_hint.height() << "; requested logical " << requested_size.width()
              << 'x' << requested_size.height() << ".\n";
    window.resize(requested_size);
    window.show();
    process_pending_events();
    std::cout << "MainWindow actual logical " << window.width() << 'x' << window.height()
              << "; frame logical " << window.frameGeometry().width() << 'x'
              << window.frameGeometry().height() << ".\n";

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

        const auto* page = workspace_stack->currentWidget();
        const QSize page_minimum_hint = page->minimumSizeHint();
        const QSize page_size_hint = page->sizeHint();
        std::cout << "Workspace " << preview.workspace_index << " minimumSizeHint "
                  << page_minimum_hint.width() << 'x' << page_minimum_hint.height()
                  << "; sizeHint " << page_size_hint.width() << 'x'
                  << page_size_hint.height() << ".\n";

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
