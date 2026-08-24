#include "SysExStreamParser.hpp"

#include <utility>

namespace taureon::sysex {

void SysExStreamParser::record(const SysExFrame& frame) noexcept {
    switch (frame.status) {
    case SysExFrameStatus::complete:
        ++diagnostics_.complete_frames;
        diagnostics_.complete_bytes += frame.bytes.size();
        break;
    case SysExFrameStatus::incomplete: ++diagnostics_.incomplete_frames; break;
    case SysExFrameStatus::malformed: ++diagnostics_.malformed_frames; break;
    }
}

SysExParseBatch SysExStreamParser::consume(const std::span<const std::uint8_t> bytes) {
    SysExParseBatch batch;
    for (const auto byte : bytes) {
        if (byte >= 0xf8) {
            batch.realtime.push_back(byte);
            ++diagnostics_.realtime_bytes;
            continue;
        }

        if (!inside_) {
            if (byte == 0xf0) {
                current_.assign(1, byte);
                inside_ = true;
                current_data_loss_ = pending_data_loss_;
                pending_data_loss_ = false;
            } else {
                SysExFrame malformed{SysExFrameStatus::malformed, {byte},
                                     byte == 0xf7 ? "unexpected SysEx end" :
                                                    "byte outside SysEx frame",
                                     std::nullopt, false};
                record(malformed);
                batch.frames.push_back(std::move(malformed));
            }
            continue;
        }

        if (byte == 0xf0) {
            SysExFrame malformed{SysExFrameStatus::malformed, std::move(current_),
                                 "nested SysEx start", std::nullopt, current_data_loss_};
            record(malformed);
            batch.frames.push_back(std::move(malformed));
            current_.assign(1, byte);
            current_data_loss_ = false;
            continue;
        }

        current_.push_back(byte);
        if (byte == 0xf7) {
            SysExFrame frame{current_data_loss_ ? SysExFrameStatus::malformed :
                                                 SysExFrameStatus::complete,
                             std::move(current_),
                             current_data_loss_ ? "capture affected by data loss" : "",
                             std::nullopt, current_data_loss_};
            record(frame);
            batch.frames.push_back(std::move(frame));
            current_.clear();
            inside_ = false;
            current_data_loss_ = false;
        } else if (byte >= 0x80) {
            SysExFrame malformed{SysExFrameStatus::malformed, std::move(current_),
                                 "non-realtime status inside SysEx", std::nullopt,
                                 current_data_loss_};
            record(malformed);
            batch.frames.push_back(std::move(malformed));
            current_.clear();
            inside_ = false;
            current_data_loss_ = false;
        }
    }
    return batch;
}

std::vector<SysExFrame> SysExStreamParser::finish() {
    std::vector<SysExFrame> result;
    if (inside_) {
        SysExFrame frame{current_data_loss_ ? SysExFrameStatus::malformed :
                                             SysExFrameStatus::incomplete,
                         std::move(current_),
                         current_data_loss_ ? "incomplete capture affected by data loss" :
                                              "unterminated SysEx frame",
                         std::nullopt, current_data_loss_};
        record(frame);
        result.push_back(std::move(frame));
    } else if (pending_data_loss_) {
        SysExFrame frame{SysExFrameStatus::malformed, {},
                         "capture ended after data loss without a following SysEx frame",
                         std::nullopt, true};
        record(frame);
        result.push_back(std::move(frame));
    }
    current_.clear();
    inside_ = false;
    current_data_loss_ = false;
    pending_data_loss_ = false;
    return result;
}

void SysExStreamParser::reset() noexcept {
    current_.clear();
    inside_ = false;
    current_data_loss_ = false;
    pending_data_loss_ = false;
}

void SysExStreamParser::notify_data_loss() noexcept {
    ++diagnostics_.data_loss_events;
    if (inside_) current_data_loss_ = true;
    else pending_data_loss_ = true;
}

bool SysExStreamParser::inside_frame() const noexcept { return inside_; }

const SysExParserDiagnostics& SysExStreamParser::diagnostics() const noexcept {
    return diagnostics_;
}

} // namespace taureon::sysex
