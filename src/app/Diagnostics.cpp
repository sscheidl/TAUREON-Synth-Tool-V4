#include "app/Diagnostics.hpp"

#include <fstream>
#include <string_view>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

namespace taureon::app {
namespace {

const char* connection_state(const ConnectionPresentationState value) noexcept {
    switch (value) {
    case ConnectionPresentationState::disconnected: return "disconnected";
    case ConnectionPresentationState::ready: return "ready";
    case ConnectionPresentationState::connected: return "connected";
    case ConnectionPresentationState::degraded: return "degraded";
    case ConnectionPresentationState::error: return "error";
    }
    return "not observed";
}

const char* transfer_state(const transfer::TransferState value) noexcept {
    switch (value) {
    case transfer::TransferState::idle: return "idle";
    case transfer::TransferState::preparing: return "preparing";
    case transfer::TransferState::running: return "running";
    case transfer::TransferState::cancelling: return "cancelling";
    case transfer::TransferState::completed: return "completed";
    case transfer::TransferState::cancelled: return "cancelled";
    case transfer::TransferState::failed: return "failed";
    }
    return "not observed";
}

const char* match_state(const profiles::ProfileMatchStatus value) noexcept {
    switch (value) {
    case profiles::ProfileMatchStatus::Explicit: return "explicit";
    case profiles::ProfileMatchStatus::ConfidentSuggestion: return "confident suggestion";
    case profiles::ProfileMatchStatus::Ambiguous: return "ambiguous";
    case profiles::ProfileMatchStatus::NoMatch: return "no match";
    case profiles::ProfileMatchStatus::Invalid: return "invalid";
    case profiles::ProfileMatchStatus::GenericFallback: return "generic fallback";
    }
    return "not observed";
}

std::string route(const std::optional<midi::MidiRouteIdentity>& value) {
    if (!value) return "not observed";
    const auto serialized = midi::serialize_route({midi::current_route_schema_version, *value});
    return serialized ? serialized.value() : "not observed: invalid route identity";
}

std::string backend(const std::optional<midi::MidiRouteIdentity>& receive,
                    const std::optional<midi::MidiRouteIdentity>& transmit) {
    if (receive && transmit && receive->backend != transmit->backend) return "not observed: inconsistent routes";
    if (receive) return midi::to_string(receive->backend);
    if (transmit) return midi::to_string(transmit->backend);
    return "not observed";
}

std::string error_summary(const std::optional<midi::MidiError>& value) {
    if (!value) return "not observed";
    return "observed: " + value->source + " (code " +
           std::to_string(static_cast<int>(value->code)) + ")";
}

midi::Result<void> write_atomic(const std::filesystem::path& path, std::string_view text) {
    std::error_code error;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) return midi::Result<void>::failure(
            {midi::MidiErrorCode::io_error, "cannot create diagnostic bundle directory", "diagnostics", std::nullopt});
    }
    auto temporary = path;
    temporary += ".taureon.tmp";
    std::filesystem::remove(temporary, error);
    error.clear();
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) return midi::Result<void>::failure(
            {midi::MidiErrorCode::io_error, "cannot create diagnostic bundle temporary file", "diagnostics", std::nullopt});
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        stream.flush();
        if (!stream) {
            stream.close();
            std::filesystem::remove(temporary, error);
            return midi::Result<void>::failure(
                {midi::MidiErrorCode::io_error, "cannot write diagnostic bundle temporary file", "diagnostics", std::nullopt});
        }
    }
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::filesystem::remove(temporary, error);
        return midi::Result<void>::failure(
            {midi::MidiErrorCode::io_error, "cannot replace diagnostic bundle atomically", "diagnostics", std::nullopt});
    }
#else
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(temporary, error);
        return midi::Result<void>::failure(
            {midi::MidiErrorCode::io_error, "cannot replace diagnostic bundle atomically", "diagnostics", std::nullopt});
    }
#endif
    return midi::Result<void>::success();
}

} // namespace

DiagnosticSnapshot DiagnosticBundle::make_snapshot(
    const ConnectionSnapshot& connection, const SysExTransferSnapshot& transfer,
    const MonitorQueueStats& queue, std::string application_version, std::string build_revision) {
    DiagnosticSnapshot result;
    result.application_version = std::move(application_version);
    result.build_revision = build_revision.empty() ? "not embedded" : std::move(build_revision);
#ifdef _WIN32
    result.operating_system = "Windows (runtime build not observed)";
#else
    result.operating_system = "non-Windows build host";
#endif
    result.process_architecture = sizeof(void*) == 8 ? "64-bit" : "32-bit";
    result.selected_backend = backend(connection.receive_route, connection.transmit_route);
    result.active_backend = connection.state == ConnectionPresentationState::connected ||
                                    connection.state == ConnectionPresentationState::degraded
                                ? result.selected_backend
                                : "not observed";
    result.windows_midi_runtime = "not observed: no safe runtime/API-mode snapshot contract";
    result.receive_route = route(connection.receive_route);
    result.transmit_route = route(connection.transmit_route);
    result.connection_state = connection_state(connection.state);
    result.transfer_state = transfer_state(transfer.send_progress.state);
    result.profile_id = transfer.profile_id.value_or("not observed");
    result.profile_match_status = match_state(transfer.profile_match_status);
    result.profile_evidence = transfer.profile_match_message.empty() ? "not observed" : transfer.profile_match_message;
    result.profile_validation = transfer.profile_warnings.empty() ? "not observed" : transfer.profile_warnings.front();
    result.last_transport_error = connection.state == ConnectionPresentationState::error
                                      ? "observed: transport/application error"
                                      : "not observed";
    result.last_application_error = error_summary(transfer.send_error);
    result.reconnect_transition = connection.state == ConnectionPresentationState::degraded
                                      ? "selected route unavailable; deliberate reselection required"
                                      : "not observed";
    result.receive_count = "not observed: safe snapshot has no directional RX counter";
    result.transmit_count = transfer.send_progress.messages_accepted;
    result.dropped_count = transfer.application_dropped_events + queue.dropped;
    result.overflow_count = std::to_string(queue.dropped);
    result.late_callback_count = "not observed: queue rejection is not a native late-callback counter";
    result.queue_current_size = queue.current_size;
    result.queue_high_water_mark = queue.high_water_mark;
    result.sysex_frame_count = transfer.frames.size();
    result.sysex_byte_count = transfer.byte_count;
    return result;
}

std::string DiagnosticBundle::text(const DiagnosticSnapshot& snapshot) {
    // This deliberately serializes only this metadata snapshot. It has no raw MIDI/SysEx,
    // document names, manager contents, transfer logs, or source file data.
    std::string result = "TAUREON diagnostic bundle v1\n";
    const auto field = [&result](std::string_view key, const auto& value) {
        result.append(key);
        result.push_back('=');
        result.append(std::to_string(value));
        result.push_back('\n');
    };
    const auto string_field = [&result](std::string_view key, std::string_view value) {
        result.append(key);
        result.push_back('=');
        result.append(value);
        result.push_back('\n');
    };
    string_field("application_version", snapshot.application_version);
    string_field("build_revision", snapshot.build_revision);
    string_field("operating_system", snapshot.operating_system);
    string_field("process_architecture", snapshot.process_architecture);
    string_field("selected_backend", snapshot.selected_backend);
    string_field("active_backend", snapshot.active_backend);
    string_field("windows_midi_runtime", snapshot.windows_midi_runtime);
    string_field("receive_route", snapshot.receive_route);
    string_field("transmit_route", snapshot.transmit_route);
    string_field("connection_state", snapshot.connection_state);
    string_field("transfer_state", snapshot.transfer_state);
    string_field("profile_id", snapshot.profile_id);
    string_field("profile_match_status", snapshot.profile_match_status);
    string_field("profile_evidence", snapshot.profile_evidence);
    string_field("profile_validation", snapshot.profile_validation);
    string_field("last_transport_error", snapshot.last_transport_error);
    string_field("last_application_error", snapshot.last_application_error);
    string_field("reconnect_transition", snapshot.reconnect_transition);
    string_field("receive_count", snapshot.receive_count);
    field("transmit_count", snapshot.transmit_count);
    field("dropped_count", snapshot.dropped_count);
    string_field("overflow_count", snapshot.overflow_count);
    string_field("late_callback_count", snapshot.late_callback_count);
    field("queue_current_size", snapshot.queue_current_size);
    field("queue_high_water_mark", snapshot.queue_high_water_mark);
    field("sysex_frame_count", snapshot.sysex_frame_count);
    field("sysex_byte_count", snapshot.sysex_byte_count);
    return result;
}

midi::Result<void> DiagnosticBundle::write(const std::filesystem::path& path,
                                           const DiagnosticSnapshot& snapshot) {
    return write_atomic(path, text(snapshot));
}

} // namespace taureon::app
