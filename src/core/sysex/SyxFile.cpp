#include "SyxFile.hpp"

#include <algorithm>
#include "SysExStreamParser.hpp"

#include <fstream>
#include <iterator>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace taureon::sysex {
namespace {

midi::MidiError io_error(std::string message) {
    return {midi::MidiErrorCode::io_error, std::move(message), "filesystem", std::nullopt};
}

midi::Result<void> write_atomic(const std::filesystem::path& path,
                                const std::vector<std::uint8_t>& bytes,
                                const bool replace_existing) {
    std::error_code error;
    if (!replace_existing && std::filesystem::exists(path, error)) {
        return midi::Result<void>::failure(io_error("destination already exists"));
    }
    if (error) return midi::Result<void>::failure(io_error("cannot inspect destination"));

    auto temporary = path;
    temporary += ".taureon.tmp";
    std::filesystem::remove(temporary, error);
    error.clear();
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) return midi::Result<void>::failure(io_error("cannot create temporary .syx file"));
        stream.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
        stream.flush();
        if (!stream) {
            stream.close();
            std::filesystem::remove(temporary, error);
            return midi::Result<void>::failure(io_error("failed to write temporary .syx file"));
        }
    }

#ifdef _WIN32
    const DWORD flags = MOVEFILE_WRITE_THROUGH | (replace_existing ? MOVEFILE_REPLACE_EXISTING : 0);
    if (!MoveFileExW(temporary.c_str(), path.c_str(), flags)) {
        const auto native = static_cast<std::int64_t>(GetLastError());
        std::filesystem::remove(temporary, error);
        return midi::Result<void>::failure(
            {midi::MidiErrorCode::io_error, "atomic .syx replacement failed", "MoveFileExW", native});
    }
#else
    if (replace_existing) std::filesystem::remove(path, error);
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(temporary, error);
        return midi::Result<void>::failure(io_error("atomic .syx replacement failed"));
    }
#endif
    return midi::Result<void>::success();
}

} // namespace

bool SyxDocument::all_complete() const noexcept {
    return !frames.empty() &&
           std::all_of(frames.begin(), frames.end(), [](const SysExFrame& frame) {
               return frame.status == SysExFrameStatus::complete && !frame.affected_by_data_loss;
           });
}

std::size_t SyxDocument::complete_frame_count() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        frames.begin(), frames.end(),
        [](const SysExFrame& frame) { return frame.status == SysExFrameStatus::complete; }));
}

midi::Result<SyxDocument> load_syx_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return midi::Result<SyxDocument>::failure(io_error("cannot open .syx file"));
    }
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(stream)),
                                    std::istreambuf_iterator<char>());
    if (!stream.eof() && stream.fail()) {
        return midi::Result<SyxDocument>::failure(io_error("failed to read .syx file"));
    }
    SysExStreamParser parser;
    auto batch = parser.consume(bytes);
    auto final = parser.finish();
    batch.frames.insert(batch.frames.end(), std::make_move_iterator(final.begin()),
                        std::make_move_iterator(final.end()));
    return midi::Result<SyxDocument>::success({std::move(bytes), std::move(batch.frames)});
}

midi::Result<void> save_syx_frames(const std::filesystem::path& path,
                                    const std::vector<SysExFrame>& frames,
                                    const bool replace_existing) {
    if (frames.empty()) {
        return midi::Result<void>::failure(
            {midi::MidiErrorCode::incomplete_data, "no SysEx frames to save", {}, std::nullopt});
    }
    std::vector<std::uint8_t> bytes;
    for (const auto& frame : frames) {
        if (frame.status != SysExFrameStatus::complete || frame.affected_by_data_loss) {
            return midi::Result<void>::failure(
                {midi::MidiErrorCode::incomplete_data,
                 "normal .syx save accepts complete unaffected frames only", {}, std::nullopt});
        }
        bytes.insert(bytes.end(), frame.bytes.begin(), frame.bytes.end());
    }
    return write_atomic(path, bytes, replace_existing);
}

midi::Result<void> save_syx_raw(const std::filesystem::path& path, const SyxDocument& document,
                                 const bool allow_noncomplete, const bool replace_existing) {
    if (!allow_noncomplete && !document.all_complete()) {
        return midi::Result<void>::failure(
            {midi::MidiErrorCode::incomplete_data,
             "raw .syx save requires explicit permission for noncomplete data", {}, std::nullopt});
    }
    return write_atomic(path, document.raw_bytes, replace_existing);
}

} // namespace taureon::sysex
