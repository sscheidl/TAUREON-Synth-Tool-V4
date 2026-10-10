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
#include <QComboBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QWheelEvent>

#include <filesystem>
#include <fstream>
#include <iterator>
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
        auto* scope_notice = settings.findChild<QLabel*>("settingsScopeNotice");
        TAUREON_REQUIRE(scope_notice != nullptr &&
                        scope_notice->text().contains("does not gate Raw Send"));
        auto* backend_preference = settings.findChild<QComboBox*>("settingsPreferredBackend");
        auto* confirmation_preference = settings.findChild<QComboBox*>("settingsConfirmationPolicy");
        auto* generic_pacing = settings.findChild<QSpinBox*>("settingsSysExPacing");
        TAUREON_REQUIRE(backend_preference != nullptr && generic_pacing != nullptr &&
                        confirmation_preference != nullptr);
        TAUREON_REQUIRE(!confirmation_preference->isEnabled() &&
                        confirmation_preference->toolTip().contains("not enforced"));
        backend_preference->clearFocus();
        generic_pacing->clearFocus();
        const int backend_before_wheel = backend_preference->currentIndex();
        const int pacing_before_wheel = generic_pacing->value();
        QWheelEvent backend_wheel(QPointF{5, 5}, QPointF{5, 5}, QPoint{}, QPoint{0, -120},
                                  Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QWheelEvent pacing_wheel(QPointF{5, 5}, QPointF{5, 5}, QPoint{}, QPoint{0, 120},
                                 Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(backend_preference, &backend_wheel);
        QApplication::sendEvent(generic_pacing, &pacing_wheel);
        TAUREON_REQUIRE(backend_preference->currentIndex() == backend_before_wheel);
        TAUREON_REQUIRE(generic_pacing->value() == pacing_before_wheel);
        settings.show();
        QApplication::processEvents(QEventLoop::AllEvents);
        backend_preference->setFocus();
        QApplication::sendEvent(backend_preference, &backend_wheel);
        generic_pacing->setFocus();
        QApplication::sendEvent(generic_pacing, &pacing_wheel);
        TAUREON_REQUIRE(backend_preference->currentIndex() == backend_before_wheel);
        TAUREON_REQUIRE(generic_pacing->value() == pacing_before_wheel);

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

        // Only the explicit engine check creates transports: enumeration only, result shown
        // and exported in the MIDI engines field.
        auto* check_engines = diagnostics.findChild<QPushButton*>("diagnosticsCheckEngines");
        auto* details = diagnostics.findChild<QPlainTextEdit*>("diagnosticsSnapshotText");
        TAUREON_REQUIRE(check_engines != nullptr && details != nullptr);
        TAUREON_REQUIRE(details->toPlainText().contains("MIDI engines: not checked"));
        diagnostics.show();
        check_engines->click();
        // The panel polls its worker futures on a 250 ms timer; an iteration-bounded wait can
        // end on a fast runner before that timer fires once.
        const auto engines_shown = [&] {
            const auto text = details->toPlainText();
            return text.contains("WMS: available, 1 RX / 1 TX routes") &&
                   text.contains("WinMM: available, 1 RX / 1 TX routes");
        };
        QElapsedTimer engines_wait;
        engines_wait.start();
        while (!engines_shown() && engines_wait.elapsed() < 5000) {
            QApplication::processEvents(QEventLoop::AllEvents);
            std::this_thread::yield();
        }
        TAUREON_REQUIRE(engines_shown());
        TAUREON_REQUIRE(check_engines->isEnabled());
        TAUREON_REQUIRE(diagnostics.findChild<QLabel*>("diagnosticsStatus")->text().contains(
            "no port was opened"));
        TAUREON_REQUIRE(transport != nullptr);
        diagnostics.hide();
        const auto engines_bundle = std::filesystem::path{TAUREON_TEST_OUTPUT_DIR} /
                                    "stage5-diagnostics-engines.txt";
        std::filesystem::remove(engines_bundle, error);
        TAUREON_REQUIRE(diagnostics.export_bundle(engines_bundle));
        std::ifstream engines_file(engines_bundle);
        const std::string engines_text{std::istreambuf_iterator<char>{engines_file},
                                       std::istreambuf_iterator<char>{}};
        engines_file.close();
        TAUREON_REQUIRE(engines_text.find("WinMM: available, 1 RX / 1 TX routes") != std::string::npos);
        std::filesystem::remove(engines_bundle, error);
    });
}
