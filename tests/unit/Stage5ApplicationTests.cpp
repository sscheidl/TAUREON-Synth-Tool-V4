#include "TestSupport.hpp"

#include "app/ConnectionController.hpp"
#include "app/ConnectionWorker.hpp"
#include "app/MonitorEventQueue.hpp"
#include "gui/MidiMonitorModel.hpp"
#include "gui/MidiMonitorFilterModel.hpp"
#include "gui/MonitorEventBridge.hpp"
#include "transports/fake/FakeMidiTransport.hpp"

#include <QApplication>
#include <QItemSelectionModel>
#include <QMetaObject>
#include <QTableView>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>

using namespace taureon;

namespace {

midi::MidiEndpointDescriptor endpoint(const midi::MidiDirection direction, std::string id) {
    const midi::MidiRouteIdentity identity{midi::MidiBackend::windows_midi_services, direction,
                                           midi::WmsRouteIdentity{std::move(id), 0}};
    return {identity, direction == midi::MidiDirection::input ? "RX" : "TX", midi::MidiProtocol::midi1,
            {direction == midi::MidiDirection::input, direction == midi::MidiDirection::output, true, false},
            std::nullopt, std::nullopt};
}

app::MonitorEvent monitor_event(const std::uint64_t sequence, const std::uint8_t note,
                                const midi::MidiDirection direction = midi::MidiDirection::input) {
    return {sequence, direction,
            {midi::MidiBackend::windows_midi_services,
             midi::Midi1NativeMessage{{0x90, note, 0x7f}}, midi::MidiTimestamp{sequence, "fake"}}};
}

void connection_tests() {
    const auto rx = endpoint(midi::MidiDirection::input, "rx-exact");
    const auto tx = endpoint(midi::MidiDirection::output, "tx-exact");
    midi::FakeMidiTransport transport(midi::MidiBackend::windows_midi_services, {rx, tx});
    app::ConnectionController controller(transport);
    TAUREON_REQUIRE(controller.refresh());
    TAUREON_REQUIRE(controller.select_route(rx.identity));
    TAUREON_REQUIRE(controller.select_route(tx.identity));
    TAUREON_REQUIRE(controller.connect_selected());
    TAUREON_REQUIRE(controller.snapshot().state == app::ConnectionPresentationState::connected);

    transport.remove_endpoint(tx.identity);
    const auto degraded = controller.snapshot();
    TAUREON_REQUIRE(degraded.state == app::ConnectionPresentationState::degraded);
    TAUREON_REQUIRE(degraded.transmit_route == tx.identity);
    TAUREON_REQUIRE(controller.disconnect());

    const midi::MidiRouteIdentity missing{midi::MidiBackend::windows_midi_services,
                                          midi::MidiDirection::output,
                                          midi::WmsRouteIdentity{"missing", 0}};
    const auto failure = controller.select_route(missing);
    TAUREON_REQUIRE(!failure);
    TAUREON_REQUIRE(failure.error().code == midi::MidiErrorCode::endpoint_missing);
}

void fake_close_while_activity_is_in_flight() {
    struct SendGate {
        std::mutex mutex;
        std::condition_variable changed;
        bool send_entered{};
        bool release_send{};
    };

    auto gate = std::make_shared<SendGate>();
    app::MonitorEventQueue presentation_queue(4);
    std::atomic<std::uint64_t> monitor_sequence{};
    gui::MidiMonitorModel presentation_model(4);
    gui::MonitorEventBridge presentation_bridge(presentation_queue, presentation_model);
    midi::FakeMidiTransport* transport{};

    {
        auto worker = std::make_unique<app::ConnectionWorker>(
            [&](const midi::MidiBackend backend) {
                const auto rx = endpoint(midi::MidiDirection::input, "close-rx");
                const auto tx = endpoint(midi::MidiDirection::output, "close-tx");
                auto fake = std::make_unique<midi::FakeMidiTransport>(backend, std::vector{rx, tx});
                fake->set_send_hook([gate](const midi::NativeMidiMessage&) {
                    std::unique_lock lock(gate->mutex);
                    gate->send_entered = true;
                    gate->changed.notify_all();
                    gate->changed.wait(lock, [&] { return gate->release_send; });
                    return midi::Result<void>::success();
                });
                transport = fake.get();
                return fake;
            },
            [&presentation_queue, &monitor_sequence](const midi::NativeMidiMessage& message) {
                static_cast<void>(presentation_queue.push(
                    {monitor_sequence.fetch_add(1, std::memory_order_relaxed),
                     midi::MidiDirection::input, message}));
            });

        const auto selected = worker->select_backend(midi::MidiBackend::windows_midi_services).get();
        TAUREON_REQUIRE(selected && selected.value().endpoints.size() == 2);
        TAUREON_REQUIRE(worker->connect(selected.value().endpoints.at(0).identity,
                                        selected.value().endpoints.at(1).identity).get());

        const std::vector<std::uint8_t> first{0xF0, 0x7D, 0x41, 0xF7};
        const std::vector<std::uint8_t> second{0xF0, 0x7D, 0x42, 0xF7};
        sysex::SyxDocument document;
        document.raw_bytes = first;
        document.raw_bytes.insert(document.raw_bytes.end(), second.begin(), second.end());
        document.frames = {
            {sysex::SysExFrameStatus::complete, first, {}, std::nullopt, false},
            {sysex::SysExFrameStatus::complete, second, {}, std::nullopt, false}};
        TAUREON_REQUIRE(worker->load_sysex_document(std::move(document), "close-in-flight.syx").get());
        TAUREON_REQUIRE(worker->start_raw_sysex_send(std::chrono::milliseconds{0}).get());
        {
            std::unique_lock lock(gate->mutex);
            gate->changed.wait(lock, [&] { return gate->send_entered; });
        }
        const auto in_flight = worker->sysex_snapshot().get();
        TAUREON_REQUIRE(in_flight &&
                        in_flight.value().send_progress.state == transfer::TransferState::running);

        // This mirrors the production close order: presentation acceptance closes before
        // worker teardown. A fake callback after that point cannot reach the destroyed view.
        presentation_bridge.shutdown();
        TAUREON_REQUIRE(transport != nullptr);
        transport->emit_received(
            {midi::MidiBackend::windows_midi_services, midi::Midi1NativeMessage{{0x90, 0x40, 0x7F}},
             std::nullopt});
        TAUREON_REQUIRE(presentation_queue.stats().rejected_after_close == 1);

        const auto cancelling = worker->cancel_sysex_transfer().get();
        TAUREON_REQUIRE(cancelling &&
                        cancelling.value().send_progress.state == transfer::TransferState::cancelling);
        {
            std::scoped_lock lock(gate->mutex);
            gate->release_send = true;
        }
        gate->changed.notify_all();

        // The production close path waits for the accepted in-flight send to reach a terminal
        // state before it disconnects the transport. It is the deterministic synchronization
        // boundary for the terminal snapshot; no sleep or timeout is involved.
        TAUREON_REQUIRE(worker->disconnect().get());
        const auto terminal = worker->sysex_snapshot().get();
        TAUREON_REQUIRE(terminal &&
                        terminal.value().send_progress.state == transfer::TransferState::cancelled);
        // ConnectionWorker destruction joins its worker; no UI object is reachable because the
        // presentation acceptance gate is already closed.
    }

    TAUREON_REQUIRE(presentation_queue.stats().current_size == 0);
    TAUREON_REQUIRE(!presentation_queue.push(monitor_event(10, 69)));
}

void selection_and_shutdown_tests() {
    gui::MidiMonitorModel model(3);
    QTableView table;
    table.setModel(&model);

    model.append_batch({monitor_event(1, 60), monitor_event(2, 61)});
    table.selectionModel()->setCurrentIndex(
        model.index(1, gui::MidiMonitorModel::Raw),
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    TAUREON_REQUIRE(table.selectionModel()->currentIndex().row() == 1);
    TAUREON_REQUIRE(table.selectionModel()->selectedRows().size() == 1);

    // Inserting after the selected event preserves its logical row.
    model.append_batch({monitor_event(3, 62)});
    TAUREON_REQUIRE(table.selectionModel()->currentIndex().row() == 1);
    TAUREON_REQUIRE(table.selectionModel()->currentIndex().data(Qt::DisplayRole).toString() ==
                    "90 3D 7F");

    // Removing the oldest event updates selection to the same surviving event, never a stale row.
    model.append_batch({monitor_event(4, 63)});
    TAUREON_REQUIRE(model.rowCount() == 3);
    TAUREON_REQUIRE(table.selectionModel()->currentIndex().isValid());
    TAUREON_REQUIRE(table.selectionModel()->currentIndex().row() == 0);
    TAUREON_REQUIRE(table.selectionModel()->currentIndex().data(Qt::DisplayRole).toString() ==
                    "90 3D 7F");
    for (const auto& index : table.selectionModel()->selectedRows()) {
        TAUREON_REQUIRE(index.row() >= 0 && index.row() < model.rowCount());
    }

    // A reset clears the former selection deterministically.
    model.append_batch({monitor_event(5, 64), monitor_event(6, 65), monitor_event(7, 66)});
    TAUREON_REQUIRE(model.rowCount() == 3);
    TAUREON_REQUIRE(!table.selectionModel()->currentIndex().isValid());
    TAUREON_REQUIRE(table.selectionModel()->selectedRows().empty());

    app::MonitorEventQueue pending_queue(4);
    auto pending_model = std::make_unique<gui::MidiMonitorModel>(4);
    gui::MonitorEventBridge bridge(pending_queue, *pending_model);
    TAUREON_REQUIRE(pending_queue.push(monitor_event(8, 67)));
    TAUREON_REQUIRE(QMetaObject::invokeMethod(&bridge, [&bridge] { bridge.drain_once(); },
                                               Qt::QueuedConnection));
    // QObject destruction synchronously closes the presentation gate before queued drain executes.
    pending_model.reset();
    QApplication::processEvents(QEventLoop::AllEvents);
    TAUREON_REQUIRE(bridge.presentation_stats().discarded_after_close == 1);
    TAUREON_REQUIRE(pending_queue.stats().current_size == 0);
    TAUREON_REQUIRE(!pending_queue.push(monitor_event(9, 68)));
    TAUREON_REQUIRE(pending_queue.stats().rejected_after_close == 1);
}

void queue_and_model_tests() {
    app::MonitorEventQueue queue(3);
    TAUREON_REQUIRE(queue.push(monitor_event(1, 60)));
    TAUREON_REQUIRE(queue.push(monitor_event(2, 61)));
    TAUREON_REQUIRE(queue.push(monitor_event(3, 62)));
    TAUREON_REQUIRE(!queue.push(monitor_event(4, 63)));
    TAUREON_REQUIRE(queue.stats().high_water_mark == 3);
    TAUREON_REQUIRE(queue.stats().dropped == 1);

    gui::MidiMonitorModel model(2);
    model.append_batch(queue.drain(3));
    TAUREON_REQUIRE(model.rowCount() == 2);
    TAUREON_REQUIRE(model.data(model.index(0, gui::MidiMonitorModel::Raw), Qt::DisplayRole).toString() ==
                    "90 3D 7F");
    TAUREON_REQUIRE(model.data(model.index(1, gui::MidiMonitorModel::Channel), Qt::DisplayRole).toInt() == 1);
    TAUREON_REQUIRE(queue.stats().current_size == 0);
    model.clear();
    TAUREON_REQUIRE(model.rowCount() == 0);

    app::MonitorEventQueue bridged_queue(4);
    gui::MidiMonitorModel bridged_model(4);
    gui::MonitorEventBridge bridge(bridged_queue, bridged_model);
    TAUREON_REQUIRE(bridged_queue.push(monitor_event(5, 64)));
    bridge.drain_once();
    TAUREON_REQUIRE(bridged_model.rowCount() == 1);
    TAUREON_REQUIRE(bridged_queue.push(monitor_event(6, 65)));
    bridge.set_paused(true);
    bridge.drain_once();
    TAUREON_REQUIRE(bridged_model.rowCount() == 1);
    TAUREON_REQUIRE(bridge.presentation_stats().discarded_while_paused == 1);
    bridge.shutdown();
    TAUREON_REQUIRE(!bridged_queue.push(monitor_event(7, 66)));
    TAUREON_REQUIRE(bridged_queue.stats().rejected_after_close == 1);

    gui::MidiMonitorModel filter_source(4);
    filter_source.append_batch({monitor_event(8, 67),
                                monitor_event(9, 68, midi::MidiDirection::output)});
    gui::MidiMonitorFilterModel filter;
    filter.setSourceModel(&filter_source);
    filter.set_direction("TX");
    TAUREON_REQUIRE(filter.rowCount() == 1);
    filter.set_direction({});
    filter.set_type_filter("Note On");
    TAUREON_REQUIRE(filter.rowCount() == 2);
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    return test::run([] {
        connection_tests();
        queue_and_model_tests();
        selection_and_shutdown_tests();
        fake_close_while_activity_is_in_flight();
    });
}
