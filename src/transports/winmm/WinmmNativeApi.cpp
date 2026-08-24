#include "WinmmNativeApi.hpp"

namespace taureon::midi::winmm {

MMRESULT NativeWinmmTransportApi::prepare_input(HMIDIIN handle, MIDIHDR& header) {
    return header_api_.prepare_input(handle, header);
}

MMRESULT NativeWinmmTransportApi::add_input(HMIDIIN handle, MIDIHDR& header) {
    return header_api_.add_input(handle, header);
}

MMRESULT NativeWinmmTransportApi::unprepare_input(HMIDIIN handle, MIDIHDR& header) {
    return header_api_.unprepare_input(handle, header);
}

MMRESULT NativeWinmmTransportApi::prepare_output(HMIDIOUT handle, MIDIHDR& header) {
    return header_api_.prepare_output(handle, header);
}

MMRESULT NativeWinmmTransportApi::send_output(HMIDIOUT handle, MIDIHDR& header) {
    return header_api_.send_output(handle, header);
}

MMRESULT NativeWinmmTransportApi::unprepare_output(HMIDIOUT handle, MIDIHDR& header) {
    return header_api_.unprepare_output(handle, header);
}

UINT NativeWinmmTransportApi::input_device_count() const noexcept { return midiInGetNumDevs(); }

UINT NativeWinmmTransportApi::output_device_count() const noexcept { return midiOutGetNumDevs(); }

MMRESULT NativeWinmmTransportApi::input_device_caps(const UINT index,
                                                     MIDIINCAPSW& capabilities) {
    return midiInGetDevCapsW(index, &capabilities, sizeof(capabilities));
}

MMRESULT NativeWinmmTransportApi::output_device_caps(const UINT index,
                                                      MIDIOUTCAPSW& capabilities) {
    return midiOutGetDevCapsW(index, &capabilities, sizeof(capabilities));
}

MMRESULT NativeWinmmTransportApi::open_input(HMIDIIN& handle, const UINT index,
                                              const DWORD_PTR callback,
                                              const DWORD_PTR instance) {
    return midiInOpen(&handle, index, callback, instance, CALLBACK_FUNCTION);
}

MMRESULT NativeWinmmTransportApi::start_input(HMIDIIN handle) { return midiInStart(handle); }

MMRESULT NativeWinmmTransportApi::stop_input(HMIDIIN handle) { return midiInStop(handle); }

MMRESULT NativeWinmmTransportApi::reset_input(HMIDIIN handle) { return midiInReset(handle); }

MMRESULT NativeWinmmTransportApi::close_input(HMIDIIN handle) { return midiInClose(handle); }

MMRESULT NativeWinmmTransportApi::open_output(HMIDIOUT& handle, const UINT index) {
    return midiOutOpen(&handle, index, 0, 0, CALLBACK_NULL);
}

MMRESULT NativeWinmmTransportApi::reset_output(HMIDIOUT handle) { return midiOutReset(handle); }

MMRESULT NativeWinmmTransportApi::close_output(HMIDIOUT handle) { return midiOutClose(handle); }

} // namespace taureon::midi::winmm
