#include "WinmmMidiHeader.hpp"

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <string>

namespace taureon::midi::winmm {
namespace {

Result<void> mm_result(const MMRESULT result, const char* operation) {
    if (result == MMSYSERR_NOERROR) return Result<void>::success();
    return Result<void>::failure({MidiErrorCode::native_api_error,
                                  std::string(operation) + " failed", operation,
                                  static_cast<std::int64_t>(result)});
}

} // namespace

MMRESULT NativeWinmmHeaderApi::prepare_input(HMIDIIN handle, MIDIHDR& header) {
    return midiInPrepareHeader(handle, &header, sizeof(header));
}

MMRESULT NativeWinmmHeaderApi::add_input(HMIDIIN handle, MIDIHDR& header) {
    return midiInAddBuffer(handle, &header, sizeof(header));
}

MMRESULT NativeWinmmHeaderApi::unprepare_input(HMIDIIN handle, MIDIHDR& header) {
    return midiInUnprepareHeader(handle, &header, sizeof(header));
}

MMRESULT NativeWinmmHeaderApi::prepare_output(HMIDIOUT handle, MIDIHDR& header) {
    return midiOutPrepareHeader(handle, &header, sizeof(header));
}

MMRESULT NativeWinmmHeaderApi::send_output(HMIDIOUT handle, MIDIHDR& header) {
    return midiOutLongMsg(handle, &header, sizeof(header));
}

MMRESULT NativeWinmmHeaderApi::unprepare_output(HMIDIOUT handle, MIDIHDR& header) {
    return midiOutUnprepareHeader(handle, &header, sizeof(header));
}

WinmmInputBuffer::WinmmInputBuffer(IWinmmHeaderApi& api, HMIDIIN handle,
                                   const std::size_t capacity)
    : api_(api), handle_(handle), bytes_(capacity) {
    header_.lpData = reinterpret_cast<LPSTR>(bytes_.data());
    header_.dwBufferLength = static_cast<DWORD>(bytes_.size());
}

WinmmInputBuffer::~WinmmInputBuffer() {
    if (submitted_) std::terminate();
    if (prepared_ && api_.unprepare_input(handle_, header_) != MMSYSERR_NOERROR) std::terminate();
}

Result<void> WinmmInputBuffer::prepare() {
    if (prepared_ || submitted_) {
        return Result<void>::failure(
            {MidiErrorCode::invalid_state, "input header already prepared", {}, std::nullopt});
    }
    const auto result = mm_result(api_.prepare_input(handle_, header_), "midiInPrepareHeader");
    if (result) prepared_ = true;
    return result;
}

Result<void> WinmmInputBuffer::submit() {
    if (!prepared_ || submitted_) {
        return Result<void>::failure(
            {MidiErrorCode::invalid_state, "input header is not submit-ready", {}, std::nullopt});
    }
    header_.dwBytesRecorded = 0;
    const auto result = mm_result(api_.add_input(handle_, header_), "midiInAddBuffer");
    if (result) submitted_ = true;
    return result;
}

void WinmmInputBuffer::mark_returned() noexcept { submitted_ = false; }

Result<void> WinmmInputBuffer::unprepare() {
    if (submitted_) {
        return Result<void>::failure(
            {MidiErrorCode::invalid_state, "cannot unprepare a submitted input header", {}, std::nullopt});
    }
    if (!prepared_) return Result<void>::success();
    const auto result = mm_result(api_.unprepare_input(handle_, header_), "midiInUnprepareHeader");
    if (result) prepared_ = false;
    return result;
}

bool WinmmInputBuffer::submitted() const noexcept { return submitted_; }
MIDIHDR* WinmmInputBuffer::native_header() noexcept { return &header_; }

std::vector<std::uint8_t> WinmmInputBuffer::recorded_bytes() const {
    const auto count = (std::min)(static_cast<std::size_t>(header_.dwBytesRecorded), bytes_.size());
    return {bytes_.begin(), bytes_.begin() + static_cast<std::ptrdiff_t>(count)};
}

WinmmOutputBuffer::WinmmOutputBuffer(IWinmmHeaderApi& api, HMIDIOUT handle,
                                     std::vector<std::uint8_t> payload)
    : api_(api), handle_(handle), payload_(std::move(payload)) {
    header_.lpData = reinterpret_cast<LPSTR>(payload_.data());
    header_.dwBufferLength = static_cast<DWORD>(payload_.size());
}

WinmmOutputBuffer::~WinmmOutputBuffer() {
    if (submitted_) std::terminate();
    if (prepared_ && api_.unprepare_output(handle_, header_) != MMSYSERR_NOERROR) std::terminate();
}

Result<void> WinmmOutputBuffer::prepare() {
    if (prepared_ || submitted_) {
        return Result<void>::failure(
            {MidiErrorCode::invalid_state, "output header already prepared", {}, std::nullopt});
    }
    const auto result = mm_result(api_.prepare_output(handle_, header_), "midiOutPrepareHeader");
    if (result) prepared_ = true;
    return result;
}

Result<void> WinmmOutputBuffer::submit() {
    if (!prepared_ || submitted_) {
        return Result<void>::failure(
            {MidiErrorCode::invalid_state, "output header is not submit-ready", {}, std::nullopt});
    }
    const auto result = mm_result(api_.send_output(handle_, header_), "midiOutLongMsg");
    if (result) submitted_ = true;
    return result;
}

void WinmmOutputBuffer::mark_completed() noexcept { submitted_ = false; }

Result<void> WinmmOutputBuffer::unprepare() {
    if (submitted_) {
        return Result<void>::failure(
            {MidiErrorCode::invalid_state, "cannot unprepare a submitted output header", {}, std::nullopt});
    }
    if (!prepared_) return Result<void>::success();
    const auto result = mm_result(api_.unprepare_output(handle_, header_), "midiOutUnprepareHeader");
    if (result) prepared_ = false;
    return result;
}

bool WinmmOutputBuffer::submitted() const noexcept { return submitted_; }
MIDIHDR* WinmmOutputBuffer::native_header() noexcept { return &header_; }

} // namespace taureon::midi::winmm
