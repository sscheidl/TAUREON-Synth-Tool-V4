#include "TransferEngine.hpp"

#include <variant>

namespace taureon::transfer {
namespace {

std::uint64_t message_size(const midi::NativeMidiMessage& message) {
    if (const auto* midi1 = std::get_if<midi::Midi1NativeMessage>(&message.data)) {
        return midi1->bytes.size();
    }
    return std::get<midi::UmpNativeMessage>(message.data).words.size() * sizeof(std::uint32_t);
}

midi::MidiError cancelled_error() {
    return {midi::MidiErrorCode::transfer_cancelled, "transfer cancelled", {}, std::nullopt};
}

midi::MidiError timeout_error() {
    return {midi::MidiErrorCode::timeout, "transfer timed out", {}, std::nullopt};
}

midi::MidiError disconnected_error() {
    return {midi::MidiErrorCode::transport_disconnected,
            "transport is no longer open", {}, std::nullopt};
}

} // namespace

TransferEngine::TransferEngine(midi::IMidiTransport& transport) : transport_(transport) {}

TransferEngine::~TransferEngine() {
    request_cancel();
    if (worker_.joinable()) worker_.join();
}

midi::Result<void> TransferEngine::start(std::vector<midi::NativeMidiMessage> messages,
                                         const TransferOptions options,
                                         TransferProgressHandler handler) {
    std::unique_lock lock(mutex_);
    if (worker_.joinable() || (progress_.state != TransferState::idle &&
                               progress_.state != TransferState::cancelled)) {
        return midi::Result<void>::failure(
            {midi::MidiErrorCode::invalid_state, "transfer engine is not idle", {}, std::nullopt});
    }
    progress_ = {};
    progress_.state = TransferState::preparing;
    progress_.messages_total = messages.size();
    for (const auto& message : messages) progress_.bytes_total += message_size(message);
    result_ = {TransferState::preparing, progress_, std::nullopt};

    if (cancel_requested_.load(std::memory_order_acquire)) {
        progress_.state = TransferState::cancelled;
        result_ = {TransferState::cancelled, progress_, cancelled_error()};
        const auto snapshot = progress_;
        lock.unlock();
        if (handler) handler(snapshot);
        return midi::Result<void>::success();
    }
    worker_ = std::thread([this, messages = std::move(messages), options,
                           handler = std::move(handler)]() mutable {
        worker_main(std::move(messages), options, std::move(handler));
    });
    return midi::Result<void>::success();
}

void TransferEngine::request_cancel() noexcept {
    cancel_requested_.store(true, std::memory_order_release);
    {
        std::scoped_lock lock(mutex_);
        if (progress_.state == TransferState::running ||
            progress_.state == TransferState::preparing) {
            progress_.state = TransferState::cancelling;
        } else if (progress_.state == TransferState::idle) {
            progress_.state = TransferState::cancelled;
            result_ = {TransferState::cancelled, progress_, cancelled_error()};
        }
    }
    changed_.notify_all();
}

TransferResult TransferEngine::wait() {
    if (worker_.joinable()) worker_.join();
    std::scoped_lock lock(mutex_);
    return result_;
}

TransferState TransferEngine::state() const noexcept {
    std::scoped_lock lock(mutex_);
    return progress_.state;
}

TransferProgress TransferEngine::progress() const noexcept {
    std::scoped_lock lock(mutex_);
    return progress_;
}

void TransferEngine::publish(TransferProgressHandler& handler) {
    TransferProgress snapshot;
    {
        std::scoped_lock lock(mutex_);
        snapshot = progress_;
    }
    if (handler) handler(snapshot);
}

void TransferEngine::finish(const TransferState state, std::optional<midi::MidiError> error,
                            TransferProgressHandler& handler) {
    {
        std::scoped_lock lock(mutex_);
        progress_.state = state;
        result_ = {state, progress_, std::move(error)};
    }
    publish(handler);
    changed_.notify_all();
}

void TransferEngine::worker_main(std::vector<midi::NativeMidiMessage> messages,
                                 const TransferOptions options,
                                 TransferProgressHandler handler) {
    const auto started = std::chrono::steady_clock::now();
    const auto deadline = options.timeout ?
        std::optional(started + *options.timeout) : std::nullopt;
    {
        std::scoped_lock lock(mutex_);
        progress_.state = TransferState::running;
    }
    publish(handler);

    for (std::size_t index = 0; index < messages.size(); ++index) {
        if (cancel_requested_.load(std::memory_order_acquire)) {
            finish(TransferState::cancelled, cancelled_error(), handler);
            return;
        }
        if (deadline && std::chrono::steady_clock::now() >= *deadline) {
            finish(TransferState::failed, timeout_error(), handler);
            return;
        }
        if (transport_.state() != midi::TransportState::open) {
            finish(TransferState::failed, disconnected_error(), handler);
            return;
        }
        const auto sent = transport_.send(messages[index]);
        if (!sent) {
            finish(TransferState::failed, sent.error(), handler);
            return;
        }
        {
            std::scoped_lock lock(mutex_);
            ++progress_.messages_accepted;
            progress_.bytes_accepted += message_size(messages[index]);
        }
        publish(handler);

        if (index + 1 == messages.size() || options.inter_message_delay.count() <= 0) continue;
        std::unique_lock lock(mutex_);
        ++progress_.pacing_intervals_applied;
        const auto pacing_deadline = std::chrono::steady_clock::now() +
                                     options.inter_message_delay;
        const auto wake = deadline ? (std::min)(pacing_deadline, *deadline) : pacing_deadline;
        changed_.wait_until(lock, wake, [this] {
            return cancel_requested_.load(std::memory_order_acquire);
        });
        lock.unlock();
        if (cancel_requested_.load(std::memory_order_acquire)) {
            finish(TransferState::cancelled, cancelled_error(), handler);
            return;
        }
        if (deadline && std::chrono::steady_clock::now() >= *deadline) {
            finish(TransferState::failed, timeout_error(), handler);
            return;
        }
    }
    finish(TransferState::completed, std::nullopt, handler);
}

} // namespace taureon::transfer
