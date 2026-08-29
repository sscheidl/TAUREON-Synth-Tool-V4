#pragma once

#include "core/midi/Result.hpp"
#include "core/sysex/SyxFile.hpp"
#include "profiles/ProfileRegistry.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace taureon::app {

struct SysExManagerFrameReference {
    std::uint64_t item_id{};
    std::size_t frame_index{};

    bool operator==(const SysExManagerFrameReference&) const = default;
};

struct SysExManagerFrameSnapshot {
    sysex::SysExFrame frame;
    std::vector<std::uint8_t> payload_bytes;
    std::string hash;
    std::string payload_hash;
    std::vector<SysExManagerFrameReference> exact_frame_duplicates;
    std::vector<SysExManagerFrameReference> exact_payload_duplicates;
};

struct SysExManagerItemSnapshot {
    std::uint64_t id{};
    std::filesystem::path source_path;
    std::string source_name;
    std::vector<std::uint8_t> raw_bytes;
    std::vector<SysExManagerFrameSnapshot> frames;
    std::string file_hash;
    std::optional<std::uint64_t> exact_file_duplicate_of;
    std::optional<std::string> manufacturer;
    std::optional<std::string> model;
    std::string recognition_message;
    std::size_t complete_frames{};
    std::size_t incomplete_frames{};
    std::size_t malformed_frames{};
    std::size_t tainted_frames{};

    [[nodiscard]] bool is_valid_for_transfer() const noexcept;
};

struct SysExManagerTransferItem {
    std::string source_name;
    sysex::SyxDocument document;
};

struct SysExManagerSnapshot {
    std::vector<SysExManagerItemSnapshot> items;
};

class SysExManager {
public:
    explicit SysExManager(std::shared_ptr<const profiles::ProfileRegistry> registry = {});

    [[nodiscard]] midi::Result<std::uint64_t> add_file(const std::filesystem::path& path);
    [[nodiscard]] midi::Result<void> remove_item(std::uint64_t item_id);
    [[nodiscard]] midi::Result<void> export_frames(
        std::uint64_t item_id, const std::vector<std::size_t>& frame_indices,
        const std::filesystem::path& destination) const;
    [[nodiscard]] midi::Result<void> merge_frames(
        const std::vector<SysExManagerFrameReference>& frames,
        const std::filesystem::path& destination) const;
    [[nodiscard]] std::optional<SysExManagerTransferItem> transferable_item(
        std::uint64_t item_id) const;
    [[nodiscard]] SysExManagerSnapshot snapshot() const;

private:
    [[nodiscard]] static std::string stable_hash(const std::vector<std::uint8_t>& bytes);
    [[nodiscard]] static std::vector<std::uint8_t> payload_bytes(const sysex::SysExFrame& frame);
    [[nodiscard]] static midi::MidiError validation_error(std::string message);
    void refresh_duplicate_evidence();

    std::shared_ptr<const profiles::ProfileRegistry> registry_;
    std::vector<SysExManagerItemSnapshot> items_;
    std::uint64_t next_item_id_{1};
};

} // namespace taureon::app
