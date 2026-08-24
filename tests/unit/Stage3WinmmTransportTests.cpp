#include "TestSupport.hpp"

#include "core/sysex/SysExCaptureSession.hpp"
#include "core/sysex/SyxFile.hpp"
#include "transports/winmm/WinmmTransport.hpp"

#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cwchar>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <variant>
#include <vector>

using namespace taureon::midi;
using namespace taureon::midi::winmm;

namespace {

using InputCallback = void(CALLBACK*)(HMIDIIN, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
using OutputCallback = void(CALLBACK*)(HMIDIOUT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);

class RealtimeApi final : public IWinmmTransportApi {
public:
    MMRESULT prepare_input(HMIDIIN handle, MIDIHDR& header) override {
        if (!input_open || handle != input_handle) return MMSYSERR_INVALHANDLE;
        input_headers.push_back(&header);
        return MMSYSERR_NOERROR;
    }

    MMRESULT add_input(HMIDIIN handle, MIDIHDR& header) override {
        if (!input_open || handle != input_handle) return MMSYSERR_INVALHANDLE;
        std::scoped_lock lock(input_mutex);
        if (fail_next_add) {
            fail_next_add = false;
            return MMSYSERR_ERROR;
        }
        if (std::find(submitted_headers.begin(), submitted_headers.end(), &header) ==
            submitted_headers.end()) {
            submitted_headers.push_back(&header);
        }
        input_changed.notify_all();
        return MMSYSERR_NOERROR;
    }

    MMRESULT unprepare_input(HMIDIIN handle, MIDIHDR& header) override {
        events.emplace_back("unprepare-input");
        if (!input_open || handle != input_handle) return MMSYSERR_INVALHANDLE;
        std::scoped_lock lock(input_mutex);
        std::erase(input_headers, &header);
        std::erase(submitted_headers, &header);
        return MMSYSERR_NOERROR;
    }

    MMRESULT prepare_output(HMIDIOUT handle, MIDIHDR&) override {
        events.emplace_back("prepare-output");
        return output_open && handle == output_handle ? MMSYSERR_NOERROR : MMSYSERR_INVALHANDLE;
    }

    MMRESULT send_output(HMIDIOUT handle, MIDIHDR& header) override {
        events.emplace_back("send-output");
        if (!output_open || handle != output_handle) return MMSYSERR_INVALHANDLE;
        if (fail_long_send) return MMSYSERR_ERROR;
        long_messages.emplace_back(
            reinterpret_cast<const std::uint8_t*>(header.lpData),
            reinterpret_cast<const std::uint8_t*>(header.lpData) + header.dwBufferLength);
        reinterpret_cast<OutputCallback>(output_callback)(
            output_handle, MOM_DONE, output_instance, reinterpret_cast<DWORD_PTR>(&header), 0);
        return MMSYSERR_NOERROR;
    }

    MMRESULT unprepare_output(HMIDIOUT handle, MIDIHDR&) override {
        events.emplace_back("unprepare-output");
        return output_open && handle == output_handle ? MMSYSERR_NOERROR : MMSYSERR_INVALHANDLE;
    }

    UINT input_device_count() const noexcept override { return 1; }
    UINT output_device_count() const noexcept override { return 1; }

    MMRESULT input_device_caps(UINT index, MIDIINCAPSW& capabilities) override {
        if (index != 0) return MMSYSERR_BADDEVICEID;
        capabilities.wMid = 1;
        capabilities.wPid = 30;
        capabilities.vDriverVersion = 256;
        wcscpy_s(capabilities.szPname, L"TAUREON Stage 3 fake input");
        return MMSYSERR_NOERROR;
    }

    MMRESULT output_device_caps(UINT index, MIDIOUTCAPSW& capabilities) override {
        if (index != 0) return MMSYSERR_BADDEVICEID;
        capabilities.wMid = 1;
        capabilities.wPid = 31;
        capabilities.vDriverVersion = 256;
        wcscpy_s(capabilities.szPname, L"TAUREON Stage 3 fake output");
        return MMSYSERR_NOERROR;
    }

    MMRESULT open_input(HMIDIIN& handle, UINT index, DWORD_PTR callback,
                        DWORD_PTR instance) override {
        if (index != 0) return MMSYSERR_BADDEVICEID;
        input_handle = reinterpret_cast<HMIDIIN>(static_cast<std::uintptr_t>(0x3101));
        input_callback = callback;
        input_instance = instance;
        input_open = true;
        handle = input_handle;
        return MMSYSERR_NOERROR;
    }

    MMRESULT start_input(HMIDIIN handle) override {
        return input_open && handle == input_handle ? MMSYSERR_NOERROR : MMSYSERR_INVALHANDLE;
    }

    MMRESULT stop_input(HMIDIIN handle) override {
        return input_open && handle == input_handle ? MMSYSERR_NOERROR : MMSYSERR_INVALHANDLE;
    }

    MMRESULT reset_input(HMIDIIN handle) override {
        if (!input_open || handle != input_handle) return MMSYSERR_INVALHANDLE;
        std::vector<MIDIHDR*> submitted;
        {
            std::scoped_lock lock(input_mutex);
            submitted = submitted_headers;
            submitted_headers.clear();
        }
        for (auto* header : submitted) {
            header->dwBytesRecorded = 0;
            reinterpret_cast<InputCallback>(input_callback)(
                input_handle, MIM_LONGDATA, input_instance,
                reinterpret_cast<DWORD_PTR>(header), 0);
        }
        return MMSYSERR_NOERROR;
    }

    MMRESULT close_input(HMIDIIN handle) override {
        events.emplace_back("close-input");
        if (!input_open || handle != input_handle) return MMSYSERR_INVALHANDLE;
        input_open = false;
        return MMSYSERR_NOERROR;
    }

    MMRESULT open_output(HMIDIOUT& handle, UINT index, DWORD_PTR callback,
                         DWORD_PTR instance) override {
        if (index != 0) return MMSYSERR_BADDEVICEID;
        output_handle = reinterpret_cast<HMIDIOUT>(static_cast<std::uintptr_t>(0x3102));
        output_callback = callback;
        output_instance = instance;
        output_open = true;
        handle = output_handle;
        return MMSYSERR_NOERROR;
    }

    MMRESULT send_short(HMIDIOUT handle, DWORD message) override {
        if (!output_open || handle != output_handle) return MMSYSERR_INVALHANDLE;
        if (fail_short_send) return MMSYSERR_ERROR;
        {
            std::unique_lock lock(block_mutex);
            if (block_next_short) {
                short_send_blocked = true;
                block_changed.notify_all();
                block_changed.wait(lock, [&] { return release_short_send; });
                block_next_short = false;
                short_send_blocked = false;
                release_short_send = false;
            }
        }
        short_messages.push_back(message);
        return MMSYSERR_NOERROR;
    }

    MMRESULT reset_output(HMIDIOUT handle) override {
        return output_open && handle == output_handle ? MMSYSERR_NOERROR : MMSYSERR_INVALHANDLE;
    }

    MMRESULT close_output(HMIDIOUT handle) override {
        events.emplace_back("close-output");
        if (!output_open || handle != output_handle) return MMSYSERR_INVALHANDLE;
        output_open = false;
        return MMSYSERR_NOERROR;
    }

    void emit_short(const DWORD message, const DWORD timestamp) {
        reinterpret_cast<InputCallback>(input_callback)(input_handle, MIM_DATA, input_instance,
                                                        message, timestamp);
    }

    void emit_short_error(const DWORD message, const DWORD timestamp) {
        reinterpret_cast<InputCallback>(input_callback)(input_handle, MIM_ERROR, input_instance,
                                                        message, timestamp);
    }

    void emit_long(const std::vector<std::uint8_t>& bytes, const UINT message = MIM_LONGDATA,
                   const DWORD timestamp = 0) {
        MIDIHDR* header{};
        {
            std::unique_lock lock(input_mutex);
            TAUREON_REQUIRE(input_changed.wait_for(
                lock, std::chrono::seconds(1), [&] { return !submitted_headers.empty(); }));
            header = submitted_headers.front();
            submitted_headers.erase(submitted_headers.begin());
        }
        TAUREON_REQUIRE(bytes.size() <= header->dwBufferLength);
        std::copy(bytes.begin(), bytes.end(), reinterpret_cast<std::uint8_t*>(header->lpData));
        header->dwBytesRecorded = static_cast<DWORD>(bytes.size());
        reinterpret_cast<InputCallback>(input_callback)(
            input_handle, message, input_instance, reinterpret_cast<DWORD_PTR>(header), timestamp);
    }

    void block_one_short_send() {
        std::scoped_lock lock(block_mutex);
        block_next_short = true;
    }

    void fail_one_input_requeue() {
        std::scoped_lock lock(input_mutex);
        fail_next_add = true;
    }

    bool wait_until_short_send_is_blocked() {
        std::unique_lock lock(block_mutex);
        return block_changed.wait_for(lock, std::chrono::seconds(1),
                                      [&] { return short_send_blocked; });
    }

    void release_blocked_short_send() {
        {
            std::scoped_lock lock(block_mutex);
            release_short_send = true;
        }
        block_changed.notify_all();
    }

    HMIDIIN input_handle{};
    HMIDIOUT output_handle{};
    DWORD_PTR input_callback{};
    DWORD_PTR input_instance{};
    DWORD_PTR output_callback{};
    DWORD_PTR output_instance{};
    bool input_open{};
    bool output_open{};
    bool fail_short_send{};
    bool fail_long_send{};
    bool fail_next_add{};
    std::vector<MIDIHDR*> input_headers;
    std::vector<MIDIHDR*> submitted_headers;
    std::vector<DWORD> short_messages;
    std::vector<std::vector<std::uint8_t>> long_messages;
    std::vector<std::string> events;
    std::mutex input_mutex;
    std::condition_variable input_changed;
    std::mutex block_mutex;
    std::condition_variable block_changed;
    bool block_next_short{};
    bool short_send_blocked{};
    bool release_short_send{};
};

class CaptureRecorder {
public:
    void consume(const MidiStreamEvent& event) {
        auto completed = capture_.consume(event);
        {
            std::scoped_lock lock(mutex_);
            ++event_count_;
            if (const auto* loss = std::get_if<MidiDataLossEvent>(&event.payload)) {
                losses_.push_back(*loss);
            }
            frames_.insert(frames_.end(), completed.begin(), completed.end());
        }
        changed_.notify_all();
    }

    bool wait_for_events(const std::size_t count) {
        std::unique_lock lock(mutex_);
        return changed_.wait_for(lock, std::chrono::seconds(2),
                                 [&] { return event_count_ >= count; });
    }

    bool wait_for_frames(const std::size_t count) {
        std::unique_lock lock(mutex_);
        return changed_.wait_for(lock, std::chrono::seconds(2),
                                 [&] { return frames_.size() >= count; });
    }

    bool wait_for_losses(const std::size_t count) {
        std::unique_lock lock(mutex_);
        return changed_.wait_for(lock, std::chrono::seconds(2),
                                 [&] { return losses_.size() >= count; });
    }

    void finish() {
        auto remaining = capture_.finish();
        std::scoped_lock lock(mutex_);
        frames_.insert(frames_.end(), remaining.begin(), remaining.end());
    }

    std::vector<taureon::sysex::SysExFrame> frames() const {
        std::scoped_lock lock(mutex_);
        return frames_;
    }

    std::vector<MidiDataLossEvent> losses() const {
        std::scoped_lock lock(mutex_);
        return losses_;
    }

private:
    taureon::sysex::SysExCaptureSession capture_;
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::size_t event_count_{};
    std::vector<taureon::sysex::SysExFrame> frames_;
    std::vector<MidiDataLossEvent> losses_;
};

MidiConnectionRequest both_routes(const std::vector<MidiEndpointDescriptor>& endpoints) {
    return {endpoints[0].identity, endpoints[1].identity};
}

void realtime_send_receive_and_owned_completion() {
    auto api = std::make_shared<RealtimeApi>();
    WinmmTransport transport(api);
    const auto endpoints = transport.enumerate();
    TAUREON_REQUIRE(endpoints);
    TAUREON_REQUIRE(endpoints.value().size() == 2);

    std::mutex received_mutex;
    std::condition_variable received_changed;
    std::vector<NativeMidiMessage> received;
    transport.set_message_handler([&](const NativeMidiMessage& message) {
        {
            std::scoped_lock lock(received_mutex);
            received.push_back(message);
        }
        received_changed.notify_all();
    });

    TAUREON_REQUIRE(transport.open(
        {endpoints.value()[0].identity, endpoints.value()[1].identity}));

    api->emit_short(0x00643C90u, 1234);
    {
        std::unique_lock lock(received_mutex);
        TAUREON_REQUIRE(received_changed.wait_for(
            lock, std::chrono::seconds(1), [&] { return !received.empty(); }));
    }
    TAUREON_REQUIRE(received.front().backend == MidiBackend::winmm);
    TAUREON_REQUIRE(std::get<Midi1NativeMessage>(received.front().data).bytes ==
                    std::vector<std::uint8_t>({0x90, 0x3C, 0x64}));
    TAUREON_REQUIRE(received.front().timestamp ==
                    std::optional<MidiTimestamp>({1234, "winmm-milliseconds"}));
    api->emit_short_error(0x00643D90u, 1235);

    TAUREON_REQUIRE(transport.send(
        {MidiBackend::winmm, Midi1NativeMessage{{0x80, 0x3C, 0x00}}, std::nullopt}));
    TAUREON_REQUIRE(api->short_messages == std::vector<DWORD>({0x00003C80u}));

    const std::vector<std::uint8_t> sysex{0xF0, 0x7D, 0x01, 0x02, 0x03, 0xF7};
    TAUREON_REQUIRE(transport.send(
        {MidiBackend::winmm, Midi1NativeMessage{sysex}, std::nullopt}));
    TAUREON_REQUIRE(api->long_messages ==
                    std::vector<std::vector<std::uint8_t>>({sysex}));

    api->fail_short_send = true;
    const auto failed_short = transport.send(
        {MidiBackend::winmm, Midi1NativeMessage{{0x90, 0x3D, 0x01}}, std::nullopt});
    TAUREON_REQUIRE(!failed_short);
    TAUREON_REQUIRE(failed_short.error().native_api == "midiOutShortMsg");
    api->fail_short_send = false;

    api->fail_long_send = true;
    const auto failed_long = transport.send(
        {MidiBackend::winmm, Midi1NativeMessage{sysex}, std::nullopt});
    TAUREON_REQUIRE(!failed_long);
    TAUREON_REQUIRE(failed_long.error().native_api == "midiOutLongMsg");
    api->fail_long_send = false;

    TAUREON_REQUIRE(transport.close());
    TAUREON_REQUIRE(transport.state() == TransportState::closed);
    TAUREON_REQUIRE(!api->input_open);
    TAUREON_REQUIRE(!api->output_open);
    TAUREON_REQUIRE(api->input_headers.empty());

    const auto unprepare = std::find(api->events.begin(), api->events.end(), "unprepare-output");
    const auto close = std::find(api->events.begin(), api->events.end(), "close-output");
    TAUREON_REQUIRE(unprepare != api->events.end());
    TAUREON_REQUIRE(close != api->events.end());
    TAUREON_REQUIRE(unprepare < close);

    const auto diagnostics = transport.diagnostics();
    TAUREON_REQUIRE(diagnostics.native_callbacks >= 5);
    TAUREON_REQUIRE(diagnostics.delivered_messages == 1);
    TAUREON_REQUIRE(diagnostics.transmitted_messages == 2);
    TAUREON_REQUIRE(diagnostics.dropped_events == 1);
    TAUREON_REQUIRE(diagnostics.queue_high_water_mark >= 1);
}

void ordered_transport_loss_reaches_sysex_capture() {
    auto api = std::make_shared<RealtimeApi>();
    WinmmTransport transport(api);
    const auto endpoints = transport.enumerate();
    TAUREON_REQUIRE(endpoints);

    CaptureRecorder recorder;
    transport.set_stream_event_handler(
        [&](const MidiStreamEvent& event) { recorder.consume(event); });
    TAUREON_REQUIRE(transport.open(both_routes(endpoints.value())));

    // Control: a complete frame is accepted and can be saved.
    api->emit_long({0xF0, 0x7D, 0x01, 0xF7});
    TAUREON_REQUIRE(recorder.wait_for_frames(1));
    auto frames = recorder.frames();
    TAUREON_REQUIRE(frames[0].status == taureon::sysex::SysExFrameStatus::complete);
    TAUREON_REQUIRE(!frames[0].affected_by_data_loss);
    const auto control_path = std::filesystem::current_path() / "stage3-winmm-control.syx";
    std::error_code cleanup_error;
    std::filesystem::remove(control_path, cleanup_error);
    std::filesystem::remove(control_path.string() + ".taureon.tmp", cleanup_error);
    TAUREON_REQUIRE(taureon::sysex::save_syx_frames(control_path, {frames[0]}));
    std::filesystem::remove(control_path, cleanup_error);

    // MIM_LONGERROR is ordered between the opening and closing fragments.
    api->emit_long({0xF0, 0x01});
    api->emit_long({}, MIM_LONGERROR);
    TAUREON_REQUIRE(recorder.wait_for_losses(1));
    api->emit_long({0x02, 0xF7});
    TAUREON_REQUIRE(recorder.wait_for_frames(2));
    frames = recorder.frames();
    TAUREON_REQUIRE(frames[1].status == taureon::sysex::SysExFrameStatus::malformed);
    TAUREON_REQUIRE(frames[1].affected_by_data_loss);
    TAUREON_REQUIRE(!taureon::sysex::save_syx_frames(control_path, {frames[1]}, true));

    // A later independent frame must not inherit the earlier loss marker.
    api->emit_long({0xF0, 0x03, 0xF7});
    TAUREON_REQUIRE(recorder.wait_for_frames(3));
    frames = recorder.frames();
    TAUREON_REQUIRE(frames[2].status == taureon::sysex::SysExFrameStatus::complete);
    TAUREON_REQUIRE(!frames[2].affected_by_data_loss);

    // MIM_ERROR is visible transport evidence, but it is a short-message error and
    // therefore does not taint an otherwise complete SysEx frame.
    api->emit_long({0xF0, 0x04});
    api->emit_short_error(0x00013C90u, 44);
    TAUREON_REQUIRE(recorder.wait_for_losses(2));
    api->emit_long({0xF7});
    TAUREON_REQUIRE(recorder.wait_for_frames(4));
    frames = recorder.frames();
    TAUREON_REQUIRE(frames[3].status == taureon::sysex::SysExFrameStatus::complete);
    TAUREON_REQUIRE(!frames[3].affected_by_data_loss);

    // A failed midiInAddBuffer requeue is emitted after the delivered fragment and
    // before the next fragment, so the active capture is rejected.
    api->fail_one_input_requeue();
    api->emit_long({0xF0, 0x05});
    TAUREON_REQUIRE(recorder.wait_for_losses(3));
    api->emit_long({0xF7});
    TAUREON_REQUIRE(recorder.wait_for_frames(5));
    frames = recorder.frames();
    TAUREON_REQUIRE(frames[4].status == taureon::sysex::SysExFrameStatus::malformed);
    TAUREON_REQUIRE(frames[4].affected_by_data_loss);

    // If no later data arrives, close still drains the ordered loss marker and
    // finish() reports an unusable capture rather than silently discarding it.
    api->emit_long({0xF0, 0x06});
    api->emit_long({}, MIM_LONGERROR);
    TAUREON_REQUIRE(recorder.wait_for_losses(4));
    TAUREON_REQUIRE(transport.close());
    recorder.finish();
    frames = recorder.frames();
    TAUREON_REQUIRE(frames.size() == 6);
    TAUREON_REQUIRE(frames[5].status == taureon::sysex::SysExFrameStatus::malformed);
    TAUREON_REQUIRE(frames[5].affected_by_data_loss);

    const auto losses = recorder.losses();
    TAUREON_REQUIRE(losses[0].reason == MidiDataLossReason::native_long_error);
    TAUREON_REQUIRE(losses[0].affects_sysex);
    TAUREON_REQUIRE(losses[1].reason == MidiDataLossReason::native_short_error);
    TAUREON_REQUIRE(!losses[1].affects_sysex);
    TAUREON_REQUIRE(losses[2].reason == MidiDataLossReason::input_requeue_failure);
    TAUREON_REQUIRE(losses[2].affects_sysex);
    TAUREON_REQUIRE(losses[3].reason == MidiDataLossReason::native_long_error);
}

void sustained_queue_overflow_taints_each_affected_frame() {
    auto api = std::make_shared<RealtimeApi>();
    WinmmTransport transport(api);
    const auto endpoints = transport.enumerate();
    TAUREON_REQUIRE(endpoints);

    CaptureRecorder recorder;
    transport.set_stream_event_handler(
        [&](const MidiStreamEvent& event) { recorder.consume(event); });
    TAUREON_REQUIRE(transport.open(both_routes(endpoints.value())));

    api->emit_long({0xF0, 0x11});
    TAUREON_REQUIRE(recorder.wait_for_events(1));

    api->block_one_short_send();
    bool send_succeeded{};
    std::thread blocked_sender([&] {
        send_succeeded = static_cast<bool>(transport.send(
            {MidiBackend::winmm, Midi1NativeMessage{{0x90, 0x40, 0x01}}, std::nullopt}));
    });
    TAUREON_REQUIRE(api->wait_until_short_send_is_blocked());
    for (std::size_t index = 0; index < 1100; ++index) {
        api->emit_short(0x000000F8u, static_cast<DWORD>(index));
    }

    // Long-header returns are non-droppable. Each later run of dropped callbacks must place a
    // fresh ordered loss marker before the next frame instead of globally coalescing the entire
    // blocked-worker interval into the first marker.
    api->emit_long({0xF7});
    api->emit_short(0x000000F8u, 2001);
    api->emit_long({0xF0, 0x12, 0xF7});
    api->release_blocked_short_send();
    blocked_sender.join();
    TAUREON_REQUIRE(send_succeeded);
    TAUREON_REQUIRE(recorder.wait_for_losses(2));
    TAUREON_REQUIRE(recorder.wait_for_frames(2));
    auto frames = recorder.frames();
    TAUREON_REQUIRE(frames[0].status == taureon::sysex::SysExFrameStatus::malformed);
    TAUREON_REQUIRE(frames[0].affected_by_data_loss);
    TAUREON_REQUIRE(frames[1].status == taureon::sysex::SysExFrameStatus::malformed);
    TAUREON_REQUIRE(frames[1].affected_by_data_loss);
    const auto losses = recorder.losses();
    TAUREON_REQUIRE(losses.size() == 2);
    TAUREON_REQUIRE(std::all_of(losses.begin(), losses.end(), [](const auto& loss) {
        return loss.reason == MidiDataLossReason::queue_overflow && loss.affects_sysex;
    }));

    // Once the overflow episode has drained, a new independent frame remains clean.
    api->emit_long({0xF0, 0x13, 0xF7});
    TAUREON_REQUIRE(recorder.wait_for_frames(3));
    frames = recorder.frames();
    TAUREON_REQUIRE(frames[2].status == taureon::sysex::SysExFrameStatus::complete);
    TAUREON_REQUIRE(!frames[2].affected_by_data_loss);
    TAUREON_REQUIRE(transport.close());
}

} // namespace

int main() {
    return taureon::test::run([] {
        realtime_send_receive_and_owned_completion();
        ordered_transport_loss_reaches_sysex_capture();
        sustained_queue_overflow_taints_each_affected_frame();
    });
}
