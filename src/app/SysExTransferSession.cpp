#include "app/SysExTransferSession.hpp"

#include "core/sysex/SysEx7.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

namespace taureon::app {
namespace {

midi::MidiError invalid_state(std::string message) {
    return {midi::MidiErrorCode::invalid_state, std::move(message), "sysex-transfer",
            std::nullopt};
}

midi::MidiError incomplete_data(std::string message) {
    return {midi::MidiErrorCode::incomplete_data, std::move(message), "sysex-transfer",
            std::nullopt};
}

std::string path_filename(const std::filesystem::path& path) {
    const auto value = path.filename().generic_u8string();
    return {value.begin(), value.end()};
}

bool verified_complete(const sysex::SysExFrame& frame) {
    return frame.status == sysex::SysExFrameStatus::complete &&
           !frame.affected_by_data_loss;
}

} // namespace

SysExTransferSession::SysExTransferSession(
    std::shared_ptr<const profiles::ProfileRegistry> profile_registry)
    : profile_registry_(std::move(profile_registry)) {}

midi::Result<void> SysExTransferSession::load_file(const std::filesystem::path& path) {
    if (receiving_) {
        return midi::Result<void>::failure(
            invalid_state("finish the active receive capture before loading a file"));
    }
    auto loaded = sysex::load_syx_file(path);
    if (!loaded) return midi::Result<void>::failure(loaded.error());
    replace_document(SysExSourceKind::imported_file, path_filename(path),
                     std::move(loaded.value()));
    record_log("Loaded read-only source " + source_name_);
    return midi::Result<void>::success();
}

midi::Result<void> SysExTransferSession::load_document(sysex::SyxDocument document,
                                                       std::string source_name) {
    if (receiving_) {
        return midi::Result<void>::failure(
            invalid_state("finish the active receive capture before loading a SysEx document"));
    }
    if (!document.all_complete()) {
        return midi::Result<void>::failure(
            incomplete_data("Manager handoff accepts complete, untainted SysEx frames only"));
    }
    if (source_name.empty()) source_name = "SysEx Manager item";
    replace_document(SysExSourceKind::imported_file, std::move(source_name), std::move(document));
    record_log("Loaded inspected bytes from SysEx Manager");
    return midi::Result<void>::success();
}

midi::Result<void> SysExTransferSession::begin_receive() {
    if (receiving_) {
        return midi::Result<void>::failure(invalid_state("receive capture is already active"));
    }
    capture_.reset();
    document_ = {};
    profile_match_ = {};
    profile_match_.message = profile_registry_ ? "capture has no complete frame yet" :
                                                 "profile registry is unavailable";
    send_progress_ = {};
    send_error_.reset();
    source_kind_ = SysExSourceKind::received_capture;
    source_name_ = "Live capture";
    receiving_ = true;
    record_log("Receive capture started");
    return midi::Result<void>::success();
}

void SysExTransferSession::consume(const midi::MidiStreamEvent& event) {
    if (!receiving_) return;
    auto frames = capture_.consume(event);
    for (auto& frame : frames) {
        document_.raw_bytes.insert(document_.raw_bytes.end(), frame.bytes.begin(), frame.bytes.end());
        document_.frames.push_back(std::move(frame));
    }
    evaluate_profile();
}

midi::Result<void> SysExTransferSession::finish_receive() {
    if (!receiving_) {
        return midi::Result<void>::failure(invalid_state("receive capture is not active"));
    }
    auto frames = capture_.finish();
    for (auto& frame : frames) {
        document_.raw_bytes.insert(document_.raw_bytes.end(), frame.bytes.begin(), frame.bytes.end());
        document_.frames.push_back(std::move(frame));
    }
    receiving_ = false;
    source_name_ = "Received capture";
    evaluate_profile();
    record_log("Receive capture stopped");
    return midi::Result<void>::success();
}

midi::Result<void> SysExTransferSession::clear() {
    if (receiving_) {
        return midi::Result<void>::failure(
            invalid_state("finish the active receive capture before clearing"));
    }
    source_kind_ = SysExSourceKind::none;
    source_name_.clear();
    document_ = {};
    profile_match_ = {};
    send_progress_ = {};
    send_error_.reset();
    record_log("Transfer workspace cleared");
    return midi::Result<void>::success();
}

midi::Result<void> SysExTransferSession::save_verified_received(
    const std::filesystem::path& path) const {
    if (receiving_) {
        return midi::Result<void>::failure(
            invalid_state("finish the active receive capture before saving"));
    }
    if (source_kind_ != SysExSourceKind::received_capture) {
        return midi::Result<void>::failure(
            invalid_state("Save received data requires a completed receive capture"));
    }
    return sysex::save_syx_frames(path, document_.frames, false);
}

midi::Result<std::vector<midi::NativeMidiMessage>> SysExTransferSession::build_raw_send(
    const midi::MidiBackend backend, const std::optional<std::uint8_t> group) const {
    if (receiving_) {
        return midi::Result<std::vector<midi::NativeMidiMessage>>::failure(
            invalid_state("finish the active receive capture before sending"));
    }
    if (document_.frames.empty() ||
        !std::all_of(document_.frames.begin(), document_.frames.end(), verified_complete)) {
        return midi::Result<std::vector<midi::NativeMidiMessage>>::failure(
            incomplete_data("Raw Send accepts complete, untainted SysEx frames only"));
    }

    std::vector<midi::NativeMidiMessage> messages;
    messages.reserve(document_.frames.size());
    if (backend == midi::MidiBackend::winmm) {
        for (const auto& frame : document_.frames) {
            messages.push_back(
                {backend, midi::Midi1NativeMessage{frame.bytes}, std::nullopt});
        }
        return midi::Result<std::vector<midi::NativeMidiMessage>>::success(std::move(messages));
    }

    if (!group || *group > 15) {
        return midi::Result<std::vector<midi::NativeMidiMessage>>::failure(
            {midi::MidiErrorCode::invalid_route,
             "WMS Raw Send requires the exact selected TX group", "sysex-transfer",
             std::nullopt});
    }
    for (const auto& frame : document_.frames) {
        auto encoded = sysex::encode_sysex7(frame, *group);
        if (!encoded) {
            return midi::Result<std::vector<midi::NativeMidiMessage>>::failure(encoded.error());
        }
        std::vector<std::uint32_t> words;
        words.reserve(encoded.value().size() * 2);
        for (const auto& packet : encoded.value()) {
            words.push_back(packet.word0);
            words.push_back(packet.word1);
        }
        messages.push_back({backend, midi::UmpNativeMessage{std::move(words)}, std::nullopt});
    }
    return midi::Result<std::vector<midi::NativeMidiMessage>>::success(std::move(messages));
}

void SysExTransferSession::set_transfer_progress(const transfer::TransferProgress& progress,
                                                 std::optional<midi::MidiError> error) {
    send_progress_ = progress;
    send_error_ = std::move(error);
}

void SysExTransferSession::record_log(std::string message) {
    constexpr std::size_t log_capacity = 100;
    if (log_.size() == log_capacity) log_.erase(log_.begin());
    log_.push_back(std::move(message));
}

SysExTransferSnapshot SysExTransferSession::snapshot() const {
    SysExTransferSnapshot result;
    result.source_kind = source_kind_;
    result.source_name = source_name_;
    result.frames = document_.frames;
    result.byte_count = document_.raw_bytes.size();
    for (const auto& frame : document_.frames) {
        switch (frame.status) {
        case sysex::SysExFrameStatus::complete: ++result.complete_frames; break;
        case sysex::SysExFrameStatus::incomplete: ++result.incomplete_frames; break;
        case sysex::SysExFrameStatus::malformed: ++result.malformed_frames; break;
        }
        if (frame.affected_by_data_loss) ++result.tainted_frames;
    }
    result.receiving = receiving_;
    result.can_raw_send = !receiving_ && document_.all_complete();
    result.can_save_verified_received = !receiving_ &&
        source_kind_ == SysExSourceKind::received_capture && document_.all_complete();
    result.send_progress = send_progress_;
    result.send_error = send_error_;
    result.pacing_delay = pacing_delay_;
    result.log = log_;
    result.profile_match_status = profile_match_.status;
    result.profile_match_message = profile_match_.message;
    result.profile_id = profile_match_.selected_profile_id;
    if (profile_registry_ && result.profile_id) {
        if (const auto* profile = profile_registry_->find(*result.profile_id)) {
            result.profile_display_name = profile->display_name;
            result.manufacturer = profile->manufacturer;
            result.model = profile->model;
            result.profile_supports_transfer = profile->support.transfer;
            result.profile_supports_validated_restore = profile->support.validated_restore;
            result.profile_warnings = profile->warnings;
        }
    }
    return result;
}

bool SysExTransferSession::receiving() const noexcept { return receiving_; }

void SysExTransferSession::replace_document(const SysExSourceKind source_kind,
                                            std::string source_name,
                                            sysex::SyxDocument document) {
    source_kind_ = source_kind;
    source_name_ = std::move(source_name);
    document_ = std::move(document);
    evaluate_profile();
    send_progress_ = {};
    send_error_.reset();
}

void SysExTransferSession::evaluate_profile() {
    profile_match_ = {};
    if (!profile_registry_ || document_.frames.empty()) {
        profile_match_.message = profile_registry_ ? "no SysEx frame is available for matching" :
                                                     "profile registry is unavailable";
        return;
    }
    profile_match_ = profile_registry_->match(document_.frames.front());
}

} // namespace taureon::app
