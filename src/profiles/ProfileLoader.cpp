#include "ProfileLoader.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <fstream>
#include <limits>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>

namespace taureon::profiles {
namespace {

struct JsonValue {
    using Array = std::vector<JsonValue>;
    using Object = std::map<std::string, JsonValue, std::less<>>;
    std::variant<std::nullptr_t, bool, std::int64_t, std::string, Array, Object> value;
};

class JsonFailure final : public std::runtime_error {
public:
    JsonFailure(std::size_t offset, std::string message)
        : std::runtime_error("byte " + std::to_string(offset) + ": " + std::move(message)) {}
};

class JsonParser {
public:
    explicit JsonParser(std::string_view input) : input_(input) {}

    JsonValue parse() {
        skip_space();
        auto result = parse_value();
        skip_space();
        if (position_ != input_.size()) fail("unexpected trailing data");
        return result;
    }

private:
    [[noreturn]] void fail(std::string message) const {
        throw JsonFailure(position_, std::move(message));
    }

    void skip_space() {
        while (position_ < input_.size() &&
               (input_[position_] == ' ' || input_[position_] == '\t' ||
                input_[position_] == '\r' || input_[position_] == '\n')) {
            ++position_;
        }
    }

    bool consume(char value) {
        if (position_ >= input_.size() || input_[position_] != value) return false;
        ++position_;
        return true;
    }

    JsonValue parse_value() {
        if (position_ >= input_.size()) fail("expected a JSON value");
        switch (input_[position_]) {
        case '{': return JsonValue{parse_object()};
        case '[': return JsonValue{parse_array()};
        case '"': return JsonValue{parse_string()};
        case 't': parse_literal("true"); return JsonValue{true};
        case 'f': parse_literal("false"); return JsonValue{false};
        case 'n': parse_literal("null"); return JsonValue{nullptr};
        default:
            if (input_[position_] == '-' || std::isdigit(static_cast<unsigned char>(input_[position_]))) {
                return JsonValue{parse_integer()};
            }
            fail("unexpected character");
        }
    }

    JsonValue::Object parse_object() {
        consume('{');
        skip_space();
        JsonValue::Object object;
        if (consume('}')) return object;
        while (true) {
            if (position_ >= input_.size() || input_[position_] != '"') {
                fail("object key must be a string");
            }
            auto key = parse_string();
            skip_space();
            if (!consume(':')) fail("expected ':' after object key");
            skip_space();
            auto [_, inserted] = object.emplace(std::move(key), parse_value());
            if (!inserted) fail("duplicate object key");
            skip_space();
            if (consume('}')) break;
            if (!consume(',')) fail("expected ',' or '}'");
            skip_space();
        }
        return object;
    }

    JsonValue::Array parse_array() {
        consume('[');
        skip_space();
        JsonValue::Array array;
        if (consume(']')) return array;
        while (true) {
            array.push_back(parse_value());
            skip_space();
            if (consume(']')) break;
            if (!consume(',')) fail("expected ',' or ']'");
            skip_space();
        }
        return array;
    }

    static void append_utf8(std::string& output, std::uint32_t codepoint) {
        if (codepoint <= 0x7F) {
            output.push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7FF) {
            output.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else {
            output.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
    }

    std::string parse_string() {
        consume('"');
        std::string result;
        while (position_ < input_.size()) {
            const auto value = input_[position_++];
            if (value == '"') return result;
            if (static_cast<unsigned char>(value) < 0x20) fail("control character in string");
            if (value != '\\') {
                result.push_back(value);
                continue;
            }
            if (position_ >= input_.size()) fail("unterminated escape sequence");
            const auto escaped = input_[position_++];
            switch (escaped) {
            case '"': result.push_back('"'); break;
            case '\\': result.push_back('\\'); break;
            case '/': result.push_back('/'); break;
            case 'b': result.push_back('\b'); break;
            case 'f': result.push_back('\f'); break;
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            case 'u': {
                if (input_.size() - position_ < 4) fail("incomplete unicode escape");
                std::uint32_t codepoint{};
                for (int index = 0; index < 4; ++index) {
                    const auto digit = input_[position_++];
                    codepoint <<= 4;
                    if (digit >= '0' && digit <= '9') codepoint |= digit - '0';
                    else if (digit >= 'a' && digit <= 'f') codepoint |= digit - 'a' + 10;
                    else if (digit >= 'A' && digit <= 'F') codepoint |= digit - 'A' + 10;
                    else fail("invalid unicode escape");
                }
                if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
                    fail("surrogate unicode escapes are not supported");
                }
                append_utf8(result, codepoint);
                break;
            }
            default: fail("invalid escape sequence");
            }
        }
        fail("unterminated string");
    }

    std::int64_t parse_integer() {
        const auto start = position_;
        if (consume('-') && position_ >= input_.size()) fail("incomplete number");
        if (consume('0')) {
            if (position_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[position_]))) {
                fail("leading zero in number");
            }
        } else {
            if (position_ >= input_.size() || !std::isdigit(static_cast<unsigned char>(input_[position_]))) {
                fail("invalid integer");
            }
            while (position_ < input_.size() &&
                   std::isdigit(static_cast<unsigned char>(input_[position_]))) ++position_;
        }
        if (position_ < input_.size() &&
            (input_[position_] == '.' || input_[position_] == 'e' || input_[position_] == 'E')) {
            fail("profile numbers must be integers");
        }
        std::int64_t result{};
        const auto text = input_.substr(start, position_ - start);
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
        if (parsed.ec != std::errc{}) fail("integer is out of range");
        return result;
    }

    void parse_literal(std::string_view literal) {
        if (input_.substr(position_, literal.size()) != literal) fail("invalid literal");
        position_ += literal.size();
    }

    std::string_view input_;
    std::size_t position_{};
};

const JsonValue::Object& object(const JsonValue& value, std::string_view path) {
    const auto* result = std::get_if<JsonValue::Object>(&value.value);
    if (!result) throw std::runtime_error(std::string(path) + " must be an object");
    return *result;
}

const JsonValue::Array& array(const JsonValue& value, std::string_view path) {
    const auto* result = std::get_if<JsonValue::Array>(&value.value);
    if (!result) throw std::runtime_error(std::string(path) + " must be an array");
    return *result;
}

const JsonValue& required(const JsonValue::Object& value, std::string_view key,
                          std::string_view path) {
    const auto found = value.find(key);
    if (found == value.end()) {
        throw std::runtime_error(std::string(path) + '.' + std::string(key) + " is required");
    }
    return found->second;
}

const JsonValue* optional(const JsonValue::Object& value, std::string_view key) {
    const auto found = value.find(key);
    return found == value.end() ? nullptr : &found->second;
}

std::string string_value(const JsonValue& value, std::string_view path) {
    const auto* result = std::get_if<std::string>(&value.value);
    if (!result) throw std::runtime_error(std::string(path) + " must be a string");
    return *result;
}

bool bool_value(const JsonValue& value, std::string_view path) {
    const auto* result = std::get_if<bool>(&value.value);
    if (!result) throw std::runtime_error(std::string(path) + " must be a boolean");
    return *result;
}

std::int64_t integer_value(const JsonValue& value, std::string_view path) {
    const auto* result = std::get_if<std::int64_t>(&value.value);
    if (!result) throw std::runtime_error(std::string(path) + " must be an integer");
    return *result;
}

template <typename Integer>
Integer bounded_integer(const JsonValue& value, std::string_view path, std::int64_t minimum,
                        std::int64_t maximum) {
    const auto result = integer_value(value, path);
    if (result < minimum || result > maximum) {
        throw std::runtime_error(std::string(path) + " is outside the allowed range");
    }
    return static_cast<Integer>(result);
}

void reject_unknown(const JsonValue::Object& value, std::initializer_list<std::string_view> keys,
                    std::string_view path) {
    for (const auto& [key, _] : value) {
        if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
            throw std::runtime_error(std::string(path) + " contains unknown field '" + key + "'");
        }
    }
}

std::vector<std::string> string_array(const JsonValue& value, std::string_view path) {
    std::vector<std::string> result;
    const auto& values = array(value, path);
    result.reserve(values.size());
    for (std::size_t index = 0; index < values.size(); ++index) {
        result.push_back(string_value(values[index], std::string(path) + '[' +
                                                        std::to_string(index) + ']'));
    }
    return result;
}

std::vector<std::uint8_t> byte_array(const JsonValue& value, std::string_view path) {
    std::vector<std::uint8_t> result;
    const auto& values = array(value, path);
    result.reserve(values.size());
    for (std::size_t index = 0; index < values.size(); ++index) {
        result.push_back(bounded_integer<std::uint8_t>(
            values[index], std::string(path) + '[' + std::to_string(index) + ']', 0, 255));
    }
    return result;
}

std::vector<NamedNumber> named_numbers(const JsonValue& value, std::string_view path,
                                       std::uint16_t maximum) {
    std::vector<NamedNumber> result;
    const auto& values = array(value, path);
    for (std::size_t index = 0; index < values.size(); ++index) {
        const auto item_path = std::string(path) + '[' + std::to_string(index) + ']';
        const auto& item = object(values[index], item_path);
        reject_unknown(item, {"number", "name"}, item_path);
        result.push_back({bounded_integer<std::uint16_t>(required(item, "number", item_path),
                                                        item_path + ".number", 0, maximum),
                          string_value(required(item, "name", item_path), item_path + ".name")});
    }
    return result;
}

std::optional<std::string> optional_string(const JsonValue::Object& value, std::string_view key,
                                           std::string_view path) {
    const auto* field = optional(value, key);
    if (!field || std::holds_alternative<std::nullptr_t>(field->value)) return std::nullopt;
    return string_value(*field, std::string(path) + '.' + std::string(key));
}

DeviceProfile decode_profile(const JsonValue& root_value) {
    const auto& root = object(root_value, "profile");
    reject_unknown(root,
                   {"schema_version", "profile_id", "profile_version", "generic",
                    "manufacturer", "model", "variant", "display_name", "aliases",
                    "support", "metadata", "defaults", "warnings", "recognition",
                    "provenance", "protocol_module_id"},
                   "profile");

    DeviceProfile profile;
    profile.schema_version = bounded_integer<std::uint32_t>(
        required(root, "schema_version", "profile"), "profile.schema_version", 0,
        std::numeric_limits<std::uint32_t>::max());
    profile.profile_id = string_value(required(root, "profile_id", "profile"),
                                      "profile.profile_id");
    profile.profile_version = string_value(required(root, "profile_version", "profile"),
                                           "profile.profile_version");
    profile.generic = bool_value(required(root, "generic", "profile"), "profile.generic");
    profile.manufacturer = optional_string(root, "manufacturer", "profile");
    profile.model = optional_string(root, "model", "profile");
    profile.variant = optional_string(root, "variant", "profile");
    profile.display_name = string_value(required(root, "display_name", "profile"),
                                        "profile.display_name");
    profile.aliases = string_array(required(root, "aliases", "profile"), "profile.aliases");
    profile.protocol_module_id = optional_string(root, "protocol_module_id", "profile");

    const auto& support = object(required(root, "support", "profile"), "profile.support");
    reject_unknown(support,
                   {"detect", "read", "inspect", "extract", "modify", "serialize",
                    "transfer", "validated_restore"},
                   "profile.support");
    profile.support = {
        bool_value(required(support, "detect", "profile.support"), "profile.support.detect"),
        bool_value(required(support, "read", "profile.support"), "profile.support.read"),
        bool_value(required(support, "inspect", "profile.support"), "profile.support.inspect"),
        bool_value(required(support, "extract", "profile.support"), "profile.support.extract"),
        bool_value(required(support, "modify", "profile.support"), "profile.support.modify"),
        bool_value(required(support, "serialize", "profile.support"), "profile.support.serialize"),
        bool_value(required(support, "transfer", "profile.support"), "profile.support.transfer"),
        bool_value(required(support, "validated_restore", "profile.support"),
                   "profile.support.validated_restore")};

    const auto& metadata = object(required(root, "metadata", "profile"), "profile.metadata");
    reject_unknown(metadata, {"channels", "cc", "rpn", "nrpn", "bank_organization"},
                   "profile.metadata");
    for (const auto& channel : array(required(metadata, "channels", "profile.metadata"),
                                     "profile.metadata.channels")) {
        profile.metadata.channels.push_back(
            bounded_integer<std::uint8_t>(channel, "profile.metadata.channels[]", 1, 16));
    }
    profile.metadata.control_changes = named_numbers(
        required(metadata, "cc", "profile.metadata"), "profile.metadata.cc", 127);
    profile.metadata.rpn = named_numbers(required(metadata, "rpn", "profile.metadata"),
                                         "profile.metadata.rpn", 16383);
    profile.metadata.nrpn = named_numbers(required(metadata, "nrpn", "profile.metadata"),
                                          "profile.metadata.nrpn", 16383);
    if (const auto* bank_value = optional(metadata, "bank_organization");
        bank_value && !std::holds_alternative<std::nullptr_t>(bank_value->value)) {
        const auto& bank = object(*bank_value, "profile.metadata.bank_organization");
        reject_unknown(bank, {"description", "bank_count", "slots_per_bank"},
                       "profile.metadata.bank_organization");
        BankOrganization decoded{
            string_value(required(bank, "description", "profile.metadata.bank_organization"),
                         "profile.metadata.bank_organization.description")};
        if (const auto* count = optional(bank, "bank_count")) {
            decoded.bank_count = bounded_integer<std::uint16_t>(
                *count, "profile.metadata.bank_organization.bank_count", 1, 65535);
        }
        if (const auto* slots = optional(bank, "slots_per_bank")) {
            decoded.slots_per_bank = bounded_integer<std::uint16_t>(
                *slots, "profile.metadata.bank_organization.slots_per_bank", 1, 65535);
        }
        profile.metadata.bank_organization = std::move(decoded);
    }

    const auto& defaults = object(required(root, "defaults", "profile"), "profile.defaults");
    reject_unknown(defaults, {"midi_channel", "sysex_inter_message_delay_ms"},
                   "profile.defaults");
    if (const auto* channel = optional(defaults, "midi_channel");
        channel && !std::holds_alternative<std::nullptr_t>(channel->value)) {
        profile.defaults.midi_channel = bounded_integer<std::uint8_t>(
            *channel, "profile.defaults.midi_channel", 1, 16);
    }
    if (const auto* delay = optional(defaults, "sysex_inter_message_delay_ms");
        delay && !std::holds_alternative<std::nullptr_t>(delay->value)) {
        profile.defaults.sysex_inter_message_delay_ms = bounded_integer<std::uint32_t>(
            *delay, "profile.defaults.sysex_inter_message_delay_ms", 0, 10000);
    }
    profile.warnings = string_array(required(root, "warnings", "profile"), "profile.warnings");

    const auto& recognition =
        object(required(root, "recognition", "profile"), "profile.recognition");
    reject_unknown(recognition, {"native_identity", "universal_identity", "sysex_fingerprints"},
                   "profile.recognition");
    if (const auto* native_value = optional(recognition, "native_identity");
        native_value && !std::holds_alternative<std::nullptr_t>(native_value->value)) {
        const auto& native = object(*native_value, "profile.recognition.native_identity");
        reject_unknown(native, {"manufacturer", "model", "variant"},
                       "profile.recognition.native_identity");
        profile.recognition.native_identity = NativeIdentityEvidence{
            string_value(required(native, "manufacturer", "profile.recognition.native_identity"),
                         "profile.recognition.native_identity.manufacturer"),
            string_value(required(native, "model", "profile.recognition.native_identity"),
                         "profile.recognition.native_identity.model"),
            optional_string(native, "variant", "profile.recognition.native_identity")};
    }
    if (const auto* universal_value = optional(recognition, "universal_identity");
        universal_value && !std::holds_alternative<std::nullptr_t>(universal_value->value)) {
        const auto& universal = object(*universal_value, "profile.recognition.universal_identity");
        reject_unknown(universal, {"manufacturer_id", "family", "model"},
                       "profile.recognition.universal_identity");
        profile.recognition.universal_identity = UniversalIdentityEvidence{
            byte_array(required(universal, "manufacturer_id",
                                "profile.recognition.universal_identity"),
                       "profile.recognition.universal_identity.manufacturer_id"),
            bounded_integer<std::uint16_t>(
                required(universal, "family", "profile.recognition.universal_identity"),
                "profile.recognition.universal_identity.family", 0, 16383),
            bounded_integer<std::uint16_t>(
                required(universal, "model", "profile.recognition.universal_identity"),
                "profile.recognition.universal_identity.model", 0, 16383)};
    }
    const auto& fingerprints = array(
        required(recognition, "sysex_fingerprints", "profile.recognition"),
        "profile.recognition.sysex_fingerprints");
    for (std::size_t index = 0; index < fingerprints.size(); ++index) {
        const auto path = "profile.recognition.sysex_fingerprints[" + std::to_string(index) + ']';
        const auto& fingerprint = object(fingerprints[index], path);
        reject_unknown(fingerprint, {"id", "offset", "bytes"}, path);
        profile.recognition.sysex_fingerprints.push_back({
            string_value(required(fingerprint, "id", path), path + ".id"),
            bounded_integer<std::size_t>(required(fingerprint, "offset", path), path + ".offset",
                                         0, 1024 * 1024),
            byte_array(required(fingerprint, "bytes", path), path + ".bytes")});
    }

    const auto& provenance =
        object(required(root, "provenance", "profile"), "profile.provenance");
    reject_unknown(provenance, {"source", "owner", "license", "redistribution", "acquisition"},
                   "profile.provenance");
    profile.provenance = {
        string_value(required(provenance, "source", "profile.provenance"),
                     "profile.provenance.source"),
        string_value(required(provenance, "owner", "profile.provenance"),
                     "profile.provenance.owner"),
        string_value(required(provenance, "license", "profile.provenance"),
                     "profile.provenance.license"),
        string_value(required(provenance, "redistribution", "profile.provenance"),
                     "profile.provenance.redistribution"),
        string_value(required(provenance, "acquisition", "profile.provenance"),
                     "profile.provenance.acquisition")};
    return profile;
}

midi::MidiError invalid_profile(std::string message) {
    return {midi::MidiErrorCode::malformed_data, std::move(message), "profile-schema", std::nullopt};
}

bool empty(const std::string& value) { return value.empty(); }

template <typename Value, typename Projection>
bool has_duplicate(const std::vector<Value>& values, Projection projection) {
    std::set<std::invoke_result_t<Projection, const Value&>> seen;
    for (const auto& value : values) {
        if (!seen.insert(projection(value)).second) return true;
    }
    return false;
}

} // namespace

midi::Result<void> validate_profile(const DeviceProfile& profile) {
    if (profile.schema_version != current_profile_schema_version) {
        return midi::Result<void>::failure(invalid_profile(
            "unsupported profile schema version " + std::to_string(profile.schema_version)));
    }
    static const std::regex id_pattern("^[a-z0-9]+(?:[._-][a-z0-9]+)*$");
    static const std::regex version_pattern("^[0-9]+\\.[0-9]+\\.[0-9]+$");
    if (!std::regex_match(profile.profile_id, id_pattern)) {
        return midi::Result<void>::failure(invalid_profile("profile_id is not stable/canonical"));
    }
    if (!std::regex_match(profile.profile_version, version_pattern)) {
        return midi::Result<void>::failure(invalid_profile("profile_version must be major.minor.patch"));
    }
    if (empty(profile.display_name)) {
        return midi::Result<void>::failure(invalid_profile("display_name must not be empty"));
    }
    if (profile.generic) {
        if (profile.manufacturer || profile.model || profile.variant ||
            profile.recognition.native_identity || profile.recognition.universal_identity ||
            !profile.recognition.sysex_fingerprints.empty()) {
            return midi::Result<void>::failure(
                invalid_profile("Generic profile must not claim device identity"));
        }
        if (profile.support.detect || profile.support.read || profile.support.inspect ||
            profile.support.extract || profile.support.modify || profile.support.serialize ||
            profile.support.transfer || profile.support.validated_restore) {
            return midi::Result<void>::failure(
                invalid_profile("Generic profile must not claim device support levels"));
        }
    } else if (!profile.manufacturer || empty(*profile.manufacturer) || !profile.model ||
               empty(*profile.model)) {
        return midi::Result<void>::failure(
            invalid_profile("non-Generic profile requires manufacturer and model"));
    }

    const std::array claims{profile.support.detect, profile.support.read, profile.support.inspect,
                            profile.support.extract, profile.support.modify, profile.support.serialize,
                            profile.support.transfer, profile.support.validated_restore};
    for (std::size_t index = 1; index < claims.size(); ++index) {
        if (claims[index] && !claims[index - 1]) {
            return midi::Result<void>::failure(
                invalid_profile("support levels contain a contradictory gap"));
        }
    }
    if (has_duplicate(profile.aliases, [](const auto& value) { return value; })) {
        return midi::Result<void>::failure(invalid_profile("aliases contain a duplicate"));
    }
    if (has_duplicate(profile.metadata.channels, [](const auto value) { return value; })) {
        return midi::Result<void>::failure(invalid_profile("channels contain a duplicate"));
    }
    const auto validate_named = [](const std::vector<NamedNumber>& values, std::string_view name) {
        if (has_duplicate(values, [](const NamedNumber& value) { return value.number; })) {
            return midi::Result<void>::failure(invalid_profile(std::string(name) +
                                                               " contains a duplicate number"));
        }
        if (std::any_of(values.begin(), values.end(), [](const NamedNumber& value) {
                return value.name.empty();
            })) {
            return midi::Result<void>::failure(
                invalid_profile(std::string(name) + " contains an empty name"));
        }
        return midi::Result<void>::success();
    };
    for (const auto result : {validate_named(profile.metadata.control_changes, "cc"),
                              validate_named(profile.metadata.rpn, "rpn"),
                              validate_named(profile.metadata.nrpn, "nrpn")}) {
        if (!result) return result;
    }
    if (profile.metadata.bank_organization &&
        empty(profile.metadata.bank_organization->description)) {
        return midi::Result<void>::failure(
            invalid_profile("bank_organization description must not be empty"));
    }
    if (std::any_of(profile.warnings.begin(), profile.warnings.end(), empty)) {
        return midi::Result<void>::failure(invalid_profile("warnings contain an empty value"));
    }
    if (profile.recognition.universal_identity) {
        const auto size = profile.recognition.universal_identity->manufacturer_id.size();
        if (size != 1 && size != 3) {
            return midi::Result<void>::failure(
                invalid_profile("Universal Identity manufacturer_id must have 1 or 3 bytes"));
        }
        if (std::any_of(profile.recognition.universal_identity->manufacturer_id.begin(),
                        profile.recognition.universal_identity->manufacturer_id.end(),
                        [](const auto value) { return value > 0x7F; })) {
            return midi::Result<void>::failure(
                invalid_profile("Universal Identity manufacturer bytes must be 7-bit"));
        }
    }
    if (has_duplicate(profile.recognition.sysex_fingerprints,
                      [](const SysExFingerprint& value) { return value.id; })) {
        return midi::Result<void>::failure(invalid_profile("fingerprint IDs contain a duplicate"));
    }
    for (const auto& fingerprint : profile.recognition.sysex_fingerprints) {
        if (fingerprint.id.empty() || fingerprint.bytes.size() < 4) {
            return midi::Result<void>::failure(
                invalid_profile("fingerprint requires an ID and at least four bytes"));
        }
    }
    if (profile.provenance.source.empty() || profile.provenance.owner.empty() ||
        profile.provenance.license.empty() || profile.provenance.redistribution.empty() ||
        profile.provenance.acquisition.empty()) {
        return midi::Result<void>::failure(
            invalid_profile("all provenance fields are required and non-empty"));
    }
    return midi::Result<void>::success();
}

midi::Result<DeviceProfile> parse_profile_json(std::string_view json, std::string source_name) {
    try {
        auto profile = decode_profile(JsonParser(json).parse());
        if (const auto validation = validate_profile(profile); !validation) {
            auto error = validation.error();
            error.message = source_name + ": " + error.message;
            return midi::Result<DeviceProfile>::failure(std::move(error));
        }
        return midi::Result<DeviceProfile>::success(std::move(profile));
    } catch (const std::exception& error) {
        return midi::Result<DeviceProfile>::failure(
            invalid_profile(source_name + ": " + error.what()));
    }
}

midi::Result<DeviceProfile> load_profile_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return midi::Result<DeviceProfile>::failure(
            {midi::MidiErrorCode::io_error, "cannot open profile file: " + path.string(),
             "profile-file-open", std::nullopt});
    }
    std::ostringstream content;
    content << input.rdbuf();
    if (!input.good() && !input.eof()) {
        return midi::Result<DeviceProfile>::failure(
            {midi::MidiErrorCode::io_error, "cannot read profile file: " + path.string(),
             "profile-file-read", std::nullopt});
    }
    return parse_profile_json(content.str(), path.string());
}

} // namespace taureon::profiles
