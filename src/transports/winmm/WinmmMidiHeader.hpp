#pragma once

#include "core/midi/Result.hpp"

#include <windows.h>
#include <mmsystem.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace taureon::midi::winmm {

class IWinmmHeaderApi {
public:
    virtual ~IWinmmHeaderApi() = default;
    virtual MMRESULT prepare_input(HMIDIIN handle, MIDIHDR& header) = 0;
    virtual MMRESULT add_input(HMIDIIN handle, MIDIHDR& header) = 0;
    virtual MMRESULT unprepare_input(HMIDIIN handle, MIDIHDR& header) = 0;
    virtual MMRESULT prepare_output(HMIDIOUT handle, MIDIHDR& header) = 0;
    virtual MMRESULT send_output(HMIDIOUT handle, MIDIHDR& header) = 0;
    virtual MMRESULT unprepare_output(HMIDIOUT handle, MIDIHDR& header) = 0;
};

class NativeWinmmHeaderApi final : public IWinmmHeaderApi {
public:
    MMRESULT prepare_input(HMIDIIN handle, MIDIHDR& header) override;
    MMRESULT add_input(HMIDIIN handle, MIDIHDR& header) override;
    MMRESULT unprepare_input(HMIDIIN handle, MIDIHDR& header) override;
    MMRESULT prepare_output(HMIDIOUT handle, MIDIHDR& header) override;
    MMRESULT send_output(HMIDIOUT handle, MIDIHDR& header) override;
    MMRESULT unprepare_output(HMIDIOUT handle, MIDIHDR& header) override;
};

class WinmmInputBuffer {
public:
    WinmmInputBuffer(IWinmmHeaderApi& api, HMIDIIN handle, std::size_t capacity);
    ~WinmmInputBuffer();

    WinmmInputBuffer(const WinmmInputBuffer&) = delete;
    WinmmInputBuffer& operator=(const WinmmInputBuffer&) = delete;

    [[nodiscard]] Result<void> prepare();
    [[nodiscard]] Result<void> submit();
    void mark_returned() noexcept;
    [[nodiscard]] Result<void> unprepare();
    [[nodiscard]] bool submitted() const noexcept;
    [[nodiscard]] MIDIHDR* native_header() noexcept;
    [[nodiscard]] std::vector<std::uint8_t> recorded_bytes() const;

private:
    IWinmmHeaderApi& api_;
    HMIDIIN handle_{};
    std::vector<std::uint8_t> bytes_;
    MIDIHDR header_{};
    bool prepared_{};
    bool submitted_{};
};

class WinmmOutputBuffer {
public:
    WinmmOutputBuffer(IWinmmHeaderApi& api, HMIDIOUT handle,
                      std::vector<std::uint8_t> payload);
    ~WinmmOutputBuffer();

    WinmmOutputBuffer(const WinmmOutputBuffer&) = delete;
    WinmmOutputBuffer& operator=(const WinmmOutputBuffer&) = delete;

    [[nodiscard]] Result<void> prepare();
    [[nodiscard]] Result<void> submit();
    void mark_completed() noexcept;
    [[nodiscard]] Result<void> unprepare();
    [[nodiscard]] bool submitted() const noexcept;
    [[nodiscard]] MIDIHDR* native_header() noexcept;

private:
    IWinmmHeaderApi& api_;
    HMIDIOUT handle_{};
    std::vector<std::uint8_t> payload_;
    MIDIHDR header_{};
    bool prepared_{};
    bool submitted_{};
};

} // namespace taureon::midi::winmm
