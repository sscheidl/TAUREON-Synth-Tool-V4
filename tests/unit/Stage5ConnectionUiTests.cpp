#include "TestSupport.hpp"

#include "app/ConnectionWorker.hpp"
#include "app/MonitorEventQueue.hpp"
#include "gui/MainWindow.hpp"
#include "transports/fake/FakeMidiTransport.hpp"

#include <QApplication>
#include <QComboBox>
#include <QEventLoop>
#include <QPushButton>
#include <QStatusBar>

#include <mutex>
#include <string>
#include <thread>

using namespace taureon;

namespace {

midi::MidiEndpointDescriptor endpoint(const midi::MidiDirection direction, std::string id) {
    midi::MidiRouteIdentity identity;
    identity.backend = midi::MidiBackend::windows_midi_services;
    identity.direction = direction;
    identity.native = midi::WmsRouteIdentity{std::move(id), 3};
    return {identity, direction == midi::MidiDirection::input ? "Fake RX" : "Fake TX",
            midi::MidiProtocol::midi1,
            {direction == midi::MidiDirection::input, direction == midi::MidiDirection::output,
             true, false},
            std::nullopt, std::string{"Test function block"}};
}

template <typename Predicate>
bool process_until(Predicate&& predicate) {
    constexpr int iteration_limit = 100'000;
    for (int iteration = 0; iteration < iteration_limit; ++iteration) {
        if (predicate()) return true;
        QApplication::processEvents(QEventLoop::AllEvents);
        std::this_thread::yield();
    }
    return predicate();
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    return test::run([&] {
        const auto receive_endpoint = endpoint(midi::MidiDirection::input, "fake-rx-id");
        const auto transmit_endpoint = endpoint(midi::MidiDirection::output, "fake-tx-id");
        std::mutex transport_mutex;
        midi::FakeMidiTransport* transport_ptr = nullptr;
        app::ConnectionWorker worker([&](const midi::MidiBackend backend) {
            TAUREON_REQUIRE(backend == midi::MidiBackend::windows_midi_services);
            auto transport = std::make_unique<midi::FakeMidiTransport>(
                backend, std::vector{receive_endpoint, transmit_endpoint});
            {
                std::scoped_lock lock(transport_mutex);
                transport_ptr = transport.get();
            }
            return transport;
        });
        app::MonitorEventQueue monitor_queue(32);
        gui::MainWindow window(monitor_queue, worker);
        window.show();

        auto* backend = window.findChild<QComboBox*>("backendSelector");
        auto* receive = window.findChild<QComboBox*>("receiveRouteSelector");
        auto* transmit = window.findChild<QComboBox*>("transmitRouteSelector");
        auto* connect = window.findChild<QPushButton*>("connectButton");
        TAUREON_REQUIRE(backend != nullptr);
        TAUREON_REQUIRE(receive != nullptr);
        TAUREON_REQUIRE(transmit != nullptr);
        TAUREON_REQUIRE(connect != nullptr);

        backend->setCurrentIndex(1);
        TAUREON_REQUIRE(process_until([&] {
            return receive->count() == 2 && transmit->count() == 2 && receive->isEnabled();
        }));
        TAUREON_REQUIRE(receive->itemText(1).contains("fake-rx-id"));
        TAUREON_REQUIRE(receive->itemText(1).contains("group 4"));
        TAUREON_REQUIRE(transmit->itemText(1).contains("fake-tx-id"));

        receive->setCurrentIndex(1);
        TAUREON_REQUIRE(connect->isEnabled());
        connect->click();
        TAUREON_REQUIRE(process_until([&] { return connect->text() == "Disconnect"; }));
        TAUREON_REQUIRE(transmit->currentIndex() == 0);
        connect->click();
        TAUREON_REQUIRE(process_until([&] { return connect->text() == "Connect"; }));

        receive->setCurrentIndex(0);
        transmit->setCurrentIndex(1);
        connect->click();
        TAUREON_REQUIRE(process_until([&] { return connect->text() == "Disconnect"; }));
        TAUREON_REQUIRE(receive->currentIndex() == 0);

        {
            std::scoped_lock lock(transport_mutex);
            TAUREON_REQUIRE(transport_ptr != nullptr);
            transport_ptr->remove_endpoint(transmit_endpoint.identity);
        }
        TAUREON_REQUIRE(process_until([&] {
            return window.statusBar()->currentMessage().startsWith("Degraded:");
        }));
        TAUREON_REQUIRE(connect->text() == "Disconnect");

        connect->click();
        TAUREON_REQUIRE(process_until([&] { return connect->text() == "Connect"; }));
    });
}
