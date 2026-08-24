#include "TestSupport.hpp"

#include "core/midi/RoutePersistence.hpp"
#include "core/midi/RouteResolver.hpp"
#include "core/midi/TransportState.hpp"
#include "transports/fake/FakeMidiTransport.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

using namespace taureon::midi;

namespace {

MidiRouteIdentity wms_route(const MidiDirection direction, std::string endpoint,
                            const std::uint8_t group) {
    return {MidiBackend::windows_midi_services, direction,
            WmsRouteIdentity{std::move(endpoint), group}};
}

MidiRouteIdentity winmm_route(const MidiDirection direction, std::string name,
                              const std::uint16_t product = 26) {
    return {MidiBackend::winmm, direction,
            WinmmRouteIdentity{std::move(name), 1, product, 256}};
}

MidiEndpointDescriptor endpoint(MidiRouteIdentity identity, std::string display,
                                const std::uint32_t index) {
    const bool input = identity.direction == MidiDirection::input;
    return {std::move(identity), std::move(display), MidiProtocol::midi1,
            {input, !input, true, false}, index, std::nullopt};
}

void persistence_tests() {
    const PersistedMidiRoute wms{1, wms_route(MidiDirection::input,
                                              R"(\\?\swd#MIDI;route=one%two)", 15)};
    const auto serialized_wms = serialize_route(wms);
    TAUREON_REQUIRE(serialized_wms);
    TAUREON_REQUIRE(serialized_wms.value().find("backend=wms") != std::string::npos);
    const auto restored_wms = deserialize_route(serialized_wms.value());
    TAUREON_REQUIRE(restored_wms);
    TAUREON_REQUIRE(restored_wms.value() == wms);

    const PersistedMidiRoute winmm{1, winmm_route(MidiDirection::output, "Port; A = exact")};
    const auto serialized_winmm = serialize_route(winmm);
    TAUREON_REQUIRE(serialized_winmm);
    TAUREON_REQUIRE(serialized_winmm.value().find("backend=winmm") != std::string::npos);
    TAUREON_REQUIRE(serialized_winmm.value().find("index") == std::string::npos);
    const auto restored_winmm = deserialize_route(serialized_winmm.value());
    TAUREON_REQUIRE(restored_winmm);
    TAUREON_REQUIRE(restored_winmm.value() == winmm);

    TAUREON_REQUIRE(!deserialize_route("version=2;backend=wms;direction=input;endpoint=x;group=0"));
    TAUREON_REQUIRE(!deserialize_route("version=1;backend=wms;direction=input;endpoint=x;group=16"));
    TAUREON_REQUIRE(!deserialize_route("version=1;backend=winmm;direction=output;name=x;wmid=1;wpid=2"));
    TAUREON_REQUIRE(!deserialize_route(
        "version=1;backend=winmm;direction=output;name=x;wmid=1;wpid=2;driver=3;index=4"));
    TAUREON_REQUIRE(!deserialize_route("version=1;backend=wms;backend=winmm;direction=input"));
}

void resolver_tests() {
    const auto persisted = winmm_route(MidiDirection::output, "Port 10");
    const auto same_display_other_backend = endpoint(
        wms_route(MidiDirection::output, "endpoint-10", 0), "Port 10", 0);
    const auto exact = endpoint(persisted, "Port 10", 17);
    const auto near_duplicate = endpoint(winmm_route(MidiDirection::output, "Port 10", 27),
                                         "Port 10", 18);

    const auto resolved = resolve_route(persisted,
                                        {near_duplicate, same_display_other_backend, exact});
    TAUREON_REQUIRE(resolved.status == RouteResolutionStatus::exact);
    TAUREON_REQUIRE(resolved.endpoint->runtime_index_hint.value() == 17U);

    const auto reordered = resolve_route(persisted, {exact, near_duplicate});
    TAUREON_REQUIRE(reordered.status == RouteResolutionStatus::exact);
    TAUREON_REQUIRE(resolve_route(persisted, {same_display_other_backend, near_duplicate}).status ==
                    RouteResolutionStatus::missing);
    TAUREON_REQUIRE(resolve_route(persisted, {exact, exact}).status ==
                    RouteResolutionStatus::ambiguous);
    TAUREON_REQUIRE(resolve_route(
        {MidiBackend::winmm, MidiDirection::output, WmsRouteIdentity{"wrong-variant", 0}},
        {exact}).status == RouteResolutionStatus::invalid);
}

void state_tests() {
    TransportStateMachine state;
    TAUREON_REQUIRE(state.state() == TransportState::closed);
    TAUREON_REQUIRE(state.begin_close());
    TAUREON_REQUIRE(state.state() == TransportState::closed);
    TAUREON_REQUIRE(state.begin_open());
    TAUREON_REQUIRE(state.state() == TransportState::opening);
    TAUREON_REQUIRE(!state.begin_open());
    TAUREON_REQUIRE(state.complete_open());
    TAUREON_REQUIRE(state.begin_close());
    TAUREON_REQUIRE(state.complete_close());
    state.fail();
    TAUREON_REQUIRE(state.state() == TransportState::failed);
    TAUREON_REQUIRE(state.begin_close());
    TAUREON_REQUIRE(state.complete_close());
}

void fake_transport_tests() {
    const auto rx = endpoint(wms_route(MidiDirection::input, "endpoint-rx", 1), "RX", 1);
    const auto tx = endpoint(wms_route(MidiDirection::output, "endpoint-tx", 7), "TX", 2);
    FakeMidiTransport transport(MidiBackend::windows_midi_services, {rx, tx});
    MidiConnectionRequest request{rx.identity, tx.identity};

    for (int cycle = 0; cycle < 100; ++cycle) {
        TAUREON_REQUIRE(transport.open(request));
        TAUREON_REQUIRE(transport.state() == TransportState::open);
        TAUREON_REQUIRE(transport.close());
        TAUREON_REQUIRE(transport.state() == TransportState::closed);
    }

    bool received = false;
    TAUREON_REQUIRE(transport.open(request));
    transport.set_message_handler([&](const NativeMidiMessage& message) {
        received = std::holds_alternative<UmpNativeMessage>(message.data);
    });
    TAUREON_REQUIRE(transport.send({MidiBackend::windows_midi_services,
                                    UmpNativeMessage{{0x40903c01}},
                                    MidiTimestamp{1234, "fake"}}));
    TAUREON_REQUIRE(received);

    EndpointChangeKind change = EndpointChangeKind::metadata_changed;
    transport.set_endpoint_change_handler([&](const EndpointChange& event) { change = event.kind; });
    transport.remove_endpoint(rx.identity);
    TAUREON_REQUIRE(change == EndpointChangeKind::selected_route_unavailable);
    TAUREON_REQUIRE(transport.state() == TransportState::failed);
    TAUREON_REQUIRE(transport.close());

    const auto wrong_backend = winmm_route(MidiDirection::input, "RX");
    const auto failure = transport.open({wrong_backend, tx.identity});
    TAUREON_REQUIRE(!failure);
    TAUREON_REQUIRE(failure.error().code == MidiErrorCode::invalid_route);
    TAUREON_REQUIRE(transport.close());
}

void message_representation_tests() {
    const NativeMidiMessage unknown_ump{MidiBackend::windows_midi_services,
                                        UmpNativeMessage{{0xf1234567, 0x89abcdef}},
                                        MidiTimestamp{55, "wms-native-ticks"}};
    TAUREON_REQUIRE(std::get<UmpNativeMessage>(unknown_ump.data).words.size() == 2);
    TAUREON_REQUIRE(unknown_ump.timestamp->native_value == 55);

    const NativeMidiMessage midi1{MidiBackend::winmm,
                                  Midi1NativeMessage{{0xf0, 0x7d, 0x00, 0xf7}},
                                  std::nullopt};
    TAUREON_REQUIRE(std::get<Midi1NativeMessage>(midi1.data).bytes.front() == 0xf0);
    TAUREON_REQUIRE(std::get<Midi1NativeMessage>(midi1.data).bytes.back() == 0xf7);
}

void error_tests() {
    const MidiError native{MidiErrorCode::open_failure, "open failed", "midiInOpen", 4};
    const auto result = Result<void>::failure(native);
    TAUREON_REQUIRE(!result);
    TAUREON_REQUIRE(result.error() == native);
    TAUREON_REQUIRE(result.error().native_api == "midiInOpen");
    TAUREON_REQUIRE(result.error().native_code == 4);
}

} // namespace

int main() {
    return taureon::test::run([] {
        persistence_tests();
        resolver_tests();
        state_tests();
        fake_transport_tests();
        message_representation_tests();
        error_tests();
    });
}
