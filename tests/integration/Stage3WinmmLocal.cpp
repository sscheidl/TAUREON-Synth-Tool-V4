#include "transports/winmm/WinmmTransport.hpp"
#include "core/sysex/SysExStreamParser.hpp"

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <mutex>
#include <string>
#include <vector>

using namespace taureon::midi;
using taureon::midi::winmm::WinmmTransport;
namespace sysex = taureon::sysex;

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

bool safe_name(const std::string& name) { return name.starts_with("TAUREON S3 WMS "); }

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
        return present ? EXIT_FAILURE : EXIT_SUCCESS;
    }
    if (argc != 5 || std::string(argv[1]) != "--lifecycle") return EXIT_FAILURE;

    const std::string input_name = argv[2];
    const std::string output_name = argv[3];
    const auto cycles = static_cast<unsigned>(std::stoul(argv[4]));
    if (cycles == 0 || !safe_name(input_name) || !safe_name(output_name)) return EXIT_FAILURE;

    WinmmTransport transport;
    const auto endpoints = transport.enumerate();
    if (!endpoints) return EXIT_FAILURE;
    const auto inputs = find_named(endpoints.value(), MidiDirection::input, input_name);
    const auto outputs = find_named(endpoints.value(), MidiDirection::output, output_name);
    if (inputs.size() != 1 || outputs.size() != 1) return EXIT_FAILURE;

    std::mutex mutex;
    std::condition_variable changed;
    bool short_received = false;
    sysex::SysExStreamParser parser;
    std::vector<sysex::SysExFrame> received_frames;
    const std::vector<std::uint8_t> short_message{0x90, 0x3C, 0x41};
    transport.set_message_handler([&](const NativeMidiMessage& message) {
        if (const auto* midi1 = std::get_if<Midi1NativeMessage>(&message.data)) {
            {
                std::scoped_lock lock(mutex);
                if (midi1->bytes == short_message) {
                    short_received = true;
                } else {
                    auto batch = parser.consume(midi1->bytes);
                    received_frames.insert(received_frames.end(),
                                           std::make_move_iterator(batch.frames.begin()),
                                           std::make_move_iterator(batch.frames.end()));
                }
            }
            changed.notify_all();
        }
    });

    std::vector<std::uint8_t> sysex_message(10 * 1024 + 17);
    sysex_message.front() = 0xF0;
    sysex_message.back() = 0xF7;
    for (std::size_t index = 1; index + 1 < sysex_message.size(); ++index) {
        sysex_message[index] = static_cast<std::uint8_t>(index & 0x7F);
    }
    DWORD steady_minimum = MAXDWORD;
    DWORD steady_maximum = 0;
    const unsigned steady_start = cycles >= 20 ? (cycles / 2) + 1 : 1;

    for (unsigned cycle = 1; cycle <= cycles; ++cycle) {
        {
            std::scoped_lock lock(mutex);
            short_received = false;
            received_frames.clear();
            parser.reset();
        }
        const auto opened = transport.open({inputs.front().identity, outputs.front().identity});
        if (!opened) return EXIT_FAILURE;
        if (!transport.send({MidiBackend::winmm, Midi1NativeMessage{short_message}, std::nullopt}) ||
            !transport.send({MidiBackend::winmm, Midi1NativeMessage{sysex_message}, std::nullopt})) {
            return EXIT_FAILURE;
        }
        {
            std::unique_lock lock(mutex);
            if (!changed.wait_for(lock, std::chrono::seconds(5), [&] {
                    return short_received && received_frames.size() == 1 &&
                           received_frames.front().bytes == sysex_message;
                })) {
                return EXIT_FAILURE;
            }
        }
        if (!transport.close()) return EXIT_FAILURE;
        DWORD handles{};
        if (!GetProcessHandleCount(GetCurrentProcess(), &handles)) return EXIT_FAILURE;
        if (cycle >= steady_start) {
            steady_minimum = (std::min)(steady_minimum, handles);
            steady_maximum = (std::max)(steady_maximum, handles);
        }
    }

    const auto diagnostics = transport.diagnostics();
    const bool pass = transport.state() == TransportState::closed &&
                      diagnostics.transmitted_messages == cycles * 2u &&
                      diagnostics.delivered_messages >= cycles * 2u &&
                      diagnostics.dropped_events == 0 &&
                      diagnostics.callbacks_after_acceptance_closed == 0 &&
                      steady_maximum - steady_minimum <= 1;
    std::cout << "{\"event\":\"stage3_winmm_realtime\",\"cycles\":" << cycles
              << ",\"tx\":" << diagnostics.transmitted_messages
              << ",\"rx\":" << diagnostics.delivered_messages
              << ",\"dropped\":" << diagnostics.dropped_events
              << ",\"late\":" << diagnostics.callbacks_after_acceptance_closed
              << ",\"steady_span\":" << steady_maximum - steady_minimum
              << ",\"pass\":" << (pass ? "true" : "false") << "}" << std::endl;
    return pass ? EXIT_SUCCESS : EXIT_FAILURE;
}
