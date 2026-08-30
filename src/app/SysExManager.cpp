#include "app/SysExManager.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace taureon::app {
namespace {

bool is_syx_path(const std::filesystem::path& path) {
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](const unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return extension == ".syx";
}

std::string file_name(const std::filesystem::path& path) {
    const auto native = path.filename().u8string();
    return {native.begin(), native.end()};
}

bool frame_is_verified_complete(const sysex::SysExFrame& frame) {
    return frame.status == sysex::SysExFrameStatus::complete && !frame.affected_by_data_loss;
}

using FrameIndex = std::pair<std::size_t, std::size_t>;

} // namespace

bool SysExManagerItemSnapshot::is_valid_for_transfer() const noexcept {
    return item_is_valid_for_transfer(*this);
}

bool SysExManager::StoredItem::is_valid_for_transfer() const noexcept {
    return item_is_valid_for_transfer(*this);
}

SysExManager::SysExManager(std::shared_ptr<const profiles::ProfileRegistry> registry)
    : registry_(std::move(registry)) {}

midi::Result<std::uint64_t> SysExManager::add_file(const std::filesystem::path& path) {
    if (!is_syx_path(path)) {
        return midi::Result<std::uint64_t>::failure(
            error(midi::MidiErrorCode::invalid_argument, "SysEx Manager accepts .syx files only"));
    }
    auto loaded = sysex::load_syx_file(path);
    if (!loaded) return midi::Result<std::uint64_t>::failure(loaded.error());
    return add_document(std::move(loaded.value()), file_name(path));
}

midi::Result<std::uint64_t> SysExManager::add_document(sysex::SyxDocument document,
                                                        std::string source_name) {
    if (source_name.empty()) {
        return midi::Result<std::uint64_t>::failure(
            error(midi::MidiErrorCode::invalid_argument, "SysEx workspace source name is required"));
    }

    StoredItem item;
    item.id = next_item_id_++;
    item.source_name = std::move(source_name);
    item.raw_bytes = std::move(document.raw_bytes);
    item.file_hash = stable_hash(item.raw_bytes);
    item.frames.reserve(document.frames.size());
    for (auto& frame : document.frames) {
        if (frame.status == sysex::SysExFrameStatus::complete) {
            ++item.complete_frames;
        } else if (frame.status == sysex::SysExFrameStatus::incomplete) {
            ++item.incomplete_frames;
        } else {
            ++item.malformed_frames;
        }
        if (frame.affected_by_data_loss) ++item.tainted_frames;

        StoredFrame stored;
        stored.payload_bytes = payload_bytes(frame);
        stored.hash = stable_hash(frame.bytes);
        if (!stored.payload_bytes.empty()) stored.payload_hash = stable_hash(stored.payload_bytes);
        stored.frame = std::move(frame);
        item.frames.push_back(std::move(stored));
    }

    item.recognition_message = "No verified device evidence";
    if (registry_) {
        const auto complete = std::find_if(item.frames.begin(), item.frames.end(),
                                           [](const auto& candidate) {
                                               return frame_is_verified_complete(candidate.frame);
                                           });
        if (complete != item.frames.end()) {
            const auto match = registry_->match(complete->frame);
            item.recognition_message = match.message;
            if (match.selected_profile_id) {
                if (const auto* profile = registry_->find(*match.selected_profile_id); profile &&
                    !profile->generic) {
                    item.manufacturer = profile->manufacturer;
                    item.model = profile->model;
                }
            }
        } else {
            item.recognition_message =
                "Device identification rejected: no verified complete SysEx frame";
        }
    }

    const auto id = item.id;
    items_.push_back(std::move(item));
    refresh_duplicate_evidence();
    return midi::Result<std::uint64_t>::success(id);
}

midi::Result<void> SysExManager::remove_item(const std::uint64_t item_id) {
    const auto item = std::find_if(items_.begin(), items_.end(),
                                   [item_id](const auto& candidate) { return candidate.id == item_id; });
    if (item == items_.end()) {
        return midi::Result<void>::failure(
            error(midi::MidiErrorCode::not_found, "SysEx workspace item was not found"));
    }
    items_.erase(item);
    refresh_duplicate_evidence();
    return midi::Result<void>::success();
}

midi::Result<void> SysExManager::export_frames(
    const std::uint64_t item_id, const std::vector<std::size_t>& frame_indices,
    const std::filesystem::path& destination, const bool replace_existing) const {
    const auto item = std::find_if(items_.begin(), items_.end(),
                                   [item_id](const auto& candidate) { return candidate.id == item_id; });
    if (item == items_.end()) {
        return midi::Result<void>::failure(
            error(midi::MidiErrorCode::not_found, "SysEx workspace item was not found"));
    }
    if (frame_indices.empty()) {
        return midi::Result<void>::failure(
            error(midi::MidiErrorCode::invalid_argument, "Select at least one frame to export"));
    }

    std::vector<sysex::SysExFrame> selected;
    selected.reserve(frame_indices.size());
    for (const auto index : frame_indices) {
        if (index >= item->frames.size()) {
            return midi::Result<void>::failure(
                error(midi::MidiErrorCode::invalid_argument, "Selected SysEx frame is unavailable"));
        }
        const auto& frame = item->frames.at(index).frame;
        if (!frame_is_verified_complete(frame)) {
            return midi::Result<void>::failure(error(
                midi::MidiErrorCode::incomplete_data,
                "Only verified complete, unaffected frames can be exported as a normal .syx file"));
        }
        selected.push_back(frame);
    }
    return sysex::save_syx_frames(destination, selected, replace_existing);
}

midi::Result<void> SysExManager::merge_frames(
    const std::vector<SysExManagerFrameReference>& references,
    const std::filesystem::path& destination, const bool replace_existing) const {
    if (references.empty()) {
        return midi::Result<void>::failure(
            error(midi::MidiErrorCode::invalid_argument, "Select at least one frame to merge"));
    }
    std::vector<sysex::SysExFrame> selected;
    selected.reserve(references.size());
    for (const auto& reference : references) {
        const auto item = std::find_if(items_.begin(), items_.end(), [&reference](const auto& candidate) {
            return candidate.id == reference.item_id;
        });
        if (item == items_.end()) {
            return midi::Result<void>::failure(
                error(midi::MidiErrorCode::not_found, "SysEx workspace item was not found"));
        }
        if (reference.frame_index >= item->frames.size()) {
            return midi::Result<void>::failure(
                error(midi::MidiErrorCode::invalid_argument, "Selected SysEx frame is unavailable"));
        }
        const auto& frame = item->frames.at(reference.frame_index).frame;
        if (!frame_is_verified_complete(frame)) {
            return midi::Result<void>::failure(error(
                midi::MidiErrorCode::incomplete_data,
                "Incomplete, malformed, or tainted data cannot be merged as verified .syx"));
        }
        selected.push_back(frame);
    }
    return sysex::save_syx_frames(destination, selected, replace_existing);
}

bool SysExManager::can_transfer(const std::uint64_t item_id) const noexcept {
    const auto item = std::find_if(items_.begin(), items_.end(),
                                   [item_id](const auto& candidate) { return candidate.id == item_id; });
    return item != items_.end() && item->is_valid_for_transfer();
}

midi::Result<std::vector<std::uint8_t>> SysExManager::frame_bytes(
    const SysExManagerFrameReference& reference) const {
    const auto item = std::find_if(items_.begin(), items_.end(), [&reference](const auto& candidate) {
        return candidate.id == reference.item_id;
    });
    if (item == items_.end()) {
        return midi::Result<std::vector<std::uint8_t>>::failure(
            error(midi::MidiErrorCode::not_found, "SysEx workspace item was not found"));
    }
    if (reference.frame_index >= item->frames.size()) {
        return midi::Result<std::vector<std::uint8_t>>::failure(
            error(midi::MidiErrorCode::invalid_argument, "Selected SysEx frame is unavailable"));
    }
    return midi::Result<std::vector<std::uint8_t>>::success(
        item->frames.at(reference.frame_index).frame.bytes);
}

std::optional<SysExManagerTransferItem> SysExManager::transferable_item(
    const std::uint64_t item_id) const {
    const auto item = std::find_if(items_.begin(), items_.end(),
                                   [item_id](const auto& candidate) { return candidate.id == item_id; });
    if (item == items_.end() || !item->is_valid_for_transfer()) return std::nullopt;
    sysex::SyxDocument document;
    document.raw_bytes = item->raw_bytes;
    document.frames.reserve(item->frames.size());
    for (const auto& frame : item->frames) document.frames.push_back(frame.frame);
    return SysExManagerTransferItem{item->source_name, std::move(document)};
}

SysExManagerSnapshot SysExManager::snapshot() const {
    SysExManagerSnapshot result;
    result.items.reserve(items_.size());
    for (const auto& item : items_) result.items.push_back(summary_of(item));
    return result;
}

std::string SysExManager::stable_hash(const std::vector<std::uint8_t>& bytes) {
    // This is a deterministic identity aid, not a cryptographic integrity claim.
    std::uint64_t state = 14695981039346656037ULL;
    for (const auto byte : bytes) {
        state ^= byte;
        state *= 1099511628211ULL;
    }
    std::ostringstream stream;
    stream << "fnv1a64:" << std::hex << std::setfill('0') << std::setw(16) << state;
    return stream.str();
}

std::vector<std::uint8_t> SysExManager::payload_bytes(const sysex::SysExFrame& frame) {
    if (!frame_is_verified_complete(frame) || frame.bytes.size() < 2 ||
        frame.bytes.front() != 0xF0 || frame.bytes.back() != 0xF7) {
        return {};
    }
    return {frame.bytes.begin() + 1, frame.bytes.end() - 1};
}

midi::MidiError SysExManager::error(const midi::MidiErrorCode code, std::string message) {
    return {code, std::move(message), "sysex-manager", std::nullopt};
}

SysExManagerFrameSnapshot SysExManager::summary_of(const StoredFrame& frame) {
    return {frame.frame.status, frame.frame.bytes.size(), frame.frame.issue, frame.frame.group,
            frame.frame.affected_by_data_loss, frame.hash, frame.payload_hash,
            frame.exact_frame_duplicates, frame.exact_payload_duplicates};
}

SysExManagerItemSnapshot SysExManager::summary_of(const StoredItem& item) {
    SysExManagerItemSnapshot result;
    result.id = item.id;
    result.source_name = item.source_name;
    result.byte_count = item.raw_bytes.size();
    result.frames.reserve(item.frames.size());
    for (const auto& frame : item.frames) result.frames.push_back(summary_of(frame));
    result.file_hash = item.file_hash;
    result.exact_file_duplicate_of = item.exact_file_duplicate_of;
    result.manufacturer = item.manufacturer;
    result.model = item.model;
    result.recognition_message = item.recognition_message;
    result.complete_frames = item.complete_frames;
    result.incomplete_frames = item.incomplete_frames;
    result.malformed_frames = item.malformed_frames;
    result.tainted_frames = item.tainted_frames;
    return result;
}

void SysExManager::refresh_duplicate_evidence() {
    for (auto& item : items_) {
        item.exact_file_duplicate_of.reset();
        for (auto& frame : item.frames) {
            frame.exact_frame_duplicates.clear();
            frame.exact_payload_duplicates.clear();
        }
    }

    std::unordered_map<std::string, std::vector<std::size_t>> file_buckets;
    std::unordered_map<std::string, std::vector<FrameIndex>> frame_buckets;
    std::unordered_map<std::string, std::vector<FrameIndex>> payload_buckets;

    for (std::size_t item_index = 0; item_index < items_.size(); ++item_index) {
        auto& item = items_.at(item_index);
        auto& same_file_hash = file_buckets[item.file_hash];
        for (const auto prior_index : same_file_hash) {
            const auto& prior = items_.at(prior_index);
            if (item.raw_bytes == prior.raw_bytes && !item.exact_file_duplicate_of) {
                item.exact_file_duplicate_of = prior.id;
            }
        }
        same_file_hash.push_back(item_index);

        for (std::size_t frame_index = 0; frame_index < item.frames.size(); ++frame_index) {
            auto& frame = item.frames.at(frame_index);
            const SysExManagerFrameReference reference{item.id, frame_index};
            auto& same_frame_hash = frame_buckets[frame.hash];
            for (const auto [prior_item_index, prior_frame_index] : same_frame_hash) {
                auto& prior_item = items_.at(prior_item_index);
                auto& prior = prior_item.frames.at(prior_frame_index);
                if (frame.frame.bytes == prior.frame.bytes) {
                    frame.exact_frame_duplicates.push_back({prior_item.id, prior_frame_index});
                    prior.exact_frame_duplicates.push_back(reference);
                }
            }
            same_frame_hash.push_back({item_index, frame_index});

            if (frame.payload_hash.empty()) continue;
            auto& same_payload_hash = payload_buckets[frame.payload_hash];
            for (const auto [prior_item_index, prior_frame_index] : same_payload_hash) {
                auto& prior_item = items_.at(prior_item_index);
                auto& prior = prior_item.frames.at(prior_frame_index);
                if (frame.payload_bytes == prior.payload_bytes) {
                    frame.exact_payload_duplicates.push_back({prior_item.id, prior_frame_index});
                    prior.exact_payload_duplicates.push_back(reference);
                }
            }
            same_payload_hash.push_back({item_index, frame_index});
        }
    }
}

} // namespace taureon::app
