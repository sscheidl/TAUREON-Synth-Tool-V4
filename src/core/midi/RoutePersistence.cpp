#include "RoutePersistence.hpp"

#include <charconv>
#include <cctype>
#include <map>
#include <set>
#include <sstream>
#include <vector>

namespace taureon::midi {
namespace {

MidiError serialization_error(std::string message) {
    return {MidiErrorCode::serialization_error, std::move(message), {}, std::nullopt};
}

std::string encode(const std::string_view value) {
    constexpr char digits[] = "0123456789ABCDEF";
    std::string result;
    for (const unsigned char ch : value) {
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            result.push_back(static_cast<char>(ch));
        } else {
            result.push_back('%');
            result.push_back(digits[ch >> 4]);
            result.push_back(digits[ch & 0x0f]);
        }
    }
    return result;
}

int hex_value(const char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

Result<std::string> decode(const std::string_view value) {
    std::string result;
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] != '%') {
            result.push_back(value[index]);
            continue;
        }
        if (index + 2 >= value.size()) {
            return Result<std::string>::failure(serialization_error("truncated percent escape"));
        }
        const int high = hex_value(value[index + 1]);
        const int low = hex_value(value[index + 2]);
        if (high < 0 || low < 0) {
            return Result<std::string>::failure(serialization_error("invalid percent escape"));
        }
        result.push_back(static_cast<char>((high << 4) | low));
        index += 2;
    }
    return Result<std::string>::success(std::move(result));
}

template <typename Integer>
Result<Integer> parse_integer(const std::map<std::string, std::string>& values,
                              const std::string& key) {
    const auto found = values.find(key);
    if (found == values.end() || found->second.empty()) {
        return Result<Integer>::failure(serialization_error("missing field: " + key));
    }
    Integer value{};
    const auto* begin = found->second.data();
    const auto* end = begin + found->second.size();
    const auto parsed = std::from_chars(begin, end, value);
    if (parsed.ec != std::errc{} || parsed.ptr != end) {
        return Result<Integer>::failure(serialization_error("invalid integer field: " + key));
    }
    return Result<Integer>::success(value);
}

Result<std::map<std::string, std::string>> parse_fields(const std::string_view text) {
    std::map<std::string, std::string> fields;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find(';', start);
        const auto part = text.substr(start, end == std::string_view::npos ? text.size() - start : end - start);
        const auto separator = part.find('=');
        if (separator == std::string_view::npos || separator == 0) {
            return Result<std::map<std::string, std::string>>::failure(
                serialization_error("invalid route field"));
        }
        const std::string key(part.substr(0, separator));
        if (fields.contains(key)) {
            return Result<std::map<std::string, std::string>>::failure(
                serialization_error("duplicate route field: " + key));
        }
        fields.emplace(key, std::string(part.substr(separator + 1)));
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return Result<std::map<std::string, std::string>>::success(std::move(fields));
}

bool contains_only(const std::map<std::string, std::string>& fields,
                   const std::set<std::string>& allowed) {
    for (const auto& [key, value] : fields) {
        static_cast<void>(value);
        if (!allowed.contains(key)) return false;
    }
    return true;
}

} // namespace

Result<std::string> serialize_route(const PersistedMidiRoute& route) {
    if (route.schema_version != current_route_schema_version || !is_valid(route.identity)) {
        return Result<std::string>::failure(serialization_error("route cannot be serialized"));
    }
    std::ostringstream stream;
    stream << "version=" << route.schema_version << ";backend=" << to_string(route.identity.backend)
           << ";direction=" << to_string(route.identity.direction);
    if (const auto* wms = std::get_if<WmsRouteIdentity>(&route.identity.native)) {
        stream << ";endpoint=" << encode(wms->endpoint_device_id)
               << ";group=" << static_cast<unsigned>(wms->group);
    } else if (const auto* winmm = std::get_if<WinmmRouteIdentity>(&route.identity.native)) {
        stream << ";name=" << encode(winmm->port_name)
               << ";wmid=" << winmm->manufacturer_id
               << ";wpid=" << winmm->product_id
               << ";driver=" << winmm->driver_version;
    }
    return Result<std::string>::success(stream.str());
}

Result<PersistedMidiRoute> deserialize_route(const std::string_view text) {
    const auto parsed_fields = parse_fields(text);
    if (!parsed_fields) return Result<PersistedMidiRoute>::failure(parsed_fields.error());
    const auto& fields = parsed_fields.value();

    const auto version = parse_integer<std::uint32_t>(fields, "version");
    if (!version) return Result<PersistedMidiRoute>::failure(version.error());
    if (version.value() != current_route_schema_version) {
        return Result<PersistedMidiRoute>::failure(serialization_error("unsupported route schema version"));
    }
    const auto backend_field = fields.find("backend");
    const auto direction_field = fields.find("direction");
    if (backend_field == fields.end() || direction_field == fields.end()) {
        return Result<PersistedMidiRoute>::failure(serialization_error("missing backend or direction"));
    }

    MidiRouteIdentity identity;
    if (direction_field->second == "input") identity.direction = MidiDirection::input;
    else if (direction_field->second == "output") identity.direction = MidiDirection::output;
    else return Result<PersistedMidiRoute>::failure(serialization_error("invalid direction"));

    if (backend_field->second == "wms") {
        if (!contains_only(fields, {"version", "backend", "direction", "endpoint", "group"})) {
            return Result<PersistedMidiRoute>::failure(serialization_error("unknown WMS route field"));
        }
        const auto endpoint_field = fields.find("endpoint");
        const auto group = parse_integer<unsigned>(fields, "group");
        if (endpoint_field == fields.end() || !group || group.value() > 15) {
            return Result<PersistedMidiRoute>::failure(serialization_error("invalid WMS identity"));
        }
        const auto endpoint = decode(endpoint_field->second);
        if (!endpoint) return Result<PersistedMidiRoute>::failure(endpoint.error());
        identity.backend = MidiBackend::windows_midi_services;
        identity.native = WmsRouteIdentity{endpoint.value(), static_cast<std::uint8_t>(group.value())};
    } else if (backend_field->second == "winmm") {
        if (!contains_only(fields,
                           {"version", "backend", "direction", "name", "wmid", "wpid", "driver"})) {
            return Result<PersistedMidiRoute>::failure(serialization_error("unknown WinMM route field"));
        }
        const auto name_field = fields.find("name");
        const auto wmid = parse_integer<std::uint16_t>(fields, "wmid");
        const auto wpid = parse_integer<std::uint16_t>(fields, "wpid");
        const auto driver = parse_integer<std::uint32_t>(fields, "driver");
        if (name_field == fields.end() || !wmid || !wpid || !driver) {
            return Result<PersistedMidiRoute>::failure(serialization_error("invalid WinMM identity"));
        }
        const auto name = decode(name_field->second);
        if (!name) return Result<PersistedMidiRoute>::failure(name.error());
        identity.backend = MidiBackend::winmm;
        identity.native = WinmmRouteIdentity{name.value(), wmid.value(), wpid.value(), driver.value()};
    } else {
        return Result<PersistedMidiRoute>::failure(serialization_error("invalid backend"));
    }

    if (!is_valid(identity)) {
        return Result<PersistedMidiRoute>::failure(serialization_error("invalid route identity"));
    }
    return Result<PersistedMidiRoute>::success({version.value(), std::move(identity)});
}

} // namespace taureon::midi
