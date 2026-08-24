#include "transports/wms/WmsTransport.hpp"
#include "../HandleGrowth.hpp"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace taureon::midi;
using taureon::midi::wms::WmsTransport;

namespace {

std::vector<MidiEndpointDescriptor> find_named(
    const std::vector<MidiEndpointDescriptor>& endpoints, const MidiDirection direction,
    const std::string& name) {
    std::vector<MidiEndpointDescriptor> matches;
    std::copy_if(endpoints.begin(), endpoints.end(), std::back_inserter(matches),
                 [&](const auto& endpoint) {
                     const auto* identity = std::get_if<WmsRouteIdentity>(&endpoint.identity.native);
                     return identity != nullptr && identity->group == 0 &&
                            endpoint.identity.direction == direction && endpoint.display_name == name;
                 });
    return matches;
}

bool safe_name(const std::string& name) { return name.starts_with("TAUREON S2 WMS "); }

} // namespace

int main(int argc, char** argv) {
    if (argc == 4 && std::string(argv[1]) == "--assert-absent") {
        WmsTransport transport;
        const auto endpoints = transport.enumerate();
        if (!endpoints) return 77;
        const auto present = std::any_of(endpoints.value().begin(), endpoints.value().end(),
                                         [&](const auto& endpoint) {
                                             return endpoint.display_name == argv[2] ||
                                                    endpoint.display_name == argv[3];
                                         });
        std::cout << "{\"event\":\"stage2_wms_pair_absent\",\"absent\":"
                  << (present ? "false" : "true") << "}" << std::endl;
        return present ? EXIT_FAILURE : EXIT_SUCCESS;
    }

    if (argc != 5 || std::string(argv[1]) != "--lifecycle") {
        std::cerr << "usage: stage2_wms_local --lifecycle <input-name> <output-name> <cycles>"
                  << std::endl;
        return EXIT_FAILURE;
    }
    const std::string input_name = argv[2];
    const std::string output_name = argv[3];
    const auto cycles = static_cast<unsigned>(std::stoul(argv[4]));
    if (cycles == 0 || !safe_name(input_name) || !safe_name(output_name)) {
        std::cerr << "unsafe or invalid local WMS regression arguments" << std::endl;
        return EXIT_FAILURE;
    }

    WmsTransport transport;
    const auto endpoints = transport.enumerate();
    if (!endpoints) {
        std::cout << "SKIP: " << endpoints.error().message << std::endl;
        return 77;
    }
    const auto inputs = find_named(endpoints.value(), MidiDirection::input, input_name);
    const auto outputs = find_named(endpoints.value(), MidiDirection::output, output_name);
    if (inputs.size() != 1 || outputs.size() != 1) {
        std::cerr << "temporary WMS pair is missing or ambiguous through WMS" << std::endl;
        return EXIT_FAILURE;
    }

    DWORD steady_minimum = MAXDWORD;
    DWORD steady_maximum = 0;
    DWORD handles_after_first = 0;
    DWORD handles_after_final = 0;
    std::vector<std::uint32_t> handle_samples;
    handle_samples.reserve(cycles);
    const unsigned steady_start = cycles >= 20 ? (cycles / 2) + 1 : 1;
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
        DWORD handles = 0;
        if (!GetProcessHandleCount(GetCurrentProcess(), &handles)) return EXIT_FAILURE;
        handle_samples.push_back(static_cast<std::uint32_t>(handles));
        if (cycle == 1) handles_after_first = handles;
        handles_after_final = handles;
        if (cycle >= steady_start) {
            steady_minimum = (std::min)(steady_minimum, handles);
            steady_maximum = (std::max)(steady_maximum, handles);
        }
    }
    const auto growth = taureon::test::analyze_handle_growth(handle_samples, steady_start - 1);
    const auto steady_span = steady_maximum - steady_minimum;
    const bool pass = !growth.sustained_growth && transport.state() == TransportState::closed;
    std::cout << "{\"event\":\"stage2_wms_lifecycle\",\"cycles\":" << cycles
              << ",\"handles_after_first\":" << handles_after_first
              << ",\"handles_after_final\":" << handles_after_final
              << ",\"steady_start\":" << steady_start
              << ",\"steady_minimum\":" << steady_minimum
              << ",\"steady_maximum\":" << steady_maximum
              << ",\"steady_span\":" << steady_span
              << ",\"reference_maximum\":" << growth.reference_maximum
              << ",\"steady_slope\":" << static_cast<double>(growth.steady_slope)
              << ",\"leading_median\":" << static_cast<double>(growth.leading_median)
              << ",\"trailing_median\":" << static_cast<double>(growth.trailing_median)
              << ",\"new_steady_high\":" << (growth.new_steady_high ? "true" : "false")
              << ",\"sustained_growth\":" << (growth.sustained_growth ? "true" : "false")
              << ",\"handle_samples\":[";
    for (std::size_t index = 0; index < handle_samples.size(); ++index) {
        if (index != 0) std::cout << ',';
        std::cout << handle_samples[index];
    }
    std::cout << ']'
              << ",\"pass\":" << (pass ? "true" : "false") << "}" << std::endl;
    return pass ? EXIT_SUCCESS : EXIT_FAILURE;
}
