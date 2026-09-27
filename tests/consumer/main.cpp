#include "taureon/core/midi/MidiMessage.hpp"
#include "taureon/core/midi/MidiError.hpp"
#include "taureon/core/midi/EngineVersion.hpp"
#include "taureon/core/midi/MidiTypes.hpp"
#include "taureon/core/midi/Result.hpp"
#include "taureon/core/midi/RoutePersistence.hpp"
#include "taureon/core/midi/RouteResolver.hpp"
#include "taureon/core/midi/TransportState.hpp"
#include "taureon/core/sysex/SysEx7.hpp"
#include "taureon/core/sysex/SysExCaptureSession.hpp"
#include "taureon/core/sysex/SysExFrame.hpp"
#include "taureon/core/sysex/SysExStreamParser.hpp"
#include "taureon/core/sysex/SyxFile.hpp"
#include "taureon/core/transfer/TransferEngine.hpp"
#include "taureon/transports/IMidiTransport.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace {

class ConsumerTransport final : public taureon::midi::IMidiTransport {
public:
    taureon::midi::MidiBackend backend() const noexcept override {
        return taureon::midi::MidiBackend::external;
    }
    taureon::midi::MidiTransportCapabilities capabilities() const noexcept override {
        return {false, true, false, true, false};
    }
    taureon::midi::MidiTransportDiagnostics diagnostics() const noexcept override {
        taureon::midi::MidiTransportDiagnostics result;
        result.transmitted_messages = sent_.load();
        return result;
    }
    taureon::midi::TransportState state() const noexcept override {
        return open_.load() ? taureon::midi::TransportState::open :
                              taureon::midi::TransportState::closed;
    }
    taureon::midi::Result<std::vector<taureon::midi::MidiEndpointDescriptor>> enumerate() override {
        return taureon::midi::Result<std::vector<taureon::midi::MidiEndpointDescriptor>>::success(
            {{{backend(), taureon::midi::MidiDirection::output,
               taureon::midi::ExternalRouteIdentity{"org.taureon.consumer", "out-1"}},
              "Consumer output", taureon::midi::MidiProtocol::midi1,
              {false, true, true, false}, std::nullopt, std::nullopt}});
    }
    taureon::midi::Result<void> open(const taureon::midi::MidiConnectionRequest& request) override {
        const auto endpoints = enumerate();
        if (!request.transmit_route || request.receive_route || !endpoints ||
            *request.transmit_route != endpoints.value().front().identity) {
            return taureon::midi::Result<void>::failure(
                {taureon::midi::MidiErrorCode::invalid_route, "wrong external route", {}, std::nullopt});
        }
        open_.store(true);
        return taureon::midi::Result<void>::success();
    }
    taureon::midi::Result<void> close() override {
        open_.store(false);
        return taureon::midi::Result<void>::success();
    }
    taureon::midi::Result<void> send(const taureon::midi::NativeMidiMessage& message) override {
        if (!open_.load() || message.backend != backend()) return taureon::midi::Result<void>::failure(
            {taureon::midi::MidiErrorCode::invalid_state, "consumer transport not ready", {}, std::nullopt});
        sent_.fetch_add(1);
        return taureon::midi::Result<void>::success();
    }
    void set_message_handler(taureon::midi::MidiMessageHandler) override {}
    void set_stream_event_handler(taureon::midi::MidiStreamEventHandler) override {}
    void set_endpoint_change_handler(taureon::midi::EndpointChangeHandler) override {}

private:
    std::atomic<bool> open_{false};
    std::atomic<std::uint64_t> sent_{0};
};

} // namespace

int main() {
    static_assert(taureon::midi::engine_version_major == 0 &&
                  taureon::midi::engine_version_minor == 3);
    const std::array<std::uint8_t, 3> pressure{0xa2, 62, 40};
    const auto midi = taureon::midi::parse_midi1_message(pressure);
    if (!midi || midi.value().kind != taureon::midi::Midi1MessageKind::polyphonic_aftertouch ||
        midi.value().channel != 2 || midi.value().data1 != 62 || midi.value().data2 != 40) {
        return 1;
    }

    const std::vector<std::uint8_t> bytes{0xf0, 0x7d, 1, 2, 3, 4, 5, 6, 7, 0xf7};
    taureon::sysex::SysExStreamParser parser;
    const auto parsed = parser.consume(bytes);
    if (parsed.frames.size() != 1 || parsed.frames.front().bytes != bytes) return 2;
    const auto packets = taureon::sysex::encode_sysex7(parsed.frames.front(), 3);
    if (!packets || packets.value().size() != 2) return 3;
    taureon::sysex::SysEx7Assembler assembler;
    std::vector<taureon::sysex::SysExFrame> reconstructed;
    for (const auto& packet : packets.value()) {
        auto frames = assembler.consume(packet);
        reconstructed.insert(reconstructed.end(), frames.begin(), frames.end());
    }
    if (reconstructed.size() != 1 || reconstructed.front().bytes != bytes) return 4;

    // Link and run the installed worker against a transport defined entirely by this consumer.
    ConsumerTransport transport;
    const auto endpoints = transport.enumerate();
    if (!endpoints || endpoints.value().size() != 1 ||
        !taureon::midi::is_valid(endpoints.value().front().identity)) return 5;
    if (!transport.open({std::nullopt, endpoints.value().front().identity})) return 5;
    taureon::transfer::TransferEngine transfer(transport);
    taureon::midi::NativeMidiMessage note;
    note.backend = transport.backend();
    note.data = taureon::midi::Midi1NativeMessage{{0x90, 60, 100}};
    if (!transfer.start({note})) return 6;
    const auto result = transfer.wait();
    if (result.state != taureon::transfer::TransferState::completed ||
        result.progress.messages_accepted != 1 ||
        transport.diagnostics().transmitted_messages != 1) return 7;
    if (!transport.close()) return 8;
    return 0;
}
