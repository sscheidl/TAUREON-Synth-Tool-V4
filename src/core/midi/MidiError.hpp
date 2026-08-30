#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace taureon::midi {

enum class MidiErrorCode {
    backend_unavailable,
    endpoint_missing,
    endpoint_ambiguous,
    endpoint_disappeared,
    open_failure,
    close_failure,
    invalid_route,
    unsupported_capability,
    native_api_error,
    shutdown_cancelled,
    invalid_state,
    serialization_error,
    queue_overflow,
    malformed_data,
    incomplete_data,
    io_error,
    timeout,
    transfer_cancelled,
    transport_disconnected,
    // N-1: new codes are appended here so existing values keep their ordinal position.
    not_found,
    invalid_argument,
};

struct MidiError {
    MidiErrorCode code{MidiErrorCode::native_api_error};
    std::string message;
    std::string native_api;
    std::optional<std::int64_t> native_code;

    bool operator==(const MidiError&) const = default;
};

} // namespace taureon::midi
