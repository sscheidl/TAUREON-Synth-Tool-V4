#pragma once

#include <taureon/transports/IMidiTransport.hpp>

#include <memory>

namespace taureon::app {

[[nodiscard]] std::unique_ptr<midi::IMidiTransport> create_native_transport(
    midi::MidiBackend backend);

} // namespace taureon::app
