#include "gui/MainWindow.hpp"
#include "app/ConnectionWorker.hpp"
#include "app/MonitorEventQueue.hpp"
#include "app/NativeTransportFactory.hpp"
#include "profiles/ProfileRegistry.hpp"

#include <QApplication>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <memory>
#include <string_view>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    taureon::app::MonitorEventQueue monitor_queue(4096);
    std::atomic<std::uint64_t> monitor_sequence{};
    auto profile_registry = std::make_shared<taureon::profiles::ProfileRegistry>();
    std::vector<taureon::profiles::ProfileLoadIssue> profile_issues;
    const auto profile_directory =
        std::filesystem::path{QCoreApplication::applicationDirPath().toStdWString()} /
        "resources" / "device_profiles";
    static_cast<void>(profile_registry->load_directory(profile_directory, profile_issues));
    taureon::app::ConnectionWorker connection_worker(
        taureon::app::create_native_transport,
        [&monitor_queue, &monitor_sequence](const taureon::midi::NativeMidiMessage& message) {
            static_cast<void>(monitor_queue.push(
                {monitor_sequence.fetch_add(1, std::memory_order_relaxed),
                 taureon::midi::MidiDirection::input, message}));
        }, profile_registry);
    taureon::gui::MainWindow window(monitor_queue, connection_worker, profile_registry);
    const bool smoke_test = std::any_of(argv + 1, argv + argc, [](const char* argument) {
        return std::string_view(argument) == "--smoke-test";
    });
    if (smoke_test) {
        return window.has_expected_shell() ? 0 : 1;
    }

    window.show();
    return app.exec();
}
