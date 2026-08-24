#include "SysEx7.hpp"

#include <algorithm>
#include <string>

namespace taureon::sysex {
namespace {

midi::MidiError malformed(std::string message) {
    return {midi::MidiErrorCode::malformed_data, std::move(message), "UMP SysEx7", std::nullopt};
}

UmpSysEx7Packet make_packet(const std::uint8_t group, const SysEx7PacketStatus status,
                            const std::span<const std::uint8_t> payload) {
    std::array<std::uint8_t, 6> data{};
    std::copy(payload.begin(), payload.end(), data.begin());
    const auto word0 = (std::uint32_t{0x3} << 28) |
                       (static_cast<std::uint32_t>(group) << 24) |
                       (static_cast<std::uint32_t>(status) << 20) |
                       (static_cast<std::uint32_t>(payload.size()) << 16) |
                       (static_cast<std::uint32_t>(data[0]) << 8) |
                       static_cast<std::uint32_t>(data[1]);
    const auto word1 = (static_cast<std::uint32_t>(data[2]) << 24) |
                       (static_cast<std::uint32_t>(data[3]) << 16) |
                       (static_cast<std::uint32_t>(data[4]) << 8) |
                       static_cast<std::uint32_t>(data[5]);
    return {word0, word1};
}

SysExFrame malformed_sequence(const std::uint8_t group, std::vector<std::uint8_t> bytes,
                              std::string issue, const bool affected_by_data_loss = false) {
    return {SysExFrameStatus::malformed, std::move(bytes), std::move(issue), group,
            affected_by_data_loss};
}

} // namespace

midi::Result<DecodedSysEx7Packet> decode_sysex7_packet(const UmpSysEx7Packet& packet) {
    if ((packet.word0 >> 28) != 0x3) {
        return midi::Result<DecodedSysEx7Packet>::failure(malformed("not a 64-bit data message"));
    }
    const auto group = static_cast<std::uint8_t>((packet.word0 >> 24) & 0x0f);
    const auto status_value = static_cast<std::uint8_t>((packet.word0 >> 20) & 0x0f);
    const auto count = static_cast<std::uint8_t>((packet.word0 >> 16) & 0x0f);
    if (status_value > 3 || count > 6) {
        return midi::Result<DecodedSysEx7Packet>::failure(
            malformed("invalid SysEx7 status or byte count"));
    }
    const std::array<std::uint8_t, 6> bytes{
        static_cast<std::uint8_t>((packet.word0 >> 8) & 0xff),
        static_cast<std::uint8_t>(packet.word0 & 0xff),
        static_cast<std::uint8_t>((packet.word1 >> 24) & 0xff),
        static_cast<std::uint8_t>((packet.word1 >> 16) & 0xff),
        static_cast<std::uint8_t>((packet.word1 >> 8) & 0xff),
        static_cast<std::uint8_t>(packet.word1 & 0xff)};
    if (std::any_of(bytes.begin(), bytes.begin() + count,
                    [](const std::uint8_t value) { return value >= 0x80; })) {
        return midi::Result<DecodedSysEx7Packet>::failure(
            malformed("SysEx7 payload contains non-7-bit data"));
    }
    return midi::Result<DecodedSysEx7Packet>::success(
        {group, static_cast<SysEx7PacketStatus>(status_value),
         std::vector<std::uint8_t>(bytes.begin(), bytes.begin() + count)});
}

midi::Result<std::vector<UmpSysEx7Packet>> encode_sysex7(const SysExFrame& frame,
                                                          const std::uint8_t group) {
    if (group > 15) {
        return midi::Result<std::vector<UmpSysEx7Packet>>::failure(
            {midi::MidiErrorCode::invalid_route, "SysEx7 group is out of range", {}, std::nullopt});
    }
    if (frame.status != SysExFrameStatus::complete || frame.affected_by_data_loss ||
        frame.bytes.size() < 2 || frame.bytes.front() != 0xf0 || frame.bytes.back() != 0xf7) {
        return midi::Result<std::vector<UmpSysEx7Packet>>::failure(
            malformed("only a complete framed MIDI 1.0 SysEx message can be encoded"));
    }
    const std::span payload(frame.bytes.data() + 1, frame.bytes.size() - 2);
    if (std::any_of(payload.begin(), payload.end(),
                    [](const std::uint8_t value) { return value >= 0x80; })) {
        return midi::Result<std::vector<UmpSysEx7Packet>>::failure(
            malformed("MIDI 1.0 SysEx payload contains non-7-bit data"));
    }

    std::vector<UmpSysEx7Packet> packets;
    if (payload.size() <= 6) {
        packets.push_back(make_packet(group, SysEx7PacketStatus::complete, payload));
        return midi::Result<std::vector<UmpSysEx7Packet>>::success(std::move(packets));
    }
    for (std::size_t offset = 0; offset < payload.size(); offset += 6) {
        const auto count = (std::min)(std::size_t{6}, payload.size() - offset);
        const auto part = payload.subspan(offset, count);
        const bool first = offset == 0;
        const bool last = offset + count == payload.size();
        const auto status = first ? SysEx7PacketStatus::start :
                            last ? SysEx7PacketStatus::end :
                                   SysEx7PacketStatus::continuation;
        packets.push_back(make_packet(group, status, part));
    }
    return midi::Result<std::vector<UmpSysEx7Packet>>::success(std::move(packets));
}

std::vector<SysExFrame> SysEx7Assembler::consume(const UmpSysEx7Packet& packet) {
    const auto packet_group = static_cast<std::uint8_t>((packet.word0 >> 24) & 0x0f);
    const auto decoded = decode_sysex7_packet(packet);
    if (!decoded) {
        auto& state = groups_[packet_group];
        if (state.active) {
            auto bytes = std::move(state.bytes);
            const bool affected = state.affected_by_data_loss || state.pending_data_loss;
            state = {};
            return {{SysExFrameStatus::malformed, std::move(bytes),
                     "invalid SysEx7 packet interrupted active sequence: " +
                         decoded.error().message,
                     packet_group, affected}};
        }
        const bool affected = state.pending_data_loss;
        state = {};
        return {{SysExFrameStatus::malformed, {}, decoded.error().message, packet_group, affected}};
    }
    const auto& value = decoded.value();
    auto& state = groups_[value.group];
    std::vector<SysExFrame> frames;

    if (value.status == SysEx7PacketStatus::complete) {
        if (state.active) {
            frames.push_back(malformed_sequence(value.group, std::move(state.bytes),
                                                "complete packet interrupted active sequence",
                                                state.affected_by_data_loss));
            state = {};
        }
        std::vector<std::uint8_t> bytes{0xf0};
        bytes.insert(bytes.end(), value.payload.begin(), value.payload.end());
        bytes.push_back(0xf7);
        const bool affected = state.pending_data_loss;
        state.pending_data_loss = false;
        frames.push_back({affected ? SysExFrameStatus::malformed : SysExFrameStatus::complete,
                          std::move(bytes),
                          affected ? "capture affected by data loss" : "", value.group,
                          affected});
        return frames;
    }

    if (value.status == SysEx7PacketStatus::start) {
        if (state.active) {
            frames.push_back(malformed_sequence(value.group, std::move(state.bytes),
                                                "start packet interrupted active sequence",
                                                state.affected_by_data_loss));
            state = {};
        }
        state.active = true;
        state.affected_by_data_loss = state.pending_data_loss;
        state.pending_data_loss = false;
        state.bytes.assign(1, 0xf0);
        state.bytes.insert(state.bytes.end(), value.payload.begin(), value.payload.end());
        return frames;
    }

    if (!state.active) {
        std::vector<std::uint8_t> bytes(value.payload.begin(), value.payload.end());
        const bool affected = state.pending_data_loss;
        state.pending_data_loss = false;
        return {malformed_sequence(value.group, std::move(bytes),
                                   value.status == SysEx7PacketStatus::continuation ?
                                       "continuation without start" : "end without start",
                                   affected)};
    }

    state.bytes.insert(state.bytes.end(), value.payload.begin(), value.payload.end());
    if (value.status == SysEx7PacketStatus::end) {
        state.bytes.push_back(0xf7);
        frames.push_back({state.affected_by_data_loss ? SysExFrameStatus::malformed :
                                                       SysExFrameStatus::complete,
                          std::move(state.bytes),
                          state.affected_by_data_loss ? "capture affected by data loss" : "",
                          value.group, state.affected_by_data_loss});
        state = {};
    }
    return frames;
}

std::vector<SysExFrame> SysEx7Assembler::finish() {
    std::vector<SysExFrame> frames;
    for (std::uint8_t group = 0; group < groups_.size(); ++group) {
        auto& state = groups_[group];
        if (state.active) {
            frames.push_back({state.affected_by_data_loss ? SysExFrameStatus::malformed :
                                                           SysExFrameStatus::incomplete,
                              std::move(state.bytes),
                              state.affected_by_data_loss ?
                                  "unterminated SysEx7 sequence affected by data loss" :
                                  "unterminated SysEx7 packet sequence",
                              group, state.affected_by_data_loss});
            state = {};
        } else if (state.pending_data_loss) {
            frames.push_back({SysExFrameStatus::malformed, {},
                              "capture ended after data loss without a following SysEx7 frame",
                              group, true});
            state = {};
        }
    }
    return frames;
}

void SysEx7Assembler::notify_data_loss(const std::uint8_t group) noexcept {
    if (group >= groups_.size()) return;
    auto& state = groups_[group];
    if (state.active) state.affected_by_data_loss = true;
    else state.pending_data_loss = true;
}

void SysEx7Assembler::reset() noexcept {
    for (auto& state : groups_) state = {};
}

} // namespace taureon::sysex
