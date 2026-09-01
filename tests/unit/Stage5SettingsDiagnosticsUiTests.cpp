#include "TestSupport.hpp"

#include "app/BoundedLog.hpp"
#include "app/ConnectionWorker.hpp"
#include "app/Diagnostics.hpp"
#include "app/MonitorEventQueue.hpp"
#include "app/Settings.hpp"
#include "gui/DiagnosticsPanel.hpp"
#include "gui/SettingsPanel.hpp"
#include "transports/fake/FakeMidiTransport.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QEventLoop>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

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
        auto export_policy = std::make_shared<app::DiagnosticExportPolicy>();
        const auto path = std::filesystem::path{TAUREON_TEST_OUTPUT_DIR} / "stage5-settings-ui.txt";
        std::error_code error;
        std::filesystem::remove(path, error);

        gui::DiagnosticsPanel diagnostics(worker, queue, export_policy);
        gui::SettingsPanel settings(path, log, export_policy);
        TAUREON_REQUIRE(diagnostics.has_required_controls());
        TAUREON_REQUIRE(settings.has_required_controls());
        TAUREON_REQUIRE(diagnostics.findChild<QPushButton*>("diagnosticsExportBundle") != nullptr);
        TAUREON_REQUIRE(settings.findChild<QPushButton*>("settingsSave") != nullptr);
        TAUREON_REQUIRE(settings.findChild<QLabel*>("settingsPreferredReceiveRoute")->text().contains("No exact"));
        auto* application_scope = settings.findChild<QLabel*>("settingsApplicationScope");
        TAUREON_REQUIRE(application_scope != nullptr &&
                        application_scope->text().contains("every other preference") &&
                        application_scope->text().contains("stored only") &&
                        application_scope->text().contains("log rotation size"));

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

        auto* include_route_identity = settings.findChild<QCheckBox*>("settingsBundleRouteIdentity");
        TAUREON_REQUIRE(include_route_identity != nullptr);
        TAUREON_REQUIRE(export_policy->include_route_identity);
        include_route_identity->setChecked(false);
        TAUREON_REQUIRE(export_policy->include_route_identity);
        TAUREON_REQUIRE(settings.save());
        TAUREON_REQUIRE(!export_policy->include_route_identity);

        const auto default_bundle = std::filesystem::path{TAUREON_TEST_OUTPUT_DIR} /
                                    "stage5-diagnostics-default-policy.txt";
        std::filesystem::remove(default_bundle, error);
        gui::DiagnosticsPanel default_policy_diagnostics(worker, queue, {});
        TAUREON_REQUIRE(default_policy_diagnostics.export_bundle(default_bundle));
        TAUREON_REQUIRE(std::filesystem::exists(default_bundle));
        std::filesystem::remove(default_bundle, error);

        diagnostics.show();
        QEventLoop wait_for_visible_refresh;
        QTimer::singleShot(600, &wait_for_visible_refresh, &QEventLoop::quit);
        wait_for_visible_refresh.exec();
        TAUREON_REQUIRE(diagnostics.findChild<QLabel*>("diagnosticsStatus")->text().contains(
            "Safe snapshots refreshed"));
        diagnostics.hide();
        TAUREON_REQUIRE(transport == nullptr);
    });
}
