#include "TestSupport.hpp"

#include "app/MonitorEventQueue.hpp"
#include "gui/MidiMonitorModel.hpp"
#include "gui/MonitorEventBridge.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <thread>

using namespace taureon;

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    return test::run([&] {
        constexpr std::uint64_t event_count = 100'000;
        constexpr std::size_t queue_capacity = 8'192;
        constexpr std::size_t history_limit = 10'000;
        constexpr qint64 heartbeat_target_ms = 250;

        app::MonitorEventQueue queue(queue_capacity);
        gui::MidiMonitorModel model(history_limit);
        gui::MonitorEventBridge bridge(queue, model);

        QElapsedTimer elapsed;
        QElapsedTimer heartbeat_elapsed;
        elapsed.start();
        heartbeat_elapsed.start();
        qint64 maximum_heartbeat_delay_ms = 0;
        std::uint64_t heartbeat_count = 0;
        QTimer heartbeat;
        heartbeat.setInterval(1);
        heartbeat.setTimerType(Qt::PreciseTimer);
        QObject::connect(&heartbeat, &QTimer::timeout, [&] {
            maximum_heartbeat_delay_ms =
                std::max(maximum_heartbeat_delay_ms, heartbeat_elapsed.restart());
            ++heartbeat_count;
        });
        heartbeat.start();

        std::atomic<bool> producer_done{false};
        std::thread producer([&] {
            for (std::uint64_t sequence = 0; sequence < event_count; ++sequence) {
                const auto note = static_cast<std::uint8_t>(sequence % 128);
                static_cast<void>(queue.push(
                    {sequence, midi::MidiDirection::input,
                     {midi::MidiBackend::windows_midi_services,
                      midi::Midi1NativeMessage{{0x90, note, 0x7F}},
                      midi::MidiTimestamp{sequence, "stress"}}}));
            }
            producer_done.store(true, std::memory_order_release);
        });

        while (!producer_done.load(std::memory_order_acquire) || queue.stats().current_size != 0) {
            bridge.drain_once();
            QCoreApplication::processEvents(QEventLoop::AllEvents);
        }
        producer.join();
        bridge.drain_once();
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        heartbeat.stop();

        const auto queue_stats = queue.stats();
        const auto presentation = bridge.presentation_stats();
        TAUREON_REQUIRE(queue_stats.accepted + queue_stats.dropped == event_count);
        TAUREON_REQUIRE(queue_stats.current_size == 0);
        TAUREON_REQUIRE(queue_stats.high_water_mark <= queue_capacity);
        TAUREON_REQUIRE(presentation.displayed == queue_stats.accepted);
        TAUREON_REQUIRE(presentation.discarded_while_paused == 0);
        TAUREON_REQUIRE(model.rowCount() ==
                        static_cast<int>(std::min<std::uint64_t>(queue_stats.accepted, history_limit)));
        TAUREON_REQUIRE(model.rowCount() <= static_cast<int>(history_limit));
        TAUREON_REQUIRE(heartbeat_count > 0);
        TAUREON_REQUIRE(maximum_heartbeat_delay_ms <= heartbeat_target_ms);

        std::cout << "{\"events\":" << event_count
                  << ",\"accepted\":" << queue_stats.accepted
                  << ",\"dropped\":" << queue_stats.dropped
                  << ",\"queue_high_water\":" << queue_stats.high_water_mark
                  << ",\"model_rows\":" << model.rowCount()
                  << ",\"heartbeats\":" << heartbeat_count
                  << ",\"max_heartbeat_delay_ms\":" << maximum_heartbeat_delay_ms
                  << ",\"elapsed_ms\":" << elapsed.elapsed() << "}\n";
    });
}
