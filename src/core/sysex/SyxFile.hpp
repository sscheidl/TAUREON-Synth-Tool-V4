#pragma once

#include "SysExFrame.hpp"
#include "core/midi/Result.hpp"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace taureon::sysex {

struct SyxDocument {
    std::vector<std::uint8_t> raw_bytes;
    std::vector<SysExFrame> frames;

    [[nodiscard]] bool all_complete() const noexcept;
    [[nodiscard]] std::size_t complete_frame_count() const noexcept;
};

[[nodiscard]] midi::Result<SyxDocument> load_syx_file(const std::filesystem::path& path);
[[nodiscard]] midi::Result<void> save_syx_frames(const std::filesystem::path& path,
                                                 const std::vector<SysExFrame>& frames,
                                                 bool replace_existing = false);
[[nodiscard]] midi::Result<void> save_syx_raw(const std::filesystem::path& path,
                                              const SyxDocument& document,
                                              bool allow_noncomplete,
                                              bool replace_existing = false);

} // namespace taureon::sysex
