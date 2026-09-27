#include "MidiTypes.hpp"

namespace taureon::midi {

bool is_valid(const MidiRouteIdentity& identity) noexcept {
    switch (identity.backend) {
    case MidiBackend::windows_midi_services: {
        const auto* native = std::get_if<WmsRouteIdentity>(&identity.native);
        return native != nullptr && !native->endpoint_device_id.empty() && native->group < 16;
    }
    case MidiBackend::winmm: {
        const auto* native = std::get_if<WinmmRouteIdentity>(&identity.native);
        return native != nullptr && !native->port_name.empty();
    }
    case MidiBackend::external: {
        const auto* native = std::get_if<ExternalRouteIdentity>(&identity.native);
        return native != nullptr && !native->provider_id.empty() && !native->endpoint_id.empty();
    }
    }
    return false;
}

const char* to_string(const MidiBackend backend) noexcept {
    switch (backend) {
    case MidiBackend::windows_midi_services: return "wms";
    case MidiBackend::winmm: return "winmm";
    case MidiBackend::external: return "external";
    }
    return "invalid";
}

const char* to_string(const MidiDirection direction) noexcept {
    switch (direction) {
    case MidiDirection::input: return "input";
    case MidiDirection::output: return "output";
    }
    return "invalid";
}

} // namespace taureon::midi
