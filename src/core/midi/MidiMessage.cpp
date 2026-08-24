#include "MidiMessage.hpp"

#include <algorithm>
#include <string>

namespace taureon::midi {
namespace {

Result<ParsedMidi1Message> invalid(std::string message) {
    return Result<ParsedMidi1Message>::failure(
        {MidiErrorCode::malformed_data, std::move(message), {}, std::nullopt});
}

Result<ParsedMidi1Message> incomplete(std::string message) {
    return Result<ParsedMidi1Message>::failure(
        {MidiErrorCode::incomplete_data, std::move(message), {}, std::nullopt});
}

bool valid_data(const std::span<const std::uint8_t> bytes, const std::size_t begin) {
    return std::all_of(bytes.begin() + static_cast<std::ptrdiff_t>(begin), bytes.end(),
                       [](const std::uint8_t value) { return value < 0x80; });
}

} // namespace

Result<ParsedMidi1Message> parse_midi1_message(const std::span<const std::uint8_t> bytes) {
    if (bytes.empty()) return incomplete("empty MIDI 1.0 message");
    const auto status = bytes.front();
    if (status < 0x80) return invalid("MIDI 1.0 message has no status byte");

    ParsedMidi1Message parsed;
    parsed.raw.assign(bytes.begin(), bytes.end());

    if (status < 0xf0) {
        const auto family = static_cast<std::uint8_t>(status & 0xf0);
        const std::size_t expected = family == 0xc0 || family == 0xd0 ? 2 : 3;
        if (bytes.size() < expected) return incomplete("truncated channel voice message");
        if (bytes.size() != expected || !valid_data(bytes, 1)) {
            return invalid("invalid channel voice message length or data byte");
        }
        parsed.channel = static_cast<std::uint8_t>(status & 0x0f);
        parsed.data1 = bytes[1];
        if (expected == 3) parsed.data2 = bytes[2];
        switch (family) {
        case 0x80: parsed.kind = Midi1MessageKind::note_off; break;
        case 0x90: parsed.kind = Midi1MessageKind::note_on; break;
        case 0xa0: parsed.kind = Midi1MessageKind::polyphonic_aftertouch; break;
        case 0xb0: parsed.kind = Midi1MessageKind::control_change; break;
        case 0xc0: parsed.kind = Midi1MessageKind::program_change; break;
        case 0xd0: parsed.kind = Midi1MessageKind::channel_pressure; break;
        case 0xe0:
            parsed.kind = Midi1MessageKind::pitch_bend;
            parsed.value14 = static_cast<std::uint16_t>(bytes[1] | (bytes[2] << 7));
            break;
        default: parsed.kind = Midi1MessageKind::unknown; break;
        }
        return Result<ParsedMidi1Message>::success(std::move(parsed));
    }

    if (status == 0xf0) {
        if (bytes.size() < 2 || bytes.back() != 0xf7) {
            return incomplete("truncated MIDI 1.0 SysEx message");
        }
        for (std::size_t index = 1; index + 1 < bytes.size(); ++index) {
            if (bytes[index] >= 0x80) return invalid("invalid status byte inside SysEx message");
        }
        parsed.kind = Midi1MessageKind::system_exclusive;
        return Result<ParsedMidi1Message>::success(std::move(parsed));
    }

    std::size_t expected = 1;
    switch (status) {
    case 0xf1: expected = 2; parsed.kind = Midi1MessageKind::system_common; break;
    case 0xf2: expected = 3; parsed.kind = Midi1MessageKind::system_common; break;
    case 0xf3: expected = 2; parsed.kind = Midi1MessageKind::system_common; break;
    case 0xf6: parsed.kind = Midi1MessageKind::system_common; break;
    case 0xf8: parsed.kind = Midi1MessageKind::timing_clock; break;
    case 0xfa: parsed.kind = Midi1MessageKind::transport_start; break;
    case 0xfb: parsed.kind = Midi1MessageKind::transport_continue; break;
    case 0xfc: parsed.kind = Midi1MessageKind::transport_stop; break;
    case 0xfe: parsed.kind = Midi1MessageKind::active_sensing; break;
    case 0xff: parsed.kind = Midi1MessageKind::system_reset; break;
    case 0xf4:
    case 0xf5:
    case 0xf9:
    case 0xfd: parsed.kind = Midi1MessageKind::unknown; break;
    case 0xf7: return invalid("unexpected SysEx end byte");
    default: parsed.kind = Midi1MessageKind::system_realtime; break;
    }
    if (bytes.size() < expected) return incomplete("truncated system message");
    if (bytes.size() != expected || !valid_data(bytes, 1)) {
        return invalid("invalid system message length or data byte");
    }
    if (expected > 1) parsed.data1 = bytes[1];
    if (expected > 2) {
        parsed.data2 = bytes[2];
        parsed.value14 = static_cast<std::uint16_t>(bytes[1] | (bytes[2] << 7));
    }
    return Result<ParsedMidi1Message>::success(std::move(parsed));
}

} // namespace taureon::midi
