#include "WinmmTransport.hpp"

#include "WinmmMidiHeader.hpp"
#include "core/midi/RouteResolver.hpp"

#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <atomic>
#include <chrono>
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

namespace taureon::midi::winmm {
namespace {

std::string utf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                                         static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                        static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

MidiError native_error(const MidiErrorCode code, std::string operation, const MMRESULT result) {
    wchar_t text[MAXERRORLENGTH]{};
    std::string message = operation + " failed";
    if (midiInGetErrorTextW(result, text, MAXERRORLENGTH) == MMSYSERR_NOERROR) {
        message += ": " + utf8(text);
    }
    return {code, std::move(message), std::move(operation), static_cast<std::int64_t>(result)};
}

MidiError resolution_error(const RouteResolutionStatus status) {
    if (status == RouteResolutionStatus::missing) {
        return {MidiErrorCode::endpoint_missing, "WinMM route is missing", {}, std::nullopt};
    }
    if (status == RouteResolutionStatus::ambiguous) {
        return {MidiErrorCode::endpoint_ambiguous, "WinMM route is ambiguous", {}, std::nullopt};
    }
    return {MidiErrorCode::invalid_route, "WinMM route is invalid", {}, std::nullopt};
}

} // namespace

struct WinmmTransport::Impl {
    struct CallbackEvent {
        UINT message{};
        DWORD_PTR parameter{};
    };

    NativeWinmmHeaderApi header_api;
    HMIDIIN input_handle{};
    HMIDIOUT output_handle{};
    std::vector<std::unique_ptr<WinmmInputBuffer>> input_buffers;
    std::atomic<bool> accepting_callbacks{false};
    std::atomic<std::uint64_t> native_callbacks{0};
    std::atomic<std::uint64_t> delivered_messages{0};
    std::atomic<std::uint64_t> dropped_callbacks{0};
    std::atomic<std::uint64_t> callbacks_after_acceptance_closed{0};

    std::mutex queue_mutex;
    std::condition_variable queue_changed;
    std::deque<std::function<void()>> commands;
    std::deque<CallbackEvent> callbacks;
    bool stopping{};
    std::thread worker;

    std::mutex handler_mutex;
    MidiMessageHandler message_handler;
    EndpointChangeHandler endpoint_handler;

    Impl() : worker([this] { worker_main(); }) {}

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

    static void CALLBACK input_callback(HMIDIIN, const UINT message, const DWORD_PTR instance,
                                        const DWORD_PTR parameter, DWORD_PTR) {
        auto* self = reinterpret_cast<Impl*>(instance);
        if (self == nullptr || (message != MIM_DATA && message != MIM_LONGDATA &&
                                message != MIM_ERROR && message != MIM_LONGERROR)) {
            return;
        }
        ++self->native_callbacks;
        const bool header_completion = message == MIM_LONGDATA || message == MIM_LONGERROR;
        if (!self->accepting_callbacks.load(std::memory_order_acquire) && !header_completion) {
            ++self->callbacks_after_acceptance_closed;
            return;
        }
        {
            std::scoped_lock lock(self->queue_mutex);
            constexpr std::size_t callback_capacity = 1024;
            if (self->callbacks.size() >= callback_capacity) {
                if (header_completion) {
                    const auto short_event = std::find_if(
                        self->callbacks.begin(), self->callbacks.end(), [](const CallbackEvent& event) {
                            return event.message == MIM_DATA || event.message == MIM_ERROR;
                        });
                    if (short_event != self->callbacks.end()) {
                        self->callbacks.erase(short_event);
                        ++self->dropped_callbacks;
                    } else {
                        ++self->dropped_callbacks;
                        return;
                    }
                } else {
                    ++self->dropped_callbacks;
                    return;
                }
            }
            self->callbacks.push_back({message, parameter});
        }
        self->queue_changed.notify_one();
    }

    void worker_main() {
        for (;;) {
            std::function<void()> command;
            std::optional<CallbackEvent> callback;
            {
                std::unique_lock lock(queue_mutex);
                queue_changed.wait(lock, [&] {
                    return stopping || !callbacks.empty() || !commands.empty();
                });
                if (!commands.empty()) {
                    command = std::move(commands.front());
                    commands.pop_front();
                } else if (!callbacks.empty()) {
                    callback = callbacks.front();
                    callbacks.pop_front();
                } else if (stopping) {
                    break;
                }
            }
            if (callback) process_callback(*callback);
            else if (command) command();
        }
    }

    void process_callback(const CallbackEvent& event) {
        if (event.message != MIM_LONGDATA && event.message != MIM_LONGERROR) return;
        auto* header = reinterpret_cast<MIDIHDR*>(event.parameter);
        for (auto& buffer : input_buffers) {
            if (buffer->native_header() != header) continue;
            buffer->mark_returned();
            if (event.message == MIM_LONGDATA && accepting_callbacks.load(std::memory_order_acquire)) {
                const auto bytes = buffer->recorded_bytes();
                MidiMessageHandler handler;
                {
                    std::scoped_lock lock(handler_mutex);
                    handler = message_handler;
                }
                if (handler && !bytes.empty()) {
                    handler({MidiBackend::winmm, Midi1NativeMessage{bytes}, std::nullopt});
                    ++delivered_messages;
                }
                static_cast<void>(buffer->submit());
            }
            queue_changed.notify_all();
            return;
        }
    }

    Result<std::vector<MidiEndpointDescriptor>> enumerate_on_worker() const {
        std::vector<MidiEndpointDescriptor> endpoints;
        for (UINT index = 0; index < midiInGetNumDevs(); ++index) {
            MIDIINCAPSW caps{};
            const auto result = midiInGetDevCapsW(index, &caps, sizeof(caps));
            if (result != MMSYSERR_NOERROR) {
                return Result<std::vector<MidiEndpointDescriptor>>::failure(
                    native_error(MidiErrorCode::native_api_error, "midiInGetDevCapsW", result));
            }
            const auto name = utf8(caps.szPname);
            endpoints.push_back({{MidiBackend::winmm, MidiDirection::input,
                                  WinmmRouteIdentity{name, caps.wMid, caps.wPid,
                                                     caps.vDriverVersion}},
                                 name, MidiProtocol::midi1,
                                 {true, false, true, false}, index, std::nullopt});
        }
        for (UINT index = 0; index < midiOutGetNumDevs(); ++index) {
            MIDIOUTCAPSW caps{};
            const auto result = midiOutGetDevCapsW(index, &caps, sizeof(caps));
            if (result != MMSYSERR_NOERROR) {
                return Result<std::vector<MidiEndpointDescriptor>>::failure(
                    native_error(MidiErrorCode::native_api_error, "midiOutGetDevCapsW", result));
            }
            const auto name = utf8(caps.szPname);
            endpoints.push_back({{MidiBackend::winmm, MidiDirection::output,
                                  WinmmRouteIdentity{name, caps.wMid, caps.wPid,
                                                     caps.vDriverVersion}},
                                 name, MidiProtocol::midi1,
                                 {false, true, true, false}, index, std::nullopt});
        }
        return Result<std::vector<MidiEndpointDescriptor>>::success(std::move(endpoints));
    }

    Result<std::uint32_t> resolve_index(const MidiRouteIdentity& route,
                                        const std::vector<MidiEndpointDescriptor>& endpoints) const {
        if (route.backend != MidiBackend::winmm) {
            return Result<std::uint32_t>::failure(
                {MidiErrorCode::invalid_route, "route is not WinMM", {}, std::nullopt});
        }
        const auto resolved = resolve_route(route, endpoints);
        if (resolved.status != RouteResolutionStatus::exact ||
            !resolved.endpoint->runtime_index_hint) {
            return Result<std::uint32_t>::failure(resolution_error(resolved.status));
        }
        return Result<std::uint32_t>::success(*resolved.endpoint->runtime_index_hint);
    }

    Result<void> open_on_worker(const MidiConnectionRequest& request) {
        const auto endpoints = enumerate_on_worker();
        if (!endpoints) return Result<void>::failure(endpoints.error());

        if (request.receive_route) {
            if (request.receive_route->direction != MidiDirection::input) {
                return Result<void>::failure(
                    {MidiErrorCode::invalid_route, "RX route direction mismatch", {}, std::nullopt});
            }
            const auto index = resolve_index(*request.receive_route, endpoints.value());
            if (!index) return Result<void>::failure(index.error());
            const auto result = midiInOpen(&input_handle, index.value(),
                                           reinterpret_cast<DWORD_PTR>(&input_callback),
                                           reinterpret_cast<DWORD_PTR>(this), CALLBACK_FUNCTION);
            if (result != MMSYSERR_NOERROR) {
                return Result<void>::failure(
                    native_error(MidiErrorCode::open_failure, "midiInOpen", result));
            }
            for (int count = 0; count < 2; ++count) {
                auto buffer = std::make_unique<WinmmInputBuffer>(header_api, input_handle, 4096);
                auto prepared = buffer->prepare();
                if (!prepared) {
                    static_cast<void>(close_on_worker());
                    return prepared;
                }
                auto submitted = buffer->submit();
                if (!submitted) {
                    static_cast<void>(close_on_worker());
                    return submitted;
                }
                input_buffers.push_back(std::move(buffer));
            }
            accepting_callbacks.store(true, std::memory_order_release);
            const auto started = midiInStart(input_handle);
            if (started != MMSYSERR_NOERROR) {
                static_cast<void>(close_on_worker());
                return Result<void>::failure(
                    native_error(MidiErrorCode::open_failure, "midiInStart", started));
            }
        }

        if (request.transmit_route) {
            if (request.transmit_route->direction != MidiDirection::output) {
                static_cast<void>(close_on_worker());
                return Result<void>::failure(
                    {MidiErrorCode::invalid_route, "TX route direction mismatch", {}, std::nullopt});
            }
            const auto index = resolve_index(*request.transmit_route, endpoints.value());
            if (!index) {
                static_cast<void>(close_on_worker());
                return Result<void>::failure(index.error());
            }
            const auto result = midiOutOpen(&output_handle, index.value(), 0, 0, CALLBACK_NULL);
            if (result != MMSYSERR_NOERROR) {
                static_cast<void>(close_on_worker());
                return Result<void>::failure(
                    native_error(MidiErrorCode::open_failure, "midiOutOpen", result));
            }
        }
        return Result<void>::success();
    }

    bool all_input_buffers_returned() const {
        for (const auto& buffer : input_buffers) {
            if (buffer->submitted()) return false;
        }
        return true;
    }

    void drain_one_callback(std::unique_lock<std::mutex>& lock) {
        const auto event = callbacks.front();
        callbacks.pop_front();
        lock.unlock();
        process_callback(event);
        lock.lock();
    }

    Result<void> wait_for_returned_buffers() {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        std::unique_lock lock(queue_mutex);
        while (!all_input_buffers_returned()) {
            if (!callbacks.empty()) {
                drain_one_callback(lock);
                continue;
            }
            if (!queue_changed.wait_until(lock, deadline, [&] { return !callbacks.empty(); })) {
                return Result<void>::failure(
                    {MidiErrorCode::close_failure,
                     "timed out waiting for WinMM to return submitted input headers",
                     "midiInReset", std::nullopt});
            }
        }
        return Result<void>::success();
    }

    Result<void> close_on_worker() {
        std::optional<MidiError> first_error;
        accepting_callbacks.store(false, std::memory_order_release);

        if (input_handle) {
            const auto stopped = midiInStop(input_handle);
            if (stopped != MMSYSERR_NOERROR && !first_error) {
                first_error = native_error(MidiErrorCode::close_failure, "midiInStop", stopped);
            }
            const auto reset = midiInReset(input_handle);
            if (reset != MMSYSERR_NOERROR && !first_error) {
                first_error = native_error(MidiErrorCode::close_failure, "midiInReset", reset);
            }
            const auto returned = wait_for_returned_buffers();
            if (!returned && !first_error) first_error = returned.error();
            if (returned) {
                for (auto& buffer : input_buffers) {
                    const auto result = buffer->unprepare();
                    if (!result && !first_error) first_error = result.error();
                }
            }
            if (!first_error) {
                input_buffers.clear();
                const auto closed = midiInClose(input_handle);
                if (closed != MMSYSERR_NOERROR) {
                    first_error = native_error(MidiErrorCode::close_failure, "midiInClose", closed);
                } else {
                    input_handle = nullptr;
                }
            }
        }

        if (output_handle) {
            const auto reset = midiOutReset(output_handle);
            if (reset != MMSYSERR_NOERROR && !first_error) {
                first_error = native_error(MidiErrorCode::close_failure, "midiOutReset", reset);
            }
            const auto closed = midiOutClose(output_handle);
            if (closed != MMSYSERR_NOERROR && !first_error) {
                first_error = native_error(MidiErrorCode::close_failure, "midiOutClose", closed);
            } else if (closed == MMSYSERR_NOERROR) {
                output_handle = nullptr;
            }
        }
        if (first_error) return Result<void>::failure(std::move(*first_error));
        return Result<void>::success();
    }
};

WinmmTransport::WinmmTransport() : impl_(std::make_unique<Impl>()) {}

WinmmTransport::~WinmmTransport() { static_cast<void>(close()); }

MidiBackend WinmmTransport::backend() const noexcept { return MidiBackend::winmm; }

MidiTransportCapabilities WinmmTransport::capabilities() const noexcept {
    return {true, true, false, true, false};
}

MidiTransportDiagnostics WinmmTransport::diagnostics() const noexcept {
    return {impl_->native_callbacks.load(), impl_->delivered_messages.load(),
            impl_->dropped_callbacks.load(),
            impl_->callbacks_after_acceptance_closed.load()};
}

TransportState WinmmTransport::state() const noexcept { return lifecycle_.state(); }

Result<std::vector<MidiEndpointDescriptor>> WinmmTransport::enumerate() {
    return impl_->invoke([this] { return impl_->enumerate_on_worker(); });
}

Result<void> WinmmTransport::open(const MidiConnectionRequest& request) {
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

Result<void> WinmmTransport::close() {
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

Result<void> WinmmTransport::send(const NativeMidiMessage&) {
    return Result<void>::failure(
        {MidiErrorCode::unsupported_capability,
         "production WinMM message sending begins in Stage 3", {}, std::nullopt});
}

void WinmmTransport::set_message_handler(MidiMessageHandler handler) {
    std::scoped_lock lock(impl_->handler_mutex);
    impl_->message_handler = std::move(handler);
}

void WinmmTransport::set_endpoint_change_handler(EndpointChangeHandler handler) {
    std::scoped_lock lock(impl_->handler_mutex);
    impl_->endpoint_handler = std::move(handler);
}

} // namespace taureon::midi::winmm
