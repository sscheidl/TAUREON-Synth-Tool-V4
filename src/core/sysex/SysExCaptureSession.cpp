#include "SysExCaptureSession.hpp"

#include <iterator>

namespace taureon::sysex {

std::vector<SysExFrame> SysExCaptureSession::consume(const midi::MidiStreamEvent& event) {
    if (const auto* loss = std::get_if<midi::MidiDataLossEvent>(&event.payload)) {
        if (!loss->affects_sysex) return {};
        if (loss->backend == midi::MidiBackend::winmm) {
            midi1_parser_.notify_data_loss();
        } else if (loss->group) {
            sysex7_assembler_.notify_data_loss(*loss->group);
        }
        return {};
    }

    const auto& message = std::get<midi::NativeMidiMessage>(event.payload);
    if (const auto* midi1 = std::get_if<midi::Midi1NativeMessage>(&message.data)) {
        auto batch = midi1_parser_.consume(midi1->bytes);
        return std::move(batch.frames);
    }

    const auto& words = std::get<midi::UmpNativeMessage>(message.data).words;
    std::vector<SysExFrame> frames;
    for (std::size_t offset = 0; offset < words.size();) {
        const auto message_type = static_cast<std::uint8_t>((words[offset] >> 28u) & 0x0fu);
        constexpr std::size_t packet_sizes[16]{1, 1, 1, 2, 2, 4, 1, 1,
                                                2, 2, 3, 3, 4, 4, 4, 4};
        const auto size = packet_sizes[message_type];
        if (offset + size > words.size()) break;
        if (message_type == 3) {
            const auto produced = sysex7_assembler_.consume({words[offset], words[offset + 1]});
            frames.insert(frames.end(), produced.begin(), produced.end());
        }
        offset += size;
    }
    return frames;
}

std::vector<SysExFrame> SysExCaptureSession::finish() {
    auto frames = midi1_parser_.finish();
    auto ump_frames = sysex7_assembler_.finish();
    frames.insert(frames.end(), std::make_move_iterator(ump_frames.begin()),
                  std::make_move_iterator(ump_frames.end()));
    return frames;
}

void SysExCaptureSession::reset() noexcept {
    midi1_parser_.reset();
    sysex7_assembler_.reset();
}

} // namespace taureon::sysex
