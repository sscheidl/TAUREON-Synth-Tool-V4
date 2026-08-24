#include "TestSupport.hpp"

#include "transports/winmm/WinmmTransport.hpp"

#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cwchar>
#include <memory>
#include <mutex>
#include <string>
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

    MMRESULT add_input(HMIDIIN handle, MIDIHDR&) override {
        return input_open && handle == input_handle ? MMSYSERR_NOERROR : MMSYSERR_INVALHANDLE;
    }

    MMRESULT unprepare_input(HMIDIIN handle, MIDIHDR& header) override {
        events.emplace_back("unprepare-input");
        if (!input_open || handle != input_handle) return MMSYSERR_INVALHANDLE;
        std::erase(input_headers, &header);
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
        for (auto* header : input_headers) {
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
    std::vector<MIDIHDR*> input_headers;
    std::vector<DWORD> short_messages;
    std::vector<std::vector<std::uint8_t>> long_messages;
    std::vector<std::string> events;
};

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

} // namespace

int main() {
    return taureon::test::run([] { realtime_send_receive_and_owned_completion(); });
}
