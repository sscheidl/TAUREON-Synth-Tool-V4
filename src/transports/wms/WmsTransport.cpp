#include "WmsTransport.hpp"

#include "core/midi/RouteResolver.hpp"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.h>

#include "winmidi/init/Microsoft.Windows.Devices.Midi2.Initialization.hpp"

#include <algorithm>
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
    std::mutex queue_mutex;
    std::condition_variable queue_changed;
    std::deque<std::function<void()>> commands;
    bool stopping{};
    bool startup_complete{};
    std::thread worker;

    bool apartment_initialized{};
    std::shared_ptr<init::MidiDesktopAppSdkInitializer> initializer;
    std::optional<MidiError> startup_error;
    native::MidiSession session{nullptr};
    std::vector<native::MidiEndpointConnection> connections;

    std::mutex handler_mutex;
    MidiMessageHandler message_handler;
    EndpointChangeHandler endpoint_handler;

    Impl() : worker([this] { worker_main(); }) {
        std::unique_lock lock(queue_mutex);
        queue_changed.wait(lock, [this] { return startup_complete; });
    }

    ~Impl() {
        static_cast<void>(invoke([this] { return close_on_worker(); }));
        {
            std::scoped_lock lock(queue_mutex);
            stopping = true;
        }
        queue_changed.notify_all();
        if (worker.joinable()) worker.join();
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
            {
                std::unique_lock lock(queue_mutex);
                queue_changed.wait(lock, [this] { return stopping || !commands.empty(); });
                if (!commands.empty()) {
                    command = std::move(commands.front());
                    commands.pop_front();
                } else if (stopping) {
                    break;
                }
            }
            if (command) command();
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
                if (!connection.Open()) {
                    static_cast<void>(close_on_worker());
                    return Result<void>::failure(
                        wms_error(MidiErrorCode::open_failure, "WMS endpoint open failed"));
                }
                connections.push_back(std::move(connection));
            }
            return Result<void>::success();
        } catch (const winrt::hresult_error& error) {
            static_cast<void>(close_on_worker());
            return Result<void>::failure(
                wms_error(MidiErrorCode::open_failure, "WMS session open failed", &error));
        }
    }

    Result<void> close_on_worker() {
        try {
            if (session) {
                for (const auto& connection : connections) {
                    session.DisconnectEndpointConnection(connection.ConnectionId());
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
};

WmsTransport::WmsTransport() : impl_(std::make_unique<Impl>()) {}

WmsTransport::~WmsTransport() { static_cast<void>(close()); }

MidiBackend WmsTransport::backend() const noexcept {
    return MidiBackend::windows_midi_services;
}

MidiTransportCapabilities WmsTransport::capabilities() const noexcept {
    return {true, true, false, true, true};
}

MidiTransportDiagnostics WmsTransport::diagnostics() const noexcept { return {}; }

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

Result<void> WmsTransport::send(const NativeMidiMessage&) {
    return Result<void>::failure(
        {MidiErrorCode::unsupported_capability,
         "production WMS message sending begins in Stage 3", {}, std::nullopt});
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
