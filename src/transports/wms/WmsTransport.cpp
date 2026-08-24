#include "WmsTransport.hpp"

#include "core/midi/RouteResolver.hpp"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.h>

#include "winmidi/init/Microsoft.Windows.Devices.Midi2.Initialization.hpp"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace taureon::midi::wms {
namespace native = winrt::Microsoft::Windows::Devices::Midi2;
namespace init = Microsoft::Windows::Devices::Midi2::Initialization;

namespace {

MidiError wms_error(const MidiErrorCode code, std::string message,
                    const winrt::hresult_error* error = nullptr) {
    if (error == nullptr) return {code, std::move(message), "WMS", std::nullopt};
    return {code, std::move(message) + ": " + winrt::to_string(error->message()), "WMS",
            static_cast<std::int64_t>(error->code())};
}

MidiError resolution_error(const RouteResolutionStatus status) {
    if (status == RouteResolutionStatus::missing) {
        return {MidiErrorCode::endpoint_missing, "WMS route is missing", {}, std::nullopt};
    }
    if (status == RouteResolutionStatus::ambiguous) {
        return {MidiErrorCode::endpoint_ambiguous, "WMS route is ambiguous", {}, std::nullopt};
    }
    return {MidiErrorCode::invalid_route, "WMS route is invalid", {}, std::nullopt};
}

} // namespace

struct WmsTransport::Impl {
    struct CallbackGate {
        std::mutex mutex;
        Impl* target{};
        std::atomic<bool> accepting{false};
        std::atomic<std::uint64_t> native_callbacks{0};
        std::atomic<std::uint64_t> late_callbacks{0};
    };

    struct ConnectionState {
        std::string endpoint_id;
        native::MidiEndpointConnection connection{nullptr};
        winrt::event_token receive_token{};
    };

    std::mutex queue_mutex;
    std::condition_variable queue_changed;
    std::deque<std::function<void()>> commands;
    std::deque<NativeMidiMessage> received_messages;
    bool stopping{};
    bool startup_complete{};
    std::thread worker;

    bool apartment_initialized{};
    std::shared_ptr<init::MidiDesktopAppSdkInitializer> initializer;
    std::optional<MidiError> startup_error;
    native::MidiSession session{nullptr};
    std::vector<ConnectionState> connections;
    native::MidiEndpointConnection transmit_connection{nullptr};
    std::optional<std::uint8_t> transmit_group;
    std::shared_ptr<CallbackGate> callback_gate{std::make_shared<CallbackGate>()};
    std::atomic<std::uint64_t> delivered_messages{0};
    std::atomic<std::uint64_t> transmitted_messages{0};
    std::atomic<std::uint64_t> dropped_messages{0};
    std::atomic<std::uint64_t> queue_high_water_mark{0};

    std::mutex handler_mutex;
    MidiMessageHandler message_handler;
    EndpointChangeHandler endpoint_handler;

    Impl() : worker([this] { worker_main(); }) {
        callback_gate->target = this;
        std::unique_lock lock(queue_mutex);
        queue_changed.wait(lock, [this] { return startup_complete; });
    }

    ~Impl() {
        static_cast<void>(invoke([this] { return close_on_worker(); }));
        {
            std::scoped_lock lock(callback_gate->mutex);
            callback_gate->target = nullptr;
        }
        {
            std::scoped_lock lock(queue_mutex);
            stopping = true;
        }
        queue_changed.notify_all();
        if (worker.joinable()) worker.join();
    }

    void enqueue_received(NativeMidiMessage message) {
        std::scoped_lock lock(queue_mutex);
        constexpr std::size_t receive_capacity = 1024;
        if (received_messages.size() >= receive_capacity) {
            ++dropped_messages;
            return;
        }
        received_messages.push_back(std::move(message));
        const auto depth = static_cast<std::uint64_t>(received_messages.size());
        auto high_water = queue_high_water_mark.load(std::memory_order_relaxed);
        while (depth > high_water &&
               !queue_high_water_mark.compare_exchange_weak(
                   high_water, depth, std::memory_order_relaxed)) {}
        queue_changed.notify_one();
    }

    void process_received(NativeMidiMessage message) {
        MidiMessageHandler handler;
        {
            std::scoped_lock lock(handler_mutex);
            handler = message_handler;
        }
        if (handler) {
            handler(message);
            ++delivered_messages;
        }
    }

    template <typename Function>
    auto invoke(Function&& function) -> decltype(function()) {
        using Return = decltype(function());
        auto task = std::make_shared<std::packaged_task<Return()>>(std::forward<Function>(function));
        auto future = task->get_future();
        {
            std::scoped_lock lock(queue_mutex);
            commands.emplace_back([task] { (*task)(); });
        }
        queue_changed.notify_one();
        return future.get();
    }

    void initialize_runtime() {
        try {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            apartment_initialized = true;
            initializer = std::make_shared<init::MidiDesktopAppSdkInitializer>();
            if (!initializer->IsServiceInstalled() || !initializer->InitializeSdkRuntime() ||
                !initializer->CheckForMinimumRequiredSdkVersion(1, 0, 17) ||
                !initializer->EnsureServiceAvailable()) {
                startup_error = wms_error(MidiErrorCode::backend_unavailable,
                                          "WMS runtime initialization failed");
            }
        } catch (const winrt::hresult_error& error) {
            startup_error = wms_error(MidiErrorCode::backend_unavailable,
                                      "WMS runtime initialization failed", &error);
        } catch (const std::exception& error) {
            startup_error = wms_error(MidiErrorCode::backend_unavailable, error.what());
        }
    }

    void shutdown_runtime() noexcept {
        if (initializer) {
            initializer->ShutdownSdkRuntime();
            initializer.reset();
        }
        if (apartment_initialized) {
            winrt::uninit_apartment();
            apartment_initialized = false;
        }
    }

    void worker_main() {
        initialize_runtime();
        {
            std::scoped_lock lock(queue_mutex);
            startup_complete = true;
        }
        queue_changed.notify_all();

        for (;;) {
            std::function<void()> command;
            std::optional<NativeMidiMessage> received;
            {
                std::unique_lock lock(queue_mutex);
                queue_changed.wait(lock, [this] {
                    return stopping || !commands.empty() || !received_messages.empty();
                });
                if (!commands.empty()) {
                    command = std::move(commands.front());
                    commands.pop_front();
                } else if (!received_messages.empty()) {
                    received = std::move(received_messages.front());
                    received_messages.pop_front();
                } else if (stopping) {
                    break;
                }
            }
            if (command) command();
            else if (received) process_received(std::move(*received));
        }
        static_cast<void>(close_on_worker());
        shutdown_runtime();
    }

    Result<void> require_runtime() const {
        if (startup_error) return Result<void>::failure(*startup_error);
        return Result<void>::success();
    }

    void append_route(std::vector<MidiEndpointDescriptor>& routes,
                      const native::MidiEndpointDeviceInformation& endpoint,
                      const std::uint8_t group, const MidiDirection direction) const {
        const auto endpoint_id = winrt::to_string(endpoint.EndpointDeviceId());
        const auto name = winrt::to_string(endpoint.Name());
        routes.push_back({{MidiBackend::windows_midi_services, direction,
                           WmsRouteIdentity{endpoint_id, group}},
                          name, MidiProtocol::ump,
                          {direction == MidiDirection::input, direction == MidiDirection::output,
                           true, true},
                          std::nullopt, std::nullopt});
    }

    Result<std::vector<MidiEndpointDescriptor>> enumerate_on_worker() const {
        const auto runtime = require_runtime();
        if (!runtime) {
            return Result<std::vector<MidiEndpointDescriptor>>::failure(runtime.error());
        }
        try {
            const auto filters = native::MidiEndpointDeviceInformationFilters::AllStandardEndpoints |
                                 native::MidiEndpointDeviceInformationFilters::DiagnosticLoopback |
                                 native::MidiEndpointDeviceInformationFilters::VirtualDeviceResponder;
            const auto endpoints = native::MidiEndpointDeviceInformation::FindAll(
                native::MidiEndpointDeviceInformationSortOrder::EndpointDeviceId, filters);
            std::vector<MidiEndpointDescriptor> routes;
            for (const auto& endpoint : endpoints) {
                for (const auto& block : endpoint.GetGroupTerminalBlocks()) {
                    const auto first_group = block.FirstGroup().Index();
                    for (std::uint8_t offset = 0; offset < block.GroupCount(); ++offset) {
                        const auto group = static_cast<std::uint8_t>(first_group + offset);
                        if (block.Direction() ==
                            native::MidiGroupTerminalBlockDirection::BlockInput) {
                            append_route(routes, endpoint, group, MidiDirection::input);
                        } else if (block.Direction() ==
                                   native::MidiGroupTerminalBlockDirection::BlockOutput) {
                            append_route(routes, endpoint, group, MidiDirection::output);
                        } else {
                            append_route(routes, endpoint, group, MidiDirection::input);
                            append_route(routes, endpoint, group, MidiDirection::output);
                        }
                    }
                }
            }
            return Result<std::vector<MidiEndpointDescriptor>>::success(std::move(routes));
        } catch (const winrt::hresult_error& error) {
            return Result<std::vector<MidiEndpointDescriptor>>::failure(
                wms_error(MidiErrorCode::native_api_error, "WMS endpoint enumeration failed", &error));
        }
    }

    Result<MidiEndpointDescriptor> resolve_endpoint(
        const MidiRouteIdentity& route,
        const std::vector<MidiEndpointDescriptor>& available) const {
        if (route.backend != MidiBackend::windows_midi_services) {
            return Result<MidiEndpointDescriptor>::failure(
                {MidiErrorCode::invalid_route, "route is not WMS", {}, std::nullopt});
        }
        const auto resolved = resolve_route(route, available);
        if (resolved.status != RouteResolutionStatus::exact) {
            return Result<MidiEndpointDescriptor>::failure(resolution_error(resolved.status));
        }
        return Result<MidiEndpointDescriptor>::success(*resolved.endpoint);
    }

    Result<void> open_on_worker(const MidiConnectionRequest& request) {
        const auto runtime = require_runtime();
        if (!runtime) return runtime;
        const auto available = enumerate_on_worker();
        if (!available) return Result<void>::failure(available.error());

        std::vector<std::string> endpoint_ids;
        std::optional<std::string> receive_endpoint_id;
        std::optional<std::uint8_t> receive_group;
        std::optional<std::string> transmit_endpoint_id;
        const auto validate = [&](const std::optional<MidiRouteIdentity>& route,
                                  const MidiDirection direction) -> Result<void> {
            if (!route) return Result<void>::success();
            if (route->direction != direction) {
                return Result<void>::failure(
                    {MidiErrorCode::invalid_route, "WMS route direction mismatch", {}, std::nullopt});
            }
            const auto resolved = resolve_endpoint(*route, available.value());
            if (!resolved) return Result<void>::failure(resolved.error());
            const auto* native_identity = std::get_if<WmsRouteIdentity>(&route->native);
            if (native_identity == nullptr) {
                return Result<void>::failure(
                    {MidiErrorCode::invalid_route, "invalid WMS identity variant", {}, std::nullopt});
            }
            if (std::find(endpoint_ids.begin(), endpoint_ids.end(),
                          native_identity->endpoint_device_id) == endpoint_ids.end()) {
                endpoint_ids.push_back(native_identity->endpoint_device_id);
            }
            if (direction == MidiDirection::input) {
                receive_endpoint_id = native_identity->endpoint_device_id;
                receive_group = native_identity->group;
            } else {
                transmit_endpoint_id = native_identity->endpoint_device_id;
                transmit_group = native_identity->group;
            }
            return Result<void>::success();
        };

        const auto rx = validate(request.receive_route, MidiDirection::input);
        if (!rx) return rx;
        const auto tx = validate(request.transmit_route, MidiDirection::output);
        if (!tx) return tx;

        try {
            session = native::MidiSession::Create(L"TAUREON V4 WMS transport");
            for (const auto& endpoint_id : endpoint_ids) {
                auto connection = session.CreateEndpointConnection(winrt::to_hstring(endpoint_id));
                winrt::event_token receive_token{};
                if (receive_endpoint_id && endpoint_id == *receive_endpoint_id) {
                    const auto gate = callback_gate;
                    const auto selected_group = *receive_group;
                    receive_token = connection.MessageReceived(
                        [gate, selected_group](native::IMidiMessageReceivedEventSource const&,
                                               native::MidiMessageReceivedEventArgs const& args) {
                            ++gate->native_callbacks;
                            if (!gate->accepting.load(std::memory_order_acquire)) {
                                ++gate->late_callbacks;
                                return;
                            }
                            try {
                                const auto packet = args.GetMessagePacket();
                                const auto native_words = packet.GetAllWords();
                                std::vector<std::uint32_t> words(native_words.begin(),
                                                                 native_words.end());
                                if (words.empty()) return;
                                const auto message_type =
                                    static_cast<std::uint8_t>((words.front() >> 28u) & 0x0Fu);
                                if (message_type >= 1 && message_type <= 5 &&
                                    ((words.front() >> 24u) & 0x0Fu) != selected_group) {
                                    return;
                                }
                                NativeMidiMessage message{
                                    MidiBackend::windows_midi_services,
                                    UmpNativeMessage{std::move(words)},
                                    MidiTimestamp{args.Timestamp(), "wms-native-ticks"}};
                                std::scoped_lock lock(gate->mutex);
                                if (!gate->accepting.load(std::memory_order_acquire) ||
                                    gate->target == nullptr) {
                                    ++gate->late_callbacks;
                                    return;
                                }
                                gate->target->enqueue_received(std::move(message));
                            } catch (...) {
                                std::scoped_lock lock(gate->mutex);
                                if (gate->target != nullptr) ++gate->target->dropped_messages;
                            }
                        });
                }
                if (!connection.Open()) {
                    static_cast<void>(close_on_worker());
                    return Result<void>::failure(
                        wms_error(MidiErrorCode::open_failure, "WMS endpoint open failed"));
                }
                if (transmit_endpoint_id && endpoint_id == *transmit_endpoint_id) {
                    transmit_connection = connection;
                }
                connections.push_back({endpoint_id, std::move(connection), receive_token});
            }
            callback_gate->accepting.store(true, std::memory_order_release);
            return Result<void>::success();
        } catch (const winrt::hresult_error& error) {
            static_cast<void>(close_on_worker());
            return Result<void>::failure(
                wms_error(MidiErrorCode::open_failure, "WMS session open failed", &error));
        }
    }

    Result<void> close_on_worker() {
        callback_gate->accepting.store(false, std::memory_order_release);
        {
            std::scoped_lock lock(queue_mutex);
            dropped_messages += static_cast<std::uint64_t>(received_messages.size());
            received_messages.clear();
        }
        try {
            if (session) {
                for (auto& state : connections) {
                    if (state.receive_token) {
                        state.connection.MessageReceived(state.receive_token);
                        state.receive_token = {};
                    }
                }
                transmit_connection = nullptr;
                transmit_group.reset();
                for (const auto& state : connections) {
                    session.DisconnectEndpointConnection(state.connection.ConnectionId());
                }
                connections.clear();
                session.Close();
                session = nullptr;
            }
            return Result<void>::success();
        } catch (const winrt::hresult_error& error) {
            connections.clear();
            session = nullptr;
            return Result<void>::failure(
                wms_error(MidiErrorCode::close_failure, "WMS session close failed", &error));
        }
    }

    static std::size_t ump_packet_word_count(const std::uint32_t first_word) noexcept {
        constexpr std::size_t sizes[16]{1, 1, 1, 2, 2, 4, 1, 1,
                                        2, 2, 3, 3, 4, 4, 4, 4};
        return sizes[(first_word >> 28u) & 0x0Fu];
    }

    Result<void> send_on_worker(const NativeMidiMessage& message) {
        if (!transmit_connection || !transmit_group) {
            return Result<void>::failure(
                {MidiErrorCode::invalid_state, "WMS output is not open", "WMS", std::nullopt});
        }
        if (message.backend != MidiBackend::windows_midi_services) {
            return Result<void>::failure(
                {MidiErrorCode::invalid_route, "message is not for WMS", "WMS", std::nullopt});
        }
        const auto* ump = std::get_if<UmpNativeMessage>(&message.data);
        if (ump == nullptr || ump->words.empty()) {
            return Result<void>::failure(
                {MidiErrorCode::malformed_data, "WMS requires non-empty UMP words", "WMS",
                 std::nullopt});
        }
        for (std::size_t offset = 0; offset < ump->words.size();) {
            const auto first_word = ump->words[offset];
            const auto packet_size = ump_packet_word_count(first_word);
            if (offset + packet_size > ump->words.size()) {
                return Result<void>::failure(
                    {MidiErrorCode::incomplete_data, "truncated UMP packet", "WMS",
                     std::nullopt});
            }
            const auto message_type = static_cast<std::uint8_t>((first_word >> 28u) & 0x0Fu);
            if (message_type >= 1 && message_type <= 5 &&
                ((first_word >> 24u) & 0x0Fu) != *transmit_group) {
                return Result<void>::failure(
                    {MidiErrorCode::invalid_route,
                     "UMP group does not match the selected WMS transmit route", "WMS",
                     std::nullopt});
            }
            offset += packet_size;
        }

        try {
            auto timestamp = native::MidiClock::TimestampConstantSendImmediately();
            if (message.timestamp && message.timestamp->domain == "wms-native-ticks") {
                timestamp = message.timestamp->native_value;
            }
            const auto result = transmit_connection.SendMultipleMessagesWordList(
                timestamp, ump->words);
            if (!native::MidiEndpointConnection::SendMessageSucceeded(result)) {
                return Result<void>::failure(
                    {MidiErrorCode::native_api_error, "WMS UMP send failed", "WMS",
                     static_cast<std::int64_t>(result)});
            }
            ++transmitted_messages;
            return Result<void>::success();
        } catch (const winrt::hresult_error& error) {
            return Result<void>::failure(
                wms_error(MidiErrorCode::native_api_error, "WMS UMP send failed", &error));
        }
    }
};

WmsTransport::WmsTransport() : impl_(std::make_unique<Impl>()) {}

WmsTransport::~WmsTransport() { static_cast<void>(close()); }

MidiBackend WmsTransport::backend() const noexcept {
    return MidiBackend::windows_midi_services;
}

MidiTransportCapabilities WmsTransport::capabilities() const noexcept {
    return {true, true, false, true, true};
}

MidiTransportDiagnostics WmsTransport::diagnostics() const noexcept {
    return {impl_->callback_gate->native_callbacks.load(), impl_->delivered_messages.load(),
            impl_->transmitted_messages.load(), impl_->dropped_messages.load(),
            impl_->callback_gate->late_callbacks.load(),
            impl_->queue_high_water_mark.load()};
}

TransportState WmsTransport::state() const noexcept { return lifecycle_.state(); }

Result<std::vector<MidiEndpointDescriptor>> WmsTransport::enumerate() {
    return impl_->invoke([this] { return impl_->enumerate_on_worker(); });
}

Result<void> WmsTransport::open(const MidiConnectionRequest& request) {
    if (!request.receive_route && !request.transmit_route) {
        return Result<void>::failure(
            {MidiErrorCode::invalid_route, "connection requires an RX or TX route", {}, std::nullopt});
    }
    const auto begun = lifecycle_.begin_open();
    if (!begun) return begun;
    auto result = impl_->invoke([this, request] { return impl_->open_on_worker(request); });
    if (!result) {
        lifecycle_.fail();
        return result;
    }
    result = lifecycle_.complete_open();
    if (!result) lifecycle_.fail();
    return result;
}

Result<void> WmsTransport::close() {
    const auto begun = lifecycle_.begin_close();
    if (!begun) return begun;
    if (lifecycle_.state() == TransportState::closed) return Result<void>::success();
    auto result = impl_->invoke([this] { return impl_->close_on_worker(); });
    if (!result) {
        lifecycle_.fail();
        return result;
    }
    result = lifecycle_.complete_close();
    if (!result) lifecycle_.fail();
    return result;
}

Result<void> WmsTransport::send(const NativeMidiMessage& message) {
    if (lifecycle_.state() != TransportState::open) {
        return Result<void>::failure(
            {MidiErrorCode::invalid_state, "WMS transport is not open", "WMS", std::nullopt});
    }
    return impl_->invoke([this, message] { return impl_->send_on_worker(message); });
}

void WmsTransport::set_message_handler(MidiMessageHandler handler) {
    std::scoped_lock lock(impl_->handler_mutex);
    impl_->message_handler = std::move(handler);
}

void WmsTransport::set_endpoint_change_handler(EndpointChangeHandler handler) {
    std::scoped_lock lock(impl_->handler_mutex);
    impl_->endpoint_handler = std::move(handler);
}

} // namespace taureon::midi::wms
