#pragma once

#include "WinmmMidiHeader.hpp"

#include <memory>

namespace taureon::midi::winmm {

class IWinmmTransportApi : public IWinmmHeaderApi {
public:
    ~IWinmmTransportApi() override = default;

    [[nodiscard]] virtual UINT input_device_count() const noexcept = 0;
    [[nodiscard]] virtual UINT output_device_count() const noexcept = 0;
    [[nodiscard]] virtual MMRESULT input_device_caps(UINT index, MIDIINCAPSW& capabilities) = 0;
    [[nodiscard]] virtual MMRESULT output_device_caps(UINT index, MIDIOUTCAPSW& capabilities) = 0;
    [[nodiscard]] virtual MMRESULT open_input(HMIDIIN& handle, UINT index, DWORD_PTR callback,
                                              DWORD_PTR instance) = 0;
    [[nodiscard]] virtual MMRESULT start_input(HMIDIIN handle) = 0;
    [[nodiscard]] virtual MMRESULT stop_input(HMIDIIN handle) = 0;
    [[nodiscard]] virtual MMRESULT reset_input(HMIDIIN handle) = 0;
    [[nodiscard]] virtual MMRESULT close_input(HMIDIIN handle) = 0;
    [[nodiscard]] virtual MMRESULT open_output(HMIDIOUT& handle, UINT index) = 0;
    [[nodiscard]] virtual MMRESULT reset_output(HMIDIOUT handle) = 0;
    [[nodiscard]] virtual MMRESULT close_output(HMIDIOUT handle) = 0;
};

class NativeWinmmTransportApi final : public IWinmmTransportApi {
public:
    MMRESULT prepare_input(HMIDIIN handle, MIDIHDR& header) override;
    MMRESULT add_input(HMIDIIN handle, MIDIHDR& header) override;
    MMRESULT unprepare_input(HMIDIIN handle, MIDIHDR& header) override;
    MMRESULT prepare_output(HMIDIOUT handle, MIDIHDR& header) override;
    MMRESULT send_output(HMIDIOUT handle, MIDIHDR& header) override;
    MMRESULT unprepare_output(HMIDIOUT handle, MIDIHDR& header) override;

    UINT input_device_count() const noexcept override;
    UINT output_device_count() const noexcept override;
    MMRESULT input_device_caps(UINT index, MIDIINCAPSW& capabilities) override;
    MMRESULT output_device_caps(UINT index, MIDIOUTCAPSW& capabilities) override;
    MMRESULT open_input(HMIDIIN& handle, UINT index, DWORD_PTR callback,
                        DWORD_PTR instance) override;
    MMRESULT start_input(HMIDIIN handle) override;
    MMRESULT stop_input(HMIDIIN handle) override;
    MMRESULT reset_input(HMIDIIN handle) override;
    MMRESULT close_input(HMIDIIN handle) override;
    MMRESULT open_output(HMIDIOUT& handle, UINT index) override;
    MMRESULT reset_output(HMIDIOUT handle) override;
    MMRESULT close_output(HMIDIOUT handle) override;

private:
    NativeWinmmHeaderApi header_api_;
};

using WinmmTransportApiPtr = std::shared_ptr<IWinmmTransportApi>;

} // namespace taureon::midi::winmm
