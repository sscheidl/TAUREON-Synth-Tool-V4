#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace taureon::sysex {

enum class SysExFrameStatus {
    complete,
    incomplete,
    malformed,
};

struct SysExFrame {
    SysExFrameStatus status{SysExFrameStatus::incomplete};
    std::vector<std::uint8_t> bytes;
    std::string issue;
    std::optional<std::uint8_t> group;
    bool affected_by_data_loss{};

    bool operator==(const SysExFrame&) const = default;
};

struct SysExParserDiagnostics {
    std::uint64_t complete_frames{};
    std::uint64_t incomplete_frames{};
    std::uint64_t malformed_frames{};
    std::uint64_t complete_bytes{};
    std::uint64_t realtime_bytes{};
    std::uint64_t data_loss_events{};
};

} // namespace taureon::sysex
