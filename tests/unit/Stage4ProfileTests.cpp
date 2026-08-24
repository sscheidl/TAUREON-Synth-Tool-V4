#include "TestSupport.hpp"

#include "core/sysex/SysEx7.hpp"
#include "core/sysex/SyxFile.hpp"
#include "profiles/ProfileLoader.hpp"
#include "profiles/ProfileRegistry.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace taureon;
using namespace taureon::profiles;

namespace {

const std::filesystem::path source_root{TAUREON_SOURCE_DIR};
const auto profile_directory = source_root / "resources" / "device_profiles";
const auto fixture_path =
    source_root / "tests" / "fixtures" / "novation_summit_crazy_sine.syx";

std::string read_text(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    TAUREON_REQUIRE(stream.good());
    std::ostringstream content;
    content << stream.rdbuf();
    TAUREON_REQUIRE(stream.good() || stream.eof());
    return content.str();
}

std::string replace_once(std::string text, const std::string& from, const std::string& to) {
    const auto position = text.find(from);
    TAUREON_REQUIRE(position != std::string::npos);
    text.replace(position, from.size(), to);
    return text;
}

DeviceProfile load_profile(std::string_view name) {
    auto loaded = load_profile_file(profile_directory / name);
    TAUREON_REQUIRE(loaded);
    return std::move(loaded.value());
}

ProfileRegistry load_registry() {
    ProfileRegistry registry;
    std::vector<ProfileLoadIssue> issues;
    TAUREON_REQUIRE(registry.load_directory(profile_directory, issues));
    TAUREON_REQUIRE(issues.empty());
    return registry;
}

sysex::SysExFrame unknown_frame() {
    return {sysex::SysExFrameStatus::complete, {0xF0, 0x7D, 0x01, 0xF7}, {}, std::nullopt,
            false};
}

void schema_and_registry_validation() {
    auto registry = load_registry();
    const auto profiles = registry.profiles();
    TAUREON_REQUIRE(profiles.size() == 2);
    TAUREON_REQUIRE(profiles[0].profile_id == "generic");
    TAUREON_REQUIRE(profiles[1].profile_id == "novation.summit");
    TAUREON_REQUIRE(profiles[0].generic);
    TAUREON_REQUIRE(!profiles[1].generic);

    const auto summit_path = profile_directory / "novation-summit.profile.json";
    const auto valid_json = read_text(summit_path);
    TAUREON_REQUIRE(!parse_profile_json(
        replace_once(valid_json, "\"schema_version\": 1", "\"schema_version\": 2"),
        "unsupported-version"));
    TAUREON_REQUIRE(!parse_profile_json("{}", "missing-fields"));
    TAUREON_REQUIRE(!parse_profile_json(
        replace_once(valid_json, "\"schema_version\": 1", "\"schema_version\": \"1\""),
        "wrong-type"));
    TAUREON_REQUIRE(!parse_profile_json(
        replace_once(valid_json, "\"midi_channel\": null", "\"midi_channel\": 17"),
        "range-error"));
    TAUREON_REQUIRE(!parse_profile_json(
        replace_once(valid_json, "\"inspect\": false", "\"inspect\": true"),
        "contradictory-support"));
    TAUREON_REQUIRE(!parse_profile_json(
        replace_once(valid_json, "\"bytes\": [240,", "\"bytes\": [300,"),
        "malformed-fingerprint"));
    TAUREON_REQUIRE(!parse_profile_json(
        replace_once(valid_json, "\"summit-single-patch-header\"",
                     "\"summit-single-patch-header\", \"unexpected\": true"),
        "unknown-safety-field"));

    auto summit = load_profile("novation-summit.profile.json");
    ProfileRegistry duplicates;
    TAUREON_REQUIRE(duplicates.register_profile(summit));
    const auto same_version = duplicates.register_profile(summit);
    TAUREON_REQUIRE(!same_version);
    TAUREON_REQUIRE(same_version.error().message.find("duplicate profile_id") != std::string::npos);
    summit.profile_version = "2.0.0";
    const auto conflicting_version = duplicates.register_profile(summit);
    TAUREON_REQUIRE(!conflicting_version);
    TAUREON_REQUIRE(conflicting_version.error().message.find("2.0.0") != std::string::npos);

    // An invalid real profile is rejected without displacing the valid Generic fallback.
    ProfileRegistry fallback_registry;
    TAUREON_REQUIRE(fallback_registry.register_profile(load_profile("generic.profile.json")));
    auto invalid_real = load_profile("novation-summit.profile.json");
    invalid_real.support.validated_restore = true;
    TAUREON_REQUIRE(!fallback_registry.register_profile(std::move(invalid_real)));
    const auto fallback = fallback_registry.match(unknown_frame());
    TAUREON_REQUIRE(fallback.status == ProfileMatchStatus::GenericFallback);
    TAUREON_REQUIRE(fallback.selected_profile_id == std::optional<std::string>{"generic"});

    ProfileRegistry no_generic_registry;
    TAUREON_REQUIRE(
        no_generic_registry.register_profile(load_profile("novation-summit.profile.json")));
    TAUREON_REQUIRE(no_generic_registry.match(unknown_frame()).status ==
                    ProfileMatchStatus::NoMatch);
}

void deterministic_matching_and_isolation() {
    auto document = sysex::load_syx_file(fixture_path);
    TAUREON_REQUIRE(document);
    TAUREON_REQUIRE(document.value().raw_bytes.size() == 527);
    TAUREON_REQUIRE(document.value().frames.size() == 1);
    const auto& fixture = document.value().frames.front();
    TAUREON_REQUIRE(fixture.status == sysex::SysExFrameStatus::complete);
    TAUREON_REQUIRE(!fixture.affected_by_data_loss);
    TAUREON_REQUIRE(fixture.bytes.front() == 0xF0);
    TAUREON_REQUIRE(fixture.bytes.back() == 0xF7);
    const std::vector<std::uint8_t> expected_header{0xF0, 0x00, 0x20, 0x29,
                                                     0x01, 0x11, 0x01, 0x33};
    TAUREON_REQUIRE(std::equal(expected_header.begin(), expected_header.end(),
                               fixture.bytes.begin()));

    auto registry = load_registry();
    const auto bytes_before = fixture.bytes;
    const auto match = registry.match(fixture);
    TAUREON_REQUIRE(match.status == ProfileMatchStatus::ConfidentSuggestion);
    TAUREON_REQUIRE(match.selected_profile_id ==
                    std::optional<std::string>{"novation.summit"});
    TAUREON_REQUIRE(match.evidence.size() == 1);
    TAUREON_REQUIRE(match.evidence.front().kind == ProfileEvidenceKind::sysex_fingerprint);
    TAUREON_REQUIRE(fixture.bytes == bytes_before);

    // Registration order cannot alter the result.
    ProfileRegistry reverse;
    TAUREON_REQUIRE(reverse.register_profile(load_profile("novation-summit.profile.json")));
    TAUREON_REQUIRE(reverse.register_profile(load_profile("generic.profile.json")));
    const auto reverse_match = reverse.match(fixture);
    TAUREON_REQUIRE(reverse_match.status == match.status);
    TAUREON_REQUIRE(reverse_match.selected_profile_id == match.selected_profile_id);

    auto unknown = unknown_frame();
    const auto unknown_before = unknown.bytes;
    const auto fallback = registry.match(unknown);
    TAUREON_REQUIRE(fallback.status == ProfileMatchStatus::GenericFallback);
    TAUREON_REQUIRE(fallback.selected_profile_id == std::optional<std::string>{"generic"});
    TAUREON_REQUIRE(unknown.bytes == unknown_before);

    ProfileMatchInput explicit_input;
    explicit_input.saved_profile_id = "novation.summit";
    const auto explicit_result = registry.match(unknown, explicit_input);
    TAUREON_REQUIRE(explicit_result.status == ProfileMatchStatus::Explicit);
    TAUREON_REQUIRE(explicit_result.selected_profile_id ==
                    std::optional<std::string>{"novation.summit"});
    explicit_input.saved_profile_id = "missing.profile";
    TAUREON_REQUIRE(registry.match(unknown, explicit_input).status == ProfileMatchStatus::Invalid);

    ProfileMatchInput manual_input;
    manual_input.manual_profile_id = "novation.summit";
    TAUREON_REQUIRE(registry.match(unknown, manual_input).status == ProfileMatchStatus::Explicit);

    // A display/port name and backend-specific route identities never identify the synthesizer.
    ProfileMatchInput route_input;
    route_input.port_display_name = "Novation Summit MIDI";
    route_input.route_identity = midi::MidiRouteIdentity{
        midi::MidiBackend::winmm, midi::MidiDirection::input,
        midi::WinmmRouteIdentity{"Novation Summit MIDI", 1, 2, 3}};
    const auto winmm_fallback = registry.match(unknown, route_input);
    TAUREON_REQUIRE(winmm_fallback.status == ProfileMatchStatus::GenericFallback);
    route_input.route_identity = midi::MidiRouteIdentity{
        midi::MidiBackend::windows_midi_services, midi::MidiDirection::input,
        midi::WmsRouteIdentity{"different-route", 7}};
    const auto wms_fallback = registry.match(unknown, route_input);
    TAUREON_REQUIRE(wms_fallback.status == ProfileMatchStatus::GenericFallback);
    TAUREON_REQUIRE(wms_fallback.selected_profile_id == winmm_fallback.selected_profile_id);

    // UMP remains outside profile matching and therefore byte/word authoritative.
    midi::NativeMidiMessage ump{midi::MidiBackend::windows_midi_services,
                                midi::UmpNativeMessage{{0x30167D01, 0x02030000}}, std::nullopt};
    const auto ump_before = ump;
    (void)registry.match(fixture, route_input);
    TAUREON_REQUIRE(ump == ump_before);

    // Synthetic equally plausible profile proves visible ambiguity without adding a second real profile.
    auto clone = load_profile("novation-summit.profile.json");
    clone.profile_id = "test.summit-clone";
    clone.display_name = "Synthetic ambiguity candidate";
    TAUREON_REQUIRE(registry.register_profile(std::move(clone)));
    const auto ambiguous = registry.match(fixture);
    TAUREON_REQUIRE(ambiguous.status == ProfileMatchStatus::Ambiguous);
    TAUREON_REQUIRE(!ambiguous.selected_profile_id);
    TAUREON_REQUIRE(ambiguous.evidence.size() == 2);
    TAUREON_REQUIRE(ambiguous.evidence[0].profile_id == "novation.summit");
    TAUREON_REQUIRE(ambiguous.evidence[1].profile_id == "test.summit-clone");
}

void capability_and_taint_safety() {
    auto registry = load_registry();
    const auto* summit = registry.find("novation.summit");
    TAUREON_REQUIRE(summit != nullptr);
    TAUREON_REQUIRE(summit->support.supports(SupportLevel::detect));
    TAUREON_REQUIRE(!summit->support.supports(SupportLevel::read));
    TAUREON_REQUIRE(!summit->support.supports(SupportLevel::inspect));
    TAUREON_REQUIRE(!summit->support.supports(SupportLevel::extract));
    TAUREON_REQUIRE(!summit->support.supports(SupportLevel::modify));
    TAUREON_REQUIRE(!summit->support.supports(SupportLevel::serialize));
    TAUREON_REQUIRE(!summit->support.supports(SupportLevel::transfer));
    TAUREON_REQUIRE(!summit->support.supports(SupportLevel::validated_restore));
    TAUREON_REQUIRE(!summit->defaults.sysex_inter_message_delay_ms);
    TAUREON_REQUIRE(!summit->warnings.empty());
    TAUREON_REQUIRE(!summit->protocol_module_id);

    auto document = sysex::load_syx_file(fixture_path);
    TAUREON_REQUIRE(document);
    auto tainted = document.value().frames.front();
    const auto original_bytes = tainted.bytes;
    tainted.status = sysex::SysExFrameStatus::malformed;
    tainted.affected_by_data_loss = true;
    tainted.issue = "synthetic Stage 4 ordered DataLoss regression";
    const auto rejected = registry.match(tainted);
    TAUREON_REQUIRE(rejected.status == ProfileMatchStatus::Invalid);
    TAUREON_REQUIRE(!rejected.selected_profile_id);
    TAUREON_REQUIRE(tainted.bytes == original_bytes);
    TAUREON_REQUIRE(!sysex::encode_sysex7(tainted, 0));

    const auto output = std::filesystem::current_path() / "stage4-tainted-fixture.syx";
    std::error_code error;
    std::filesystem::remove(output, error);
    std::filesystem::remove(output.string() + ".taureon.tmp", error);
    TAUREON_REQUIRE(!sysex::save_syx_frames(output, {tainted}));
    TAUREON_REQUIRE(!std::filesystem::exists(output));
}

} // namespace

int main() {
    return taureon::test::run([] {
        schema_and_registry_validation();
        deterministic_matching_and_isolation();
        capability_and_taint_safety();
    });
}
