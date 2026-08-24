#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace taureon::midi {

enum class MidiBackend {
    windows_midi_services,
    winmm,
};

enum class MidiDirection {
    input,
    output,
};

enum class MidiProtocol {
    unknown,
    midi1,
    ump,
};

struct WmsRouteIdentity {
    std::string endpoint_device_id;
    std::uint8_t group{};

    bool operator==(const WmsRouteIdentity&) const = default;
};

struct WinmmRouteIdentity {
    std::string port_name;
    std::uint16_t manufacturer_id{};
    std::uint16_t product_id{};
    std::uint32_t driver_version{};

    bool operator==(const WinmmRouteIdentity&) const = default;
};

using BackendRouteIdentity = std::variant<WmsRouteIdentity, WinmmRouteIdentity>;

struct MidiRouteIdentity {
    MidiBackend backend{MidiBackend::windows_midi_services};
    MidiDirection direction{MidiDirection::input};
    BackendRouteIdentity native{WmsRouteIdentity{}};

    bool operator==(const MidiRouteIdentity&) const = default;
};

struct MidiEndpointCapabilities {
    bool can_receive{};
    bool can_send{};
    bool supports_midi1{};
    bool supports_ump{};

    bool operator==(const MidiEndpointCapabilities&) const = default;
};

struct MidiEndpointDescriptor {
    MidiRouteIdentity identity;
    std::string display_name;
    MidiProtocol protocol{MidiProtocol::unknown};
    MidiEndpointCapabilities capabilities;
    std::optional<std::uint32_t> runtime_index_hint;
    std::optional<std::string> function_block_name;

    bool operator==(const MidiEndpointDescriptor&) const = default;
};

struct Midi1NativeMessage {
    std::vector<std::uint8_t> bytes;

    bool operator==(const Midi1NativeMessage&) const = default;
};

struct UmpNativeMessage {
    std::vector<std::uint32_t> words;

    bool operator==(const UmpNativeMessage&) const = default;
};

struct MidiTimestamp {
    std::uint64_t native_value{};
    std::string domain;

    bool operator==(const MidiTimestamp&) const = default;
};

struct NativeMidiMessage {
    MidiBackend backend{MidiBackend::windows_midi_services};
    std::variant<Midi1NativeMessage, UmpNativeMessage> data{Midi1NativeMessage{}};
    std::optional<MidiTimestamp> timestamp;

    bool operator==(const NativeMidiMessage&) const = default;
};

enum class MidiDataLossReason {
    native_short_error,
    native_long_error,
    queue_overflow,
    input_requeue_failure,
    shutdown_discarded_data,
    backend_reported_loss,
};

struct MidiDataLossEvent {
    MidiBackend backend{MidiBackend::windows_midi_services};
    MidiDataLossReason reason{MidiDataLossReason::backend_reported_loss};
    bool affects_sysex{};
    std::optional<std::uint8_t> group;
    std::string detail;
    std::optional<std::int64_t> native_code;

    bool operator==(const MidiDataLossEvent&) const = default;
};

struct MidiStreamEvent {
    std::uint64_t sequence{};
    std::variant<NativeMidiMessage, MidiDataLossEvent> payload{NativeMidiMessage{}};

    bool operator==(const MidiStreamEvent&) const = default;
};

enum class EndpointChangeKind {
    appeared,
    disappeared,
    metadata_changed,
    selected_route_unavailable,
};

struct EndpointChange {
    EndpointChangeKind kind{EndpointChangeKind::metadata_changed};
    MidiRouteIdentity identity;
    std::optional<MidiEndpointDescriptor> descriptor;
};

[[nodiscard]] bool is_valid(const MidiRouteIdentity& identity) noexcept;
[[nodiscard]] const char* to_string(MidiBackend backend) noexcept;
[[nodiscard]] const char* to_string(MidiDirection direction) noexcept;

} // namespace taureon::midi
