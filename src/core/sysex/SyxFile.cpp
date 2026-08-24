#include "SyxFile.hpp"

#include "SysExStreamParser.hpp"

#include <algorithm>
#include <cerrno>
#include <fstream>
#include <iterator>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace taureon::sysex {
namespace {

std::string path_text(const std::filesystem::path& path) {
    const auto utf8 = path.generic_u8string();
    return {utf8.begin(), utf8.end()};
}

midi::MidiError io_error(std::string operation, const std::filesystem::path& path,
                         const std::error_code& error = {}) {
    std::string message = operation + " [" + path_text(path) + "]";
    if (error) message += ": " + error.message();
    return {midi::MidiErrorCode::io_error, std::move(message), "filesystem." + operation,
            error ? std::optional<std::int64_t>(error.value()) : std::nullopt};
}

std::error_code current_stream_error() {
    return errno == 0 ? std::error_code{} : std::error_code(errno, std::generic_category());
}

midi::Result<void> write_atomic(const std::filesystem::path& path,
                                const std::vector<std::uint8_t>& bytes,
                                const bool replace_existing) {
    std::error_code error;
    if (!replace_existing && std::filesystem::exists(path, error)) {
        return midi::Result<void>::failure(io_error("destination already exists", path));
    }
    if (error) {
        return midi::Result<void>::failure(io_error("cannot inspect destination", path, error));
    }

    auto temporary = path;
    temporary += ".taureon.tmp";
    std::filesystem::remove(temporary, error);
    if (error) {
        return midi::Result<void>::failure(
            io_error("cannot remove stale temporary file", temporary, error));
    }
    error.clear();
    {
        errno = 0;
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) {
            return midi::Result<void>::failure(
                io_error("cannot create temporary file", temporary, current_stream_error()));
        }
        stream.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
        stream.flush();
        if (!stream) {
            const auto stream_error = current_stream_error();
            stream.close();
            std::filesystem::remove(temporary, error);
            return midi::Result<void>::failure(
                io_error("failed to write temporary file", temporary, stream_error));
        }
    }

#ifdef _WIN32
    const DWORD flags = MOVEFILE_WRITE_THROUGH | (replace_existing ? MOVEFILE_REPLACE_EXISTING : 0);
    if (!MoveFileExW(temporary.c_str(), path.c_str(), flags)) {
        const auto native = GetLastError();
        const std::error_code move_error(static_cast<int>(native), std::system_category());
        std::filesystem::remove(temporary, error);
        auto result = io_error("cannot replace destination from temporary file " +
                                   path_text(temporary),
                               path, move_error);
        result.native_api = "MoveFileExW";
        return midi::Result<void>::failure(std::move(result));
    }
#else
    if (replace_existing) std::filesystem::remove(path, error);
    std::filesystem::rename(temporary, path, error);
    if (error) {
        const auto rename_error = error;
        std::filesystem::remove(temporary, error);
        return midi::Result<void>::failure(
            io_error("cannot replace destination from temporary file " + path_text(temporary),
                     path, rename_error));
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
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error) {
        return midi::Result<SyxDocument>::failure(
            io_error("cannot inspect input file", path, error));
    }
    if (!exists) {
        return midi::Result<SyxDocument>::failure(
            io_error("cannot open input file", path,
                     std::make_error_code(std::errc::no_such_file_or_directory)));
    }
    if (std::filesystem::is_directory(path, error)) {
        return midi::Result<SyxDocument>::failure(
            io_error("cannot open input file", path,
                     std::make_error_code(std::errc::is_a_directory)));
    }
    if (error) {
        return midi::Result<SyxDocument>::failure(
            io_error("cannot inspect input file type", path, error));
    }
    errno = 0;
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return midi::Result<SyxDocument>::failure(
            io_error("cannot open input file", path, current_stream_error()));
    }
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(stream)),
                                    std::istreambuf_iterator<char>());
    if (!stream.eof() && stream.fail()) {
        return midi::Result<SyxDocument>::failure(
            io_error("failed to read input file", path, current_stream_error()));
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
