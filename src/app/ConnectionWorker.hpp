#pragma once

#include "app/ConnectionController.hpp"

#include <condition_variable>
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
                              midi::MidiMessageHandler message_handler = {});
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

private:
    struct State;
    using Command = std::function<void(State&)>;

    void enqueue(Command command);
    void run();

    TransportFactory factory_;
    midi::MidiMessageHandler message_handler_;
    std::mutex mutex_;
    std::condition_variable changed_;
    std::queue<Command> commands_;
    bool stopping_{};
    std::thread worker_;
};

} // namespace taureon::app
