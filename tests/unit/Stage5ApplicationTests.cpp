#include "TestSupport.hpp"

#include "app/ConnectionController.hpp"
#include "app/MonitorEventQueue.hpp"
#include "gui/MidiMonitorModel.hpp"
#include "gui/MonitorEventBridge.hpp"
#include "transports/fake/FakeMidiTransport.hpp"

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

app::MonitorEvent monitor_event(const std::uint64_t sequence, const std::uint8_t note) {
    return {sequence, midi::MidiDirection::input,
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
    bridge.shutdown();
    TAUREON_REQUIRE(!bridged_queue.push(monitor_event(6, 65)));
    TAUREON_REQUIRE(bridged_queue.stats().rejected_after_close == 1);
}

} // namespace

int main() {
    return test::run([] {
        connection_tests();
        queue_and_model_tests();
    });
}
