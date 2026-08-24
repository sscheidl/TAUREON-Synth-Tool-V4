#include "app/MonitorEventQueue.hpp"

#include <algorithm>
#include <stdexcept>

namespace taureon::app {

MonitorEventQueue::MonitorEventQueue(const std::size_t capacity) : capacity_(capacity) {
    if (capacity == 0) throw std::invalid_argument("monitor queue capacity must be positive");
}

bool MonitorEventQueue::push(MonitorEvent event) {
    std::scoped_lock lock(mutex_);
    if (!accepting_) {
        ++stats_.rejected_after_close;
        return false;
    }
    if (events_.size() == capacity_) {
        ++stats_.dropped;
        return false;
    }
    events_.push_back(std::move(event));
    ++stats_.accepted;
    stats_.current_size = events_.size();
    stats_.high_water_mark = std::max(stats_.high_water_mark, events_.size());
    return true;
}

std::vector<MonitorEvent> MonitorEventQueue::drain(const std::size_t maximum) {
    std::scoped_lock lock(mutex_);
    const auto count = std::min(maximum, events_.size());
    std::vector<MonitorEvent> result;
    result.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        result.push_back(std::move(events_.front()));
        events_.pop_front();
    }
    stats_.current_size = events_.size();
    return result;
}

MonitorQueueStats MonitorEventQueue::stats() const noexcept {
    std::scoped_lock lock(mutex_);
    return stats_;
}

void MonitorEventQueue::close_acceptance() noexcept {
    std::scoped_lock lock(mutex_);
    accepting_ = false;
}

} // namespace taureon::app
