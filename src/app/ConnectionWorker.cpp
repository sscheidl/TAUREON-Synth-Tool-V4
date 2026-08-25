#include "app/ConnectionWorker.hpp"

#include "core/sysex/SysEx7.hpp"

#include <exception>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

namespace taureon::app {
namespace {

midi::MidiError worker_error(std::string detail) {
    return {midi::MidiErrorCode::open_failure, std::move(detail), "connection-worker",
            std::nullopt};
}

} // namespace

struct ConnectionWorker::State {
    explicit State(std::shared_ptr<const profiles::ProfileRegistry> profile_registry)
        : sysex(std::move(profile_registry)) {}

    std::unique_ptr<midi::IMidiTransport> transport;
    std::unique_ptr<ConnectionController> controller;
    SysExTransferSession sysex;
    std::unique_ptr<transfer::TransferEngine> transfer;

    void synchronize_transfer() {
        if (!transfer) return;
        const auto progress = transfer->progress();
        sysex.set_transfer_progress(progress);
        if (progress.state != transfer::TransferState::completed &&
            progress.state != transfer::TransferState::cancelled &&
            progress.state != transfer::TransferState::failed) {
            return;
        }
        const auto result = transfer->wait();
        sysex.set_transfer_progress(result.progress, result.error);
        switch (result.state) {
        case transfer::TransferState::completed: sysex.record_log("Raw Send completed"); break;
        case transfer::TransferState::cancelled: sysex.record_log("Raw Send cancelled"); break;
        case transfer::TransferState::failed: sysex.record_log("Raw Send failed"); break;
        default: break;
        }
        transfer.reset();
    }

    void stop_activity() {
        if (transfer) {
            transfer->request_cancel();
            const auto result = transfer->wait();
            sysex.set_transfer_progress(result.progress, result.error);
            sysex.record_log(result.state == transfer::TransferState::completed ?
                                 "Raw Send completed during connection close" :
                                 "Raw Send cancelled during connection close");
            transfer.reset();
        }
        if (sysex.receiving()) static_cast<void>(sysex.finish_receive());
    }

    SysExTransferSnapshot sysex_snapshot(
        const std::uint64_t dropped_events,
        const std::optional<std::uint64_t> last_loss_sequence) {
        synchronize_transfer();
        auto result = sysex.snapshot();
        result.application_dropped_events = dropped_events;
        result.application_last_loss_sequence = last_loss_sequence;
        if (controller) result.transmit_route = controller->snapshot().transmit_route;
        return result;
    }
};

ConnectionWorker::ConnectionWorker(TransportFactory factory,
                                   midi::MidiMessageHandler message_handler,
                                   std::shared_ptr<const profiles::ProfileRegistry> profile_registry)
    : factory_(std::move(factory)), message_handler_(std::move(message_handler)),
      profile_registry_(std::move(profile_registry)) {
    if (!factory_) throw std::invalid_argument("connection worker requires a transport factory");
    worker_ = std::thread([this] { run(); });
}

ConnectionWorker::~ConnectionWorker() {
    {
        std::scoped_lock lock(mutex_);
        stopping_ = true;
    }
    changed_.notify_one();
    if (worker_.joinable()) worker_.join();
}

void ConnectionWorker::enqueue(Command command) {
    {
        std::scoped_lock lock(mutex_);
        if (stopping_) throw std::runtime_error("connection worker is stopping");
        commands_.push(std::move(command));
    }
    changed_.notify_one();
}

void ConnectionWorker::enqueue_stream_event(const midi::MidiStreamEvent& event) noexcept {
    try {
        std::scoped_lock lock(mutex_);
        if (stopping_) return;
        constexpr std::size_t stream_capacity = 8192;
        while (pending_stream_loss_markers_ > 0 && stream_events_.size() < stream_capacity) {
            stream_events_.push_back(pending_loss_marker_locked());
            consume_pending_loss_marker_locked();
        }
        if (stream_events_.size() == stream_capacity) {
            record_stream_drop_locked(event);
        } else {
            stream_events_.push_back(event);
        }
        changed_.notify_one();
    } catch (...) {
        std::scoped_lock lock(mutex_);
        record_stream_drop_locked(event);
        changed_.notify_one();
    }
}

void ConnectionWorker::record_stream_drop_locked(const midi::MidiStreamEvent& event) noexcept {
    pending_stream_loss_sequence_ = event.sequence;
    if (const auto* loss = std::get_if<midi::MidiDataLossEvent>(&event.payload)) {
        pending_stream_loss_backend_ = loss->backend;
        pending_stream_loss_group_ = loss->group;
    } else {
        const auto& message = std::get<midi::NativeMidiMessage>(event.payload);
        pending_stream_loss_backend_ = message.backend;
        pending_stream_loss_group_.reset();
        if (const auto* ump = std::get_if<midi::UmpNativeMessage>(&message.data);
            ump != nullptr && !ump->words.empty()) {
            pending_stream_loss_group_ =
                static_cast<std::uint8_t>((ump->words.front() >> 24u) & 0x0fu);
        }
    }
    ++pending_stream_loss_markers_;
    dropped_stream_events_.fetch_add(1, std::memory_order_relaxed);
}

midi::MidiStreamEvent ConnectionWorker::pending_loss_marker_locked() const {
    return {pending_stream_loss_sequence_,
            midi::MidiDataLossEvent{pending_stream_loss_backend_,
                                    midi::MidiDataLossReason::queue_overflow, true,
                                    pending_stream_loss_group_,
                                    "Stage 5 application stream queue overflow",
                                    std::nullopt}};
}

void ConnectionWorker::consume_pending_loss_marker_locked() noexcept {
    last_synthetic_loss_sequence_ = pending_stream_loss_sequence_;
    --pending_stream_loss_markers_;
}

std::optional<std::uint64_t> ConnectionWorker::last_synthetic_loss_sequence() const noexcept {
    return last_synthetic_loss_sequence_;
}

void ConnectionWorker::drain_stream_events(State& state) {
    for (;;) {
        std::optional<midi::MidiStreamEvent> event;
        {
            std::scoped_lock lock(mutex_);
            if (!stream_events_.empty()) {
                event = std::move(stream_events_.front());
                stream_events_.pop_front();
            } else if (pending_stream_loss_markers_ > 0) {
                event = pending_loss_marker_locked();
                consume_pending_loss_marker_locked();
            } else {
                break;
            }
        }
        state.sysex.consume(*event);
    }
}

std::future<midi::Result<ConnectionSnapshot>> ConnectionWorker::select_backend(
    const midi::MidiBackend backend) {
    auto promise = std::make_shared<std::promise<midi::Result<ConnectionSnapshot>>>();
    auto future = promise->get_future();
    enqueue([this, promise, backend](State& state) {
        try {
            if (state.controller) {
                drain_stream_events(state);
                state.stop_activity();
                const auto close_result = state.controller->disconnect();
                if (!close_result) {
                    promise->set_value(midi::Result<ConnectionSnapshot>::failure(close_result.error()));
                    return;
                }
            }
            state.controller.reset();
            state.transport.reset();
            state.transport = factory_(backend);
            if (!state.transport || state.transport->backend() != backend) {
                throw std::runtime_error("transport factory returned no matching backend");
            }
            state.transport->set_message_handler(message_handler_);
            state.transport->set_stream_event_handler(
                [this](const midi::MidiStreamEvent& event) { enqueue_stream_event(event); });
            state.controller = std::make_unique<ConnectionController>(*state.transport);
            const auto refresh_result = state.controller->refresh();
            if (!refresh_result) {
                promise->set_value(midi::Result<ConnectionSnapshot>::failure(refresh_result.error()));
                return;
            }
            promise->set_value(
                midi::Result<ConnectionSnapshot>::success(state.controller->snapshot()));
        } catch (const std::exception& error) {
            promise->set_value(midi::Result<ConnectionSnapshot>::failure(worker_error(error.what())));
        }
    });
    return future;
}

std::future<midi::Result<ConnectionSnapshot>> ConnectionWorker::connect(
    std::optional<midi::MidiRouteIdentity> receive_route,
    std::optional<midi::MidiRouteIdentity> transmit_route) {
    auto promise = std::make_shared<std::promise<midi::Result<ConnectionSnapshot>>>();
    auto future = promise->get_future();
    enqueue([promise, receive_route = std::move(receive_route),
             transmit_route = std::move(transmit_route)](State& state) {
        if (!state.controller) {
            promise->set_value(midi::Result<ConnectionSnapshot>::failure(
                worker_error("select a concrete backend before connecting")));
            return;
        }
        state.controller->clear_route(midi::MidiDirection::input);
        state.controller->clear_route(midi::MidiDirection::output);
        if (receive_route) {
            const auto selected = state.controller->select_route(*receive_route);
            if (!selected) {
                promise->set_value(midi::Result<ConnectionSnapshot>::failure(selected.error()));
                return;
            }
        }
        if (transmit_route) {
            const auto selected = state.controller->select_route(*transmit_route);
            if (!selected) {
                promise->set_value(midi::Result<ConnectionSnapshot>::failure(selected.error()));
                return;
            }
        }
        const auto opened = state.controller->connect_selected();
        if (!opened) {
            promise->set_value(midi::Result<ConnectionSnapshot>::failure(opened.error()));
            return;
        }
        promise->set_value(midi::Result<ConnectionSnapshot>::success(state.controller->snapshot()));
    });
    return future;
}

std::future<midi::Result<ConnectionSnapshot>> ConnectionWorker::disconnect() {
    auto promise = std::make_shared<std::promise<midi::Result<ConnectionSnapshot>>>();
    auto future = promise->get_future();
    enqueue([promise, this](State& state) {
        if (!state.controller) {
            promise->set_value(midi::Result<ConnectionSnapshot>::failure(
                worker_error("no backend is selected")));
            return;
        }
        drain_stream_events(state);
        state.stop_activity();
        const auto closed = state.controller->disconnect();
        if (!closed) {
            promise->set_value(midi::Result<ConnectionSnapshot>::failure(closed.error()));
            return;
        }
        promise->set_value(midi::Result<ConnectionSnapshot>::success(state.controller->snapshot()));
    });
    return future;
}

std::future<midi::Result<SysExTransferSnapshot>> ConnectionWorker::load_sysex(
    std::filesystem::path path) {
    auto promise = std::make_shared<std::promise<midi::Result<SysExTransferSnapshot>>>();
    auto future = promise->get_future();
    enqueue([promise, path = std::move(path), this](State& state) {
        state.synchronize_transfer();
        if (state.transfer) {
            promise->set_value(midi::Result<SysExTransferSnapshot>::failure(
                worker_error("cancel the active transfer before loading another file")));
            return;
        }
        const auto loaded = state.sysex.load_file(path);
        if (!loaded) {
            promise->set_value(midi::Result<SysExTransferSnapshot>::failure(loaded.error()));
            return;
        }
        promise->set_value(midi::Result<SysExTransferSnapshot>::success(
            state.sysex_snapshot(dropped_stream_events_.load(std::memory_order_relaxed),
                                 last_synthetic_loss_sequence())));
    });
    return future;
}

std::future<midi::Result<SysExTransferSnapshot>> ConnectionWorker::begin_sysex_receive() {
    auto promise = std::make_shared<std::promise<midi::Result<SysExTransferSnapshot>>>();
    auto future = promise->get_future();
    enqueue([promise, this](State& state) {
        drain_stream_events(state);
        state.synchronize_transfer();
        if (state.transfer) {
            promise->set_value(midi::Result<SysExTransferSnapshot>::failure(
                worker_error("cancel the active Raw Send before receiving")));
            return;
        }
        if (!state.controller) {
            promise->set_value(midi::Result<SysExTransferSnapshot>::failure(
                worker_error("connect an exact RX route before receiving")));
            return;
        }
        const auto connection = state.controller->snapshot();
        if (connection.state != ConnectionPresentationState::connected ||
            !connection.receive_route) {
            promise->set_value(midi::Result<SysExTransferSnapshot>::failure(
                worker_error("connect an exact RX route before receiving")));
            return;
        }
        const auto started = state.sysex.begin_receive();
        if (!started) {
            promise->set_value(midi::Result<SysExTransferSnapshot>::failure(started.error()));
            return;
        }
        promise->set_value(midi::Result<SysExTransferSnapshot>::success(
            state.sysex_snapshot(dropped_stream_events_.load(std::memory_order_relaxed),
                                 last_synthetic_loss_sequence())));
    });
    return future;
}

std::future<midi::Result<SysExTransferSnapshot>> ConnectionWorker::finish_sysex_receive() {
    auto promise = std::make_shared<std::promise<midi::Result<SysExTransferSnapshot>>>();
    auto future = promise->get_future();
    enqueue([promise, this](State& state) {
        drain_stream_events(state);
        const auto finished = state.sysex.finish_receive();
        if (!finished) {
            promise->set_value(midi::Result<SysExTransferSnapshot>::failure(finished.error()));
            return;
        }
        promise->set_value(midi::Result<SysExTransferSnapshot>::success(
            state.sysex_snapshot(dropped_stream_events_.load(std::memory_order_relaxed),
                                 last_synthetic_loss_sequence())));
    });
    return future;
}

std::future<midi::Result<SysExTransferSnapshot>> ConnectionWorker::clear_sysex() {
    auto promise = std::make_shared<std::promise<midi::Result<SysExTransferSnapshot>>>();
    auto future = promise->get_future();
    enqueue([promise, this](State& state) {
        state.synchronize_transfer();
        if (state.transfer) {
            promise->set_value(midi::Result<SysExTransferSnapshot>::failure(
                worker_error("cancel the active transfer before clearing")));
            return;
        }
        const auto cleared = state.sysex.clear();
        if (!cleared) {
            promise->set_value(midi::Result<SysExTransferSnapshot>::failure(cleared.error()));
            return;
        }
        promise->set_value(midi::Result<SysExTransferSnapshot>::success(
            state.sysex_snapshot(dropped_stream_events_.load(std::memory_order_relaxed),
                                 last_synthetic_loss_sequence())));
    });
    return future;
}

std::future<midi::Result<SysExTransferSnapshot>> ConnectionWorker::save_received_sysex(
    std::filesystem::path path) {
    auto promise = std::make_shared<std::promise<midi::Result<SysExTransferSnapshot>>>();
    auto future = promise->get_future();
    enqueue([promise, path = std::move(path), this](State& state) {
        state.synchronize_transfer();
        if (state.transfer) {
            promise->set_value(midi::Result<SysExTransferSnapshot>::failure(
                worker_error("cancel the active Raw Send before saving received data")));
            return;
        }
        const auto saved = state.sysex.save_verified_received(path);
        if (!saved) {
            promise->set_value(midi::Result<SysExTransferSnapshot>::failure(saved.error()));
            return;
        }
        state.sysex.record_log("Saved verified received data to a new file");
        promise->set_value(midi::Result<SysExTransferSnapshot>::success(
            state.sysex_snapshot(dropped_stream_events_.load(std::memory_order_relaxed),
                                 last_synthetic_loss_sequence())));
    });
    return future;
}

std::future<midi::Result<SysExTransferSnapshot>> ConnectionWorker::start_raw_sysex_send(
    const std::chrono::milliseconds inter_frame_delay) {
    auto promise = std::make_shared<std::promise<midi::Result<SysExTransferSnapshot>>>();
    auto future = promise->get_future();
    enqueue([promise, inter_frame_delay, this](State& state) {
        state.synchronize_transfer();
        if (state.transfer) {
            promise->set_value(midi::Result<SysExTransferSnapshot>::failure(
                worker_error("a SysEx transfer is already active")));
            return;
        }
        if (!state.transport || !state.controller) {
            promise->set_value(midi::Result<SysExTransferSnapshot>::failure(
                worker_error("connect an exact TX route before Raw Send")));
            return;
        }
        const auto connection = state.controller->snapshot();
        if (connection.state != ConnectionPresentationState::connected ||
            !connection.transmit_route) {
            promise->set_value(midi::Result<SysExTransferSnapshot>::failure(
                worker_error("connect an exact TX route before Raw Send")));
            return;
        }
        std::optional<std::uint8_t> group;
        if (const auto* wms = std::get_if<midi::WmsRouteIdentity>(
                &connection.transmit_route->native)) {
            group = wms->group;
        }
        auto messages = state.sysex.build_raw_send(state.transport->backend(), group);
        if (!messages) {
            promise->set_value(midi::Result<SysExTransferSnapshot>::failure(messages.error()));
            return;
        }
        state.transfer = std::make_unique<transfer::TransferEngine>(*state.transport);
        const auto started = state.transfer->start(std::move(messages.value()),
                                                   {inter_frame_delay, std::nullopt});
        if (!started) {
            state.transfer.reset();
            promise->set_value(midi::Result<SysExTransferSnapshot>::failure(started.error()));
            return;
        }
        state.sysex.record_log("Raw Send started by explicit user action");
        state.sysex.set_transfer_progress(state.transfer->progress());
        promise->set_value(midi::Result<SysExTransferSnapshot>::success(
            state.sysex_snapshot(dropped_stream_events_.load(std::memory_order_relaxed),
                                 last_synthetic_loss_sequence())));
    });
    return future;
}

std::future<midi::Result<SysExTransferSnapshot>> ConnectionWorker::cancel_sysex_transfer() {
    auto promise = std::make_shared<std::promise<midi::Result<SysExTransferSnapshot>>>();
    auto future = promise->get_future();
    enqueue([promise, this](State& state) {
        state.synchronize_transfer();
        if (!state.transfer) {
            promise->set_value(midi::Result<SysExTransferSnapshot>::success(
                state.sysex_snapshot(dropped_stream_events_.load(std::memory_order_relaxed),
                                     last_synthetic_loss_sequence())));
            return;
        }
        state.transfer->request_cancel();
        state.sysex.set_transfer_progress(state.transfer->progress());
        promise->set_value(midi::Result<SysExTransferSnapshot>::success(
            state.sysex_snapshot(dropped_stream_events_.load(std::memory_order_relaxed),
                                 last_synthetic_loss_sequence())));
    });
    return future;
}

std::future<midi::Result<SysExTransferSnapshot>> ConnectionWorker::sysex_snapshot() {
    auto promise = std::make_shared<std::promise<midi::Result<SysExTransferSnapshot>>>();
    auto future = promise->get_future();
    enqueue([promise, this](State& state) {
        drain_stream_events(state);
        promise->set_value(midi::Result<SysExTransferSnapshot>::success(
            state.sysex_snapshot(dropped_stream_events_.load(std::memory_order_relaxed),
                                 last_synthetic_loss_sequence())));
    });
    return future;
}

std::future<midi::Result<ConnectionSnapshot>> ConnectionWorker::snapshot() {
    auto promise = std::make_shared<std::promise<midi::Result<ConnectionSnapshot>>>();
    auto future = promise->get_future();
    enqueue([promise](State& state) {
        if (!state.controller) {
            promise->set_value(midi::Result<ConnectionSnapshot>::failure(
                worker_error("no backend is selected")));
            return;
        }
        promise->set_value(midi::Result<ConnectionSnapshot>::success(state.controller->snapshot()));
    });
    return future;
}

void ConnectionWorker::run() {
    State state(profile_registry_);
    for (;;) {
        Command command;
        std::optional<midi::MidiStreamEvent> stream_event;
        {
            std::unique_lock lock(mutex_);
            changed_.wait(lock, [this] {
                return stopping_ || !commands_.empty() || !stream_events_.empty() ||
                       pending_stream_loss_markers_ > 0;
            });
            if (!commands_.empty()) {
                command = std::move(commands_.front());
                commands_.pop();
            } else if (!stream_events_.empty()) {
                stream_event = std::move(stream_events_.front());
                stream_events_.pop_front();
            } else if (pending_stream_loss_markers_ > 0) {
                stream_event = pending_loss_marker_locked();
                consume_pending_loss_marker_locked();
            } else if (stopping_) {
                break;
            }
        }
        if (command) command(state);
        else if (stream_event) state.sysex.consume(*stream_event);
    }
    state.stop_activity();
    if (state.controller) static_cast<void>(state.controller->disconnect());
    state.controller.reset();
    state.transport.reset();
}

} // namespace taureon::app
