#pragma once

#include "SysExFrame.hpp"
#include "core/midi/Result.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace taureon::sysex {

enum class SysEx7PacketStatus : std::uint8_t {
    complete = 0,
    start = 1,
    continuation = 2,
    end = 3,
};

struct UmpSysEx7Packet {
    std::uint32_t word0{};
    std::uint32_t word1{};

    bool operator==(const UmpSysEx7Packet&) const = default;
};

struct DecodedSysEx7Packet {
    std::uint8_t group{};
    SysEx7PacketStatus status{SysEx7PacketStatus::complete};
    std::vector<std::uint8_t> payload;
};

[[nodiscard]] midi::Result<DecodedSysEx7Packet> decode_sysex7_packet(
    const UmpSysEx7Packet& packet);
[[nodiscard]] midi::Result<std::vector<UmpSysEx7Packet>> encode_sysex7(
    const SysExFrame& frame, std::uint8_t group);

class SysEx7Assembler {
public:
    [[nodiscard]] std::vector<SysExFrame> consume(const UmpSysEx7Packet& packet);
    [[nodiscard]] std::vector<SysExFrame> finish();
    void notify_data_loss(std::uint8_t group) noexcept;
    void reset() noexcept;

private:
    struct GroupState {
        bool active{};
        bool affected_by_data_loss{};
        bool pending_data_loss{};
        std::vector<std::uint8_t> bytes;
    };

    std::array<GroupState, 16> groups_;
};

} // namespace taureon::sysex
