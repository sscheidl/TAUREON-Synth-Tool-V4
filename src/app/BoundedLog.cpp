#include "app/BoundedLog.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace taureon::app {
namespace {
int severity(const LogLevel level) noexcept {
    switch (level) {
    case LogLevel::error: return 0;
    case LogLevel::warning: return 1;
    case LogLevel::info: return 2;
    case LogLevel::debug: return 3;
    }
    return 3;
}
}

BoundedLog::BoundedLog(const std::size_t capacity) : capacity_(capacity) {
    if (capacity == 0) throw std::invalid_argument("log capacity must be positive");
}

void BoundedLog::configure(const LogLevel minimum_level, const std::size_t capacity) {
    if (capacity == 0) throw std::invalid_argument("log capacity must be positive");
    std::scoped_lock lock(mutex_);
    minimum_level_ = minimum_level;
    capacity_ = capacity;
    while (entries_.size() > capacity_) entries_.pop_front();
}

void BoundedLog::append(const LogLevel level, std::string message) {
    std::scoped_lock lock(mutex_);
    if (severity(level) > severity(minimum_level_)) return;
    entries_.push_back({level, std::move(message)});
    while (entries_.size() > capacity_) entries_.pop_front();
}

std::vector<LogEntry> BoundedLog::snapshot() const {
    std::scoped_lock lock(mutex_);
    return {entries_.begin(), entries_.end()};
}

std::size_t BoundedLog::capacity() const noexcept {
    std::scoped_lock lock(mutex_);
    return capacity_;
}

std::size_t BoundedLog::size() const noexcept {
    std::scoped_lock lock(mutex_);
    return entries_.size();
}

} // namespace taureon::app
