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
#include <iostream>
#include <string_view>
#include <utility>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // Read-only product-path diagnosis: no endpoint is opened and no MIDI is sent.
    if (argc == 3 && std::string_view{argv[1]} == "--list-midi") {
        const std::string_view backend_name{argv[2]};
        if (backend_name != "wms" && backend_name != "winmm") {
            std::cerr << "Expected --list-midi wms|winmm\n";
            return 2;
        }
        try {
            auto transport = taureon::app::create_native_transport(backend_name == "wms" ?
                taureon::midi::MidiBackend::windows_midi_services : taureon::midi::MidiBackend::winmm);
            const auto endpoints = transport->enumerate();
            if (!endpoints) {
                std::cerr << endpoints.error().message << '\n';
                return 1;
            }
            std::size_t inputs = 0;
            std::size_t outputs = 0;
            for (const auto& endpoint : endpoints.value()) {
                if (endpoint.identity.direction == taureon::midi::MidiDirection::input) ++inputs;
                else ++outputs;
                std::cout << (endpoint.identity.direction == taureon::midi::MidiDirection::input ? "RX " : "TX ")
                          << endpoint.display_name << '\n';
            }
            std::cout << backend_name << ": " << inputs << " input routes, " << outputs
                      << " output routes; enumeration only, no endpoint opened\n";
            return 0;
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            return 1;
        }
    }

    taureon::app::MonitorEventQueue monitor_queue(4096);
    std::atomic<std::uint64_t> monitor_sequence{};
    auto profile_registry = std::make_shared<taureon::profiles::ProfileRegistry>();
    std::vector<taureon::profiles::ProfileLoadIssue> profile_issues;
    const auto profile_directory =
        std::filesystem::path{QCoreApplication::applicationDirPath().toStdWString()} /
        "resources" / "device_profiles";
    const auto profile_load = profile_registry->load_directory(profile_directory, profile_issues);
    if (!profile_load) profile_issues.push_back({profile_directory, profile_load.error()});
    taureon::app::ConnectionWorker connection_worker(
        taureon::app::create_native_transport,
        [&monitor_queue, &monitor_sequence](const taureon::midi::NativeMidiMessage& message) {
            static_cast<void>(monitor_queue.push(
                {monitor_sequence.fetch_add(1, std::memory_order_relaxed),
                 taureon::midi::MidiDirection::input, message}));
        }, profile_registry);
    taureon::gui::MainWindow window(monitor_queue, connection_worker, profile_registry,
                                    std::move(profile_issues));
    const bool smoke_test = std::any_of(argv + 1, argv + argc, [](const char* argument) {
        return std::string_view(argument) == "--smoke-test";
    });
    if (smoke_test) {
        return window.has_expected_shell() ? 0 : 1;
    }

    window.show();
    return app.exec();
}
