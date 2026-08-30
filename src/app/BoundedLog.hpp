#pragma once

#include "app/Settings.hpp"

#include <cstddef>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace taureon::app {

struct LogEntry {
    LogLevel level{LogLevel::info};
    std::string message;
};

class BoundedLog final {
public:
    explicit BoundedLog(std::size_t capacity = 1'000);

    void configure(LogLevel minimum_level, std::size_t capacity);
    void append(LogLevel level, std::string message);
    [[nodiscard]] std::vector<LogEntry> snapshot() const;
    [[nodiscard]] std::size_t capacity() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;

private:
    mutable std::mutex mutex_;
    std::deque<LogEntry> entries_;
    LogLevel minimum_level_{LogLevel::info};
    std::size_t capacity_{};
};

} // namespace taureon::app
