#include <taureon/transports/wms/WmsTransport.hpp>

int main() {
    taureon::midi::wms::WmsTransport transport;
    const auto capabilities = transport.capabilities();
    return transport.backend() == taureon::midi::MidiBackend::windows_midi_services &&
                   capabilities.supports_input && capabilities.supports_output &&
                   capabilities.supports_ump
               ? 0
               : 1;
}
