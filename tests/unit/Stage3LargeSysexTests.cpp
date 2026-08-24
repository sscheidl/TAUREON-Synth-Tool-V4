#include "TestSupport.hpp"

#include "core/sysex/SysEx7.hpp"
#include "core/sysex/SysExStreamParser.hpp"
#include "core/transfer/TransferEngine.hpp"
#include "transports/fake/FakeMidiTransport.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

using namespace taureon;

namespace {

std::vector<std::uint8_t> large_frame(const std::size_t bytes) {
    TAUREON_REQUIRE(bytes >= 2);
    std::vector<std::uint8_t> frame(bytes);
    frame.front() = 0xf0;
    frame.back() = 0xf7;
    for (std::size_t index = 1; index + 1 < bytes; ++index) {
        frame[index] = static_cast<std::uint8_t>(index & 0x7f);
    }
    return frame;
}

void verify_large(const std::size_t size) {
    const auto original = large_frame(size);
    sysex::SysExStreamParser parser;
    std::vector<sysex::SysExFrame> parsed;
    const std::vector<std::size_t> chunks{1, 7, 4093, 31, 8192};
    std::size_t offset = 0;
    std::size_t chunk_index = 0;
    while (offset < original.size()) {
        const auto count = (std::min)(chunks[chunk_index++ % chunks.size()],
                                      original.size() - offset);
        const auto batch = parser.consume(
            std::span<const std::uint8_t>(original.data() + offset, count));
        parsed.insert(parsed.end(), batch.frames.begin(), batch.frames.end());
        offset += count;
    }
    const auto final = parser.finish();
    parsed.insert(parsed.end(), final.begin(), final.end());
    TAUREON_REQUIRE(parsed.size() == 1);
    TAUREON_REQUIRE(parsed.front().status == sysex::SysExFrameStatus::complete);
    TAUREON_REQUIRE(parsed.front().bytes == original);

    const auto packets = sysex::encode_sysex7(parsed.front(), 9);
    TAUREON_REQUIRE(packets);
    sysex::SysEx7Assembler assembler;
    std::vector<sysex::SysExFrame> reconstructed;
    for (const auto& packet : packets.value()) {
        const auto frames = assembler.consume(packet);
        reconstructed.insert(reconstructed.end(), frames.begin(), frames.end());
    }
    TAUREON_REQUIRE(assembler.finish().empty());
    TAUREON_REQUIRE(reconstructed.size() == 1);
    TAUREON_REQUIRE(reconstructed.front().bytes == original);

    auto truncated = original;
    truncated.pop_back();
    sysex::SysExStreamParser truncated_parser;
    static_cast<void>(truncated_parser.consume(truncated));
    const auto incomplete = truncated_parser.finish();
    TAUREON_REQUIRE(incomplete.size() == 1);
    TAUREON_REQUIRE(incomplete.front().status == sysex::SysExFrameStatus::incomplete);

    auto malformed = original;
    malformed[malformed.size() / 2] = 0xf0;
    sysex::SysExStreamParser malformed_parser;
    const auto malformed_batch = malformed_parser.consume(malformed);
    TAUREON_REQUIRE(std::any_of(malformed_batch.frames.begin(), malformed_batch.frames.end(),
                               [](const auto& frame) {
                                   return frame.status == sysex::SysExFrameStatus::malformed;
                               }));
}

void many_frames_over_one_mib() {
    std::vector<std::uint8_t> stream;
    constexpr std::size_t frame_count = 2048;
    const auto one = large_frame(600);
    stream.reserve(frame_count * one.size());
    for (std::size_t index = 0; index < frame_count; ++index) {
        stream.insert(stream.end(), one.begin(), one.end());
    }
    TAUREON_REQUIRE(stream.size() > 1024 * 1024);
    sysex::SysExStreamParser parser;
    std::size_t frames = 0;
    for (std::size_t offset = 0; offset < stream.size(); offset += 997) {
        const auto count = (std::min)(std::size_t{997}, stream.size() - offset);
        frames += parser.consume(std::span<const std::uint8_t>(stream.data() + offset, count))
                      .frames.size();
    }
    frames += parser.finish().size();
    TAUREON_REQUIRE(frames == frame_count);
    TAUREON_REQUIRE(parser.diagnostics().complete_frames == frame_count);
    TAUREON_REQUIRE(parser.diagnostics().complete_bytes == stream.size());
}

void large_transfer_cancellation_is_responsive() {
    const midi::MidiRouteIdentity route{
        midi::MidiBackend::winmm, midi::MidiDirection::output,
        midi::WinmmRouteIdentity{"Stage 3 large fake", 1, 32, 256}};
    midi::FakeMidiTransport transport(
        midi::MidiBackend::winmm,
        {{route, "Stage 3 large fake", midi::MidiProtocol::midi1,
          {false, true, true, false}, 0, std::nullopt}});
    TAUREON_REQUIRE(transport.open({std::nullopt, route}));
    std::vector<midi::NativeMidiMessage> messages;
    for (int index = 0; index < 16; ++index) {
        messages.push_back({midi::MidiBackend::winmm,
                            midi::Midi1NativeMessage{large_frame(64 * 1024)}, std::nullopt});
    }
    transfer::TransferEngine engine(transport);
    TAUREON_REQUIRE(engine.start(std::move(messages), {}, [&](const auto& progress) {
        if (progress.messages_accepted == 1) engine.request_cancel();
    }));
    const auto result = engine.wait();
    TAUREON_REQUIRE(result.state == transfer::TransferState::cancelled);
    TAUREON_REQUIRE(result.progress.messages_accepted == 1);
    TAUREON_REQUIRE(transport.diagnostics().transmitted_messages == 1);
    TAUREON_REQUIRE(transport.close());
}

} // namespace

int main() {
    return test::run([] {
        verify_large(64 * 1024);
        verify_large(600 * 1024);
        verify_large(900 * 1024);
        verify_large(1024 * 1024 + 257);
        many_frames_over_one_mib();
        large_transfer_cancellation_is_responsive();
    });
}
