#pragma once

#include "WinmmNativeApi.hpp"

#include <taureon/transports/winmm/WinmmTransport.hpp>

#include <memory>

namespace taureon::midi::winmm {

// Internal test seam. It is deliberately not installed with the reusable backend.
class WinmmTransportTestAccess {
public:
    static std::unique_ptr<WinmmTransport> create(WinmmTransportApiPtr native_api) {
        return std::unique_ptr<WinmmTransport>(new WinmmTransport(std::move(native_api)));
    }
};

} // namespace taureon::midi::winmm
