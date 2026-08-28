#include "TestSupport.hpp"

#include "app/ConnectionWorker.hpp"
#include "app/MonitorEventQueue.hpp"
#include "gui/MainWindow.hpp"
#include "gui/SysExTransferPanel.hpp"
#include "gui/SysExManagerPanel.hpp"
#include "core/sysex/SysEx7.hpp"
#include "transports/fake/FakeMidiTransport.hpp"

#include <QApplication>
#include <QComboBox>
#include <QEventLoop>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QTableView>

#include <filesystem>
#include <memory>
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
        auto profile_registry = std::make_shared<profiles::ProfileRegistry>();
        std::vector<profiles::ProfileLoadIssue> profile_issues;
        TAUREON_REQUIRE(profile_registry->load_directory(
            std::filesystem::path{TAUREON_SOURCE_DIR} / "resources" / "device_profiles",
            profile_issues));
        TAUREON_REQUIRE(profile_issues.empty());
        app::ConnectionWorker worker([&](const midi::MidiBackend backend) {
            TAUREON_REQUIRE(backend == midi::MidiBackend::windows_midi_services);
            auto transport = std::make_unique<midi::FakeMidiTransport>(
                backend, std::vector{receive_endpoint, transmit_endpoint});
            {
                std::scoped_lock lock(transport_mutex);
                transport_ptr = transport.get();
            }
            return transport;
        }, {}, profile_registry);
        app::MonitorEventQueue monitor_queue(32);
        gui::MainWindow window(monitor_queue, worker, profile_registry);
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

        auto* transfer_panel = dynamic_cast<gui::SysExTransferPanel*>(
            window.findChild<QWidget*>("sysExTransferPanel"));
        auto* receive_sysex = window.findChild<QPushButton*>("sysExReceive");
        auto* raw_send = window.findChild<QPushButton*>("sysExRawSend");
        auto* validated_restore = window.findChild<QPushButton*>("sysExValidatedRestore");
        auto* frame_table = window.findChild<QTableView*>("sysExFrameTable");
        auto* route_label = window.findChild<QLabel*>("sysExTxRoute");
        auto* profile_label = window.findChild<QLabel*>("sysExProfileMatch");
        TAUREON_REQUIRE(transfer_panel != nullptr);
        TAUREON_REQUIRE(transfer_panel->has_required_controls());
        TAUREON_REQUIRE(receive_sysex != nullptr);
        TAUREON_REQUIRE(raw_send != nullptr);
        TAUREON_REQUIRE(validated_restore != nullptr);
        TAUREON_REQUIRE(frame_table != nullptr);
        TAUREON_REQUIRE(route_label != nullptr);
        TAUREON_REQUIRE(profile_label != nullptr);
        TAUREON_REQUIRE(!validated_restore->isEnabled());
        auto* manager_panel = dynamic_cast<gui::SysExManagerPanel*>(
            window.findChild<QWidget*>("sysExManagerPanel"));
        TAUREON_REQUIRE(manager_panel != nullptr);
        TAUREON_REQUIRE(manager_panel->has_required_controls());

        receive_sysex->click();
        TAUREON_REQUIRE(process_until([&] { return receive_sysex->text() == "Stop Receive"; }));
        const sysex::SysExFrame received_frame{sysex::SysExFrameStatus::complete,
                                               {0xF0, 0x7D, 0x22, 0xF7}, {},
                                               std::optional<std::uint8_t>{3}, false};
        const auto encoded = sysex::encode_sysex7(received_frame, 3);
        TAUREON_REQUIRE(encoded);
        midi::UmpNativeMessage ump;
        for (const auto& packet : encoded.value()) {
            ump.words.push_back(packet.word0);
            ump.words.push_back(packet.word1);
        }
        {
            std::scoped_lock lock(transport_mutex);
            transport_ptr->emit_received(
                {midi::MidiBackend::windows_midi_services, ump, std::nullopt});
        }
        receive_sysex->click();
        TAUREON_REQUIRE(process_until([&] {
            return receive_sysex->text() == "Receive" && frame_table->model()->rowCount() == 1;
        }));

        connect->click();
        TAUREON_REQUIRE(process_until([&] { return connect->text() == "Connect"; }));

        receive->setCurrentIndex(0);
        transmit->setCurrentIndex(1);
        connect->click();
        TAUREON_REQUIRE(process_until([&] { return connect->text() == "Disconnect"; }));
        TAUREON_REQUIRE(receive->currentIndex() == 0);

        const auto fixture = std::filesystem::path{TAUREON_SOURCE_DIR} / "tests" / "fixtures" /
                             "novation_summit_crazy_sine.syx";
        transfer_panel->request_load(fixture);
        TAUREON_REQUIRE(process_until([&] {
            return frame_table->model()->rowCount() == 1 && raw_send->isEnabled();
        }));
        {
            std::scoped_lock lock(transport_mutex);
            TAUREON_REQUIRE(transport_ptr->diagnostics().transmitted_messages == 0);
        }
        TAUREON_REQUIRE(route_label->text().contains("fake-tx-id"));
        TAUREON_REQUIRE(route_label->text().contains("group 4"));
        TAUREON_REQUIRE(profile_label->text().contains("Novation Summit"));
        TAUREON_REQUIRE(profile_label->text().contains("confident suggestion"));
        TAUREON_REQUIRE(profile_label->text().contains("not declared"));
        raw_send->click();
        TAUREON_REQUIRE(process_until([&] {
            std::scoped_lock lock(transport_mutex);
            return transport_ptr->diagnostics().transmitted_messages == 1;
        }));

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
