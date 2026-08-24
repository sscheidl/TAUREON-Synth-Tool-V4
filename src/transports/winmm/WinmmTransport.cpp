#include "WinmmTransport.hpp"

#include "WinmmNativeApi.hpp"
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
#include <stdexcept>
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

WinmmTransportApiPtr require_native_api(WinmmTransportApiPtr native_api) {
    if (!native_api) throw std::invalid_argument("WinMM native API must not be null");
    return native_api;
}

} // namespace

struct WinmmTransport::Impl {
    struct CallbackEvent {
        enum class Kind { native, data_loss };

        std::uint64_t sequence{};
        Kind kind{Kind::native};
        UINT message{};
        DWORD_PTR parameter{};
        DWORD_PTR timestamp{};
        bool accepted_for_delivery{};
        MidiDataLossEvent loss;
    };

    WinmmTransportApiPtr native_api;
    HMIDIIN input_handle{};
    HMIDIOUT output_handle{};
    std::vector<std::unique_ptr<WinmmInputBuffer>> input_buffers;
    std::vector<std::unique_ptr<WinmmOutputBuffer>> output_buffers;
    std::atomic<bool> accepting_callbacks{false};
    std::atomic<std::uint64_t> native_callbacks{0};
    std::atomic<std::uint64_t> delivered_messages{0};
    std::atomic<std::uint64_t> transmitted_messages{0};
    std::atomic<std::uint64_t> dropped_callbacks{0};
    std::atomic<std::uint64_t> callbacks_after_acceptance_closed{0};
    std::atomic<std::uint64_t> queue_high_water_mark{0};
    std::atomic<std::uint64_t> next_stream_sequence{0};

    std::mutex queue_mutex;
    std::condition_variable queue_changed;
    std::deque<std::function<void()>> commands;
    std::deque<CallbackEvent> callbacks;
    std::deque<MIDIHDR*> output_completions;
    bool stopping{};
    std::thread worker;

    std::mutex handler_mutex;
    MidiMessageHandler message_handler;
    MidiStreamEventHandler stream_event_handler;
    EndpointChangeHandler endpoint_handler;

    void insert_callback_locked(CallbackEvent event) {
        const auto position = std::upper_bound(
            callbacks.begin(), callbacks.end(), event.sequence,
            [](const std::uint64_t sequence, const CallbackEvent& queued) {
                return sequence < queued.sequence;
            });
        callbacks.insert(position, std::move(event));
        const auto depth = static_cast<std::uint64_t>(callbacks.size());
        auto high_water = queue_high_water_mark.load(std::memory_order_relaxed);
        while (depth > high_water &&
               !queue_high_water_mark.compare_exchange_weak(
                   high_water, depth, std::memory_order_relaxed)) {}
    }

    void enqueue_loss(MidiDataLossEvent loss) {
        {
            std::scoped_lock lock(queue_mutex);
            insert_callback_locked(
                {next_stream_sequence.fetch_add(1, std::memory_order_relaxed),
                 CallbackEvent::Kind::data_loss, 0, 0, 0, true, std::move(loss)});
        }
        queue_changed.notify_one();
    }

    explicit Impl(WinmmTransportApiPtr api)
        : native_api(std::move(api)), worker([this] { worker_main(); }) {}

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
                                        const DWORD_PTR parameter, const DWORD_PTR timestamp) {
        auto* self = reinterpret_cast<Impl*>(instance);
        if (self == nullptr || (message != MIM_DATA && message != MIM_LONGDATA &&
                                message != MIM_ERROR && message != MIM_LONGERROR)) {
            return;
        }
        ++self->native_callbacks;
        const bool header_completion = message == MIM_LONGDATA || message == MIM_LONGERROR;
        const bool accepted = self->accepting_callbacks.load(std::memory_order_acquire);
        if (!accepted && !header_completion) {
            ++self->callbacks_after_acceptance_closed;
            return;
        }
        const auto sequence =
            self->next_stream_sequence.fetch_add(1, std::memory_order_relaxed);
        {
            std::scoped_lock lock(self->queue_mutex);
            constexpr std::size_t callback_capacity = 1024;
            const bool non_droppable = header_completion || message == MIM_ERROR;
            if (!non_droppable && self->callbacks.size() >= callback_capacity) {
                ++self->dropped_callbacks;
                // Coalesce only consecutive drops. A non-droppable callback (notably a returned
                // long-input header) ends the run, so any later drop receives a new ordered marker
                // before subsequent frame data. Global coalescing could otherwise under-report a
                // sustained overflow across multiple SysEx frame boundaries.
                const bool consecutive_overflow_marker =
                    !self->callbacks.empty() &&
                    self->callbacks.back().kind == CallbackEvent::Kind::data_loss &&
                    self->callbacks.back().loss.reason == MidiDataLossReason::queue_overflow;
                if (!consecutive_overflow_marker) {
                    self->insert_callback_locked(
                        {sequence, CallbackEvent::Kind::data_loss, 0, 0, 0, true,
                         {MidiBackend::winmm, MidiDataLossReason::queue_overflow, true,
                          std::nullopt, "WinMM callback queue overflow", std::nullopt}});
                }
                self->queue_changed.notify_one();
                return;
            }
            self->insert_callback_locked(
                {sequence, CallbackEvent::Kind::native, message, parameter, timestamp, accepted,
                 {}});
        }
        self->queue_changed.notify_one();
    }

    static void CALLBACK output_callback(HMIDIOUT, const UINT message, const DWORD_PTR instance,
                                         const DWORD_PTR parameter, DWORD_PTR) {
        auto* self = reinterpret_cast<Impl*>(instance);
        if (self == nullptr || message != MOM_DONE) return;
        ++self->native_callbacks;
        {
            std::scoped_lock lock(self->queue_mutex);
            self->output_completions.push_back(reinterpret_cast<MIDIHDR*>(parameter));
            const auto depth = static_cast<std::uint64_t>(self->output_completions.size());
            auto high_water = self->queue_high_water_mark.load(std::memory_order_relaxed);
            while (depth > high_water &&
                   !self->queue_high_water_mark.compare_exchange_weak(
                       high_water, depth, std::memory_order_relaxed)) {}
        }
        self->queue_changed.notify_one();
    }

    void worker_main() {
        for (;;) {
            std::function<void()> command;
            std::optional<CallbackEvent> callback;
            MIDIHDR* output_completion{};
            {
                std::unique_lock lock(queue_mutex);
                queue_changed.wait(lock, [&] {
                    return stopping || !output_completions.empty() || !callbacks.empty() ||
                           !commands.empty();
                });
                if (!commands.empty()) {
                    command = std::move(commands.front());
                    commands.pop_front();
                } else if (!output_completions.empty()) {
                    output_completion = output_completions.front();
                    output_completions.pop_front();
                } else if (!callbacks.empty()) {
                    callback = std::move(callbacks.front());
                    callbacks.pop_front();
                } else if (stopping) {
                    break;
                }
            }
            if (output_completion) process_output_completion(output_completion);
            else if (callback) process_callback(*callback);
            else if (command) command();
        }
    }

    static std::size_t short_message_size(const std::uint8_t status) noexcept {
        if (status < 0x80) return 0;
        if (status < 0xF0) {
            const auto kind = static_cast<std::uint8_t>(status & 0xF0);
            return kind == 0xC0 || kind == 0xD0 ? 2 : 3;
        }
        switch (status) {
        case 0xF1:
        case 0xF3:
            return 2;
        case 0xF2:
            return 3;
        case 0xF6:
        case 0xF8:
        case 0xF9:
        case 0xFA:
        case 0xFB:
        case 0xFC:
        case 0xFD:
        case 0xFE:
        case 0xFF:
            return 1;
        default:
            return 0;
        }
    }

    void dispatch_loss(const std::uint64_t sequence, const MidiDataLossEvent& loss) {
        MidiStreamEventHandler handler;
        {
            std::scoped_lock lock(handler_mutex);
            handler = stream_event_handler;
        }
        if (handler) handler({sequence, loss});
    }

    void deliver(const std::uint64_t sequence, std::vector<std::uint8_t> bytes,
                 const DWORD_PTR timestamp) {
        MidiMessageHandler handler;
        MidiStreamEventHandler stream_handler;
        {
            std::scoped_lock lock(handler_mutex);
            handler = message_handler;
            stream_handler = stream_event_handler;
        }
        if (bytes.empty()) return;
        NativeMidiMessage message{
            MidiBackend::winmm, Midi1NativeMessage{std::move(bytes)},
            MidiTimestamp{static_cast<std::uint64_t>(timestamp), "winmm-milliseconds"}};
        if (stream_handler) stream_handler({sequence, message});
        if (handler) handler(message);
        if (stream_handler || handler) ++delivered_messages;
    }

    void report_native_loss(const CallbackEvent& event, const MidiDataLossReason reason,
                            const bool affects_sysex, std::string detail,
                            const std::optional<std::int64_t> native_code = std::nullopt) {
        ++dropped_callbacks;
        dispatch_loss(event.sequence,
                      {MidiBackend::winmm, reason, affects_sysex, std::nullopt,
                       std::move(detail), native_code});
    }

    void process_callback(const CallbackEvent& event) {
        if (event.kind == CallbackEvent::Kind::data_loss) {
            dispatch_loss(event.sequence, event.loss);
            return;
        }
        if (event.message == MIM_DATA || event.message == MIM_ERROR) {
            if (event.message == MIM_ERROR) {
                report_native_loss(event, MidiDataLossReason::native_short_error, false,
                                   "WinMM reported an invalid short MIDI message");
                return;
            }
            const auto packed = static_cast<DWORD>(event.parameter);
            const auto status = static_cast<std::uint8_t>(packed & 0xFFu);
            const auto size = short_message_size(status);
            if (size == 0) {
                report_native_loss(event, MidiDataLossReason::native_short_error, false,
                                   "WinMM delivered an invalid packed short MIDI message");
                return;
            }
            std::vector<std::uint8_t> bytes(size);
            for (std::size_t index = 0; index < size; ++index) {
                bytes[index] = static_cast<std::uint8_t>((packed >> (index * 8u)) & 0xFFu);
            }
            if (event.accepted_for_delivery) {
                deliver(event.sequence, std::move(bytes), event.timestamp);
            }
            return;
        }
        if (event.message != MIM_LONGDATA && event.message != MIM_LONGERROR) return;
        auto* header = reinterpret_cast<MIDIHDR*>(event.parameter);
        for (auto& buffer : input_buffers) {
            if (buffer->native_header() != header) continue;
            buffer->mark_returned();
            const auto bytes = buffer->recorded_bytes();
            if (event.message == MIM_LONGERROR) {
                report_native_loss(event, MidiDataLossReason::native_long_error, true,
                                   "WinMM reported invalid or incomplete long-message data");
            } else if (event.accepted_for_delivery) {
                deliver(event.sequence, bytes, event.timestamp);
            } else if (!bytes.empty()) {
                report_native_loss(event, MidiDataLossReason::shutdown_discarded_data, true,
                                   "WinMM returned undelivered long-message bytes during shutdown");
            }
            if (accepting_callbacks.load(std::memory_order_acquire)) {
                const auto submitted = buffer->submit();
                if (!submitted) {
                    ++dropped_callbacks;
                    enqueue_loss({MidiBackend::winmm,
                                  MidiDataLossReason::input_requeue_failure, true, std::nullopt,
                                  "WinMM failed to requeue an input MIDIHDR",
                                  submitted.error().native_code});
                }
            }
            queue_changed.notify_all();
            return;
        }
    }

    void process_output_completion(MIDIHDR* header) {
        for (auto& buffer : output_buffers) {
            if (buffer->native_header() != header) continue;
            buffer->mark_completed();
            queue_changed.notify_all();
            return;
        }
    }

    Result<std::vector<MidiEndpointDescriptor>> enumerate_on_worker() const {
        std::vector<MidiEndpointDescriptor> endpoints;
        for (UINT index = 0; index < native_api->input_device_count(); ++index) {
            MIDIINCAPSW caps{};
            const auto result = native_api->input_device_caps(index, caps);
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
        for (UINT index = 0; index < native_api->output_device_count(); ++index) {
            MIDIOUTCAPSW caps{};
            const auto result = native_api->output_device_caps(index, caps);
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
            const auto result = native_api->open_input(
                input_handle, index.value(), reinterpret_cast<DWORD_PTR>(&input_callback),
                reinterpret_cast<DWORD_PTR>(this));
            if (result != MMSYSERR_NOERROR) {
                return Result<void>::failure(
                    native_error(MidiErrorCode::open_failure, "midiInOpen", result));
            }
            for (int count = 0; count < 2; ++count) {
                auto buffer = std::make_unique<WinmmInputBuffer>(*native_api, input_handle, 4096);
                auto prepared = buffer->prepare();
                if (!prepared) {
                    static_cast<void>(close_on_worker());
                    return prepared;
                }
                input_buffers.push_back(std::move(buffer));
                auto submitted = input_buffers.back()->submit();
                if (!submitted) {
                    static_cast<void>(close_on_worker());
                    return submitted;
                }
            }
            accepting_callbacks.store(true, std::memory_order_release);
            const auto started = native_api->start_input(input_handle);
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
            const auto result = native_api->open_output(
                output_handle, index.value(), reinterpret_cast<DWORD_PTR>(&output_callback),
                reinterpret_cast<DWORD_PTR>(this));
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

    bool all_output_buffers_returned() const {
        for (const auto& buffer : output_buffers) {
            if (buffer->submitted()) return false;
        }
        return true;
    }

    void drain_one_callback(std::unique_lock<std::mutex>& lock) {
        auto event = std::move(callbacks.front());
        callbacks.pop_front();
        lock.unlock();
        process_callback(event);
        lock.lock();
    }

    void drain_pending_callbacks() {
        for (;;) {
            std::optional<CallbackEvent> event;
            {
                std::scoped_lock lock(queue_mutex);
                if (callbacks.empty()) break;
                event = std::move(callbacks.front());
                callbacks.pop_front();
            }
            process_callback(*event);
        }
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

    void drain_one_output_completion(std::unique_lock<std::mutex>& lock) {
        auto* header = output_completions.front();
        output_completions.pop_front();
        lock.unlock();
        process_output_completion(header);
        lock.lock();
    }

    Result<void> wait_for_output_buffers(const char* operation) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        std::unique_lock lock(queue_mutex);
        while (!all_output_buffers_returned()) {
            if (!output_completions.empty()) {
                drain_one_output_completion(lock);
                continue;
            }
            if (!queue_changed.wait_until(lock, deadline,
                                          [&] { return !output_completions.empty(); })) {
                return Result<void>::failure(
                    {MidiErrorCode::timeout,
                     "timed out waiting for WinMM to return submitted output headers",
                     operation, std::nullopt});
            }
        }
        return Result<void>::success();
    }

    Result<void> send_on_worker(const NativeMidiMessage& message) {
        if (!output_handle) {
            return Result<void>::failure(
                {MidiErrorCode::invalid_state, "WinMM output is not open", {}, std::nullopt});
        }
        if (message.backend != MidiBackend::winmm) {
            return Result<void>::failure(
                {MidiErrorCode::invalid_route, "message is not for WinMM", {}, std::nullopt});
        }
        const auto* midi1 = std::get_if<Midi1NativeMessage>(&message.data);
        if (midi1 == nullptr) {
            return Result<void>::failure(
                {MidiErrorCode::unsupported_capability, "WinMM requires MIDI 1.0 bytes", {},
                 std::nullopt});
        }
        if (midi1->bytes.empty()) {
            return Result<void>::failure(
                {MidiErrorCode::malformed_data, "cannot send an empty MIDI message", {},
                 std::nullopt});
        }

        const auto expected_short_size = short_message_size(midi1->bytes.front());
        if (expected_short_size != 0 && midi1->bytes.size() == expected_short_size) {
            DWORD packed{};
            for (std::size_t index = 0; index < midi1->bytes.size(); ++index) {
                packed |= static_cast<DWORD>(midi1->bytes[index]) << (index * 8u);
            }
            const auto result = native_api->send_short(output_handle, packed);
            if (result != MMSYSERR_NOERROR) {
                return Result<void>::failure(
                    native_error(MidiErrorCode::native_api_error, "midiOutShortMsg", result));
            }
            ++transmitted_messages;
            return Result<void>::success();
        }

        if (midi1->bytes.front() != 0xF0 || midi1->bytes.back() != 0xF7) {
            return Result<void>::failure(
                {MidiErrorCode::malformed_data,
                 "WinMM long messages must be complete F0...F7 SysEx frames", {}, std::nullopt});
        }

        auto buffer = std::make_unique<WinmmOutputBuffer>(*native_api, output_handle, midi1->bytes);
        const auto prepared = buffer->prepare();
        if (!prepared) return prepared;
        output_buffers.push_back(std::move(buffer));
        const auto submitted = output_buffers.back()->submit();
        if (!submitted) {
            const auto unprepared = output_buffers.back()->unprepare();
            output_buffers.pop_back();
            if (!unprepared) return unprepared;
            return submitted;
        }

        auto returned = wait_for_output_buffers("midiOutLongMsg");
        if (!returned) {
            const auto reset = native_api->reset_output(output_handle);
            if (reset != MMSYSERR_NOERROR) return returned;
            returned = wait_for_output_buffers("midiOutReset");
            if (!returned) return returned;
        }
        const auto unprepared = output_buffers.back()->unprepare();
        if (!unprepared) return unprepared;
        output_buffers.pop_back();
        ++transmitted_messages;
        return Result<void>::success();
    }

    Result<void> close_on_worker() {
        std::optional<MidiError> first_error;
        accepting_callbacks.store(false, std::memory_order_release);
        drain_pending_callbacks();

        if (input_handle) {
            const auto stopped = native_api->stop_input(input_handle);
            if (stopped != MMSYSERR_NOERROR && !first_error) {
                first_error = native_error(MidiErrorCode::close_failure, "midiInStop", stopped);
            }
            const auto reset = native_api->reset_input(input_handle);
            if (reset != MMSYSERR_NOERROR && !first_error) {
                first_error = native_error(MidiErrorCode::close_failure, "midiInReset", reset);
            }
            const auto returned = wait_for_returned_buffers();
            if (!returned && !first_error) first_error = returned.error();
            if (returned) {
                drain_pending_callbacks();
                for (auto& buffer : input_buffers) {
                    const auto result = buffer->unprepare();
                    if (!result && !first_error) first_error = result.error();
                }
            }
            if (!first_error) {
                input_buffers.clear();
                const auto closed = native_api->close_input(input_handle);
                if (closed != MMSYSERR_NOERROR) {
                    first_error = native_error(MidiErrorCode::close_failure, "midiInClose", closed);
                } else {
                    input_handle = nullptr;
                }
            }
        }

        if (output_handle) {
            const auto reset = native_api->reset_output(output_handle);
            if (reset != MMSYSERR_NOERROR) {
                if (!first_error) {
                    first_error = native_error(MidiErrorCode::close_failure, "midiOutReset", reset);
                }
            } else {
                const auto returned = wait_for_output_buffers("midiOutReset");
                if (!returned && !first_error) first_error = returned.error();
                if (returned) {
                    bool all_unprepared = true;
                    for (auto& buffer : output_buffers) {
                        const auto result = buffer->unprepare();
                        if (!result) {
                            all_unprepared = false;
                            if (!first_error) first_error = result.error();
                        }
                    }
                    if (all_unprepared) {
                        output_buffers.clear();
                        const auto closed = native_api->close_output(output_handle);
                        if (closed != MMSYSERR_NOERROR) {
                            if (!first_error) {
                                first_error = native_error(MidiErrorCode::close_failure,
                                                           "midiOutClose", closed);
                            }
                        } else {
                            output_handle = nullptr;
                        }
                    }
                }
            }
        }
        if (first_error) return Result<void>::failure(std::move(*first_error));
        return Result<void>::success();
    }
};

WinmmTransport::WinmmTransport()
    : WinmmTransport(std::make_shared<NativeWinmmTransportApi>()) {}

WinmmTransport::WinmmTransport(WinmmTransportApiPtr native_api)
    : impl_(std::make_unique<Impl>(require_native_api(std::move(native_api)))) {}

WinmmTransport::~WinmmTransport() { static_cast<void>(close()); }

MidiBackend WinmmTransport::backend() const noexcept { return MidiBackend::winmm; }

MidiTransportCapabilities WinmmTransport::capabilities() const noexcept {
    return {true, true, false, true, false};
}

MidiTransportDiagnostics WinmmTransport::diagnostics() const noexcept {
    return {impl_->native_callbacks.load(), impl_->delivered_messages.load(),
            impl_->transmitted_messages.load(), impl_->dropped_callbacks.load(),
            impl_->callbacks_after_acceptance_closed.load(),
            impl_->queue_high_water_mark.load()};
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

Result<void> WinmmTransport::send(const NativeMidiMessage& message) {
    if (lifecycle_.state() != TransportState::open) {
        return Result<void>::failure(
            {MidiErrorCode::invalid_state, "WinMM transport is not open", {}, std::nullopt});
    }
    return impl_->invoke([this, message] { return impl_->send_on_worker(message); });
}

void WinmmTransport::set_message_handler(MidiMessageHandler handler) {
    std::scoped_lock lock(impl_->handler_mutex);
    impl_->message_handler = std::move(handler);
}

void WinmmTransport::set_stream_event_handler(MidiStreamEventHandler handler) {
    std::scoped_lock lock(impl_->handler_mutex);
    impl_->stream_event_handler = std::move(handler);
}

void WinmmTransport::set_endpoint_change_handler(EndpointChangeHandler handler) {
    std::scoped_lock lock(impl_->handler_mutex);
    impl_->endpoint_handler = std::move(handler);
}

} // namespace taureon::midi::winmm
