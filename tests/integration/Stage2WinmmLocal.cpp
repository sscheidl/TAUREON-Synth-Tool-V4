#include "transports/winmm/WinmmTransport.hpp"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace taureon::midi;
using taureon::midi::winmm::WinmmTransport;

namespace {

std::vector<MidiEndpointDescriptor> find_named(const std::vector<MidiEndpointDescriptor>& endpoints,
                                               const MidiDirection direction,
                                               const std::string& name) {
    std::vector<MidiEndpointDescriptor> matches;
    std::copy_if(endpoints.begin(), endpoints.end(), std::back_inserter(matches),
                 [&](const auto& endpoint) {
                     return endpoint.identity.direction == direction && endpoint.display_name == name;
                 });
    return matches;
}

bool safe_name(const std::string& name) { return name.starts_with("TAUREON S2 WMS "); }

} // namespace

int main(int argc, char** argv) {
    if (argc == 4 && std::string(argv[1]) == "--assert-absent") {
        WinmmTransport transport;
        const auto endpoints = transport.enumerate();
        if (!endpoints) return EXIT_FAILURE;
        const auto present = std::any_of(endpoints.value().begin(), endpoints.value().end(),
                                         [&](const auto& endpoint) {
                                             return endpoint.display_name == argv[2] ||
                                                    endpoint.display_name == argv[3];
                                         });
        std::cout << "{\"event\":\"stage2_winmm_pair_absent\",\"absent\":"
                  << (present ? "false" : "true") << "}" << std::endl;
        return present ? EXIT_FAILURE : EXIT_SUCCESS;
    }

    if (argc != 5 || std::string(argv[1]) != "--lifecycle") {
        std::cerr << "usage: stage2_winmm_local --lifecycle <input-name> <output-name> <cycles>"
                  << std::endl;
        return EXIT_FAILURE;
    }
    const std::string input_name = argv[2];
    const std::string output_name = argv[3];
    const auto cycles = static_cast<unsigned>(std::stoul(argv[4]));
    if (cycles == 0 || !safe_name(input_name) || !safe_name(output_name)) {
        std::cerr << "unsafe or invalid local WinMM regression arguments" << std::endl;
        return EXIT_FAILURE;
    }

    WinmmTransport transport;
    const auto endpoints = transport.enumerate();
    if (!endpoints) {
        std::cerr << endpoints.error().message << std::endl;
        return EXIT_FAILURE;
    }
    const auto inputs = find_named(endpoints.value(), MidiDirection::input, input_name);
    const auto outputs = find_named(endpoints.value(), MidiDirection::output, output_name);
    if (inputs.size() != 1 || outputs.size() != 1) {
        std::cerr << "temporary WMS pair is missing or ambiguous through WinMM" << std::endl;
        return EXIT_FAILURE;
    }

    DWORD handles_after_first = 0;
    for (unsigned cycle = 1; cycle <= cycles; ++cycle) {
        const auto opened = transport.open({inputs.front().identity, outputs.front().identity});
        if (!opened) {
            std::cerr << opened.error().message << std::endl;
            return EXIT_FAILURE;
        }
        const auto closed = transport.close();
        if (!closed) {
            std::cerr << closed.error().message << std::endl;
            return EXIT_FAILURE;
        }
        if (cycle == 1 && !GetProcessHandleCount(GetCurrentProcess(), &handles_after_first)) {
            return EXIT_FAILURE;
        }
    }
    DWORD handles_after_final = 0;
    if (!GetProcessHandleCount(GetCurrentProcess(), &handles_after_final)) return EXIT_FAILURE;
    const auto diagnostics = transport.diagnostics();
    const bool pass = handles_after_final <= handles_after_first &&
                      diagnostics.dropped_events == 0 &&
                      diagnostics.callbacks_after_acceptance_closed == 0;
    std::cout << "{\"event\":\"stage2_winmm_lifecycle\",\"cycles\":" << cycles
              << ",\"handles_after_first\":" << handles_after_first
              << ",\"handles_after_final\":" << handles_after_final
              << ",\"native_callbacks\":" << diagnostics.native_callbacks
              << ",\"dropped_events\":" << diagnostics.dropped_events
              << ",\"callbacks_after_acceptance_closed\":"
              << diagnostics.callbacks_after_acceptance_closed
              << ",\"pass\":" << (pass ? "true" : "false") << "}" << std::endl;
    return pass ? EXIT_SUCCESS : EXIT_FAILURE;
}
