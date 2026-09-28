#include <taureon/transports/winmm/WinmmTransport.hpp>

int main() {
    taureon::midi::winmm::WinmmTransport transport;
    const auto capabilities = transport.capabilities();
    return transport.backend() == taureon::midi::MidiBackend::winmm &&
                   capabilities.supports_input && capabilities.supports_output &&
                   capabilities.supports_midi1 && !capabilities.supports_ump
               ? 0
               : 1;
}
