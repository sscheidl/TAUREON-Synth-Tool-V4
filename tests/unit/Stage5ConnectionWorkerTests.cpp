#include "TestSupport.hpp"

#include "app/ConnectionWorker.hpp"
#include "transports/fake/FakeMidiTransport.hpp"

#include <mutex>
#include <thread>

using namespace taureon;

namespace {

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

} // namespace

int main() {
    return test::run([] {
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
