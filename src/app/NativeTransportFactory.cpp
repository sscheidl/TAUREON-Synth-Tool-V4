#include "app/NativeTransportFactory.hpp"

#include "transports/winmm/WinmmTransport.hpp"

#if defined(TAUREON_HAS_WMS_TRANSPORT)
#include "transports/wms/WmsTransport.hpp"
#endif

#include <stdexcept>

namespace taureon::app {

std::unique_ptr<midi::IMidiTransport> create_native_transport(const midi::MidiBackend backend) {
    if (backend == midi::MidiBackend::winmm) {
        return std::make_unique<midi::winmm::WinmmTransport>();
    }
#if defined(TAUREON_HAS_WMS_TRANSPORT)
    if (backend == midi::MidiBackend::windows_midi_services) {
        return std::make_unique<midi::wms::WmsTransport>();
    }
#endif
    throw std::runtime_error("requested MIDI backend is not available in this build");
}

} // namespace taureon::app
