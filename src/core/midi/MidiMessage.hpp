#pragma once

#include "Result.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace taureon::midi {

enum class Midi1MessageKind {
    note_off,
    note_on,
    polyphonic_aftertouch,
    control_change,
    program_change,
    channel_pressure,
    pitch_bend,
    system_exclusive,
    system_common,
    timing_clock,
    transport_start,
    transport_continue,
    transport_stop,
    active_sensing,
    system_reset,
    system_realtime,
    unknown,
};

struct ParsedMidi1Message {
    Midi1MessageKind kind{Midi1MessageKind::unknown};
    std::vector<std::uint8_t> raw;
    std::optional<std::uint8_t> channel;
    std::optional<std::uint8_t> data1;
    std::optional<std::uint8_t> data2;
    std::optional<std::uint16_t> value14;

    bool operator==(const ParsedMidi1Message&) const = default;
};

[[nodiscard]] Result<ParsedMidi1Message> parse_midi1_message(
    std::span<const std::uint8_t> bytes);

} // namespace taureon::midi
