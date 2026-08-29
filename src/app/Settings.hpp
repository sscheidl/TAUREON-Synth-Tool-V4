#pragma once

#include "core/midi/Result.hpp"
#include "core/midi/RoutePersistence.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace taureon::app {

inline constexpr std::uint32_t current_settings_schema_version = 1;

enum class ThemePreference { system, light, dark };
enum class BackendPreference { automatic, windows_midi_services, winmm };
enum class ReconnectPolicy { require_confirmation, manual_only };
enum class LogLevel { error, warning, info, debug };
enum class ConfirmationPolicy { always_confirm, confirm_when_sending };
enum class SettingsLoadStatus {
    loaded,
    migrated_from_v0,
    loaded_with_defaults,
    fallback_missing,
    fallback_corrupt,
    fallback_future_version,
};

struct Settings {
    std::uint32_t schema_version{current_settings_schema_version};
    ThemePreference theme{ThemePreference::system};
    std::uint32_t ui_scale_percent{100};
    std::filesystem::path standard_path;
    bool restore_last_session{};
    BackendPreference preferred_backend{BackendPreference::automatic};
    std::optional<midi::PersistedMidiRoute> preferred_receive_route;
    std::optional<midi::PersistedMidiRoute> preferred_transmit_route;
    ReconnectPolicy reconnect_policy{ReconnectPolicy::require_confirmation};
    bool monitor_start_paused{};
    std::size_t monitor_history_limit{10'000};
    std::uint32_t sysex_pacing_milliseconds{};
    ConfirmationPolicy confirmation_policy{ConfirmationPolicy::always_confirm};
    bool stop_capture_on_data_loss{true};
    LogLevel log_level{LogLevel::info};
    std::filesystem::path log_destination;
    std::size_t log_rotation_entries{1'000};
    bool include_route_identity_in_bundle{true};

    bool operator==(const Settings&) const = default;
};

struct SettingsLoadResult {
    Settings settings;
    SettingsLoadStatus status{SettingsLoadStatus::fallback_missing};
    std::string detail;

    [[nodiscard]] bool used_fallback() const noexcept {
        return status == SettingsLoadStatus::fallback_missing ||
               status == SettingsLoadStatus::fallback_corrupt ||
               status == SettingsLoadStatus::fallback_future_version;
    }
};

class SettingsStore {
public:
    [[nodiscard]] static Settings defaults();
    [[nodiscard]] static SettingsLoadResult load(const std::filesystem::path& path);
    [[nodiscard]] static SettingsLoadResult deserialize(std::string_view text);
    [[nodiscard]] static midi::Result<void> save(const std::filesystem::path& path,
                                                  const Settings& settings);
    [[nodiscard]] static midi::Result<std::string> serialize(const Settings& settings);
};

[[nodiscard]] const char* to_string(ThemePreference value) noexcept;
[[nodiscard]] const char* to_string(BackendPreference value) noexcept;
[[nodiscard]] const char* to_string(ReconnectPolicy value) noexcept;
[[nodiscard]] const char* to_string(LogLevel value) noexcept;
[[nodiscard]] const char* to_string(ConfirmationPolicy value) noexcept;

} // namespace taureon::app
