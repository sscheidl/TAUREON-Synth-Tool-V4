#pragma once

#include "core/midi/MidiTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

namespace taureon::app {

struct MonitorEvent {
    std::uint64_t sequence{};
    midi::MidiDirection direction{midi::MidiDirection::input};
    midi::NativeMidiMessage message;

    bool operator==(const MonitorEvent&) const = default;
};

struct MonitorQueueStats {
    std::size_t current_size{};
    std::size_t high_water_mark{};
    std::uint64_t accepted{};
    std::uint64_t dropped{};
    std::uint64_t rejected_after_close{};
};

class MonitorEventQueue {
public:
    explicit MonitorEventQueue(std::size_t capacity);

    [[nodiscard]] bool push(MonitorEvent event);
    [[nodiscard]] std::vector<MonitorEvent> drain(std::size_t maximum);
    [[nodiscard]] MonitorQueueStats stats() const noexcept;
    void close_acceptance() noexcept;

private:
    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::deque<MonitorEvent> events_;
    MonitorQueueStats stats_;
    bool accepting_{true};
};

} // namespace taureon::app
