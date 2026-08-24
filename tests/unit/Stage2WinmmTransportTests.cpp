#include "TestSupport.hpp"

#include "transports/winmm/WinmmTransport.hpp"

#include <windows.h>
#include <mmsystem.h>

#include <cstdint>
#include <cwchar>
#include <memory>
#include <string>
#include <vector>

using namespace taureon::midi;
using namespace taureon::midi::winmm;

namespace {

class SubmitFailureApi final : public IWinmmTransportApi {
public:
    MMRESULT prepare_input(HMIDIIN handle, MIDIHDR&) override {
        events.emplace_back("prepare");
        if (!handle_valid || handle != input_handle) return MMSYSERR_INVALHANDLE;
        ++prepared_headers;
        return MMSYSERR_NOERROR;
    }

    MMRESULT add_input(HMIDIIN handle, MIDIHDR&) override {
        events.emplace_back("submit-failure");
        return handle_valid && handle == input_handle ? MMSYSERR_ERROR : MMSYSERR_INVALHANDLE;
    }

    MMRESULT unprepare_input(HMIDIIN handle, MIDIHDR&) override {
        events.emplace_back("unprepare");
        if (!handle_valid || handle != input_handle) {
            unprepare_after_close = true;
            return MMSYSERR_INVALHANDLE;
        }
        --prepared_headers;
        return MMSYSERR_NOERROR;
    }

    MMRESULT prepare_output(HMIDIOUT, MIDIHDR&) override { return MMSYSERR_NOTSUPPORTED; }
    MMRESULT send_output(HMIDIOUT, MIDIHDR&) override { return MMSYSERR_NOTSUPPORTED; }
    MMRESULT unprepare_output(HMIDIOUT, MIDIHDR&) override { return MMSYSERR_NOTSUPPORTED; }

    UINT input_device_count() const noexcept override { return 1; }
    UINT output_device_count() const noexcept override { return 0; }

    MMRESULT input_device_caps(UINT index, MIDIINCAPSW& capabilities) override {
        if (index != 0) return MMSYSERR_BADDEVICEID;
        capabilities.wMid = 1;
        capabilities.wPid = 25;
        capabilities.vDriverVersion = 256;
        wcscpy_s(capabilities.szPname, L"TAUREON Stage 2 submit failure");
        return MMSYSERR_NOERROR;
    }

    MMRESULT output_device_caps(UINT, MIDIOUTCAPSW&) override {
        return MMSYSERR_BADDEVICEID;
    }

    MMRESULT open_input(HMIDIIN& handle, UINT index, DWORD_PTR, DWORD_PTR) override {
        if (index != 0) return MMSYSERR_BADDEVICEID;
        events.emplace_back("open");
        input_handle = reinterpret_cast<HMIDIIN>(static_cast<std::uintptr_t>(0x1234));
        handle = input_handle;
        handle_valid = true;
        return MMSYSERR_NOERROR;
    }

    MMRESULT start_input(HMIDIIN) override {
        events.emplace_back("unexpected-start");
        return MMSYSERR_ERROR;
    }

    MMRESULT stop_input(HMIDIIN handle) override {
        events.emplace_back("stop");
        return handle_valid && handle == input_handle ? MMSYSERR_NOERROR : MMSYSERR_INVALHANDLE;
    }

    MMRESULT reset_input(HMIDIIN handle) override {
        events.emplace_back("reset");
        return handle_valid && handle == input_handle ? MMSYSERR_NOERROR : MMSYSERR_INVALHANDLE;
    }

    MMRESULT close_input(HMIDIIN handle) override {
        events.emplace_back("close");
        if (!handle_valid || handle != input_handle) return MMSYSERR_INVALHANDLE;
        handle_valid = false;
        return MMSYSERR_NOERROR;
    }

    MMRESULT open_output(HMIDIOUT&, UINT) override { return MMSYSERR_NOTSUPPORTED; }
    MMRESULT reset_output(HMIDIOUT) override { return MMSYSERR_NOTSUPPORTED; }
    MMRESULT close_output(HMIDIOUT) override { return MMSYSERR_NOTSUPPORTED; }

    HMIDIIN input_handle{};
    bool handle_valid{};
    bool unprepare_after_close{};
    int prepared_headers{};
    std::vector<std::string> events;
};

void submit_failure_unwinds_before_handle_close() {
    auto api = std::make_shared<SubmitFailureApi>();
    {
        WinmmTransport transport(api);
        const auto endpoints = transport.enumerate();
        TAUREON_REQUIRE(endpoints);
        TAUREON_REQUIRE(endpoints.value().size() == 1);

        const auto opened = transport.open({endpoints.value().front().identity, std::nullopt});
        TAUREON_REQUIRE(!opened);
        TAUREON_REQUIRE(opened.error().code == MidiErrorCode::native_api_error);
        TAUREON_REQUIRE(opened.error().native_api == "midiInAddBuffer");
        TAUREON_REQUIRE(transport.state() == TransportState::failed);

        TAUREON_REQUIRE(!api->unprepare_after_close);
        TAUREON_REQUIRE(api->prepared_headers == 0);
        TAUREON_REQUIRE(!api->handle_valid);
        TAUREON_REQUIRE(api->events == std::vector<std::string>({
            "open", "prepare", "submit-failure", "stop", "reset", "unprepare", "close"}));

        TAUREON_REQUIRE(transport.close());
        TAUREON_REQUIRE(transport.state() == TransportState::closed);
    }

    TAUREON_REQUIRE(!api->unprepare_after_close);
    TAUREON_REQUIRE(api->prepared_headers == 0);
    TAUREON_REQUIRE(!api->handle_valid);
}

} // namespace

int main() {
    return taureon::test::run([] { submit_failure_unwinds_before_handle_close(); });
}
