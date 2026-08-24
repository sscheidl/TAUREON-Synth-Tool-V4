#pragma once

#include "Result.hpp"

#include <mutex>

namespace taureon::midi {

enum class TransportState {
    closed,
    opening,
    open,
    closing,
    failed,
};

class TransportStateMachine {
public:
    [[nodiscard]] TransportState state() const noexcept;
    [[nodiscard]] Result<void> begin_open();
    [[nodiscard]] Result<void> complete_open();
    [[nodiscard]] Result<void> begin_close();
    [[nodiscard]] Result<void> complete_close();
    void fail() noexcept;

private:
    Result<void> require_and_set(TransportState expected, TransportState next,
                                 const char* operation);

    mutable std::mutex mutex_;
    TransportState state_{TransportState::closed};
};

} // namespace taureon::midi
