#include "app/Settings.hpp"

#include <charconv>
#include <cerrno>
#include <fstream>
#include <map>
#include <string_view>
#include <system_error>
#include <type_traits>

#ifdef _WIN32
#include <windows.h>
#endif

namespace taureon::app {
namespace {

midi::MidiError settings_error(std::string message) {
    return {midi::MidiErrorCode::serialization_error, std::move(message), "settings", std::nullopt};
}

std::string escape(std::string_view value) {
    constexpr char digits[] = "0123456789ABCDEF";
    std::string result;
    for (const unsigned char character : value) {
        if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') || character == '-' || character == '_' ||
            character == '.' || character == '/') {
            result.push_back(static_cast<char>(character));
        } else {
            result.push_back('%');
            result.push_back(digits[character >> 4U]);
            result.push_back(digits[character & 0x0fU]);
        }
    }
    return result;
}

int hex_value(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

std::optional<std::string> unescape(std::string_view value) {
    std::string result;
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] != '%') {
            result.push_back(value[index]);
            continue;
        }
        if (index + 2 >= value.size()) return std::nullopt;
        const auto high = hex_value(value[index + 1]);
        const auto low = hex_value(value[index + 2]);
        if (high < 0 || low < 0) return std::nullopt;
        result.push_back(static_cast<char>((high << 4) | low));
        index += 2;
    }
    return result;
}

std::optional<std::map<std::string, std::string>> fields(std::string_view text) {
    std::map<std::string, std::string> result;
    std::size_t begin{};
    while (begin < text.size()) {
        const auto end = text.find('\n', begin);
        const auto line = text.substr(begin, end == std::string_view::npos ? text.size() - begin : end - begin);
        if (!line.empty()) {
            const auto separator = line.find('=');
            if (separator == std::string_view::npos || separator == 0) return std::nullopt;
            auto [_, inserted] = result.emplace(std::string(line.substr(0, separator)),
                                                std::string(line.substr(separator + 1)));
            if (!inserted) return std::nullopt;
        }
        if (end == std::string_view::npos) break;
        begin = end + 1;
    }
    return result;
}

template <typename Integer>
std::optional<Integer> integer(const std::map<std::string, std::string>& values, std::string_view key) {
    const auto found = values.find(std::string(key));
    if (found == values.end()) return std::nullopt;
    Integer result{};
    const auto parsed = std::from_chars(found->second.data(),
                                        found->second.data() + found->second.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != found->second.data() + found->second.size()) {
        return std::nullopt;
    }
    return result;
}

std::optional<bool> boolean(const std::map<std::string, std::string>& values, std::string_view key) {
    const auto found = values.find(std::string(key));
    if (found == values.end()) return std::nullopt;
    if (found->second == "true") return true;
    if (found->second == "false") return false;
    return std::nullopt;
}

template <typename Enum>
std::optional<Enum> enum_value(const std::map<std::string, std::string>& values, std::string_view key,
                               std::initializer_list<std::pair<std::string_view, Enum>> options) {
    const auto found = values.find(std::string(key));
    if (found == values.end()) return std::nullopt;
    for (const auto& [text, value] : options) {
        if (found->second == text) return value;
    }
    return std::nullopt;
}

std::string path_to_string(const std::filesystem::path& path) {
    const auto value = path.generic_u8string();
    return {value.begin(), value.end()};
}

std::optional<std::filesystem::path> decoded_path(const std::map<std::string, std::string>& values,
                                                  std::string_view key) {
    const auto found = values.find(std::string(key));
    if (found == values.end()) return std::nullopt;
    const auto decoded = unescape(found->second);
    if (!decoded) return std::nullopt;
    return std::filesystem::u8path(*decoded);
}

midi::Result<void> write_atomic(const std::filesystem::path& path, std::string_view text) {
    std::error_code error;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) return midi::Result<void>::failure(settings_error("cannot create settings directory"));
    }
    auto temporary = path;
    temporary += ".taureon.tmp";
    std::filesystem::remove(temporary, error);
    error.clear();
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) return midi::Result<void>::failure(settings_error("cannot create settings temporary file"));
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        stream.flush();
        if (!stream) {
            stream.close();
            std::filesystem::remove(temporary, error);
            return midi::Result<void>::failure(settings_error("cannot write settings temporary file"));
        }
    }
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::filesystem::remove(temporary, error);
        return midi::Result<void>::failure(settings_error("cannot replace settings atomically"));
    }
#else
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(temporary, error);
        return midi::Result<void>::failure(settings_error("cannot replace settings atomically"));
    }
#endif
    return midi::Result<void>::success();
}

bool valid_route(const std::optional<midi::PersistedMidiRoute>& route, midi::MidiDirection direction) {
    return !route || (route->identity.direction == direction && midi::is_valid(route->identity));
}

} // namespace

const char* to_string(const ThemePreference value) noexcept {
    switch (value) { case ThemePreference::system: return "system"; case ThemePreference::light: return "light"; case ThemePreference::dark: return "dark"; }
    return "system";
}
const char* to_string(const BackendPreference value) noexcept {
    switch (value) { case BackendPreference::automatic: return "automatic"; case BackendPreference::windows_midi_services: return "wms"; case BackendPreference::winmm: return "winmm"; }
    return "automatic";
}
const char* to_string(const ReconnectPolicy value) noexcept {
    switch (value) { case ReconnectPolicy::require_confirmation: return "require_confirmation"; case ReconnectPolicy::manual_only: return "manual_only"; }
    return "require_confirmation";
}
const char* to_string(const LogLevel value) noexcept {
    switch (value) { case LogLevel::error: return "error"; case LogLevel::warning: return "warning"; case LogLevel::info: return "info"; case LogLevel::debug: return "debug"; }
    return "info";
}
const char* to_string(const ConfirmationPolicy value) noexcept {
    switch (value) { case ConfirmationPolicy::always_confirm: return "always_confirm"; case ConfirmationPolicy::confirm_when_sending: return "confirm_when_sending"; }
    return "always_confirm";
}

Settings SettingsStore::defaults() { return {}; }

midi::Result<std::string> SettingsStore::serialize(const Settings& settings) {
    if (settings.schema_version != current_settings_schema_version || settings.ui_scale_percent < 50 ||
        settings.ui_scale_percent > 300 || settings.monitor_history_limit == 0 ||
        settings.log_rotation_entries == 0 || !valid_route(settings.preferred_receive_route, midi::MidiDirection::input) ||
        !valid_route(settings.preferred_transmit_route, midi::MidiDirection::output)) {
        return midi::Result<std::string>::failure(settings_error("settings violate the supported schema"));
    }
    auto route = [](const std::optional<midi::PersistedMidiRoute>& value) -> midi::Result<std::string> {
        if (!value) return midi::Result<std::string>::success({});
        return midi::serialize_route(*value);
    };
    const auto receive = route(settings.preferred_receive_route);
    const auto transmit = route(settings.preferred_transmit_route);
    if (!receive) return midi::Result<std::string>::failure(receive.error());
    if (!transmit) return midi::Result<std::string>::failure(transmit.error());
    std::string result;
    const auto field = [&result](std::string_view key, std::string_view value) {
        result.append(key);
        result.push_back('=');
        result.append(value);
        result.push_back('\n');
    };
    field("version", "1");
    field("theme", to_string(settings.theme));
    field("ui_scale_percent", std::to_string(settings.ui_scale_percent));
    field("standard_path", escape(path_to_string(settings.standard_path)));
    field("restore_last_session", settings.restore_last_session ? "true" : "false");
    field("preferred_backend", to_string(settings.preferred_backend));
    field("preferred_receive_route", escape(receive.value()));
    field("preferred_transmit_route", escape(transmit.value()));
    field("reconnect_policy", to_string(settings.reconnect_policy));
    field("monitor_start_paused", settings.monitor_start_paused ? "true" : "false");
    field("monitor_history_limit", std::to_string(settings.monitor_history_limit));
    field("sysex_pacing_milliseconds", std::to_string(settings.sysex_pacing_milliseconds));
    field("confirmation_policy", to_string(settings.confirmation_policy));
    field("stop_capture_on_data_loss", settings.stop_capture_on_data_loss ? "true" : "false");
    field("log_level", to_string(settings.log_level));
    field("log_destination", escape(path_to_string(settings.log_destination)));
    field("log_rotation_entries", std::to_string(settings.log_rotation_entries));
    field("include_route_identity_in_bundle", settings.include_route_identity_in_bundle ? "true" : "false");
    return midi::Result<std::string>::success(std::move(result));
}

SettingsLoadResult SettingsStore::deserialize(const std::string_view text) {
    const auto parsed = fields(text);
    if (!parsed) return {defaults(), SettingsLoadStatus::fallback_corrupt, "settings syntax is invalid"};
    const auto version = integer<std::uint32_t>(*parsed, "version");
    if (!version) return {defaults(), SettingsLoadStatus::fallback_corrupt, "settings version is missing or invalid"};
    if (*version > current_settings_schema_version) {
        return {defaults(), SettingsLoadStatus::fallback_future_version, "a newer settings schema was not loaded"};
    }
    if (*version != 0 && *version != current_settings_schema_version) {
        return {defaults(), SettingsLoadStatus::fallback_corrupt, "settings schema is unsupported"};
    }
    Settings result = defaults();
    bool partial = *version == 0;
    const auto required = [&](std::string_view key) { return parsed->contains(std::string(key)); };
    for (const auto key : {"theme", "ui_scale_percent", "standard_path", "restore_last_session", "preferred_backend",
                           "preferred_receive_route", "preferred_transmit_route", "reconnect_policy",
                           "monitor_start_paused", "monitor_history_limit", "sysex_pacing_milliseconds",
                           "confirmation_policy", "stop_capture_on_data_loss", "log_level", "log_destination",
                           "log_rotation_entries", "include_route_identity_in_bundle"}) {
        partial = partial || !required(key);
    }
    const auto theme = enum_value(*parsed, "theme",
        std::initializer_list<std::pair<std::string_view, ThemePreference>>{{"system", ThemePreference::system}, {"light", ThemePreference::light}, {"dark", ThemePreference::dark}});
    const auto backend = enum_value(*parsed, "preferred_backend",
        std::initializer_list<std::pair<std::string_view, BackendPreference>>{{"automatic", BackendPreference::automatic}, {"wms", BackendPreference::windows_midi_services}, {"winmm", BackendPreference::winmm}});
    const auto reconnect = enum_value(*parsed, "reconnect_policy",
        std::initializer_list<std::pair<std::string_view, ReconnectPolicy>>{{"require_confirmation", ReconnectPolicy::require_confirmation}, {"manual_only", ReconnectPolicy::manual_only}});
    const auto confirmation = enum_value(*parsed, "confirmation_policy",
        std::initializer_list<std::pair<std::string_view, ConfirmationPolicy>>{{"always_confirm", ConfirmationPolicy::always_confirm}, {"confirm_when_sending", ConfirmationPolicy::confirm_when_sending}});
    const auto log_level = enum_value(*parsed, "log_level",
        std::initializer_list<std::pair<std::string_view, LogLevel>>{{"error", LogLevel::error}, {"warning", LogLevel::warning}, {"info", LogLevel::info}, {"debug", LogLevel::debug}});
    if ((required("theme") && !theme) || (required("preferred_backend") && !backend) ||
        (required("reconnect_policy") && !reconnect) || (required("confirmation_policy") && !confirmation) ||
        (required("log_level") && !log_level)) {
        return {defaults(), SettingsLoadStatus::fallback_corrupt, "settings contain an invalid policy value"};
    }
    if (theme) result.theme = *theme;
    if (backend) result.preferred_backend = *backend;
    if (reconnect) result.reconnect_policy = *reconnect;
    if (confirmation) result.confirmation_policy = *confirmation;
    if (log_level) result.log_level = *log_level;
    const auto apply_integer = [&](auto& target, std::string_view key, auto minimum, auto maximum) -> bool {
        if (!required(key)) return true;
        const auto value = integer<std::decay_t<decltype(target)>>(*parsed, key);
        if (!value || *value < minimum || *value > maximum) return false;
        target = *value;
        return true;
    };
    if (!apply_integer(result.ui_scale_percent, "ui_scale_percent", 50U, 300U) ||
        !apply_integer(result.monitor_history_limit, "monitor_history_limit", std::size_t{1}, std::size_t{1'000'000}) ||
        !apply_integer(result.sysex_pacing_milliseconds, "sysex_pacing_milliseconds", 0U, 60'000U) ||
        !apply_integer(result.log_rotation_entries, "log_rotation_entries", std::size_t{1}, std::size_t{1'000'000})) {
        return {defaults(), SettingsLoadStatus::fallback_corrupt, "settings contain an invalid numeric value"};
    }
    const auto apply_bool = [&](bool& target, std::string_view key) -> bool {
        if (!required(key)) return true;
        const auto value = boolean(*parsed, key);
        if (!value) return false;
        target = *value;
        return true;
    };
    if (!apply_bool(result.restore_last_session, "restore_last_session") ||
        !apply_bool(result.monitor_start_paused, "monitor_start_paused") ||
        !apply_bool(result.stop_capture_on_data_loss, "stop_capture_on_data_loss") ||
        !apply_bool(result.include_route_identity_in_bundle, "include_route_identity_in_bundle")) {
        return {defaults(), SettingsLoadStatus::fallback_corrupt, "settings contain an invalid boolean value"};
    }
    const auto set_path = [&](std::filesystem::path& target, std::string_view key) -> bool {
        if (!required(key)) return true;
        const auto value = decoded_path(*parsed, key);
        if (!value) return false;
        target = *value;
        return true;
    };
    if (!set_path(result.standard_path, "standard_path") || !set_path(result.log_destination, "log_destination")) {
        return {defaults(), SettingsLoadStatus::fallback_corrupt, "settings contain an invalid path"};
    }
    const auto set_route = [&](std::optional<midi::PersistedMidiRoute>& target, std::string_view key,
                               midi::MidiDirection direction) -> bool {
        if (!required(key)) return true;
        const auto encoded = parsed->at(std::string(key));
        if (encoded.empty()) { target.reset(); return true; }
        const auto decoded = unescape(encoded);
        if (!decoded) return false;
        const auto route = midi::deserialize_route(*decoded);
        if (!route || route.value().identity.direction != direction) return false;
        target = route.value();
        return true;
    };
    if (!set_route(result.preferred_receive_route, "preferred_receive_route", midi::MidiDirection::input) ||
        !set_route(result.preferred_transmit_route, "preferred_transmit_route", midi::MidiDirection::output)) {
        return {defaults(), SettingsLoadStatus::fallback_corrupt, "settings contain an invalid exact route identity"};
    }
    result.schema_version = current_settings_schema_version;
    if (*version == 0) return {std::move(result), SettingsLoadStatus::migrated_from_v0, "version 0 settings migrated"};
    if (partial) return {std::move(result), SettingsLoadStatus::loaded_with_defaults, "partial settings completed with safe defaults"};
    return {std::move(result), SettingsLoadStatus::loaded, "settings loaded"};
}

SettingsLoadResult SettingsStore::load(const std::filesystem::path& path) {
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        return {defaults(), SettingsLoadStatus::fallback_missing, "settings file does not exist"};
    }
    if (error) return {defaults(), SettingsLoadStatus::fallback_corrupt, "settings file cannot be inspected"};
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return {defaults(), SettingsLoadStatus::fallback_corrupt, "settings file cannot be opened"};
    const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (!stream.eof() && stream.fail()) return {defaults(), SettingsLoadStatus::fallback_corrupt, "settings file cannot be read"};
    return deserialize(text);
}

midi::Result<void> SettingsStore::save(const std::filesystem::path& path, const Settings& settings) {
    const auto text = serialize(settings);
    if (!text) return midi::Result<void>::failure(text.error());
    return write_atomic(path, text.value());
}

} // namespace taureon::app
