#include "TestSupport.hpp"

#include "app/ConnectionController.hpp"
#include "app/MonitorEventQueue.hpp"
#include "gui/MidiMonitorModel.hpp"
#include "gui/MidiMonitorFilterModel.hpp"
#include "gui/MonitorEventBridge.hpp"
#include "transports/fake/FakeMidiTransport.hpp"

#include <QApplication>
#include <QItemSelectionModel>
#include <QMetaObject>
#include <QTableView>

#include <memory>
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
    });
}
