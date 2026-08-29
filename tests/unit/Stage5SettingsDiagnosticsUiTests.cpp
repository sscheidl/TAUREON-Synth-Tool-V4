#include "TestSupport.hpp"

#include "app/BoundedLog.hpp"
#include "app/ConnectionWorker.hpp"
#include "app/MonitorEventQueue.hpp"
#include "app/Settings.hpp"
#include "gui/DiagnosticsPanel.hpp"
#include "gui/SettingsPanel.hpp"
#include "transports/fake/FakeMidiTransport.hpp"

#include <QApplication>
#include <QEventLoop>
#include <QLabel>
#include <QPushButton>

#include <filesystem>
#include <memory>
#include <string>
#include <thread>

using namespace taureon;

namespace {

template <typename Predicate>
bool process_until(Predicate&& predicate) {
    for (int index = 0; index < 10'000; ++index) {
        if (predicate()) return true;
        QApplication::processEvents(QEventLoop::AllEvents);
        std::this_thread::yield();
    }
    return predicate();
}

midi::MidiEndpointDescriptor endpoint(midi::MidiDirection direction) {
    midi::MidiRouteIdentity identity;
    identity.backend = midi::MidiBackend::windows_midi_services;
    identity.direction = direction;
    identity.native = midi::WmsRouteIdentity{direction == midi::MidiDirection::input ? "ui-rx" : "ui-tx", 2};
    return {identity, "UI test endpoint", midi::MidiProtocol::midi1,
            {direction == midi::MidiDirection::input, direction == midi::MidiDirection::output, true, false},
            std::nullopt, "UI test group"};
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    return test::run([&] {
        midi::FakeMidiTransport* transport{};
        app::ConnectionWorker worker([&](midi::MidiBackend backend) {
            auto result = std::make_unique<midi::FakeMidiTransport>(
                backend, std::vector{endpoint(midi::MidiDirection::input), endpoint(midi::MidiDirection::output)});
            transport = result.get();
            return result;
        });
        app::MonitorEventQueue queue(8);
        auto log = std::make_shared<app::BoundedLog>(16);
        const auto path = std::filesystem::path{TAUREON_TEST_OUTPUT_DIR} / "stage5-settings-ui.txt";
        std::error_code error;
        std::filesystem::remove(path, error);

        gui::DiagnosticsPanel diagnostics(worker, queue);
        gui::SettingsPanel settings(path, log);
        TAUREON_REQUIRE(diagnostics.has_required_controls());
        TAUREON_REQUIRE(settings.has_required_controls());
        TAUREON_REQUIRE(diagnostics.findChild<QPushButton*>("diagnosticsExportBundle") != nullptr);
        TAUREON_REQUIRE(settings.findChild<QPushButton*>("settingsSave") != nullptr);
        TAUREON_REQUIRE(settings.findChild<QLabel*>("settingsPreferredReceiveRoute")->text().contains("No exact"));

        app::ConnectionSnapshot snapshot;
        snapshot.receive_route = endpoint(midi::MidiDirection::input).identity;
        snapshot.transmit_route = endpoint(midi::MidiDirection::output).identity;
        settings.set_connection_snapshot(snapshot);
        auto* capture = settings.findChild<QPushButton*>("settingsCaptureExactRoutes");
        TAUREON_REQUIRE(capture != nullptr && capture->isEnabled());
        capture->click();
        TAUREON_REQUIRE(settings.save());
        const auto reloaded = app::SettingsStore::load(path);
        TAUREON_REQUIRE(reloaded.settings.preferred_receive_route.has_value());
        TAUREON_REQUIRE(reloaded.settings.preferred_transmit_route.has_value());
        TAUREON_REQUIRE(reloaded.settings.preferred_receive_route->identity == snapshot.receive_route);
        TAUREON_REQUIRE(reloaded.settings.preferred_transmit_route->identity == snapshot.transmit_route);

        TAUREON_REQUIRE(process_until([&] {
            return diagnostics.findChild<QLabel*>("diagnosticsStatus")->text().contains("Safe snapshots") ||
                   diagnostics.findChild<QLabel*>("diagnosticsStatus")->text().contains("Refreshing");
        }));
        TAUREON_REQUIRE(transport == nullptr);
    });
}
