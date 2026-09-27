#include "core/midi/MidiMessage.hpp"
#include "core/sysex/SysEx7.hpp"
#include "core/sysex/SysExStreamParser.hpp"
#include "core/transfer/TransferEngine.hpp"

#include <array>
#include <cstdint>
#include <vector>

int main() {
    const std::array<std::uint8_t, 3> pressure{0xa2, 62, 40};
    const auto midi = taureon::midi::parse_midi1_message(pressure);
    if (!midi || midi.value().kind != taureon::midi::Midi1MessageKind::polyphonic_aftertouch ||
        midi.value().channel != 2 || midi.value().data1 != 62 || midi.value().data2 != 40) {
        return 1;
    }

    const std::vector<std::uint8_t> bytes{0xf0, 0x7d, 1, 2, 3, 4, 5, 6, 7, 0xf7};
    taureon::sysex::SysExStreamParser parser;
    const auto parsed = parser.consume(bytes);
    if (parsed.frames.size() != 1 || parsed.frames.front().bytes != bytes) return 2;
    const auto packets = taureon::sysex::encode_sysex7(parsed.frames.front(), 3);
    if (!packets || packets.value().size() != 2) return 3;
    taureon::sysex::SysEx7Assembler assembler;
    std::vector<taureon::sysex::SysExFrame> reconstructed;
    for (const auto& packet : packets.value()) {
        auto frames = assembler.consume(packet);
        reconstructed.insert(reconstructed.end(), frames.begin(), frames.end());
    }
    if (reconstructed.size() != 1 || reconstructed.front().bytes != bytes) return 4;

    // Verify that the installed transfer API is available to a separate consumer.
    [[maybe_unused]] taureon::transfer::TransferOptions options;
    return 0;
}
