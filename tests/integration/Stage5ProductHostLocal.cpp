#include "TestSupport.hpp"

#include "app/ConnectionWorker.hpp"
#include "app/MonitorEventQueue.hpp"
#include "app/NativeTransportFactory.hpp"
#include <taureon/core/sysex/SysEx7.hpp>
#include <taureon/core/sysex/SyxFile.hpp>
#include "gui/MainWindow.hpp"
#include "profiles/ProfileRegistry.hpp"
#include "../HandleGrowth.hpp"

#include <windows.h>

#include <QApplication>
#include <QComboBox>
#include <QEventLoop>
#include <QPushButton>
#include <QSpinBox>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <ranges>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using namespace taureon;

namespace {

struct TransportEvidence {
    std::mutex mutex;
    midi::MidiTransportDiagnostics final_diagnostics;
    std::uint64_t close_calls{};
    std::uint64_t successful_close_calls{};
    std::uint64_t failed_close_calls{};
    midi::TransportState final_state{midi::TransportState::failed};
    bool destroyed{};
};

class TrackingTransport final : public midi::IMidiTransport {
public:
    TrackingTransport(std::unique_ptr<midi::IMidiTransport> inner,
                      std::shared_ptr<TransportEvidence> evidence)
        : inner_(std::move(inner)), evidence_(std::move(evidence)) {}

    ~TrackingTransport() override {
        record(true);
    }

    midi::MidiBackend backend() const noexcept override { return inner_->backend(); }
    midi::MidiTransportCapabilities capabilities() const noexcept override {
        return inner_->capabilities();
    }
    midi::MidiTransportDiagnostics diagnostics() const noexcept override {
        return inner_->diagnostics();
    }
    midi::TransportState state() const noexcept override { return inner_->state(); }
    midi::Result<std::vector<midi::MidiEndpointDescriptor>> enumerate() override {
        return inner_->enumerate();
    }
    midi::Result<void> open(const midi::MidiConnectionRequest& request) override {
        return inner_->open(request);
    }
    midi::Result<void> close() override {
        const auto result = inner_->close();
        {
            std::scoped_lock lock(evidence_->mutex);
            ++evidence_->close_calls;
            if (result) ++evidence_->successful_close_calls;
            else ++evidence_->failed_close_calls;
        }
        record(false);
        return result;
    }
    midi::Result<void> send(const midi::NativeMidiMessage& message) override {
        return inner_->send(message);
    }
    void set_message_handler(midi::MidiMessageHandler handler) override {
        inner_->set_message_handler(std::move(handler));
    }
    void set_stream_event_handler(midi::MidiStreamEventHandler handler) override {
        inner_->set_stream_event_handler(std::move(handler));
    }
    void set_endpoint_change_handler(midi::EndpointChangeHandler handler) override {
        inner_->set_endpoint_change_handler(std::move(handler));
    }

private:
    void record(const bool destroyed) noexcept {
        const auto diagnostics = inner_->diagnostics();
        std::scoped_lock lock(evidence_->mutex);
        evidence_->final_diagnostics = diagnostics;
        evidence_->final_state = inner_->state();
        evidence_->destroyed = evidence_->destroyed || destroyed;
    }

    std::unique_ptr<midi::IMidiTransport> inner_;
    std::shared_ptr<TransportEvidence> evidence_;
};

bool safe_name(const std::string_view name) {
    return name.starts_with("TAUREON S5 WMS ");
}

template <typename Predicate>
bool process_until(Predicate&& predicate, const std::chrono::milliseconds timeout = 10s) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) return true;
        QApplication::processEvents(QEventLoop::AllEvents, 10);
        std::this_thread::sleep_for(1ms);
    }
    return predicate();
}

template <typename T>
T get_future(std::future<T> future) {
    TAUREON_REQUIRE(process_until([&] { return future.wait_for(0ms) == std::future_status::ready; }));
    return future.get();
}

std::shared_ptr<profiles::ProfileRegistry> load_profiles() {
    auto result = std::make_shared<profiles::ProfileRegistry>();
    std::vector<profiles::ProfileLoadIssue> issues;
    TAUREON_REQUIRE(result->load_directory(
        std::filesystem::path{TAUREON_SOURCE_DIR} / "resources" / "device_profiles", issues));
    TAUREON_REQUIRE(issues.empty());
    return result;
}

int find_route(QComboBox& selector, const std::string& name) {
    int match = -1;
    for (int index = 1; index < selector.count(); ++index) {
        if (selector.itemText(index).contains(QString::fromStdString(name))) {
            TAUREON_REQUIRE(match == -1);
            match = index;
        }
    }
    return match;
}

sysex::SyxDocument paced_document() {
    sysex::SyxDocument result;
    for (int index = 0; index < 256; ++index) {
        std::vector<std::uint8_t> bytes{
            0xF0, 0x7D, static_cast<std::uint8_t>(index & 0x7F), 0xF7};
        result.raw_bytes.insert(result.raw_bytes.end(), bytes.begin(), bytes.end());
        result.frames.push_back(
            {sysex::SysExFrameStatus::complete, std::move(bytes), {}, std::nullopt, false});
    }
    return result;
}

void send_receive_probe(const midi::MidiBackend backend, const std::string& output_name) {
    auto transport = app::create_native_transport(backend);
    const auto endpoints = transport->enumerate();
    TAUREON_REQUIRE(endpoints);
    const auto output = std::ranges::find_if(endpoints.value(), [&](const auto& endpoint) {
        return endpoint.identity.direction == midi::MidiDirection::output &&
               endpoint.display_name == output_name;
    });
    TAUREON_REQUIRE(output != endpoints.value().end());
    TAUREON_REQUIRE(std::ranges::count_if(endpoints.value(), [&](const auto& endpoint) {
        return endpoint.identity.direction == midi::MidiDirection::output &&
               endpoint.display_name == output_name;
    }) == 1);
    TAUREON_REQUIRE(transport->open({std::nullopt, output->identity}));

    const std::vector<std::uint8_t> bytes{0xF0, 0x7D, 0x52, 0x58, 0xF7};
    midi::NativeMidiMessage message;
    message.backend = backend;
    if (backend == midi::MidiBackend::winmm) {
        message.data = midi::Midi1NativeMessage{bytes};
    } else {
        const auto* identity = std::get_if<midi::WmsRouteIdentity>(&output->identity.native);
        TAUREON_REQUIRE(identity != nullptr);
        const auto encoded = sysex::encode_sysex7(
            {sysex::SysExFrameStatus::complete, bytes, {}, identity->group, false},
            identity->group);
        TAUREON_REQUIRE(encoded);
        std::vector<std::uint32_t> words;
        words.reserve(encoded.value().size() * 2);
        for (const auto& packet : encoded.value()) {
            words.push_back(packet.word0);
            words.push_back(packet.word1);
        }
        message.data = midi::UmpNativeMessage{std::move(words)};
    }
    TAUREON_REQUIRE(transport->send(message));
    TAUREON_REQUIRE(transport->close());
    TAUREON_REQUIRE(transport->state() == midi::TransportState::closed);
}

struct ProductHost {
    app::MonitorEventQueue monitor_queue{4096};
    std::atomic<std::uint64_t> sequence{};
    std::shared_ptr<profiles::ProfileRegistry> profiles{load_profiles()};
    std::shared_ptr<TransportEvidence> evidence{std::make_shared<TransportEvidence>()};
    std::unique_ptr<app::ConnectionWorker> worker;
    std::unique_ptr<gui::MainWindow> window;

    explicit ProductHost(const midi::MidiBackend backend) {
        worker = std::make_unique<app::ConnectionWorker>(
            [backend, evidence = evidence](const midi::MidiBackend requested) {
                TAUREON_REQUIRE(requested == backend);
                return std::make_unique<TrackingTransport>(
                    app::create_native_transport(requested), evidence);
            },
            [this](const midi::NativeMidiMessage& message) {
                static_cast<void>(monitor_queue.push(
                    {sequence.fetch_add(1, std::memory_order_relaxed),
                     midi::MidiDirection::input, message}));
            },
            profiles);
        window = std::make_unique<gui::MainWindow>(monitor_queue, *worker, profiles);
        window->show();
    }

    void connect(const midi::MidiBackend backend, const std::string& input_name,
                 const std::string& output_name) {
        auto* backend_selector = window->findChild<QComboBox*>("backendSelector");
        auto* receive = window->findChild<QComboBox*>("receiveRouteSelector");
        auto* transmit = window->findChild<QComboBox*>("transmitRouteSelector");
        auto* connect_button = window->findChild<QPushButton*>("connectButton");
        TAUREON_REQUIRE(backend_selector && receive && transmit && connect_button);

        backend_selector->setCurrentIndex(
            backend == midi::MidiBackend::windows_midi_services ? 1 : 2);
        TAUREON_REQUIRE(process_until([&] {
            return find_route(*receive, input_name) > 0 && find_route(*transmit, output_name) > 0;
        }));
        receive->setCurrentIndex(find_route(*receive, input_name));
        transmit->setCurrentIndex(find_route(*transmit, output_name));
        TAUREON_REQUIRE(connect_button->isEnabled());
        connect_button->click();
        TAUREON_REQUIRE(process_until([&] { return connect_button->text() == "Disconnect"; }));
    }

    std::chrono::milliseconds close_active() {
        const auto started = std::chrono::steady_clock::now();
        window->close();
        window.reset();
        worker.reset();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started);
    }
};

void assert_evidence(const std::shared_ptr<TransportEvidence>& evidence,
                     const midi::MidiBackend backend) {
    std::scoped_lock lock(evidence->mutex);
    TAUREON_REQUIRE(evidence->destroyed);
    TAUREON_REQUIRE(evidence->close_calls >= 1);
    TAUREON_REQUIRE(evidence->successful_close_calls == evidence->close_calls);
    TAUREON_REQUIRE(evidence->failed_close_calls == 0);
    TAUREON_REQUIRE(evidence->final_state == midi::TransportState::closed);
    TAUREON_REQUIRE(evidence->final_diagnostics.dropped_events == 0);
    TAUREON_REQUIRE(evidence->final_diagnostics.callbacks_after_acceptance_closed == 0);
    if (backend == midi::MidiBackend::windows_midi_services) {
        TAUREON_REQUIRE(evidence->final_diagnostics.worker_mta_apartment_observed);
    }
}

std::uint32_t process_handle_count() {
    DWORD handles{};
    TAUREON_REQUIRE(GetProcessHandleCount(GetCurrentProcess(), &handles));
    return static_cast<std::uint32_t>(handles);
}

void exercise_backend(const midi::MidiBackend backend, const std::string& input_name,
                      const std::string& output_name, const int cycles) {
    std::chrono::milliseconds maximum_shutdown{};
    std::vector<std::uint32_t> handle_samples;
    handle_samples.reserve(static_cast<std::size_t>(cycles));
    for (int cycle = 0; cycle < cycles; ++cycle) {
        {
            ProductHost host(backend);
            host.connect(backend, input_name, output_name);
            auto disconnected = get_future(host.worker->disconnect());
            TAUREON_REQUIRE(disconnected);
            const auto elapsed = host.close_active();
            maximum_shutdown = (std::max)(maximum_shutdown, elapsed);
            assert_evidence(host.evidence, backend);
        }
        QApplication::processEvents(QEventLoop::AllEvents);
        handle_samples.push_back(process_handle_count());
    }
    const auto steady_start = handle_samples.size() / 2;
    const auto handle_growth = test::analyze_handle_growth(handle_samples, steady_start);
    // The short product-host gate records process-wide handle diagnostics but is too
    // brief to distinguish a timed Qt/RPC/WMS plateau from slow cumulative growth.
    // Apply the retained directional leak criterion only to an explicit 100-cycle soak.
    const bool handle_growth_gate_applied = cycles >= 100;
    std::cout << "{\"event\":\"stage5_product_host_handle_trend\",\"backend\":\""
              << (backend == midi::MidiBackend::windows_midi_services ? "wms" : "winmm")
              << "\",\"steady_slope\":" << static_cast<double>(handle_growth.steady_slope)
              << ",\"new_steady_high\":"
              << (handle_growth.new_steady_high ? "true" : "false")
              << ",\"sustained_growth\":"
              << (handle_growth.sustained_growth ? "true" : "false")
              << ",\"gate_applied\":"
              << (handle_growth_gate_applied ? "true" : "false")
              << ",\"handle_samples\":[";
    for (std::size_t index = 0; index < handle_samples.size(); ++index) {
        if (index != 0) std::cout << ',';
        std::cout << handle_samples[index];
    }
    std::cout << "]}\n";
    if (handle_growth_gate_applied) TAUREON_REQUIRE(!handle_growth.sustained_growth);

    ProductHost receive_host(backend);
    receive_host.connect(backend, input_name, output_name);
    auto receiving = get_future(receive_host.worker->begin_sysex_receive());
    TAUREON_REQUIRE(receiving && receiving.value().receiving);
    send_receive_probe(backend, output_name);
    TAUREON_REQUIRE(process_until([&] {
        if (receive_host.sequence.load(std::memory_order_acquire) == 0) return false;
        const auto snapshot = get_future(receive_host.worker->sysex_snapshot());
        return snapshot && snapshot.value().receiving && snapshot.value().byte_count >= 5 &&
               snapshot.value().complete_frames >= 1;
    }));
    const auto receive_shutdown = receive_host.close_active();
    maximum_shutdown = (std::max)(maximum_shutdown, receive_shutdown);
    assert_evidence(receive_host.evidence, backend);
    std::uint64_t receive_active_callbacks{};
    std::uint64_t receive_active_delivered{};
    {
        std::scoped_lock lock(receive_host.evidence->mutex);
        receive_active_callbacks = receive_host.evidence->final_diagnostics.native_callbacks;
        receive_active_delivered = receive_host.evidence->final_diagnostics.delivered_messages;
    }
    TAUREON_REQUIRE(receive_active_callbacks > 0);
    TAUREON_REQUIRE(receive_active_delivered > 0);

    ProductHost send_host(backend);
    send_host.connect(backend, input_name, output_name);
    auto loaded = get_future(send_host.worker->load_sysex_document(paced_document(), "local-block-b"));
    TAUREON_REQUIRE(loaded && loaded.value().can_raw_send);
    auto sending = get_future(send_host.worker->start_raw_sysex_send(100ms));
    TAUREON_REQUIRE(sending);
    TAUREON_REQUIRE(sending.value().send_progress.messages_total == 256);
    TAUREON_REQUIRE(process_until([&] {
        auto snapshot = get_future(send_host.worker->sysex_snapshot());
        if (!snapshot) return false;
        const auto& progress = snapshot.value().send_progress;
        const bool active = progress.state == transfer::TransferState::preparing ||
                            progress.state == transfer::TransferState::running;
        return active && progress.messages_accepted > 0 &&
               progress.messages_accepted < progress.messages_total;
    }));
    const auto send_shutdown = send_host.close_active();
    maximum_shutdown = (std::max)(maximum_shutdown, send_shutdown);
    assert_evidence(send_host.evidence, backend);
    {
        std::scoped_lock lock(send_host.evidence->mutex);
        TAUREON_REQUIRE(send_host.evidence->final_diagnostics.transmitted_messages > 0);
        std::cout << "{\"event\":\"stage5_product_host\",\"backend\":\""
                  << (backend == midi::MidiBackend::windows_midi_services ? "wms" : "winmm")
                  << "\",\"cycles\":" << cycles
                  << ",\"receive_active_callbacks\":" << receive_active_callbacks
                  << ",\"receive_active_delivered\":" << receive_active_delivered
                  << ",\"tx\":" << send_host.evidence->final_diagnostics.transmitted_messages
                  << ",\"rx_callbacks\":"
                  << send_host.evidence->final_diagnostics.delivered_messages
                  << ",\"dropped\":" << send_host.evidence->final_diagnostics.dropped_events
                  << ",\"late\":"
                  << send_host.evidence->final_diagnostics.callbacks_after_acceptance_closed
                  << ",\"queue_high_water\":"
                  << send_host.evidence->final_diagnostics.queue_high_water_mark
                  << ",\"max_shutdown_ms\":" << maximum_shutdown.count()
                  << ",\"steady_handle_min\":" << handle_growth.steady_minimum
                  << ",\"steady_handle_max\":" << handle_growth.steady_maximum
                  << ",\"steady_handle_slope\":"
                  << static_cast<double>(handle_growth.steady_slope)
                  << ",\"handle_growth_gate_applied\":"
                  << (handle_growth_gate_applied ? "true" : "false")
                  << ",\"sustained_handle_growth\":"
                  << (handle_growth.sustained_growth ? "true" : "false") << "}\n";
    }
    TAUREON_REQUIRE(maximum_shutdown < 5s);
}

bool endpoints_absent(const midi::MidiBackend backend, const std::string& name_a,
                      const std::string& name_b) {
    auto transport = app::create_native_transport(backend);
    const auto endpoints = transport->enumerate();
    if (!endpoints) return false;
    return std::ranges::none_of(endpoints.value(), [&](const auto& endpoint) {
        return endpoint.display_name == name_a || endpoint.display_name == name_b;
    });
}

bool endpoints_present(const midi::MidiBackend backend, const std::string& input_name,
                       const std::string& output_name) {
    auto transport = app::create_native_transport(backend);
    const auto endpoints = transport->enumerate();
    if (!endpoints) return false;
    const auto has_route = [&](const midi::MidiDirection direction, const std::string& name) {
        return std::ranges::any_of(endpoints.value(), [&](const auto& endpoint) {
            return endpoint.identity.direction == direction && endpoint.display_name == name;
        });
    };
    return has_route(midi::MidiDirection::input, input_name) &&
           has_route(midi::MidiDirection::output, output_name);
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    return test::run([&] {
        TAUREON_REQUIRE(argc >= 4);
        const std::string mode{argv[1]};
        const std::string input_name{argv[2]};
        const std::string output_name{argv[3]};
        TAUREON_REQUIRE(safe_name(input_name) && safe_name(output_name));

        if (mode == "--assert-absent") {
            TAUREON_REQUIRE(argc == 4);
            TAUREON_REQUIRE(endpoints_absent(midi::MidiBackend::windows_midi_services,
                                             input_name, output_name));
            TAUREON_REQUIRE(endpoints_absent(midi::MidiBackend::winmm, input_name, output_name));
            return;
        }

        if (mode == "--assert-present") {
            TAUREON_REQUIRE(argc == 4);
            TAUREON_REQUIRE(endpoints_present(midi::MidiBackend::windows_midi_services,
                                              input_name, output_name));
            TAUREON_REQUIRE(endpoints_present(midi::MidiBackend::winmm,
                                              input_name, output_name));
            return;
        }

        TAUREON_REQUIRE(mode == "--exercise-wms" || mode == "--exercise-winmm");
        TAUREON_REQUIRE(argc == 5);
        APTTYPE gui_apartment_type{};
        APTTYPEQUALIFIER gui_apartment_qualifier{};
        TAUREON_REQUIRE(SUCCEEDED(
            CoGetApartmentType(&gui_apartment_type, &gui_apartment_qualifier)));
        TAUREON_REQUIRE(gui_apartment_type == APTTYPE_STA ||
                        gui_apartment_type == APTTYPE_MAINSTA);
        std::cout << "{\"event\":\"stage5_gui_apartment\",\"type\":\""
                  << (gui_apartment_type == APTTYPE_MAINSTA ? "main_sta" : "sta")
                  << "\",\"qualifier\":" << static_cast<int>(gui_apartment_qualifier)
                  << "}\n";
        const int cycles = std::stoi(argv[4]);
        TAUREON_REQUIRE(cycles > 0 && cycles <= 100);
        const auto backend = mode == "--exercise-wms" ?
            midi::MidiBackend::windows_midi_services : midi::MidiBackend::winmm;
        exercise_backend(backend, input_name, output_name, cycles);
    });
}
