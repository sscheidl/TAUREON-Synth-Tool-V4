#include "app/SysExManager.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <string_view>

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

} // namespace

bool SysExManagerItemSnapshot::is_valid_for_transfer() const noexcept {
    return !frames.empty() && incomplete_frames == 0 && malformed_frames == 0 &&
           tainted_frames == 0 && complete_frames == frames.size();
}

SysExManager::SysExManager(std::shared_ptr<const profiles::ProfileRegistry> registry)
    : registry_(std::move(registry)) {}

midi::Result<std::uint64_t> SysExManager::add_file(const std::filesystem::path& path) {
    if (!is_syx_path(path)) {
        return midi::Result<std::uint64_t>::failure(
            validation_error("SysEx Manager accepts .syx files only"));
    }
    auto loaded = sysex::load_syx_file(path);
    if (!loaded) return midi::Result<std::uint64_t>::failure(loaded.error());

    SysExManagerItemSnapshot item;
    item.id = next_item_id_++;
    item.source_path = path;
    item.source_name = file_name(path);
    item.raw_bytes = std::move(loaded.value().raw_bytes);
    item.file_hash = stable_hash(item.raw_bytes);
    item.frames.reserve(loaded.value().frames.size());
    for (auto& frame : loaded.value().frames) {
        if (frame.status == sysex::SysExFrameStatus::complete) {
            ++item.complete_frames;
        } else if (frame.status == sysex::SysExFrameStatus::incomplete) {
            ++item.incomplete_frames;
        } else {
            ++item.malformed_frames;
        }
        if (frame.affected_by_data_loss) ++item.tainted_frames;
        item.frames.push_back({std::move(frame), {}, {}, {}, {}});
        auto& stored = item.frames.back();
        stored.hash = stable_hash(stored.frame.bytes);
        stored.payload_hash = stable_hash(payload_bytes(stored.frame));
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
        return midi::Result<void>::failure(validation_error("SysEx workspace item was not found"));
    }
    items_.erase(item);
    refresh_duplicate_evidence();
    return midi::Result<void>::success();
}

midi::Result<void> SysExManager::export_frames(
    const std::uint64_t item_id, const std::vector<std::size_t>& frame_indices,
    const std::filesystem::path& destination) const {
    const auto item = std::find_if(items_.begin(), items_.end(),
                                   [item_id](const auto& candidate) { return candidate.id == item_id; });
    if (item == items_.end()) {
        return midi::Result<void>::failure(validation_error("SysEx workspace item was not found"));
    }
    if (frame_indices.empty()) {
        return midi::Result<void>::failure(validation_error("Select at least one frame to export"));
    }

    std::vector<sysex::SysExFrame> selected;
    selected.reserve(frame_indices.size());
    for (const auto index : frame_indices) {
        if (index >= item->frames.size()) {
            return midi::Result<void>::failure(validation_error("Selected SysEx frame is unavailable"));
        }
        const auto& frame = item->frames.at(index).frame;
        if (!frame_is_verified_complete(frame)) {
            return midi::Result<void>::failure(validation_error(
                "Only verified complete, unaffected frames can be exported as a normal .syx file"));
        }
        selected.push_back(frame);
    }
    return sysex::save_syx_frames(destination, selected);
}

midi::Result<void> SysExManager::merge_frames(
    const std::vector<SysExManagerFrameReference>& references,
    const std::filesystem::path& destination) const {
    if (references.empty()) {
        return midi::Result<void>::failure(validation_error("Select at least one frame to merge"));
    }
    std::vector<sysex::SysExFrame> selected;
    selected.reserve(references.size());
    for (const auto& reference : references) {
        const auto item = std::find_if(items_.begin(), items_.end(), [&reference](const auto& candidate) {
            return candidate.id == reference.item_id;
        });
        if (item == items_.end() || reference.frame_index >= item->frames.size()) {
            return midi::Result<void>::failure(validation_error("Selected SysEx frame is unavailable"));
        }
        const auto& frame = item->frames.at(reference.frame_index).frame;
        if (!frame_is_verified_complete(frame)) {
            return midi::Result<void>::failure(validation_error(
                "Incomplete, malformed, or tainted data cannot be merged as verified .syx"));
        }
        selected.push_back(frame);
    }
    return sysex::save_syx_frames(destination, selected);
}

std::optional<std::filesystem::path> SysExManager::transferable_source(
    const std::uint64_t item_id) const {
    const auto item = std::find_if(items_.begin(), items_.end(),
                                   [item_id](const auto& candidate) { return candidate.id == item_id; });
    if (item == items_.end() || !item->is_valid_for_transfer()) return std::nullopt;
    return item->source_path;
}

SysExManagerSnapshot SysExManager::snapshot() const { return {items_}; }

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

midi::MidiError SysExManager::validation_error(std::string message) {
    return {midi::MidiErrorCode::incomplete_data, std::move(message), "sysex-manager",
            std::nullopt};
}

void SysExManager::refresh_duplicate_evidence() {
    for (auto& item : items_) {
        item.exact_file_duplicate_of.reset();
        for (auto& frame : item.frames) {
            frame.exact_frame_duplicates.clear();
            frame.exact_payload_duplicates.clear();
        }
    }

    for (std::size_t left_item = 0; left_item < items_.size(); ++left_item) {
        for (std::size_t right_item = 0; right_item <= left_item; ++right_item) {
            auto& left = items_.at(left_item);
            auto& right = items_.at(right_item);
            if (right_item < left_item && left.raw_bytes == right.raw_bytes &&
                !left.exact_file_duplicate_of) {
                left.exact_file_duplicate_of = right.id;
            }
            for (std::size_t left_frame = 0; left_frame < left.frames.size(); ++left_frame) {
                const auto right_limit = right_item == left_item ? left_frame : right.frames.size();
                for (std::size_t right_frame = 0; right_frame < right_limit; ++right_frame) {
                    auto& candidate = left.frames.at(left_frame);
                    auto& prior = right.frames.at(right_frame);
                    const SysExManagerFrameReference candidate_ref{left.id, left_frame};
                    const SysExManagerFrameReference prior_ref{right.id, right_frame};
                    if (candidate.frame.bytes == prior.frame.bytes) {
                        candidate.exact_frame_duplicates.push_back(prior_ref);
                        prior.exact_frame_duplicates.push_back(candidate_ref);
                    }
                    const auto candidate_payload = payload_bytes(candidate.frame);
                    const auto prior_payload = payload_bytes(prior.frame);
                    if (!candidate_payload.empty() && candidate_payload == prior_payload) {
                        candidate.exact_payload_duplicates.push_back(prior_ref);
                        prior.exact_payload_duplicates.push_back(candidate_ref);
                    }
                }
            }
        }
    }
}

} // namespace taureon::app
