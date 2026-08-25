#pragma once

#include "core/sysex/SysExCaptureSession.hpp"
#include "core/sysex/SyxFile.hpp"
#include "core/transfer/TransferEngine.hpp"
#include "profiles/ProfileRegistry.hpp"

#include <chrono>
#include <filesystem>
#include <optional>
#include <memory>
#include <string>
#include <vector>

namespace taureon::app {

enum class SysExSourceKind { none, imported_file, received_capture };

struct SysExTransferSnapshot {
    SysExSourceKind source_kind{SysExSourceKind::none};
    std::string source_name;
    std::vector<sysex::SysExFrame> frames;
    std::uint64_t byte_count{};
    std::uint64_t complete_frames{};
    std::uint64_t incomplete_frames{};
    std::uint64_t malformed_frames{};
    std::uint64_t tainted_frames{};
    bool receiving{};
    bool can_raw_send{};
    bool can_save_verified_received{};
    transfer::TransferProgress send_progress;
    std::optional<midi::MidiError> send_error;
    std::optional<midi::MidiRouteIdentity> transmit_route;
    std::chrono::milliseconds pacing_delay{};
    std::vector<std::string> log;
    std::uint64_t application_dropped_events{};
    std::optional<std::string> profile_id;
    std::string profile_display_name;
    std::optional<std::string> manufacturer;
    std::optional<std::string> model;
    profiles::ProfileMatchStatus profile_match_status{profiles::ProfileMatchStatus::NoMatch};
    std::string profile_match_message;
    bool profile_supports_transfer{};
    bool profile_supports_validated_restore{};
    std::vector<std::string> profile_warnings;
};

class SysExTransferSession {
public:
    explicit SysExTransferSession(
        std::shared_ptr<const profiles::ProfileRegistry> profile_registry = {});
    [[nodiscard]] midi::Result<void> load_file(const std::filesystem::path& path);
    [[nodiscard]] midi::Result<void> begin_receive();
    void consume(const midi::MidiStreamEvent& event);
    [[nodiscard]] midi::Result<void> finish_receive();
    [[nodiscard]] midi::Result<void> clear();
    [[nodiscard]] midi::Result<void> save_verified_received(
        const std::filesystem::path& path) const;

    [[nodiscard]] midi::Result<std::vector<midi::NativeMidiMessage>> build_raw_send(
        midi::MidiBackend backend, std::optional<std::uint8_t> group) const;

    void set_transfer_progress(const transfer::TransferProgress& progress,
                               std::optional<midi::MidiError> error = {});
    void record_log(std::string message);
    [[nodiscard]] SysExTransferSnapshot snapshot() const;
    [[nodiscard]] bool receiving() const noexcept;

private:
    void evaluate_profile();
    void replace_document(SysExSourceKind source_kind, std::string source_name,
                          sysex::SyxDocument document);

    SysExSourceKind source_kind_{SysExSourceKind::none};
    std::string source_name_;
    sysex::SyxDocument document_;
    sysex::SysExCaptureSession capture_;
    bool receiving_{};
    transfer::TransferProgress send_progress_;
    std::optional<midi::MidiError> send_error_;
    std::chrono::milliseconds pacing_delay_{};
    std::vector<std::string> log_;
    std::shared_ptr<const profiles::ProfileRegistry> profile_registry_;
    profiles::ProfileMatchResult profile_match_;
};

} // namespace taureon::app
