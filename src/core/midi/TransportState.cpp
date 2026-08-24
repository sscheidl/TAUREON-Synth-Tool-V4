#include "TransportState.hpp"

#include <string>

namespace taureon::midi {
namespace {

MidiError invalid_state(const char* operation) {
    return {MidiErrorCode::invalid_state, std::string("invalid transport state for ") + operation,
            {}, std::nullopt};
}

} // namespace

TransportState TransportStateMachine::state() const noexcept {
    std::scoped_lock lock(mutex_);
    return state_;
}

Result<void> TransportStateMachine::require_and_set(const TransportState expected,
                                                     const TransportState next,
                                                     const char* operation) {
    std::scoped_lock lock(mutex_);
    if (state_ != expected) return Result<void>::failure(invalid_state(operation));
    state_ = next;
    return Result<void>::success();
}

Result<void> TransportStateMachine::begin_open() {
    return require_and_set(TransportState::closed, TransportState::opening, "open");
}

Result<void> TransportStateMachine::complete_open() {
    return require_and_set(TransportState::opening, TransportState::open, "complete open");
}

Result<void> TransportStateMachine::begin_close() {
    std::scoped_lock lock(mutex_);
    if (state_ == TransportState::closed) return Result<void>::success();
    if (state_ != TransportState::open && state_ != TransportState::failed) {
        return Result<void>::failure(invalid_state("close"));
    }
    state_ = TransportState::closing;
    return Result<void>::success();
}

Result<void> TransportStateMachine::complete_close() {
    std::scoped_lock lock(mutex_);
    if (state_ == TransportState::closed) return Result<void>::success();
    if (state_ != TransportState::closing) {
        return Result<void>::failure(invalid_state("complete close"));
    }
    state_ = TransportState::closed;
    return Result<void>::success();
}

void TransportStateMachine::fail() noexcept {
    std::scoped_lock lock(mutex_);
    state_ = TransportState::failed;
}

} // namespace taureon::midi
