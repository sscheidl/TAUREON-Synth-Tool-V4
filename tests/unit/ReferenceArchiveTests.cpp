// Stage: reference archive v1 (see reference/README.md).
//
// Covers exactly what reference/README.md promises:
//  1. the schema file is valid JSON and is internally self-consistent as a schema;
//  2. the example device.json loads and validates against that schema;
//  3. the fixture it references is correctly cross-referenced (exists, right size, its
//     recorded SHA-256 matches the existing authoritative provenance record, and its
//     bytes actually classify the way the entry claims);
//  4. a device.json missing a required property is rejected;
//  5. reference/ (and tests/) do not leak into the build output the app ships from.

#include "TestSupport.hpp"
#include "support/MinimalJson.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace taureon::test::json;

namespace {

const std::filesystem::path source_root{TAUREON_SOURCE_DIR};

std::string read_text(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    TAUREON_REQUIRE(stream.good());
    std::ostringstream content;
    content << stream.rdbuf();
    TAUREON_REQUIRE(stream.good() || stream.eof());
    return content.str();
}

std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    TAUREON_REQUIRE(stream.good());
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(stream)),
                                     std::istreambuf_iterator<char>());
    return bytes;
}

std::string to_lower(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

void test_schema_is_valid_and_self_consistent() {
    const auto schema_text =
        read_text(source_root / "reference" / "schema" / "device-reference.schema.json");
    const JsonValue schema = parse(schema_text); // throws on malformed JSON

    TAUREON_REQUIRE(schema.is_object());
    const JsonValue* type = schema.find("type");
    TAUREON_REQUIRE(type != nullptr && type->is_string() && type->as_string() == "object");

    const JsonValue* required = schema.find("required");
    TAUREON_REQUIRE(required != nullptr && required->is_array() && !required->as_array().empty());

    const JsonValue* properties = schema.find("properties");
    TAUREON_REQUIRE(properties != nullptr && properties->is_object());

    // Every required top-level key must actually be declared as a property.
    for (const auto& key : required->as_array()) {
        TAUREON_REQUIRE(key.is_string());
        TAUREON_REQUIRE(properties->find(key.as_string()) != nullptr);
    }
}

JsonValue load_schema() {
    return parse(read_text(source_root / "reference" / "schema" / "device-reference.schema.json"));
}

void test_example_device_json_validates() {
    const JsonValue schema = load_schema();
    const auto device_text =
        read_text(source_root / "reference" / "devices" / "novation" / "summit" / "device.json");
    const JsonValue device = parse(device_text);

    std::vector<std::string> errors;
    const bool valid = validate(schema, device, "", errors);
    for (const auto& error : errors) std::cerr << "schema violation: " << error << '\n';
    TAUREON_REQUIRE(valid);
    TAUREON_REQUIRE(errors.empty());
}

void test_fixture_is_correctly_referenced_and_classified() {
    const JsonValue device = parse(read_text(
        source_root / "reference" / "devices" / "novation" / "summit" / "device.json"));

    const JsonValue* fixtures = device.find("fixtures");
    TAUREON_REQUIRE(fixtures != nullptr && fixtures->is_array() && fixtures->as_array().size() == 1);
    const JsonValue& fixture = fixtures->as_array().front();

    const std::string relative_path = fixture.find("path")->as_string();
    const auto fixture_path = source_root / relative_path;
    TAUREON_REQUIRE(std::filesystem::exists(fixture_path));

    const auto bytes = read_bytes(fixture_path);
    const auto recorded_size = static_cast<std::size_t>(fixture.find("size_bytes")->as_number());
    TAUREON_REQUIRE(bytes.size() == recorded_size);

    // Cross-check against the existing, already-approved provenance record instead of
    // recomputing SHA-256 ourselves: this proves the reference entry and the trusted
    // provenance record agree, without adding a cryptographic hash implementation to the
    // project just for this test (see reference/README.md).
    const JsonValue* provenance_path_value = fixture.find("provenance_path");
    TAUREON_REQUIRE(provenance_path_value != nullptr);
    const JsonValue provenance =
        parse(read_text(source_root / provenance_path_value->as_string()));
    const std::string recorded_sha256 = to_lower(fixture.find("sha256")->as_string());
    const std::string provenance_sha256 = to_lower(provenance.find("sha256")->as_string());
    TAUREON_REQUIRE(recorded_sha256 == provenance_sha256);
    TAUREON_REQUIRE(recorded_sha256.size() == 64);

    // Structural classification claimed by dump_types[0]: one complete F0..F7 frame.
    TAUREON_REQUIRE(!bytes.empty());
    TAUREON_REQUIRE(bytes.front() == 0xF0);
    TAUREON_REQUIRE(bytes.back() == 0xF7);

    // The manufacturer_id bytes claimed under midi_identity actually appear at the
    // fixture's offset 1..N, matching the existing Stage 4 recognition fingerprint.
    const JsonValue* manufacturer_id = device.find("midi_identity")->find("manufacturer_id");
    const auto& id_bytes = manufacturer_id->find("bytes")->as_array();
    TAUREON_REQUIRE(bytes.size() >= 1 + id_bytes.size());
    for (std::size_t i = 0; i < id_bytes.size(); ++i) {
        TAUREON_REQUIRE(bytes[1 + i] == static_cast<std::uint8_t>(id_bytes[i].as_number()));
    }
}

void test_invalid_instance_missing_required_field_is_rejected() {
    const JsonValue schema = load_schema();

    // A deliberately incomplete instance: "manufacturer" (a required top-level property)
    // is missing entirely.
    const std::string broken_json = R"JSON({
        "schema_version": 1,
        "reference_id": "broken.device",
        "model": { "name": "X", "status": "unknown" },
        "midi_identity": {
            "manufacturer_id": { "bytes": [0], "status": "unknown" },
            "device_id_behavior": { "status": "unknown" }
        },
        "dump_types": [],
        "bank_organization": { "status": "unknown" },
        "checksum": { "status": "unknown" },
        "fixtures": [],
        "external_sources": [],
        "status_summary": "intentionally invalid fixture for a negative test"
    })JSON";
    const JsonValue broken = parse(broken_json);

    std::vector<std::string> errors;
    const bool valid = validate(schema, broken, "", errors);
    TAUREON_REQUIRE(!valid);
    bool mentions_manufacturer = false;
    for (const auto& error : errors) {
        if (error.find("manufacturer") != std::string::npos) mentions_manufacturer = true;
    }
    TAUREON_REQUIRE(mentions_manufacturer);
}

void test_reference_and_fixture_data_do_not_leak_into_build_output() {
    // (a) No deployment/packaging command in the shipped build configuration may name
    // reference/ or tests/. This deliberately matches only the commands that can put files
    // into a deliverable (install/CPack/file(COPY)/copy_directory), so prose mentioning the
    // word "reference" in a comment does not trip it, while a real leak does. Matching is
    // whole-file rather than per-line so a multi-line install() cannot slip through.
    static const std::regex deployment_leak(
        R"((install\s*\([^)]*(reference|tests))"
        R"(|copy_directory[^)]*(reference|tests))"
        R"(|file\s*\(\s*COPY[^)]*(reference|tests))"
        R"(|CPACK[^\n]*(reference|tests)))",
        std::regex::icase);
    for (const auto& cmake_file : {source_root / "CMakeLists.txt", source_root / "src" / "CMakeLists.txt"}) {
        const auto text = read_text(cmake_file);
        TAUREON_REQUIRE(!std::regex_search(text, deployment_leak));
    }

    // (b) The actual deployable output directory next to taureon_app (where
    // resources/device_profiles/ is deployed by src/CMakeLists.txt) does not contain a
    // "reference" entry, and does not contain either fixture file directly.
    const std::filesystem::path app_output_dir{TAUREON_APP_OUTPUT_DIR};
    if (std::filesystem::exists(app_output_dir)) {
        TAUREON_REQUIRE(!std::filesystem::exists(app_output_dir / "reference"));
        TAUREON_REQUIRE(!std::filesystem::exists(app_output_dir / "novation_summit_crazy_sine.syx"));
    }
}

} // namespace

int main() {
    return taureon::test::run([] {
        test_schema_is_valid_and_self_consistent();
        test_example_device_json_validates();
        test_fixture_is_correctly_referenced_and_classified();
        test_invalid_instance_missing_required_field_is_rejected();
        test_reference_and_fixture_data_do_not_leak_into_build_output();
    });
}
