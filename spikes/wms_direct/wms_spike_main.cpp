#include "route_identity.hpp"

#include <windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.Diagnostics.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.Messages.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.Utilities.SysExTransfer.h>

#include "winmidi/init/Microsoft.Windows.Devices.Midi2.Initialization.hpp"
#include "winmidi/init/WindowsMidiServicesVersion.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace midi = winrt::Microsoft::Windows::Devices::Midi2;
namespace diagnostics = winrt::Microsoft::Windows::Devices::Midi2::Diagnostics;
namespace messages = winrt::Microsoft::Windows::Devices::Midi2::Messages;
namespace sysex = winrt::Microsoft::Windows::Devices::Midi2::Utilities::SysExTransfer;
namespace init = Microsoft::Windows::Devices::Midi2::Initialization;
using namespace std::chrono_literals;

namespace {

struct ReceiveState {
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<std::uint32_t> first_words;
    std::vector<std::uint8_t> sysex_bytes;
    std::atomic<bool> accepting{true};
    std::atomic<unsigned> callbacks_after_close{0};
};

std::string json_escape(const std::string& value) {
    std::string result;
    for (const char ch : value) {
        switch (ch) {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default: result += ch; break;
        }
    }
    return result;
}

void emit_string(const char* event, const std::string& key, const std::string& value) {
    std::cout << "{\"event\":\"" << event << "\",\"" << key << "\":\""
              << json_escape(value) << "\"}" << std::endl;
}

taureon::spike::RouteDirection direction_from(
    const midi::MidiGroupTerminalBlockDirection direction) {
    if (direction == midi::MidiGroupTerminalBlockDirection::BlockInput) {
        return taureon::spike::RouteDirection::input;
    }
    if (direction == midi::MidiGroupTerminalBlockDirection::BlockOutput) {
        return taureon::spike::RouteDirection::output;
    }
    return taureon::spike::RouteDirection::bidirectional;
}

std::vector<taureon::spike::WmsRouteIdentity> enumerate_routes() {
    const auto filters = midi::MidiEndpointDeviceInformationFilters::AllStandardEndpoints |
                         midi::MidiEndpointDeviceInformationFilters::DiagnosticLoopback |
                         midi::MidiEndpointDeviceInformationFilters::VirtualDeviceResponder;
    const auto endpoints = midi::MidiEndpointDeviceInformation::FindAll(
        midi::MidiEndpointDeviceInformationSortOrder::EndpointDeviceId, filters);

    std::vector<taureon::spike::WmsRouteIdentity> routes;
    for (const auto& endpoint : endpoints) {
        const auto endpoint_id = winrt::to_string(endpoint.EndpointDeviceId());
        std::cout << "{\"event\":\"endpoint\",\"id\":\"" << json_escape(endpoint_id)
                  << "\",\"name\":\"" << json_escape(winrt::to_string(endpoint.Name()))
                  << "\",\"purpose\":" << static_cast<int>(endpoint.EndpointPurpose()) << "}"
                  << std::endl;
        for (const auto& block : endpoint.GetGroupTerminalBlocks()) {
            const auto first_group = block.FirstGroup().Index();
            for (std::uint8_t offset = 0; offset < block.GroupCount(); ++offset) {
                routes.push_back({endpoint_id, static_cast<std::uint8_t>(first_group + offset),
                                  direction_from(block.Direction())});
            }
        }
    }
    std::cout << "{\"event\":\"enumeration\",\"endpoints\":" << endpoints.Size()
              << ",\"composite_routes\":" << routes.size() << "}" << std::endl;
    return routes;
}

bool exercise_watcher() {
    const auto filters = midi::MidiEndpointDeviceInformationFilters::AllStandardEndpoints |
                         midi::MidiEndpointDeviceInformationFilters::DiagnosticLoopback;
    auto watcher = midi::MidiEndpointDeviceWatcher::Create(filters);
    std::mutex mutex;
    std::condition_variable changed;
    bool complete = false;
    bool stopped = false;
    std::atomic<unsigned> added{0};
    std::atomic<unsigned> updated{0};
    std::atomic<unsigned> removed{0};

    const auto added_token = watcher.Added([&](auto const&, auto const&) { ++added; });
    const auto updated_token = watcher.Updated([&](auto const&, auto const&) { ++updated; });
    const auto removed_token = watcher.Removed([&](auto const&, auto const&) { ++removed; });
    const auto completed_token = watcher.EnumerationCompleted([&](auto const&, auto const&) {
        std::scoped_lock lock(mutex);
        complete = true;
        changed.notify_all();
    });
    const auto stopped_token = watcher.Stopped([&](auto const&, auto const&) {
        std::scoped_lock lock(mutex);
        stopped = true;
        changed.notify_all();
    });

    watcher.Start();
    {
        std::unique_lock lock(mutex);
        changed.wait_for(lock, 5s, [&] { return complete; });
    }
    watcher.Stop();
    {
        std::unique_lock lock(mutex);
        changed.wait_for(lock, 5s, [&] { return stopped; });
    }
    watcher.Added(added_token);
    watcher.Updated(updated_token);
    watcher.Removed(removed_token);
    watcher.EnumerationCompleted(completed_token);
    watcher.Stopped(stopped_token);
    watcher = nullptr;

    std::cout << "{\"event\":\"watcher\",\"enumeration_completed\":"
              << (complete ? "true" : "false") << ",\"stopped\":"
              << (stopped ? "true" : "false") << ",\"added\":" << added.load()
              << ",\"updated\":" << updated.load() << ",\"removed\":" << removed.load()
              << "}" << std::endl;
    return complete && stopped;
}

bool exercise_diagnostic_loopback() {
    auto session = midi::MidiSession::Create(L"TAUREON Stage 1 WMS spike");
    auto sender = session.CreateEndpointConnection(
        diagnostics::MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId());
    auto receiver = session.CreateEndpointConnection(
        diagnostics::MidiDiagnostics::DiagnosticsLoopbackBEndpointDeviceId());
    auto state = std::make_shared<ReceiveState>();

    const auto token = receiver.MessageReceived(
        [state](midi::IMidiMessageReceivedEventSource const&,
                midi::MidiMessageReceivedEventArgs const& args) {
            if (!state->accepting.load(std::memory_order_acquire)) {
                ++state->callbacks_after_close;
                return;
            }
            const auto packet = args.GetMessagePacket();
            const auto first_word = args.PeekFirstWord();
            std::scoped_lock lock(state->mutex);
            state->first_words.push_back(first_word);
            if (sysex::MidiSystemExclusiveMessageHelper::MessageIsSystemExclusive7Message(first_word)) {
                const auto message = packet.as<midi::MidiMessage64>();
                const auto bytes = sysex::MidiSystemExclusiveMessageHelper::
                    GetDataBytesFromSingleSystemExclusive7Message(message);
                for (const auto byte : bytes) state->sysex_bytes.push_back(byte);
            }
            state->changed.notify_all();
        });

    if (!sender.Open() || !receiver.Open()) {
        receiver.MessageReceived(token);
        session.Close();
        return false;
    }

    const auto timestamp = midi::MidiClock::TimestampConstantSendImmediately();
    const auto group = midi::MidiGroup(5);
    const auto short_message = messages::MidiMessageBuilder::BuildMidi1ChannelVoiceMessage(
        timestamp, group, messages::Midi1ChannelVoiceMessageStatus::NoteOn,
        midi::MidiChannel(3), 60, 1);
    const auto start = messages::MidiMessageBuilder::BuildSystemExclusive7Message(
        timestamp, group, 1, 6, 0x7d, 1, 2, 3, 4, 5);
    const auto continuation = messages::MidiMessageBuilder::BuildSystemExclusive7Message(
        timestamp, group, 2, 6, 6, 7, 8, 9, 10, 11);
    const auto end = messages::MidiMessageBuilder::BuildSystemExclusive7Message(
        timestamp, group, 3, 2, 12, 13, 0, 0, 0, 0);

    bool sends_ok = midi::MidiEndpointConnection::SendMessageSucceeded(
        sender.SendSingleMessagePacket(short_message)) &&
        midi::MidiEndpointConnection::SendMessageSucceeded(sender.SendSingleMessagePacket(start)) &&
        midi::MidiEndpointConnection::SendMessageSucceeded(sender.SendSingleMessagePacket(continuation)) &&
        midi::MidiEndpointConnection::SendMessageSucceeded(sender.SendSingleMessagePacket(end));

    bool received = false;
    {
        std::unique_lock lock(state->mutex);
        received = state->changed.wait_for(lock, 5s, [&] { return state->first_words.size() >= 4; });
    }

    receiver.MessageReceived(token);
    state->accepting.store(false, std::memory_order_release);
    session.DisconnectEndpointConnection(sender.ConnectionId());
    session.DisconnectEndpointConnection(receiver.ConnectionId());
    session.Close();
    sender = nullptr;
    receiver = nullptr;
    session = nullptr;
    std::this_thread::sleep_for(50ms);

    const std::vector<std::uint8_t> expected{0x7d, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
    bool bytes_ok = false;
    std::size_t packet_count = 0;
    {
        std::scoped_lock lock(state->mutex);
        bytes_ok = state->sysex_bytes == expected;
        packet_count = state->first_words.size();
    }
    std::cout << "{\"event\":\"diagnostic_loopback\",\"send_ok\":"
              << (sends_ok ? "true" : "false") << ",\"received\":"
              << (received ? "true" : "false") << ",\"packet_count\":" << packet_count
              << ",\"sysex_sequence\":\"start-continue-end\",\"sysex_bytes\":14"
              << ",\"byte_integrity\":" << (bytes_ok ? "true" : "false")
              << ",\"callbacks_after_close\":" << state->callbacks_after_close.load() << "}"
              << std::endl;
    return sends_ok && received && bytes_ok && state->callbacks_after_close.load() == 0;
}

bool run_cycle(const unsigned cycle, const bool full_evidence) {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    auto initializer = std::make_shared<init::MidiDesktopAppSdkInitializer>();
    bool ok = initializer->IsServiceInstalled();
    if (ok) ok = initializer->InitializeSdkRuntime();
    if (ok) ok = initializer->CheckForMinimumRequiredSdkVersion(1, 0, 17);
    if (ok) ok = initializer->EnsureServiceAvailable();

    if (ok) {
        const auto routes = enumerate_routes();
        ok = !routes.empty();
        if (full_evidence) ok = exercise_watcher() && exercise_diagnostic_loopback() && ok;
    }

    initializer->ShutdownSdkRuntime();
    initializer.reset();
    winrt::uninit_apartment();
    std::cout << "{\"event\":\"lifecycle\",\"cycle\":" << cycle
              << ",\"ok\":" << (ok ? "true" : "false") << "}" << std::endl;
    return ok;
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--identity-only") {
        const bool passed = taureon::spike::run_wms_route_identity_acceptance_tests();
        std::cout << "{\"event\":\"route_identity_logic\",\"exact_composite_only\":true,"
                     "\"missing_visible\":true,\"ambiguous_visible\":true,\"passed\":"
                  << (passed ? "true" : "false") << "}" << std::endl;
        return passed ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    unsigned cycles = 1;
    if (argc == 3 && std::string(argv[1]) == "--cycles") {
        cycles = static_cast<unsigned>(std::stoul(argv[2]));
    }

    const bool identity_ok = taureon::spike::run_wms_route_identity_acceptance_tests();
    std::cout << "{\"event\":\"dependency\",\"wms_sdk\":\"1.0.17-rc.4.25\","
                 "\"cppwinrt\":\"2.0.240405.15\",\"windows_sdk\":\"10.0.26100.0\"}"
              << std::endl;
    emit_string("api_mode", "status",
                "unavailable_in_pinned_Microsoft.Windows.Devices.Midi2_1.0.17-rc.4.25");
    std::cout << "{\"event\":\"route_identity_logic\",\"passed\":"
              << (identity_ok ? "true" : "false") << "}" << std::endl;

    bool all_ok = identity_ok;
    DWORD handles_after_first_cycle = 0;
    DWORD handles_at_steady_state_start = 0;
    DWORD handles_after_final_cycle = 0;
    DWORD steady_state_minimum = MAXDWORD;
    DWORD steady_state_maximum = 0;
    bool handle_queries_ok = true;
    const unsigned steady_state_start_cycle = cycles >= 20 ? (cycles / 2) + 1 : 1;
    for (unsigned cycle = 1; cycle <= cycles; ++cycle) {
        all_ok = run_cycle(cycle, cycle == 1) && all_ok;
        DWORD current_handles = 0;
        if (!GetProcessHandleCount(GetCurrentProcess(), &current_handles)) {
            handle_queries_ok = false;
        } else {
            if (cycle == 1) handles_after_first_cycle = current_handles;
            if (cycle == steady_state_start_cycle) handles_at_steady_state_start = current_handles;
            if (cycle == cycles) handles_after_final_cycle = current_handles;
            if (cycle >= steady_state_start_cycle) {
                steady_state_minimum = (std::min)(steady_state_minimum, current_handles);
                steady_state_maximum = (std::max)(steady_state_maximum, current_handles);
            }
            if (cycle == 1 || cycle == steady_state_start_cycle || cycle == cycles) {
                std::cout << "{\"event\":\"handle_checkpoint\",\"cycle\":" << cycle
                          << ",\"count\":" << current_handles << "}" << std::endl;
            }
        }
    }
    const bool handles_stable = handle_queries_ok && steady_state_minimum == steady_state_maximum;
    all_ok = handles_stable && all_ok;
    std::cout << "{\"event\":\"handle_stability\",\"after_cycle_1\":"
              << handles_after_first_cycle << ",\"steady_state_start_cycle\":"
              << steady_state_start_cycle << ",\"at_steady_state_start\":"
              << handles_at_steady_state_start
              << ",\"after_cycle_" << cycles << "\":" << handles_after_final_cycle
              << ",\"process_lifetime_growth\":"
              << static_cast<long long>(handles_after_final_cycle) -
                     static_cast<long long>(handles_after_first_cycle)
              << ",\"steady_state_minimum\":" << steady_state_minimum
              << ",\"steady_state_maximum\":" << steady_state_maximum
              << ",\"stable\":" << (handles_stable ? "true" : "false") << "}"
              << std::endl;
    std::cout << "{\"event\":\"summary\",\"cycles\":" << cycles
              << ",\"pass\":" << (all_ok ? "true" : "false") << "}" << std::endl;
    return all_ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
