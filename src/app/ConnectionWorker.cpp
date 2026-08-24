#include "app/ConnectionWorker.hpp"

#include <exception>
#include <stdexcept>
#include <utility>

namespace taureon::app {
namespace {

midi::MidiError worker_error(std::string detail) {
    return {midi::MidiErrorCode::open_failure, std::move(detail), "connection-worker",
            std::nullopt};
}

} // namespace

struct ConnectionWorker::State {
    std::unique_ptr<midi::IMidiTransport> transport;
    std::unique_ptr<ConnectionController> controller;
};

ConnectionWorker::ConnectionWorker(TransportFactory factory,
                                   midi::MidiMessageHandler message_handler)
    : factory_(std::move(factory)), message_handler_(std::move(message_handler)) {
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

std::future<midi::Result<ConnectionSnapshot>> ConnectionWorker::select_backend(
    const midi::MidiBackend backend) {
    auto promise = std::make_shared<std::promise<midi::Result<ConnectionSnapshot>>>();
    auto future = promise->get_future();
    enqueue([this, promise, backend](State& state) {
        try {
            if (state.controller) {
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
    enqueue([promise](State& state) {
        if (!state.controller) {
            promise->set_value(midi::Result<ConnectionSnapshot>::failure(
                worker_error("no backend is selected")));
            return;
        }
        const auto closed = state.controller->disconnect();
        if (!closed) {
            promise->set_value(midi::Result<ConnectionSnapshot>::failure(closed.error()));
            return;
        }
        promise->set_value(midi::Result<ConnectionSnapshot>::success(state.controller->snapshot()));
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
    State state;
    for (;;) {
        Command command;
        {
            std::unique_lock lock(mutex_);
            changed_.wait(lock, [this] { return stopping_ || !commands_.empty(); });
            if (commands_.empty() && stopping_) break;
            command = std::move(commands_.front());
            commands_.pop();
        }
        command(state);
    }
    if (state.controller) static_cast<void>(state.controller->disconnect());
    state.controller.reset();
    state.transport.reset();
}

} // namespace taureon::app
