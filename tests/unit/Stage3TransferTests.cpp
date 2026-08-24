#include "TestSupport.hpp"

#include "core/transfer/TransferEngine.hpp"
#include "transports/fake/FakeMidiTransport.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

using namespace std::chrono_literals;
using namespace taureon;

namespace {

midi::MidiRouteIdentity tx_route() {
    return {midi::MidiBackend::winmm, midi::MidiDirection::output,
            midi::WinmmRouteIdentity{"Stage 3 Fake TX", 1, 26, 256}};
}

midi::MidiEndpointDescriptor tx_endpoint() {
    return {tx_route(), "Stage 3 Fake TX", midi::MidiProtocol::midi1,
            {false, true, true, false}, 0, std::nullopt};
}

midi::NativeMidiMessage message(const std::uint8_t note) {
    return {midi::MidiBackend::winmm,
            midi::Midi1NativeMessage{{0x90, note, 0x40}}, std::nullopt};
}

std::vector<midi::NativeMidiMessage> messages(const std::size_t count) {
    std::vector<midi::NativeMidiMessage> result;
    for (std::size_t index = 0; index < count; ++index) {
        result.push_back(message(static_cast<std::uint8_t>(index & 0x7f)));
    }
    return result;
}

void open(midi::FakeMidiTransport& transport) {
    TAUREON_REQUIRE(transport.open({std::nullopt, tx_route()}));
}

void success_pacing_progress() {
    midi::FakeMidiTransport transport(midi::MidiBackend::winmm, {tx_endpoint()});
    open(transport);
    std::vector<transfer::TransferProgress> progress;
    transfer::TransferEngine engine(transport);
    TAUREON_REQUIRE(engine.start(messages(3), {1ms, 1s},
                                  [&](const auto& value) { progress.push_back(value); }));
    const auto result = engine.wait();
    TAUREON_REQUIRE(result.state == transfer::TransferState::completed);
    TAUREON_REQUIRE(!result.error);
    TAUREON_REQUIRE(result.progress.messages_total == 3);
    TAUREON_REQUIRE(result.progress.messages_accepted == 3);
    TAUREON_REQUIRE(result.progress.bytes_total == 9);
    TAUREON_REQUIRE(result.progress.bytes_accepted == 9);
    TAUREON_REQUIRE(result.progress.pacing_intervals_applied == 2);
    TAUREON_REQUIRE(!progress.empty());
    TAUREON_REQUIRE(progress.back().state == transfer::TransferState::completed);
    TAUREON_REQUIRE(transport.close());
}

void cancellation_before_start() {
    midi::FakeMidiTransport transport(midi::MidiBackend::winmm, {tx_endpoint()});
    open(transport);
    transfer::TransferEngine engine(transport);
    engine.request_cancel();
    engine.request_cancel();
    TAUREON_REQUIRE(engine.start(messages(3)));
    const auto result = engine.wait();
    TAUREON_REQUIRE(result.state == transfer::TransferState::cancelled);
    TAUREON_REQUIRE(result.error->code == midi::MidiErrorCode::transfer_cancelled);
    TAUREON_REQUIRE(result.progress.messages_accepted == 0);
    TAUREON_REQUIRE(transport.close());
}

void cancellation_during_transfer() {
    midi::FakeMidiTransport transport(midi::MidiBackend::winmm, {tx_endpoint()});
    open(transport);
    transfer::TransferEngine engine(transport);
    TAUREON_REQUIRE(engine.start(messages(10), {1s, 5s}, [&](const auto& progress) {
        if (progress.messages_accepted == 1) engine.request_cancel();
    }));
    const auto result = engine.wait();
    TAUREON_REQUIRE(result.state == transfer::TransferState::cancelled);
    TAUREON_REQUIRE(result.progress.messages_accepted == 1);
    TAUREON_REQUIRE(transport.close());
}

void timeout_and_send_failure() {
    midi::FakeMidiTransport timeout_transport(midi::MidiBackend::winmm, {tx_endpoint()});
    open(timeout_transport);
    transfer::TransferEngine timeout_engine(timeout_transport);
    TAUREON_REQUIRE(timeout_engine.start(messages(1), {0ms, 0ms}));
    const auto timeout_result = timeout_engine.wait();
    TAUREON_REQUIRE(timeout_result.state == transfer::TransferState::failed);
    TAUREON_REQUIRE(timeout_result.error->code == midi::MidiErrorCode::timeout);
    TAUREON_REQUIRE(timeout_result.progress.messages_accepted == 0);
    TAUREON_REQUIRE(timeout_transport.close());

    midi::FakeMidiTransport failing(midi::MidiBackend::winmm, {tx_endpoint()});
    open(failing);
    int sends = 0;
    failing.set_send_hook([&](const auto&) {
        ++sends;
        if (sends == 2) {
            return midi::Result<void>::failure(
                {midi::MidiErrorCode::native_api_error, "injected send failure", "fake", 42});
        }
        return midi::Result<void>::success();
    });
    transfer::TransferEngine failing_engine(failing);
    TAUREON_REQUIRE(failing_engine.start(messages(3)));
    const auto failed = failing_engine.wait();
    TAUREON_REQUIRE(failed.state == transfer::TransferState::failed);
    TAUREON_REQUIRE(failed.error->native_code == 42);
    TAUREON_REQUIRE(failed.progress.messages_accepted == 1);
    TAUREON_REQUIRE(failing.close());
}

void disconnect_and_shutdown() {
    midi::FakeMidiTransport transport(midi::MidiBackend::winmm, {tx_endpoint()});
    open(transport);
    transfer::TransferEngine engine(transport);
    TAUREON_REQUIRE(engine.start(messages(3), {1ms, 1s}, [&](const auto& progress) {
        if (progress.messages_accepted == 1) transport.remove_endpoint(tx_route());
    }));
    const auto disconnected = engine.wait();
    TAUREON_REQUIRE(disconnected.state == transfer::TransferState::failed);
    TAUREON_REQUIRE(disconnected.error->code == midi::MidiErrorCode::transport_disconnected);
    TAUREON_REQUIRE(disconnected.progress.messages_accepted == 1);
    TAUREON_REQUIRE(transport.close());

    midi::FakeMidiTransport shutdown_transport(midi::MidiBackend::winmm, {tx_endpoint()});
    open(shutdown_transport);
    std::atomic<int> sends{};
    shutdown_transport.set_send_hook([&](const auto&) {
        ++sends;
        return midi::Result<void>::success();
    });
    {
        transfer::TransferEngine shutdown_engine(shutdown_transport);
        TAUREON_REQUIRE(shutdown_engine.start(messages(100), {1s, 10s}));
    }
    TAUREON_REQUIRE(sends.load() <= 1);
    TAUREON_REQUIRE(shutdown_transport.close());
}

void fake_receive_and_overflow_diagnostics() {
    midi::FakeMidiTransport transport(midi::MidiBackend::winmm);
    bool received = false;
    transport.set_message_handler([&](const auto& value) {
        received = value == message(64);
    });
    transport.emit_received(message(64));
    transport.report_dropped_events(3);
    TAUREON_REQUIRE(received);
    TAUREON_REQUIRE(transport.diagnostics().native_callbacks == 1);
    TAUREON_REQUIRE(transport.diagnostics().dropped_events == 3);
}

} // namespace

int main() {
    return test::run([] {
        success_pacing_progress();
        cancellation_before_start();
        cancellation_during_transfer();
        timeout_and_send_failure();
        disconnect_and_shutdown();
        fake_receive_and_overflow_diagnostics();
    });
}
