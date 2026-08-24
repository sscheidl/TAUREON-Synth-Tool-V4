#include "core/sysex/SysEx7.hpp"
#include "transports/wms/WmsTransport.hpp"
#include "../HandleGrowth.hpp"

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

using namespace taureon;
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

bool safe_name(const std::string& name) { return name.starts_with("TAUREON S3 WMS "); }

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
        return present ? EXIT_FAILURE : EXIT_SUCCESS;
    }
    if (argc != 5 || std::string(argv[1]) != "--lifecycle") return EXIT_FAILURE;

    const std::string input_name = argv[2];
    const std::string output_name = argv[3];
    const auto cycles = static_cast<unsigned>(std::stoul(argv[4]));
    if (cycles == 0 || !safe_name(input_name) || !safe_name(output_name)) return EXIT_FAILURE;

    WmsTransport transport;
    const auto endpoints = transport.enumerate();
    if (!endpoints) return 77;
    const auto inputs = find_named(endpoints.value(), MidiDirection::input, input_name);
    const auto outputs = find_named(endpoints.value(), MidiDirection::output, output_name);
    if (inputs.size() != 1 || outputs.size() != 1) return EXIT_FAILURE;

    const std::vector<std::uint8_t> sysex_bytes{0xF0, 0x7D, 0x54, 0x41, 0x55,
                                                0x52, 0x45, 0x4F, 0x4E, 0xF7};
    const auto encoded = sysex::encode_sysex7(
        {sysex::SysExFrameStatus::complete, sysex_bytes, {},
         static_cast<std::uint8_t>(0), false},
        0);
    if (!encoded) return EXIT_FAILURE;
    std::vector<std::uint32_t> outbound_words{0x20903C41u, 0x10F80000u};
    for (const auto& packet : encoded.value()) {
        outbound_words.push_back(packet.word0);
        outbound_words.push_back(packet.word1);
    }

    std::mutex mutex;
    std::condition_variable changed;
    std::vector<std::uint32_t> received_words;
    transport.set_message_handler([&](const NativeMidiMessage& message) {
        if (const auto* ump = std::get_if<UmpNativeMessage>(&message.data)) {
            {
                std::scoped_lock lock(mutex);
                received_words.insert(received_words.end(), ump->words.begin(), ump->words.end());
            }
            changed.notify_all();
        }
    });

    DWORD steady_minimum = MAXDWORD;
    DWORD steady_maximum = 0;
    std::vector<std::uint32_t> handle_samples;
    handle_samples.reserve(cycles);
    const unsigned steady_start = cycles >= 20 ? (cycles / 2) + 1 : 1;
    for (unsigned cycle = 1; cycle <= cycles; ++cycle) {
        {
            std::scoped_lock lock(mutex);
            received_words.clear();
        }
        if (!transport.open({inputs.front().identity, outputs.front().identity})) return EXIT_FAILURE;
        if (!transport.send({MidiBackend::windows_midi_services,
                             UmpNativeMessage{outbound_words}, std::nullopt})) {
            return EXIT_FAILURE;
        }
        {
            std::unique_lock lock(mutex);
            if (!changed.wait_for(lock, std::chrono::seconds(5), [&] {
                    return received_words.size() >= outbound_words.size();
                })) {
                return EXIT_FAILURE;
            }
        }

        std::vector<std::uint32_t> observed;
        {
            std::scoped_lock lock(mutex);
            observed = received_words;
        }
        if (observed.size() != outbound_words.size() || observed != outbound_words) {
            return EXIT_FAILURE;
        }
        sysex::SysEx7Assembler assembler;
        std::vector<sysex::SysExFrame> frames;
        for (std::size_t index = 2; index + 1 < observed.size(); index += 2) {
            const auto produced = assembler.consume({observed[index], observed[index + 1]});
            frames.insert(frames.end(), produced.begin(), produced.end());
        }
        if (frames.size() != 1 || frames.front().bytes != sysex_bytes ||
            frames.front().status != sysex::SysExFrameStatus::complete) {
            return EXIT_FAILURE;
        }
        if (!transport.close()) return EXIT_FAILURE;
        DWORD handles{};
        if (!GetProcessHandleCount(GetCurrentProcess(), &handles)) return EXIT_FAILURE;
        handle_samples.push_back(static_cast<std::uint32_t>(handles));
        if (cycle >= steady_start) {
            steady_minimum = (std::min)(steady_minimum, handles);
            steady_maximum = (std::max)(steady_maximum, handles);
        }
    }

    const auto diagnostics = transport.diagnostics();
    const auto growth = taureon::test::analyze_handle_growth(handle_samples, steady_start - 1);
    const bool pass = transport.state() == TransportState::closed &&
                      diagnostics.transmitted_messages == cycles &&
                      diagnostics.delivered_messages >= cycles &&
                      diagnostics.dropped_events == 0 &&
                      diagnostics.callbacks_after_acceptance_closed == 0 &&
                      !growth.sustained_growth;
    std::cout << "{\"event\":\"stage3_wms_realtime\",\"cycles\":" << cycles
              << ",\"tx\":" << diagnostics.transmitted_messages
              << ",\"rx_callbacks\":" << diagnostics.delivered_messages
              << ",\"dropped\":" << diagnostics.dropped_events
              << ",\"late\":" << diagnostics.callbacks_after_acceptance_closed
              << ",\"steady_span\":" << steady_maximum - steady_minimum
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
