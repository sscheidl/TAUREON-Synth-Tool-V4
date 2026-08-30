#pragma once

#include "app/ConnectionController.hpp"
#include "app/MonitorEventQueue.hpp"
#include "app/SysExTransferSession.hpp"
#include "core/midi/Result.hpp"
#include "core/midi/RoutePersistence.hpp"

#include <filesystem>
#include <string>

namespace taureon::app {

struct DiagnosticSnapshot {
    std::string application_version;
    std::string build_revision;
    std::string operating_system;
    std::string process_architecture;
    std::string selected_backend;
    std::string active_backend;
    std::string windows_midi_runtime;
    std::string receive_route;
    std::string transmit_route;
    std::string connection_state;
    std::string transfer_state;
    std::string profile_id;
    std::string profile_match_status;
    std::string profile_evidence;
    std::string profile_validation;
    std::string last_transport_error;
    std::string last_application_error;
    std::string reconnect_transition;
    std::string receive_count;
    std::uint64_t transmit_count{};
    std::uint64_t dropped_count{};
    std::string overflow_count;
    std::string late_callback_count;
    std::size_t queue_current_size{};
    std::size_t queue_high_water_mark{};
    std::uint64_t sysex_frame_count{};
    std::uint64_t sysex_byte_count{};
};

struct DiagnosticExportPolicy {
    bool include_route_identity{true};
};

class DiagnosticBundle {
public:
    [[nodiscard]] static DiagnosticSnapshot make_snapshot(
        const ConnectionSnapshot& connection, const SysExTransferSnapshot& transfer,
        const MonitorQueueStats& queue, std::string application_version, std::string build_revision,
        const DiagnosticExportPolicy& export_policy);
    [[nodiscard]] static midi::Result<void> write(const std::filesystem::path& path,
                                                   const DiagnosticSnapshot& snapshot);
    [[nodiscard]] static std::string text(const DiagnosticSnapshot& snapshot);
};

} // namespace taureon::app
