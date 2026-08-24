#include "TestSupport.hpp"

#include "transports/winmm/WinmmMidiHeader.hpp"

#include <windows.h>
#include <mmsystem.h>

#include <cstdint>
#include <vector>

using namespace taureon::midi::winmm;

namespace {

class FakeHeaderApi final : public IWinmmHeaderApi {
public:
    MMRESULT prepare_input(HMIDIIN, MIDIHDR&) override {
        ++input_prepares;
        return fail_input_prepare ? MMSYSERR_ERROR : MMSYSERR_NOERROR;
    }
    MMRESULT add_input(HMIDIIN, MIDIHDR&) override {
        ++input_submits;
        return fail_input_submit ? MMSYSERR_ERROR : MMSYSERR_NOERROR;
    }
    MMRESULT unprepare_input(HMIDIIN, MIDIHDR&) override {
        ++input_unprepares;
        return fail_input_unprepare ? MMSYSERR_ERROR : MMSYSERR_NOERROR;
    }
    MMRESULT prepare_output(HMIDIOUT, MIDIHDR&) override {
        ++output_prepares;
        return fail_output_prepare ? MMSYSERR_ERROR : MMSYSERR_NOERROR;
    }
    MMRESULT send_output(HMIDIOUT, MIDIHDR&) override {
        ++output_submits;
        return fail_output_submit ? MMSYSERR_ERROR : MMSYSERR_NOERROR;
    }
    MMRESULT unprepare_output(HMIDIOUT, MIDIHDR&) override {
        ++output_unprepares;
        return fail_output_unprepare ? MMSYSERR_ERROR : MMSYSERR_NOERROR;
    }

    bool fail_input_prepare{};
    bool fail_input_submit{};
    bool fail_input_unprepare{};
    bool fail_output_prepare{};
    bool fail_output_submit{};
    bool fail_output_unprepare{};
    int input_prepares{};
    int input_submits{};
    int input_unprepares{};
    int output_prepares{};
    int output_submits{};
    int output_unprepares{};
};

void input_success() {
    FakeHeaderApi api;
    auto handle = reinterpret_cast<HMIDIIN>(static_cast<std::uintptr_t>(1));
    WinmmInputBuffer buffer(api, handle, 1024);
    TAUREON_REQUIRE(buffer.prepare());
    TAUREON_REQUIRE(buffer.submit());
    TAUREON_REQUIRE(buffer.submitted());
    TAUREON_REQUIRE(!buffer.unprepare());
    buffer.mark_returned();
    api.fail_input_unprepare = true;
    TAUREON_REQUIRE(!buffer.unprepare());
    api.fail_input_unprepare = false;
    TAUREON_REQUIRE(buffer.unprepare());
    TAUREON_REQUIRE(api.input_prepares == 1);
    TAUREON_REQUIRE(api.input_submits == 1);
    TAUREON_REQUIRE(api.input_unprepares == 2);
}

void input_error_unwind() {
    FakeHeaderApi api;
    auto handle = reinterpret_cast<HMIDIIN>(static_cast<std::uintptr_t>(1));
    {
        api.fail_input_prepare = true;
        WinmmInputBuffer buffer(api, handle, 128);
        TAUREON_REQUIRE(!buffer.prepare());
    }
    TAUREON_REQUIRE(api.input_unprepares == 0);

    api.fail_input_prepare = false;
    api.fail_input_submit = true;
    {
        WinmmInputBuffer buffer(api, handle, 128);
        TAUREON_REQUIRE(buffer.prepare());
        TAUREON_REQUIRE(!buffer.submit());
    }
    TAUREON_REQUIRE(api.input_unprepares == 1);
}

void output_success_and_error_unwind() {
    FakeHeaderApi api;
    auto handle = reinterpret_cast<HMIDIOUT>(static_cast<std::uintptr_t>(2));
    {
        WinmmOutputBuffer buffer(api, handle, {0xf0, 0x7d, 0xf7});
        TAUREON_REQUIRE(buffer.prepare());
        TAUREON_REQUIRE(buffer.submit());
        TAUREON_REQUIRE(!buffer.unprepare());
        buffer.mark_completed();
        api.fail_output_unprepare = true;
        TAUREON_REQUIRE(!buffer.unprepare());
        api.fail_output_unprepare = false;
        TAUREON_REQUIRE(buffer.unprepare());
    }
    TAUREON_REQUIRE(api.output_unprepares == 2);

    api.fail_output_submit = true;
    {
        WinmmOutputBuffer buffer(api, handle, {1, 2, 3});
        TAUREON_REQUIRE(buffer.prepare());
        TAUREON_REQUIRE(!buffer.submit());
    }
    TAUREON_REQUIRE(api.output_unprepares == 3);
}

} // namespace

int main() {
    return taureon::test::run([] {
        input_success();
        input_error_unwind();
        output_success_and_error_unwind();
    });
}
