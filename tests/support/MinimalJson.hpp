#pragma once

// Minimal, dev/test-only JSON value, parser, and JSON-Schema-subset validator.
//
// This is deliberately separate from src/profiles/ProfileLoader.cpp's own internal JSON
// parser: that parser is Stage 4 production code shaped around one fixed DeviceProfile
// struct and already passed its architecture review. The reference archive under
// reference/ (see reference/README.md) is dev tooling, not runtime code, has different
// needs (arbitrary/open-ended documents, not one fixed struct), and must not risk
// regressing already-reviewed runtime parsing. Nothing in src/ depends on this header.
//
// The validator understands a deliberately small subset of JSON Schema (2020-12):
// "type" (single or array of strings, including "null"), "const", "enum", "required",
// "properties", "additionalProperties" (boolean only), "items", "minItems", "maxItems",
// "minimum", "maximum", and "pattern" (ECMAScript regex via std::regex). It does not
// support $ref/$defs, oneOf/anyOf/allOf, or format. That is enough for
// reference/schema/device-reference.schema.json and is not meant to grow into a general
// JSON Schema engine; swap in a real library first if broader coverage is ever needed.

#include <cctype>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace taureon::test::json {

class JsonValue;

using JsonArray = std::vector<JsonValue>;
using JsonObject = std::vector<std::pair<std::string, JsonValue>>;

class JsonValue {
public:
    using Storage = std::variant<std::nullptr_t, bool, double, std::string, JsonArray, JsonObject>;

    JsonValue() : storage_(nullptr) {}
    JsonValue(Storage storage) : storage_(std::move(storage)) {}

    [[nodiscard]] bool is_null() const { return std::holds_alternative<std::nullptr_t>(storage_); }
    [[nodiscard]] bool is_bool() const { return std::holds_alternative<bool>(storage_); }
    [[nodiscard]] bool is_number() const { return std::holds_alternative<double>(storage_); }
    [[nodiscard]] bool is_string() const { return std::holds_alternative<std::string>(storage_); }
    [[nodiscard]] bool is_array() const { return std::holds_alternative<JsonArray>(storage_); }
    [[nodiscard]] bool is_object() const { return std::holds_alternative<JsonObject>(storage_); }

    [[nodiscard]] const std::string& as_string() const { return std::get<std::string>(storage_); }
    [[nodiscard]] double as_number() const { return std::get<double>(storage_); }
    [[nodiscard]] bool as_bool() const { return std::get<bool>(storage_); }
    [[nodiscard]] const JsonArray& as_array() const { return std::get<JsonArray>(storage_); }
    [[nodiscard]] const JsonObject& as_object() const { return std::get<JsonObject>(storage_); }

    [[nodiscard]] const JsonValue* find(std::string_view key) const {
        if (!is_object()) return nullptr;
        for (const auto& [k, v] : std::get<JsonObject>(storage_)) {
            if (k == key) return &v;
        }
        return nullptr;
    }

    [[nodiscard]] const char* type_name() const {
        if (is_null()) return "null";
        if (is_bool()) return "boolean";
        if (is_number()) return "number";
        if (is_string()) return "string";
        if (is_array()) return "array";
        return "object";
    }

private:
    Storage storage_;
};

class JsonParseError final : public std::runtime_error {
public:
    JsonParseError(std::size_t offset, const std::string& message)
        : std::runtime_error("byte " + std::to_string(offset) + ": " + message) {}
};

namespace detail {

class Parser {
public:
    explicit Parser(std::string_view input) : input_(input) {}

    JsonValue parse() {
        skip_space();
        JsonValue result = parse_value();
        skip_space();
        if (pos_ != input_.size()) fail("unexpected trailing data after JSON value");
        return result;
    }

private:
    std::string_view input_;
    std::size_t pos_ = 0;

    [[noreturn]] void fail(const std::string& message) const { throw JsonParseError(pos_, message); }

    void skip_space() {
        while (pos_ < input_.size()) {
            const char c = input_[pos_];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                ++pos_;
            } else {
                break;
            }
        }
    }

    bool consume(char c) {
        if (pos_ < input_.size() && input_[pos_] == c) {
            ++pos_;
            return true;
        }
        return false;
    }

    void expect_literal(std::string_view literal) {
        if (input_.substr(pos_, literal.size()) != literal) fail("expected literal");
        pos_ += literal.size();
    }

    JsonValue parse_value() {
        if (pos_ >= input_.size()) fail("unexpected end of input, expected a value");
        switch (input_[pos_]) {
        case '{': return parse_object();
        case '[': return parse_array();
        case '"': return JsonValue(parse_string());
        case 't': expect_literal("true"); return JsonValue(true);
        case 'f': expect_literal("false"); return JsonValue(false);
        case 'n': expect_literal("null"); return JsonValue(nullptr);
        default:
            if (input_[pos_] == '-' || std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
                return parse_number();
            }
            fail("unexpected character");
        }
    }

    JsonValue parse_object() {
        consume('{');
        skip_space();
        JsonObject object;
        if (consume('}')) return JsonValue(std::move(object));
        while (true) {
            skip_space();
            if (pos_ >= input_.size() || input_[pos_] != '"') fail("expected a string object key");
            std::string key = parse_string();
            skip_space();
            if (!consume(':')) fail("expected ':' after object key");
            skip_space();
            for (const auto& entry : object) {
                if (entry.first == key) fail("duplicate object key '" + key + "'");
            }
            object.emplace_back(std::move(key), parse_value());
            skip_space();
            if (consume('}')) break;
            if (!consume(',')) fail("expected ',' or '}' in object");
        }
        return JsonValue(std::move(object));
    }

    JsonValue parse_array() {
        consume('[');
        skip_space();
        JsonArray array;
        if (consume(']')) return JsonValue(std::move(array));
        while (true) {
            skip_space();
            array.push_back(parse_value());
            skip_space();
            if (consume(']')) break;
            if (!consume(',')) fail("expected ',' or ']' in array");
        }
        return JsonValue(std::move(array));
    }

    static void append_utf8(std::string& out, unsigned int codepoint) {
        if (codepoint <= 0x7F) {
            out.push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
    }

    std::string parse_string() {
        consume('"');
        std::string out;
        while (true) {
            if (pos_ >= input_.size()) fail("unterminated string");
            const char c = input_[pos_++];
            if (c == '"') break;
            if (static_cast<unsigned char>(c) < 0x20) fail("control character in string");
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (pos_ >= input_.size()) fail("unterminated escape sequence");
            const char esc = input_[pos_++];
            switch (esc) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                if (pos_ + 4 > input_.size()) fail("truncated \\u escape");
                unsigned int codepoint = 0;
                for (int i = 0; i < 4; ++i) {
                    const char hex = input_[pos_++];
                    codepoint <<= 4;
                    if (hex >= '0' && hex <= '9') codepoint |= static_cast<unsigned int>(hex - '0');
                    else if (hex >= 'a' && hex <= 'f') codepoint |= static_cast<unsigned int>(hex - 'a' + 10);
                    else if (hex >= 'A' && hex <= 'F') codepoint |= static_cast<unsigned int>(hex - 'A' + 10);
                    else fail("invalid \\u escape digit");
                }
                append_utf8(out, codepoint);
                break;
            }
            default:
                fail("unsupported escape sequence");
            }
        }
        return out;
    }

    JsonValue parse_number() {
        const std::size_t start = pos_;
        if (consume('-')) {}
        if (pos_ >= input_.size() || !std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
            fail("invalid number");
        }
        while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_]))) ++pos_;
        if (consume('.')) {
            if (pos_ >= input_.size() || !std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
                fail("invalid number, digits required after '.'");
            }
            while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_]))) ++pos_;
        }
        if (pos_ < input_.size() && (input_[pos_] == 'e' || input_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < input_.size() && (input_[pos_] == '+' || input_[pos_] == '-')) ++pos_;
            if (pos_ >= input_.size() || !std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
                fail("invalid number exponent");
            }
            while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_]))) ++pos_;
        }
        const std::string token(input_.substr(start, pos_ - start));
        return JsonValue(std::stod(token));
    }
};

} // namespace detail

inline JsonValue parse(std::string_view input) {
    detail::Parser parser(input);
    return parser.parse();
}

// Validates `instance` against the JSON-Schema-subset document `schema`, appending one
// human-readable message per violation to `errors` (JSON-pointer-ish `path` prefix).
// Returns true iff no violations were appended.
inline bool validate(const JsonValue& schema, const JsonValue& instance, const std::string& path,
                      std::vector<std::string>& errors) {
    const std::size_t errors_before = errors.size();

    if (const JsonValue* const_value = schema.find("const")) {
        // Only numeric const is used by this schema; compare structurally for that case.
        if (const_value->is_number() && instance.is_number()) {
            if (instance.as_number() != const_value->as_number()) {
                errors.push_back(path + ": expected const " + std::to_string(const_value->as_number()));
            }
        }
    }

    if (const JsonValue* enum_value = schema.find("enum")) {
        bool matched = false;
        for (const auto& option : enum_value->as_array()) {
            if (option.is_string() && instance.is_string() && option.as_string() == instance.as_string()) {
                matched = true;
                break;
            }
        }
        if (!matched) errors.push_back(path + ": value not in enum");
    }

    if (const JsonValue* pattern = schema.find("pattern")) {
        if (instance.is_string()) {
            const std::regex re(pattern->as_string());
            if (!std::regex_match(instance.as_string(), re)) {
                errors.push_back(path + ": does not match pattern " + pattern->as_string());
            }
        }
    }

    if (const JsonValue* type_schema = schema.find("type")) {
        std::vector<std::string> allowed;
        if (type_schema->is_string()) {
            allowed.push_back(type_schema->as_string());
        } else if (type_schema->is_array()) {
            for (const auto& t : type_schema->as_array()) {
                if (t.is_string()) allowed.push_back(t.as_string());
            }
        }
        bool type_ok = false;
        for (const auto& t : allowed) {
            if (t == "integer" && instance.is_number()) {
                const double v = instance.as_number();
                type_ok = type_ok || (v == static_cast<double>(static_cast<long long>(v)));
            } else if (t == std::string(instance.type_name())) {
                type_ok = true;
            }
        }
        if (!allowed.empty() && !type_ok) {
            errors.push_back(path + ": expected type in schema, got " + instance.type_name());
        }
        // If null is explicitly allowed and the instance is null, no further object/array
        // keywords (required/properties/items) apply.
        if (instance.is_null()) {
            bool null_allowed = false;
            for (const auto& t : allowed) null_allowed = null_allowed || (t == "null");
            if (null_allowed) return errors.size() == errors_before;
        }
    }

    if (instance.is_number()) {
        if (const JsonValue* minimum = schema.find("minimum")) {
            if (instance.as_number() < minimum->as_number()) errors.push_back(path + ": below minimum");
        }
        if (const JsonValue* maximum = schema.find("maximum")) {
            if (instance.as_number() > maximum->as_number()) errors.push_back(path + ": above maximum");
        }
    }

    if (instance.is_object()) {
        if (const JsonValue* required = schema.find("required")) {
            for (const auto& key_value : required->as_array()) {
                if (!key_value.is_string()) continue;
                if (instance.find(key_value.as_string()) == nullptr) {
                    errors.push_back(path + ": missing required property '" + key_value.as_string() + "'");
                }
            }
        }
        const JsonValue* properties = schema.find("properties");
        const JsonValue* additional = schema.find("additionalProperties");
        const bool additional_allowed = !(additional != nullptr && additional->is_bool() && !additional->as_bool());
        for (const auto& [key, value] : instance.as_object()) {
            const JsonValue* property_schema = properties != nullptr ? properties->find(key) : nullptr;
            if (property_schema != nullptr) {
                validate(*property_schema, value, path + "/" + key, errors);
            } else if (!additional_allowed) {
                errors.push_back(path + ": unexpected property '" + key + "'");
            }
        }
    }

    if (instance.is_array()) {
        if (const JsonValue* min_items = schema.find("minItems")) {
            if (static_cast<double>(instance.as_array().size()) < min_items->as_number()) {
                errors.push_back(path + ": has fewer than minItems entries");
            }
        }
        if (const JsonValue* max_items = schema.find("maxItems")) {
            if (static_cast<double>(instance.as_array().size()) > max_items->as_number()) {
                errors.push_back(path + ": has more than maxItems entries");
            }
        }
        if (const JsonValue* items = schema.find("items")) {
            std::size_t index = 0;
            for (const auto& element : instance.as_array()) {
                validate(*items, element, path + "/" + std::to_string(index), errors);
                ++index;
            }
        }
    }

    return errors.size() == errors_before;
}

} // namespace taureon::test::json
