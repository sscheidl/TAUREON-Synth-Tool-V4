#include "MidiTypes.hpp"

namespace taureon::midi {

bool is_valid(const MidiRouteIdentity& identity) noexcept {
    if (identity.backend == MidiBackend::windows_midi_services) {
        const auto* native = std::get_if<WmsRouteIdentity>(&identity.native);
        return native != nullptr && !native->endpoint_device_id.empty() && native->group < 16;
    }
    const auto* native = std::get_if<WinmmRouteIdentity>(&identity.native);
    return native != nullptr && !native->port_name.empty();
}

const char* to_string(const MidiBackend backend) noexcept {
    switch (backend) {
    case MidiBackend::windows_midi_services: return "wms";
    case MidiBackend::winmm: return "winmm";
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
