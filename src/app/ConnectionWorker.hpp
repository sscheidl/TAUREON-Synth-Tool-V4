#pragma once

#include "app/ConnectionController.hpp"
#include "app/SysExTransferSession.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>

namespace taureon::app {

using TransportFactory =
    std::function<std::unique_ptr<midi::IMidiTransport>(midi::MidiBackend)>;

class ConnectionWorker {
public:
    explicit ConnectionWorker(TransportFactory factory,
                              midi::MidiMessageHandler message_handler = {},
                              std::shared_ptr<const profiles::ProfileRegistry> profile_registry = {});
    ~ConnectionWorker();

    ConnectionWorker(const ConnectionWorker&) = delete;
    ConnectionWorker& operator=(const ConnectionWorker&) = delete;

    [[nodiscard]] std::future<midi::Result<ConnectionSnapshot>> select_backend(
        midi::MidiBackend backend);
    [[nodiscard]] std::future<midi::Result<ConnectionSnapshot>> connect(
        std::optional<midi::MidiRouteIdentity> receive_route,
        std::optional<midi::MidiRouteIdentity> transmit_route);
    [[nodiscard]] std::future<midi::Result<ConnectionSnapshot>> disconnect();
    [[nodiscard]] std::future<midi::Result<ConnectionSnapshot>> snapshot();

    [[nodiscard]] std::future<midi::Result<SysExTransferSnapshot>> load_sysex(
        std::filesystem::path path);
    [[nodiscard]] std::future<midi::Result<SysExTransferSnapshot>> load_sysex_document(
        sysex::SyxDocument document, std::string source_name);
    [[nodiscard]] std::future<midi::Result<SysExTransferSnapshot>> begin_sysex_receive();
    [[nodiscard]] std::future<midi::Result<SysExTransferSnapshot>> finish_sysex_receive();
    [[nodiscard]] std::future<midi::Result<SysExTransferSnapshot>> clear_sysex();
    [[nodiscard]] std::future<midi::Result<SysExTransferSnapshot>> save_received_sysex(
        std::filesystem::path path);
    [[nodiscard]] std::future<midi::Result<SysExTransferSnapshot>> start_raw_sysex_send(
        std::chrono::milliseconds inter_frame_delay);
    [[nodiscard]] std::future<midi::Result<SysExTransferSnapshot>> cancel_sysex_transfer();
    [[nodiscard]] std::future<midi::Result<SysExTransferSnapshot>> sysex_snapshot();

private:
    struct State;
    using Command = std::function<void(State&)>;

    void enqueue(Command command);
    void enqueue_stream_event(const midi::MidiStreamEvent& event) noexcept;
    void record_stream_drop_locked(const midi::MidiStreamEvent& event) noexcept;
    [[nodiscard]] midi::MidiStreamEvent pending_loss_marker_locked() const;
    void consume_pending_loss_marker_locked() noexcept;
    [[nodiscard]] std::optional<std::uint64_t> last_synthetic_loss_sequence() const noexcept;
    void drain_stream_events(State& state);
    void run();

    TransportFactory factory_;
    midi::MidiMessageHandler message_handler_;
    std::shared_ptr<const profiles::ProfileRegistry> profile_registry_;
    std::mutex mutex_;
    std::condition_variable changed_;
    std::queue<Command> commands_;
    std::deque<midi::MidiStreamEvent> stream_events_;
    std::uint64_t pending_stream_loss_markers_{};
    std::uint64_t pending_stream_loss_sequence_{};
    midi::MidiBackend pending_stream_loss_backend_{midi::MidiBackend::winmm};
    std::optional<std::uint8_t> pending_stream_loss_group_;
    std::atomic<std::uint64_t> dropped_stream_events_{};
    std::optional<std::uint64_t> last_synthetic_loss_sequence_;
    bool stopping_{};
    std::thread worker_;
};

} // namespace taureon::app
