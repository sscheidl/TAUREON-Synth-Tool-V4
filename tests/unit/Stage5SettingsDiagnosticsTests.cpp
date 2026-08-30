#include "TestSupport.hpp"

#include "app/BoundedLog.hpp"
#include "app/Diagnostics.hpp"
#include "app/Settings.hpp"
#include "app/SysExTransferSession.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>

using namespace taureon;

namespace {

std::filesystem::path output_path(std::string_view name) {
    const auto path = std::filesystem::path{TAUREON_TEST_OUTPUT_DIR} / std::string(name);
    std::error_code error;
    std::filesystem::remove(path, error);
    std::filesystem::remove(path.string() + ".taureon.tmp", error);
    return path;
}

midi::PersistedMidiRoute route(midi::MidiDirection direction, std::string identifier) {
    midi::MidiRouteIdentity identity;
    identity.backend = midi::MidiBackend::windows_midi_services;
    identity.direction = direction;
    identity.native = midi::WmsRouteIdentity{std::move(identifier), 7};
    return {midi::current_route_schema_version, std::move(identity)};
}

void settings_roundtrip_and_fallback_tests() {
    app::Settings settings;
    settings.theme = app::ThemePreference::dark;
    settings.ui_scale_percent = 125;
    settings.standard_path = "C:/TAUREON";
    settings.restore_last_session = true;
    settings.preferred_backend = app::BackendPreference::windows_midi_services;
    settings.preferred_receive_route = route(midi::MidiDirection::input, "exact-rx");
    settings.preferred_transmit_route = route(midi::MidiDirection::output, "exact-tx");
    settings.reconnect_policy = app::ReconnectPolicy::manual_only;
    settings.monitor_start_paused = true;
    settings.monitor_history_limit = 2048;
    settings.sysex_pacing_milliseconds = 17;
    settings.confirmation_policy = app::ConfirmationPolicy::confirm_when_sending;
    settings.log_level = app::LogLevel::debug;
    settings.log_rotation_entries = 25;

    const auto path = output_path("stage5-settings.txt");
    TAUREON_REQUIRE(app::SettingsStore::save(path, settings));
    const auto loaded = app::SettingsStore::load(path);
    TAUREON_REQUIRE(loaded.status == app::SettingsLoadStatus::loaded);
    TAUREON_REQUIRE(loaded.settings == settings);

    const auto migrated = app::SettingsStore::deserialize(
        "version=0\ntheme=light\npreferred_receive_route=\npreferred_transmit_route=\n");
    TAUREON_REQUIRE(migrated.status == app::SettingsLoadStatus::migrated_from_v0);
    TAUREON_REQUIRE(migrated.settings.theme == app::ThemePreference::light);

    const auto future = app::SettingsStore::deserialize("version=99\n");
    TAUREON_REQUIRE(future.status == app::SettingsLoadStatus::fallback_future_version);
    TAUREON_REQUIRE(future.used_fallback());

    const auto corrupt = app::SettingsStore::deserialize("version=1\ntheme=neon\n");
    TAUREON_REQUIRE(corrupt.status == app::SettingsLoadStatus::fallback_corrupt);
    TAUREON_REQUIRE(corrupt.used_fallback());

    const auto partial = app::SettingsStore::deserialize("version=1\ntheme=dark\n");
    TAUREON_REQUIRE(partial.status == app::SettingsLoadStatus::loaded_with_defaults);
    TAUREON_REQUIRE(partial.settings.theme == app::ThemePreference::dark);
    TAUREON_REQUIRE(partial.settings.preferred_backend == app::BackendPreference::automatic);

    const auto invalid_route = app::SettingsStore::deserialize(
        "version=1\npreferred_receive_route=version%3D1%3Bbackend%3Dwms%3Bdirection%3Doutput%3Bendpoint%3Drx%3Bgroup%3D0\n");
    TAUREON_REQUIRE(invalid_route.status == app::SettingsLoadStatus::fallback_corrupt);
    TAUREON_REQUIRE(!invalid_route.settings.preferred_receive_route);
    TAUREON_REQUIRE(!invalid_route.settings.preferred_transmit_route);
}

void bounded_log_tests() {
    app::BoundedLog log(3);
    std::thread first([&] { for (int index = 0; index < 10; ++index) log.append(app::LogLevel::info, "a"); });
    std::thread second([&] { for (int index = 0; index < 10; ++index) log.append(app::LogLevel::warning, "b"); });
    first.join();
    second.join();
    TAUREON_REQUIRE(log.size() == 3);
    log.configure(app::LogLevel::warning, 2);
    TAUREON_REQUIRE(log.size() == 2);
    log.append(app::LogLevel::debug, "not retained");
    TAUREON_REQUIRE(log.size() == 2);
}

void diagnostic_bundle_exclusion_tests() {
    constexpr std::string_view marker = "PAYLOAD-MARKER-DO-NOT-EXPORT";
    sysex::SyxDocument document;
    document.raw_bytes = {0xF0, 0x7D};
    document.raw_bytes.insert(document.raw_bytes.end(), marker.begin(), marker.end());
    document.raw_bytes.push_back(0xF7);
    document.frames.push_back({sysex::SysExFrameStatus::complete, document.raw_bytes, {}, std::nullopt, false});

    app::SysExTransferSession session;
    TAUREON_REQUIRE(session.load_document(document, std::string(marker)));
    const auto transfer = session.snapshot();
    TAUREON_REQUIRE(transfer.byte_count == document.raw_bytes.size());
    TAUREON_REQUIRE(transfer.frames.size() == 1);

    app::ConnectionSnapshot connection;
    connection.state = app::ConnectionPresentationState::connected;
    connection.receive_route = route(midi::MidiDirection::input, "bundle-rx").identity;
    connection.transmit_route = route(midi::MidiDirection::output, "bundle-tx").identity;
    app::MonitorQueueStats queue;
    queue.current_size = 1;
    queue.high_water_mark = 3;
    queue.dropped = 2;
    const app::DiagnosticExportPolicy include_route_identity;
    const auto snapshot = app::DiagnosticBundle::make_snapshot(
        connection, transfer, queue, "0.0.0", "test-revision", include_route_identity);
    const auto path = output_path("stage5-diagnostic-bundle.txt");
    TAUREON_REQUIRE(app::DiagnosticBundle::write(path, snapshot));

    std::ifstream stream(path, std::ios::binary);
    const std::string bundle((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    TAUREON_REQUIRE(bundle.find(marker) == std::string::npos);
    TAUREON_REQUIRE(bundle.find("F0 7D") == std::string::npos);
    TAUREON_REQUIRE(bundle.find("application_version=0.0.0") != std::string::npos);
    TAUREON_REQUIRE(bundle.find("sysex_frame_count=1") != std::string::npos);
    TAUREON_REQUIRE(bundle.find("queue_high_water_mark=3") != std::string::npos);

    const app::DiagnosticExportPolicy omit_route_identity{false};
    const auto redacted_snapshot = app::DiagnosticBundle::make_snapshot(
        connection, transfer, queue, "0.0.0", "test-revision", omit_route_identity);
    TAUREON_REQUIRE(redacted_snapshot.receive_route == "omitted by settings");
    TAUREON_REQUIRE(redacted_snapshot.transmit_route == "omitted by settings");
    const auto redacted_path = output_path("stage5-diagnostic-bundle-routes-omitted.txt");
    TAUREON_REQUIRE(app::DiagnosticBundle::write(redacted_path, redacted_snapshot));

    std::ifstream redacted_stream(redacted_path, std::ios::binary);
    const std::string redacted_bundle(
        (std::istreambuf_iterator<char>(redacted_stream)), std::istreambuf_iterator<char>());
    TAUREON_REQUIRE(redacted_bundle.find("bundle-rx") == std::string::npos);
    TAUREON_REQUIRE(redacted_bundle.find("bundle-tx") == std::string::npos);
    TAUREON_REQUIRE(redacted_bundle.find("receive_route=omitted by settings") != std::string::npos);
    TAUREON_REQUIRE(redacted_bundle.find("transmit_route=omitted by settings") != std::string::npos);
    TAUREON_REQUIRE(redacted_bundle.find("application_version=0.0.0") != std::string::npos);
    TAUREON_REQUIRE(redacted_bundle.find("sysex_frame_count=1") != std::string::npos);
}

} // namespace

int main() {
    return test::run([] {
        settings_roundtrip_and_fallback_tests();
        bounded_log_tests();
        diagnostic_bundle_exclusion_tests();
    });
}
