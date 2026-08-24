#pragma once

#include "core/midi/MidiTypes.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace taureon::profiles {

enum class SupportLevel {
    detect,
    read,
    inspect,
    extract,
    modify,
    serialize,
    transfer,
    validated_restore,
};

struct SupportClaims {
    bool detect{};
    bool read{};
    bool inspect{};
    bool extract{};
    bool modify{};
    bool serialize{};
    bool transfer{};
    bool validated_restore{};

    [[nodiscard]] bool supports(SupportLevel level) const noexcept {
        switch (level) {
        case SupportLevel::detect: return detect;
        case SupportLevel::read: return read;
        case SupportLevel::inspect: return inspect;
        case SupportLevel::extract: return extract;
        case SupportLevel::modify: return modify;
        case SupportLevel::serialize: return serialize;
        case SupportLevel::transfer: return transfer;
        case SupportLevel::validated_restore: return validated_restore;
        }
        return false;
    }

    bool operator==(const SupportClaims&) const = default;
};

struct NamedNumber {
    std::uint16_t number{};
    std::string name;

    bool operator==(const NamedNumber&) const = default;
};

struct BankOrganization {
    std::string description;
    std::optional<std::uint16_t> bank_count;
    std::optional<std::uint16_t> slots_per_bank;

    bool operator==(const BankOrganization&) const = default;
};

struct DescriptiveMetadata {
    std::vector<std::uint8_t> channels;
    std::vector<NamedNumber> control_changes;
    std::vector<NamedNumber> rpn;
    std::vector<NamedNumber> nrpn;
    std::optional<BankOrganization> bank_organization;

    bool operator==(const DescriptiveMetadata&) const = default;
};

struct UniversalIdentityEvidence {
    std::vector<std::uint8_t> manufacturer_id;
    std::uint16_t family{};
    std::uint16_t model{};

    bool operator==(const UniversalIdentityEvidence&) const = default;
};

struct NativeIdentityEvidence {
    std::string manufacturer;
    std::string model;
    std::optional<std::string> variant;

    bool operator==(const NativeIdentityEvidence&) const = default;
};

struct SysExFingerprint {
    std::string id;
    std::size_t offset{};
    std::vector<std::uint8_t> bytes;

    bool operator==(const SysExFingerprint&) const = default;
};

struct RecognitionMetadata {
    std::optional<NativeIdentityEvidence> native_identity;
    std::optional<UniversalIdentityEvidence> universal_identity;
    std::vector<SysExFingerprint> sysex_fingerprints;

    bool operator==(const RecognitionMetadata&) const = default;
};

struct ProfileDefaults {
    std::optional<std::uint8_t> midi_channel;
    std::optional<std::uint32_t> sysex_inter_message_delay_ms;

    bool operator==(const ProfileDefaults&) const = default;
};

struct ProfileProvenance {
    std::string source;
    std::string owner;
    std::string license;
    std::string redistribution;
    std::string acquisition;

    bool operator==(const ProfileProvenance&) const = default;
};

struct DeviceProfile {
    std::uint32_t schema_version{};
    std::string profile_id;
    std::string profile_version;
    bool generic{};
    std::optional<std::string> manufacturer;
    std::optional<std::string> model;
    std::optional<std::string> variant;
    std::string display_name;
    std::vector<std::string> aliases;
    SupportClaims support;
    DescriptiveMetadata metadata;
    ProfileDefaults defaults;
    std::vector<std::string> warnings;
    RecognitionMetadata recognition;
    ProfileProvenance provenance;
    std::optional<std::string> protocol_module_id;

    bool operator==(const DeviceProfile&) const = default;
};

enum class ProfileMatchStatus {
    Explicit,
    ConfidentSuggestion,
    Ambiguous,
    NoMatch,
    Invalid,
    GenericFallback,
};

enum class ProfileEvidenceKind {
    saved_binding,
    native_identity,
    universal_identity,
    sysex_fingerprint,
    manual_selection,
    generic_fallback,
    data_integrity_rejection,
};

struct ProfileMatchEvidence {
    std::string profile_id;
    ProfileEvidenceKind kind{ProfileEvidenceKind::generic_fallback};
    std::string detail;

    bool operator==(const ProfileMatchEvidence&) const = default;
};

struct ProfileMatchInput {
    std::optional<std::string> saved_profile_id;
    std::optional<NativeIdentityEvidence> native_identity;
    std::optional<UniversalIdentityEvidence> universal_identity;
    std::optional<std::string> manual_profile_id;
    std::optional<std::string> port_display_name;
    std::optional<midi::MidiRouteIdentity> route_identity;
};

struct ProfileMatchResult {
    ProfileMatchStatus status{ProfileMatchStatus::NoMatch};
    std::optional<std::string> selected_profile_id;
    std::vector<ProfileMatchEvidence> evidence;
    std::string message;

    bool operator==(const ProfileMatchResult&) const = default;
};

} // namespace taureon::profiles
