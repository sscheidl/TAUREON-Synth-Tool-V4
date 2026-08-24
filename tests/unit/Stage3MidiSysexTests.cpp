#include "TestSupport.hpp"

#include "core/midi/MidiMessage.hpp"
#include "core/sysex/SysEx7.hpp"
#include "core/sysex/SysExStreamParser.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

using namespace taureon;

namespace {

sysex::SysExFrame complete(std::vector<std::uint8_t> bytes) {
    return {sysex::SysExFrameStatus::complete, std::move(bytes), {}, std::nullopt, false};
}

void midi_message_tests() {
    struct Example {
        std::vector<std::uint8_t> bytes;
        midi::Midi1MessageKind kind;
    };
    const std::vector<Example> examples{
        {{0x80, 60, 0}, midi::Midi1MessageKind::note_off},
        {{0x91, 61, 127}, midi::Midi1MessageKind::note_on},
        {{0xa2, 62, 40}, midi::Midi1MessageKind::polyphonic_aftertouch},
        {{0xb3, 7, 100}, midi::Midi1MessageKind::control_change},
        {{0xc4, 10}, midi::Midi1MessageKind::program_change},
        {{0xd5, 20}, midi::Midi1MessageKind::channel_pressure},
        {{0xe6, 0x01, 0x40}, midi::Midi1MessageKind::pitch_bend},
        {{0xf1, 0x01}, midi::Midi1MessageKind::system_common},
        {{0xf2, 0x01, 0x02}, midi::Midi1MessageKind::system_common},
        {{0xf8}, midi::Midi1MessageKind::timing_clock},
        {{0xfa}, midi::Midi1MessageKind::transport_start},
        {{0xfb}, midi::Midi1MessageKind::transport_continue},
        {{0xfc}, midi::Midi1MessageKind::transport_stop},
        {{0xfe}, midi::Midi1MessageKind::active_sensing},
        {{0xff}, midi::Midi1MessageKind::system_reset},
        {{0xf4}, midi::Midi1MessageKind::unknown},
        {{0xf0, 0x7d, 0x01, 0xf7}, midi::Midi1MessageKind::system_exclusive},
    };
    for (const auto& example : examples) {
        const auto parsed = midi::parse_midi1_message(example.bytes);
        TAUREON_REQUIRE(parsed);
        TAUREON_REQUIRE(parsed.value().kind == example.kind);
        TAUREON_REQUIRE(parsed.value().raw == example.bytes);
    }
    const auto bend = midi::parse_midi1_message(std::array<std::uint8_t, 3>{0xe6, 1, 64});
    TAUREON_REQUIRE(bend.value().channel == 6);
    TAUREON_REQUIRE(bend.value().value14 == 8193);

    TAUREON_REQUIRE(!midi::parse_midi1_message(std::vector<std::uint8_t>{}));
    TAUREON_REQUIRE(!midi::parse_midi1_message(std::array<std::uint8_t, 2>{0x90, 60}));
    TAUREON_REQUIRE(!midi::parse_midi1_message(std::array<std::uint8_t, 3>{0x90, 60, 0x80}));
    TAUREON_REQUIRE(!midi::parse_midi1_message(std::array<std::uint8_t, 1>{0x40}));
    TAUREON_REQUIRE(!midi::parse_midi1_message(std::array<std::uint8_t, 1>{0xf7}));
    TAUREON_REQUIRE(!midi::parse_midi1_message(
        std::array<std::uint8_t, 4>{0xf0, 1, 2, 3}));
}

void stream_parser_tests() {
    sysex::SysExStreamParser parser;
    auto first = parser.consume(std::array<std::uint8_t, 2>{0xf0, 1});
    auto second = parser.consume(std::array<std::uint8_t, 2>{2, 3});
    auto third = parser.consume(std::array<std::uint8_t, 2>{4, 0xf7});
    TAUREON_REQUIRE(first.frames.empty());
    TAUREON_REQUIRE(second.frames.empty());
    TAUREON_REQUIRE(third.frames.size() == 1);
    TAUREON_REQUIRE(third.frames.front() == complete({0xf0, 1, 2, 3, 4, 0xf7}));

    parser.reset();
    const std::vector<std::uint8_t> multiple{0xf0, 0xf7, 0xf0, 1, 0xf8, 2, 0xf7};
    const auto batch = parser.consume(multiple);
    TAUREON_REQUIRE(batch.frames.size() == 2);
    TAUREON_REQUIRE(batch.frames[0].bytes == std::vector<std::uint8_t>({0xf0, 0xf7}));
    TAUREON_REQUIRE(batch.frames[1].bytes == std::vector<std::uint8_t>({0xf0, 1, 2, 0xf7}));
    TAUREON_REQUIRE(batch.realtime == std::vector<std::uint8_t>({0xf8}));

    parser.reset();
    std::vector<sysex::SysExFrame> bytewise;
    for (const auto byte : std::vector<std::uint8_t>{0xf0, 10, 11, 12, 0xf7}) {
        const auto part = parser.consume(std::span(&byte, 1));
        bytewise.insert(bytewise.end(), part.frames.begin(), part.frames.end());
    }
    TAUREON_REQUIRE(bytewise.size() == 1);
    TAUREON_REQUIRE(bytewise.front().bytes ==
                    std::vector<std::uint8_t>({0xf0, 10, 11, 12, 0xf7}));

    parser.reset();
    static_cast<void>(parser.consume(std::array<std::uint8_t, 3>{0xf0, 1, 2}));
    const auto truncated = parser.finish();
    TAUREON_REQUIRE(truncated.size() == 1);
    TAUREON_REQUIRE(truncated.front().status == sysex::SysExFrameStatus::incomplete);

    const auto malformed = parser.consume(std::array<std::uint8_t, 4>{0xf0, 1, 0x90, 2});
    TAUREON_REQUIRE(malformed.frames.size() == 2);
    TAUREON_REQUIRE(malformed.frames.front().status == sysex::SysExFrameStatus::malformed);

    parser.reset();
    static_cast<void>(parser.consume(std::array<std::uint8_t, 2>{0xf0, 1}));
    parser.notify_data_loss();
    const auto lossy = parser.consume(std::array<std::uint8_t, 2>{2, 0xf7});
    TAUREON_REQUIRE(lossy.frames.front().status == sysex::SysExFrameStatus::malformed);
    TAUREON_REQUIRE(lossy.frames.front().affected_by_data_loss);
    TAUREON_REQUIRE(parser.diagnostics().data_loss_events == 1);

    parser.reset();
    static_cast<void>(parser.consume(std::array<std::uint8_t, 2>{0xf0, 1}));
    parser.reset();
    TAUREON_REQUIRE(!parser.inside_frame());
    TAUREON_REQUIRE(parser.finish().empty());
}

void sysex7_roundtrip(const std::size_t payload_size) {
    std::vector<std::uint8_t> bytes{0xf0};
    for (std::size_t index = 0; index < payload_size; ++index) {
        bytes.push_back(static_cast<std::uint8_t>(index & 0x7f));
    }
    bytes.push_back(0xf7);
    const auto encoded = sysex::encode_sysex7(complete(bytes), 7);
    TAUREON_REQUIRE(encoded);
    const std::size_t expected = payload_size <= 6 ? 1 : (payload_size + 5) / 6;
    TAUREON_REQUIRE(encoded.value().size() == expected);

    sysex::SysEx7Assembler assembler;
    std::vector<sysex::SysExFrame> frames;
    for (const auto& packet : encoded.value()) {
        const auto decoded = sysex::decode_sysex7_packet(packet);
        TAUREON_REQUIRE(decoded);
        TAUREON_REQUIRE(decoded.value().group == 7);
        const auto part = assembler.consume(packet);
        frames.insert(frames.end(), part.begin(), part.end());
    }
    TAUREON_REQUIRE(assembler.finish().empty());
    TAUREON_REQUIRE(frames.size() == 1);
    TAUREON_REQUIRE(frames.front().bytes == bytes);
    const auto reverse = sysex::encode_sysex7(frames.front(), 7);
    TAUREON_REQUIRE(reverse);
    TAUREON_REQUIRE(reverse.value() == encoded.value());
}

void sysex7_tests() {
    constexpr std::array<std::size_t, 10> boundaries{0, 1, 5, 6, 7, 11, 12, 13, 18, 19};
    for (const auto boundary : boundaries) {
        sysex7_roundtrip(boundary);
    }

    const auto multi = sysex::encode_sysex7(complete({0xf0, 1, 2, 3, 4, 5, 6, 7, 0xf7}), 0);
    TAUREON_REQUIRE(multi);
    TAUREON_REQUIRE(sysex::decode_sysex7_packet(multi.value().front()).value().status ==
                    sysex::SysEx7PacketStatus::start);
    TAUREON_REQUIRE(sysex::decode_sysex7_packet(multi.value().back()).value().status ==
                    sysex::SysEx7PacketStatus::end);

    sysex::SysEx7Assembler assembler;
    const sysex::UmpSysEx7Packet continuation{0x30210102, 0x00000000};
    const auto orphan = assembler.consume(continuation);
    TAUREON_REQUIRE(orphan.size() == 1);
    TAUREON_REQUIRE(orphan.front().status == sysex::SysExFrameStatus::malformed);

    const sysex::UmpSysEx7Packet invalid_count{0x30070000, 0};
    TAUREON_REQUIRE(!sysex::decode_sysex7_packet(invalid_count));
    const sysex::UmpSysEx7Packet wrong_type{0x20000000, 0};
    TAUREON_REQUIRE(!sysex::decode_sysex7_packet(wrong_type));

    assembler.reset();
    const auto start = sysex::encode_sysex7(
        complete({0xf0, 1, 2, 3, 4, 5, 6, 7, 0xf7}), 3).value().front();
    TAUREON_REQUIRE(assembler.consume(start).empty());
    const auto unfinished = assembler.finish();
    TAUREON_REQUIRE(unfinished.size() == 1);
    TAUREON_REQUIRE(unfinished.front().status == sysex::SysExFrameStatus::incomplete);

    assembler.reset();
    std::vector<sysex::SysExFrame> consecutive_frames;
    const std::vector<std::vector<std::uint8_t>> consecutive{
        {0xf0, 1, 0xf7}, {0xf0, 2, 3, 4, 5, 6, 7, 0xf7}};
    for (const auto& bytes : consecutive) {
        const auto encoded_frame = sysex::encode_sysex7(complete(bytes), 4);
        TAUREON_REQUIRE(encoded_frame);
        for (const auto& packet : encoded_frame.value()) {
            const auto produced = assembler.consume(packet);
            consecutive_frames.insert(consecutive_frames.end(), produced.begin(), produced.end());
        }
    }
    TAUREON_REQUIRE(consecutive_frames.size() == consecutive.size());
    TAUREON_REQUIRE(consecutive_frames[0].bytes == consecutive[0]);
    TAUREON_REQUIRE(consecutive_frames[1].bytes == consecutive[1]);
}

} // namespace

int main() {
    return test::run([] {
        midi_message_tests();
        stream_parser_tests();
        sysex7_tests();
    });
}
