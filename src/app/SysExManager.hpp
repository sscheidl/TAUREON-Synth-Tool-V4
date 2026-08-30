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
#include <string_view>
#include <vector>

namespace taureon::app {

struct SysExManagerFrameReference {
    std::uint64_t item_id{};
    std::size_t frame_index{};

    bool operator==(const SysExManagerFrameReference&) const = default;
};

// Refresh-safe presentation metadata. It intentionally contains no raw SysEx bytes.
struct SysExManagerFrameSnapshot {
    sysex::SysExFrameStatus status{sysex::SysExFrameStatus::incomplete};
    std::size_t byte_count{};
    std::string issue;
    std::optional<std::uint8_t> group;
    bool affected_by_data_loss{};
    std::string hash;
    std::string payload_hash;
    std::vector<SysExManagerFrameReference> exact_frame_duplicates;
    std::vector<SysExManagerFrameReference> exact_payload_duplicates;
};

struct SysExManagerItemSnapshot {
    std::uint64_t id{};
    std::string source_name;
    std::uint64_t byte_count{};
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
    inline static constexpr std::size_t kMaxDocumentRawBytes = 256U * 1024U * 1024U;
    inline static constexpr std::size_t kMaxAggregateRawBytes = 512U * 1024U * 1024U;

    explicit SysExManager(std::shared_ptr<const profiles::ProfileRegistry> registry = {});

    [[nodiscard]] midi::Result<std::uint64_t> add_file(const std::filesystem::path& path);
    // Supports an already captured/imported document without exposing mutable workspace bytes.
    [[nodiscard]] midi::Result<std::uint64_t> add_document(
        sysex::SyxDocument document, std::string source_name);
    [[nodiscard]] midi::Result<void> remove_item(std::uint64_t item_id);
    [[nodiscard]] midi::Result<void> export_frames(
        std::uint64_t item_id, const std::vector<std::size_t>& frame_indices,
        const std::filesystem::path& destination, bool replace_existing = false) const;
    [[nodiscard]] midi::Result<void> merge_frames(
        const std::vector<SysExManagerFrameReference>& frames,
        const std::filesystem::path& destination, bool replace_existing = false) const;
    [[nodiscard]] bool can_transfer(std::uint64_t item_id) const noexcept;
    // Copies one selected frame only; table refreshes never request raw frame bytes.
    [[nodiscard]] midi::Result<std::vector<std::uint8_t>> frame_bytes(
        const SysExManagerFrameReference& reference) const;
    [[nodiscard]] std::optional<SysExManagerTransferItem> transferable_item(
        std::uint64_t item_id) const;
    [[nodiscard]] SysExManagerSnapshot snapshot() const;

private:
    struct StoredFrame {
        sysex::SysExFrame frame;
        std::vector<std::uint8_t> payload_bytes;
        std::string hash;
        std::string payload_hash;
        std::vector<SysExManagerFrameReference> exact_frame_duplicates;
        std::vector<SysExManagerFrameReference> exact_payload_duplicates;
    };

    struct StoredItem {
        std::uint64_t id{};
        std::string source_name;
        std::vector<std::uint8_t> raw_bytes;
        std::vector<StoredFrame> frames;
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

    [[nodiscard]] static std::string stable_hash(const std::vector<std::uint8_t>& bytes);
    [[nodiscard]] static std::vector<std::uint8_t> payload_bytes(const sysex::SysExFrame& frame);
    [[nodiscard]] static midi::MidiError error(midi::MidiErrorCode code, std::string message);
    [[nodiscard]] static SysExManagerFrameSnapshot summary_of(const StoredFrame& frame);
    [[nodiscard]] static SysExManagerItemSnapshot summary_of(const StoredItem& item);
    [[nodiscard]] static bool exceeds_limit(std::size_t current, std::size_t addition,
                                            std::size_t limit) noexcept;
    [[nodiscard]] static midi::MidiError resource_limit_error(std::size_t actual,
                                                               std::size_t limit,
                                                               std::string_view subject);
    void refresh_duplicate_evidence();

    std::shared_ptr<const profiles::ProfileRegistry> registry_;
    std::vector<StoredItem> items_;
    std::uint64_t next_item_id_{1};
    std::size_t loaded_raw_bytes_{};
};

} // namespace taureon::app
