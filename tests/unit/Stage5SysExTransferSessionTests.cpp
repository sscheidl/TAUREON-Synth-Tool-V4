#include "TestSupport.hpp"

#include "app/SysExTransferSession.hpp"

#include <filesystem>
#include <memory>

using namespace taureon;

namespace {

midi::MidiStreamEvent message_event(std::vector<std::uint8_t> bytes) {
    return {0, midi::NativeMidiMessage{midi::MidiBackend::winmm,
                                       midi::Midi1NativeMessage{std::move(bytes)},
                                       std::nullopt}};
}

} // namespace

int main() {
    return test::run([] {
        const auto fixture = std::filesystem::path{TAUREON_SOURCE_DIR} / "tests" / "fixtures" /
                             "novation_summit_crazy_sine.syx";
        auto registry = std::make_shared<profiles::ProfileRegistry>();
        std::vector<profiles::ProfileLoadIssue> issues;
        TAUREON_REQUIRE(registry->load_directory(
            std::filesystem::path{TAUREON_SOURCE_DIR} / "resources" / "device_profiles", issues));
        TAUREON_REQUIRE(issues.empty());
        app::SysExTransferSession session(registry);
        TAUREON_REQUIRE(session.load_file(fixture));
        auto imported = session.snapshot();
        TAUREON_REQUIRE(imported.source_kind == app::SysExSourceKind::imported_file);
        TAUREON_REQUIRE(imported.source_name == "novation_summit_crazy_sine.syx");
        TAUREON_REQUIRE(imported.frames.size() == 1);
        TAUREON_REQUIRE(imported.complete_frames == 1);
        TAUREON_REQUIRE(imported.can_raw_send);
        TAUREON_REQUIRE(!imported.can_save_verified_received);
        TAUREON_REQUIRE(imported.profile_id == std::optional<std::string>{"novation.summit"});
        TAUREON_REQUIRE(imported.profile_match_status ==
                        profiles::ProfileMatchStatus::ConfidentSuggestion);
        TAUREON_REQUIRE(imported.manufacturer == std::optional<std::string>{"Novation"});
        TAUREON_REQUIRE(imported.model == std::optional<std::string>{"Summit"});
        TAUREON_REQUIRE(!imported.profile_supports_transfer);
        TAUREON_REQUIRE(!imported.profile_supports_validated_restore);
        sysex::SyxDocument inspected_document{imported.frames.front().bytes, imported.frames};
        TAUREON_REQUIRE(session.load_document(std::move(inspected_document),
                                              "Manager copy of Crazy Sine.syx"));
        const auto handed_off = session.snapshot();
        TAUREON_REQUIRE(handed_off.source_name == "Manager copy of Crazy Sine.syx");
        TAUREON_REQUIRE(handed_off.frames == imported.frames);
        TAUREON_REQUIRE(handed_off.byte_count == imported.byte_count);

        auto winmm = session.build_raw_send(midi::MidiBackend::winmm, std::nullopt);
        TAUREON_REQUIRE(winmm);
        TAUREON_REQUIRE(winmm.value().size() == 1);
        TAUREON_REQUIRE(std::get<midi::Midi1NativeMessage>(winmm.value().front().data).bytes ==
                        imported.frames.front().bytes);

        auto wms = session.build_raw_send(midi::MidiBackend::windows_midi_services,
                                          static_cast<std::uint8_t>(6));
        TAUREON_REQUIRE(wms);
        const auto& words = std::get<midi::UmpNativeMessage>(wms.value().front().data).words;
        TAUREON_REQUIRE(!words.empty());
        TAUREON_REQUIRE(((words.front() >> 24u) & 0x0fu) == 6);

        TAUREON_REQUIRE(session.begin_receive());
        const auto fresh_capture = session.snapshot();
        TAUREON_REQUIRE(!fresh_capture.profile_id);
        TAUREON_REQUIRE(fresh_capture.send_progress.state == transfer::TransferState::idle);
        session.consume(message_event({0xF0, 0x01, 0x02}));
        session.consume({1, midi::MidiDataLossEvent{midi::MidiBackend::winmm,
                                                    midi::MidiDataLossReason::queue_overflow,
                                                    true, std::nullopt, "injected loss",
                                                    std::nullopt}});
        session.consume(message_event({0x03, 0xF7}));
        TAUREON_REQUIRE(session.finish_receive());
        const auto tainted = session.snapshot();
        TAUREON_REQUIRE(tainted.frames.size() == 1);
        TAUREON_REQUIRE(tainted.malformed_frames == 1);
        TAUREON_REQUIRE(tainted.tainted_frames == 1);
        TAUREON_REQUIRE(!tainted.can_raw_send);
        TAUREON_REQUIRE(!tainted.can_save_verified_received);
        TAUREON_REQUIRE(tainted.profile_match_status == profiles::ProfileMatchStatus::Invalid);
        TAUREON_REQUIRE(!tainted.profile_id);
        TAUREON_REQUIRE(!session.build_raw_send(midi::MidiBackend::winmm, std::nullopt));

        TAUREON_REQUIRE(session.begin_receive());
        session.consume(message_event({0xF0, 0x7D, 0x11, 0xF7}));
        TAUREON_REQUIRE(session.finish_receive());
        const auto received = session.snapshot();
        TAUREON_REQUIRE(received.can_raw_send);
        TAUREON_REQUIRE(received.can_save_verified_received);

        const auto destination = std::filesystem::path{TAUREON_TEST_OUTPUT_DIR} /
                                 "stage5-received-save.syx";
        std::error_code ignored;
        std::filesystem::remove(destination, ignored);
        TAUREON_REQUIRE(session.save_verified_received(destination));
        auto saved = sysex::load_syx_file(destination);
        TAUREON_REQUIRE(saved);
        TAUREON_REQUIRE(saved.value().raw_bytes == received.frames.front().bytes);
        std::filesystem::remove(destination, ignored);
    });
}
