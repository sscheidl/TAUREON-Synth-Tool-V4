#pragma once

#include <taureon/transports/IMidiTransport.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace taureon::transfer {

enum class TransferState {
    idle,
    preparing,
    running,
    cancelling,
    completed,
    cancelled,
    failed,
};

struct TransferProgress {
    TransferState state{TransferState::idle};
    std::uint64_t messages_total{};
    std::uint64_t messages_accepted{};
    std::uint64_t bytes_total{};
    std::uint64_t bytes_accepted{};
    std::uint64_t pacing_intervals_applied{};

    bool operator==(const TransferProgress&) const = default;
};

struct TransferOptions {
    std::chrono::milliseconds inter_message_delay{};
    std::optional<std::chrono::milliseconds> timeout;
};

struct TransferResult {
    TransferState state{TransferState::idle};
    TransferProgress progress;
    std::optional<midi::MidiError> error;
};

using TransferProgressHandler = std::function<void(const TransferProgress&)>;

class TransferEngine {
public:
    // One transfer attempt per instance. The transport must outlive this engine.
    // The owner serializes start(), wait(), and destruction on one thread.
    // request_cancel(), state(), and progress() may be called from other threads
    // while the object is alive; all such calls must finish before destruction.
    explicit TransferEngine(midi::IMidiTransport& transport);
    ~TransferEngine();

    TransferEngine(const TransferEngine&) = delete;
    TransferEngine& operator=(const TransferEngine&) = delete;

    // A cancellation before start() is terminal; start() then returns invalid_state.
    // Progress handlers run on the engine worker thread. They may request_cancel(),
    // state(), or progress(), but must not throw, wait(), or destroy the engine.
    [[nodiscard]] midi::Result<void> start(std::vector<midi::NativeMidiMessage> messages,
                                           TransferOptions options = {},
                                           TransferProgressHandler handler = {});
    void request_cancel() noexcept;
    // Joins the worker. A blocking transport send can extend this wait beyond
    // TransferOptions::timeout, which is checked only between send() calls.
    [[nodiscard]] TransferResult wait();
    [[nodiscard]] TransferState state() const noexcept;
    [[nodiscard]] TransferProgress progress() const noexcept;

private:
    void worker_main(std::vector<midi::NativeMidiMessage> messages, TransferOptions options,
                     TransferProgressHandler handler);
    void publish(TransferProgressHandler& handler);
    void finish(TransferState state, std::optional<midi::MidiError> error,
                TransferProgressHandler& handler);

    midi::IMidiTransport& transport_;
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::thread worker_;
    std::atomic<bool> cancel_requested_{false};
    TransferProgress progress_;
    TransferResult result_;
};

} // namespace taureon::transfer
