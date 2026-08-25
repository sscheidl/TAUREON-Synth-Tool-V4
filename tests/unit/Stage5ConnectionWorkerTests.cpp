#include "TestSupport.hpp"

#include "app/ConnectionWorker.hpp"
#include "core/sysex/SysEx7.hpp"
#include "transports/fake/FakeMidiTransport.hpp"

#include <filesystem>
#include <condition_variable>
#include <future>
#include <mutex>
#include <thread>

using namespace taureon;

namespace {

class BlockingEnumerateTransport final : public midi::IMidiTransport {
public:
    BlockingEnumerateTransport()
        : delegate_(midi::MidiBackend::windows_midi_services) {}

    midi::MidiBackend backend() const noexcept override { return delegate_.backend(); }
    midi::MidiTransportCapabilities capabilities() const noexcept override {
        return delegate_.capabilities();
    }
    midi::MidiTransportDiagnostics diagnostics() const noexcept override {
        return delegate_.diagnostics();
    }
    midi::TransportState state() const noexcept override { return delegate_.state(); }
    midi::Result<std::vector<midi::MidiEndpointDescriptor>> enumerate() override {
        {
            std::scoped_lock lock(gate_mutex_);
            enumerate_entered_ = true;
        }
        gate_changed_.notify_all();
        std::unique_lock lock(gate_mutex_);
        gate_changed_.wait(lock, [this] { return enumerate_released_; });
        lock.unlock();
        return delegate_.enumerate();
    }
    midi::Result<void> open(const midi::MidiConnectionRequest& request) override {
        return delegate_.open(request);
    }
    midi::Result<void> close() override { return delegate_.close(); }
    midi::Result<void> send(const midi::NativeMidiMessage& message) override {
        return delegate_.send(message);
    }
    void set_message_handler(midi::MidiMessageHandler handler) override {
        delegate_.set_message_handler(std::move(handler));
    }
    void set_stream_event_handler(midi::MidiStreamEventHandler handler) override {
        delegate_.set_stream_event_handler(std::move(handler));
    }
    void set_endpoint_change_handler(midi::EndpointChangeHandler handler) override {
        delegate_.set_endpoint_change_handler(std::move(handler));
    }

    void wait_until_enumerating() {
        std::unique_lock lock(gate_mutex_);
        gate_changed_.wait(lock, [this] { return enumerate_entered_; });
    }
    void release_enumeration() {
        {
            std::scoped_lock lock(gate_mutex_);
            enumerate_released_ = true;
        }
        gate_changed_.notify_all();
    }
    void emit_received(const midi::NativeMidiMessage& message) { delegate_.emit_received(message); }

private:
    midi::FakeMidiTransport delegate_;
    std::mutex gate_mutex_;
    std::condition_variable gate_changed_;
    bool enumerate_entered_{};
    bool enumerate_released_{};
};

midi::MidiEndpointDescriptor endpoint(const midi::MidiBackend backend,
                                      const midi::MidiDirection direction, std::string id) {
    midi::MidiRouteIdentity identity;
    identity.backend = backend;
    identity.direction = direction;
    if (backend == midi::MidiBackend::windows_midi_services) {
        identity.native = midi::WmsRouteIdentity{std::move(id), 0};
    } else {
        identity.native = midi::WinmmRouteIdentity{std::move(id), 1, 2, 3};
    }
    return {identity, direction == midi::MidiDirection::input ? "RX" : "TX",
            midi::MidiProtocol::midi1,
            {direction == midi::MidiDirection::input, direction == midi::MidiDirection::output,
             true, false},
            std::nullopt, std::nullopt};
}

void bounded_application_stream_queue() {
    std::promise<BlockingEnumerateTransport*> created;
    auto created_future = created.get_future();
    app::ConnectionWorker worker([&](const midi::MidiBackend backend) {
        TAUREON_REQUIRE(backend == midi::MidiBackend::windows_midi_services);
        auto transport = std::make_unique<BlockingEnumerateTransport>();
        created.set_value(transport.get());
        return transport;
    });
    auto selected_future = worker.select_backend(midi::MidiBackend::windows_midi_services);
    auto* transport = created_future.get();
    transport->wait_until_enumerating();

    const midi::NativeMidiMessage event{
        midi::MidiBackend::windows_midi_services,
        midi::UmpNativeMessage{{0x30017D00u, 0x00000000u}}, std::nullopt};
    constexpr std::uint64_t injected = 9'000;
    constexpr std::uint64_t capacity = 8'192;
    for (std::uint64_t index = 0; index < injected; ++index) transport->emit_received(event);
    transport->release_enumeration();
    TAUREON_REQUIRE(selected_future.get());
    const auto snapshot = worker.sysex_snapshot().get();
    TAUREON_REQUIRE(snapshot);
    TAUREON_REQUIRE(snapshot.value().application_dropped_events == injected - capacity);
}

} // namespace

int main() {
    return test::run([] {
        bounded_application_stream_queue();
        bool empty_factory_rejected = false;
        try {
            app::ConnectionWorker invalid_worker({});
        } catch (const std::invalid_argument&) {
            empty_factory_rejected = true;
        }
        TAUREON_REQUIRE(empty_factory_rejected);

        const auto test_thread = std::this_thread::get_id();
        std::mutex factory_mutex;
        std::thread::id factory_thread;
        midi::FakeMidiTransport* current_transport = nullptr;
        app::ConnectionWorker worker([&](const midi::MidiBackend backend) {
            const auto rx = endpoint(backend, midi::MidiDirection::input, "rx");
            const auto tx = endpoint(backend, midi::MidiDirection::output, "tx");
            auto transport = std::make_unique<midi::FakeMidiTransport>(backend,
                                                                       std::vector{rx, tx});
            {
                std::scoped_lock lock(factory_mutex);
                factory_thread = std::this_thread::get_id();
                current_transport = transport.get();
            }
            return transport;
        });

        auto selected = worker.select_backend(midi::MidiBackend::windows_midi_services).get();
        TAUREON_REQUIRE(selected);
        TAUREON_REQUIRE(selected.value().endpoints.size() == 2);
        {
            std::scoped_lock lock(factory_mutex);
            TAUREON_REQUIRE(factory_thread != test_thread);
        }
        const auto rx = selected.value().endpoints[0].identity;
        const auto tx = selected.value().endpoints[1].identity;
        auto connected = worker.connect(rx, tx).get();
        TAUREON_REQUIRE(connected);
        TAUREON_REQUIRE(connected.value().state == app::ConnectionPresentationState::connected);

        const auto fixture = std::filesystem::path{TAUREON_SOURCE_DIR} / "tests" / "fixtures" /
                             "novation_summit_crazy_sine.syx";
        const auto loaded = worker.load_sysex(fixture).get();
        TAUREON_REQUIRE(loaded);
        TAUREON_REQUIRE(loaded.value().can_raw_send);
        const auto send_started = worker.start_raw_sysex_send(std::chrono::milliseconds{0}).get();
        TAUREON_REQUIRE(send_started);
        midi::Result<app::SysExTransferSnapshot> sent = worker.sysex_snapshot().get();
        for (int attempt = 0;
             attempt < 10'000 && sent &&
             sent.value().send_progress.state != transfer::TransferState::completed;
             ++attempt) {
            std::this_thread::yield();
            sent = worker.sysex_snapshot().get();
        }
        TAUREON_REQUIRE(sent);
        TAUREON_REQUIRE(sent.value().send_progress.state == transfer::TransferState::completed);
        {
            std::scoped_lock lock(factory_mutex);
            TAUREON_REQUIRE(current_transport->diagnostics().transmitted_messages == 1);
        }

        const sysex::SysExFrame received_frame{
            sysex::SysExFrameStatus::complete, {0xF0, 0x7D, 0x11, 0xF7}, {},
            std::optional<std::uint8_t>{static_cast<std::uint8_t>(0)}, false};
        const auto encoded = sysex::encode_sysex7(received_frame, 0);
        TAUREON_REQUIRE(encoded);
        midi::UmpNativeMessage received_ump;
        for (const auto& packet : encoded.value()) {
            received_ump.words.push_back(packet.word0);
            received_ump.words.push_back(packet.word1);
        }

        TAUREON_REQUIRE(worker.begin_sysex_receive().get());
        {
            std::scoped_lock lock(factory_mutex);
            current_transport->emit_received(
                {midi::MidiBackend::windows_midi_services, received_ump, std::nullopt});
        }
        const auto captured = worker.finish_sysex_receive().get();
        TAUREON_REQUIRE(captured);
        TAUREON_REQUIRE(captured.value().frames.size() == 1);
        TAUREON_REQUIRE(captured.value().complete_frames == 1);
        TAUREON_REQUIRE(captured.value().can_save_verified_received);

        TAUREON_REQUIRE(worker.begin_sysex_receive().get());
        {
            std::scoped_lock lock(factory_mutex);
            current_transport->emit_data_loss(
                {midi::MidiBackend::windows_midi_services,
                 midi::MidiDataLossReason::queue_overflow, true,
                 std::optional<std::uint8_t>{static_cast<std::uint8_t>(0)}, "injected",
                 std::nullopt});
            current_transport->emit_received(
                {midi::MidiBackend::windows_midi_services, received_ump, std::nullopt});
        }
        const auto tainted = worker.finish_sysex_receive().get();
        TAUREON_REQUIRE(tainted);
        TAUREON_REQUIRE(tainted.value().tainted_frames == 1);
        TAUREON_REQUIRE(!tainted.value().can_raw_send);
        TAUREON_REQUIRE(!tainted.value().can_save_verified_received);

        {
            std::scoped_lock lock(factory_mutex);
            TAUREON_REQUIRE(current_transport != nullptr);
            current_transport->remove_endpoint(tx);
        }
        const auto degraded = worker.snapshot().get();
        TAUREON_REQUIRE(degraded);
        TAUREON_REQUIRE(degraded.value().state == app::ConnectionPresentationState::degraded);
        TAUREON_REQUIRE(degraded.value().transmit_route == tx);
        TAUREON_REQUIRE(worker.disconnect().get());

        const auto switched = worker.select_backend(midi::MidiBackend::winmm).get();
        TAUREON_REQUIRE(switched);
        TAUREON_REQUIRE(switched.value().endpoints.size() == 2);
        TAUREON_REQUIRE(switched.value().endpoints.front().identity.backend ==
                        midi::MidiBackend::winmm);
    });
}
