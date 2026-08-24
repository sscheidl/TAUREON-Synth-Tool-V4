#include "gui/MainWindow.hpp"
#include "app/ConnectionWorker.hpp"
#include "app/MonitorEventQueue.hpp"
#include "app/NativeTransportFactory.hpp"

#include <QApplication>

#include <algorithm>
#include <atomic>
#include <string_view>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    taureon::app::MonitorEventQueue monitor_queue(4096);
    std::atomic<std::uint64_t> monitor_sequence{};
    taureon::app::ConnectionWorker connection_worker(
        taureon::app::create_native_transport,
        [&monitor_queue, &monitor_sequence](const taureon::midi::NativeMidiMessage& message) {
            static_cast<void>(monitor_queue.push(
                {monitor_sequence.fetch_add(1, std::memory_order_relaxed),
                 taureon::midi::MidiDirection::input, message}));
        });
    taureon::gui::MainWindow window(monitor_queue, connection_worker);
    const bool smoke_test = std::any_of(argv + 1, argv + argc, [](const char* argument) {
        return std::string_view(argument) == "--smoke-test";
    });
    if (smoke_test) {
        return window.has_expected_shell() ? 0 : 1;
    }

    window.show();
    return app.exec();
}
