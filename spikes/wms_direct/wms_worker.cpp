#include "wms_worker.hpp"

#include <winrt/Windows.Foundation.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.Diagnostics.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.Messages.h>

#include "winmidi/init/Microsoft.Windows.Devices.Midi2.Initialization.hpp"

#include <QPointer>
#include <QThread>
#include <QTimer>

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>

namespace midi = winrt::Microsoft::Windows::Devices::Midi2;
namespace diagnostics = winrt::Microsoft::Windows::Devices::Midi2::Diagnostics;
namespace messages = winrt::Microsoft::Windows::Devices::Midi2::Messages;
namespace init = Microsoft::Windows::Devices::Midi2::Initialization;

struct CallbackGate {
    std::atomic<bool> accepting{false};
    std::atomic<int> count{0};
    std::atomic<int> after_stop{0};
    QPointer<WmsWorker> target;
};

struct WmsWorker::Impl {
    bool running{false};
    bool apartment_initialized{false};
    std::shared_ptr<init::MidiDesktopAppSdkInitializer> initializer;
    midi::MidiSession session{nullptr};
    midi::MidiEndpointConnection sender{nullptr};
    midi::MidiEndpointConnection receiver{nullptr};
    winrt::event_token receive_token{};
    std::shared_ptr<CallbackGate> gate{std::make_shared<CallbackGate>()};
    QTimer* timer{nullptr};
};

WmsWorker::WmsWorker(QObject* parent) : QObject(parent), impl_(std::make_unique<Impl>()) {
    impl_->gate->target = this;
}

WmsWorker::~WmsWorker() {
    if (impl_->running) stop();
    impl_->gate->target = nullptr;
}

void WmsWorker::start() {
    if (impl_->running) return;
    const auto start_time = std::chrono::steady_clock::now();
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        impl_->apartment_initialized = true;
        impl_->initializer = std::make_shared<init::MidiDesktopAppSdkInitializer>();
        if (!impl_->initializer->IsServiceInstalled() ||
            !impl_->initializer->InitializeSdkRuntime() ||
            !impl_->initializer->CheckForMinimumRequiredSdkVersion(1, 0, 17) ||
            !impl_->initializer->EnsureServiceAvailable()) {
            throw std::runtime_error("WMS initialization or pinned runtime check failed");
        }

        impl_->session = midi::MidiSession::Create(L"TAUREON Stage 1 Qt coexistence harness");
        impl_->sender = impl_->session.CreateEndpointConnection(
            diagnostics::MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId());
        impl_->receiver = impl_->session.CreateEndpointConnection(
            diagnostics::MidiDiagnostics::DiagnosticsLoopbackBEndpointDeviceId());
        impl_->gate->accepting.store(true, std::memory_order_release);
        const auto gate = impl_->gate;
        impl_->receive_token = impl_->receiver.MessageReceived(
            [gate](midi::IMidiMessageReceivedEventSource const&,
                   midi::MidiMessageReceivedEventArgs const&) {
                if (!gate->accepting.load(std::memory_order_acquire)) {
                    ++gate->after_stop;
                    return;
                }
                const int count = ++gate->count;
                const QPointer<WmsWorker> target = gate->target;
                if (target) {
                    QMetaObject::invokeMethod(target, "publishCount", Qt::QueuedConnection,
                                              Q_ARG(int, count));
                }
            });
        if (!impl_->sender.Open() || !impl_->receiver.Open()) {
            throw std::runtime_error("WMS diagnostic loopback open failed");
        }

        impl_->timer = new QTimer(this);
        impl_->timer->setInterval(10);
        connect(impl_->timer, &QTimer::timeout, this, [this] {
            const auto packet = messages::MidiMessageBuilder::BuildMidi1ChannelVoiceMessage(
                midi::MidiClock::TimestampConstantSendImmediately(),
                midi::MidiGroup(static_cast<std::uint8_t>(0)),
                messages::Midi1ChannelVoiceMessageStatus::NoteOn,
                midi::MidiChannel(static_cast<std::uint8_t>(0)), 60, 1);
            impl_->sender.SendSingleMessagePacket(packet);
        });
        impl_->timer->start();
        impl_->running = true;
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();
        const auto thread_id = reinterpret_cast<qulonglong>(QThread::currentThreadId());
        std::cout << "{\"event\":\"qt_worker_initialized\",\"duration_ms\":" << elapsed
                  << ",\"worker_thread_id\":" << thread_id << "}" << std::endl;
        emit initialized(elapsed, thread_id);
    } catch (const std::exception& error) {
        emit failed(QString::fromUtf8(error.what()));
        stop();
    } catch (const winrt::hresult_error& error) {
        emit failed(QString::fromStdWString(error.message().c_str()));
        stop();
    }
}

void WmsWorker::stop() {
    impl_->gate->accepting.store(false, std::memory_order_release);
    if (impl_->timer) {
        impl_->timer->stop();
        impl_->timer->deleteLater();
        impl_->timer = nullptr;
    }
    if (impl_->receiver && impl_->receive_token) {
        impl_->receiver.MessageReceived(impl_->receive_token);
        impl_->receive_token = {};
    }
    if (impl_->session) {
        if (impl_->sender) impl_->session.DisconnectEndpointConnection(impl_->sender.ConnectionId());
        if (impl_->receiver) impl_->session.DisconnectEndpointConnection(impl_->receiver.ConnectionId());
        impl_->session.Close();
    }
    impl_->sender = nullptr;
    impl_->receiver = nullptr;
    impl_->session = nullptr;
    if (impl_->initializer) {
        impl_->initializer->ShutdownSdkRuntime();
        impl_->initializer.reset();
    }
    if (impl_->apartment_initialized) {
        winrt::uninit_apartment();
        impl_->apartment_initialized = false;
    }
    impl_->running = false;
    const int after_stop = impl_->gate->after_stop.load();
    std::cout << "{\"event\":\"qt_worker_stopped\",\"callbacks_after_stop\":"
              << after_stop << "}" << std::endl;
    emit stopped(after_stop);
}

void WmsWorker::publishCount(const int count) {
    if (impl_->gate->accepting.load(std::memory_order_acquire)) emit countChanged(count);
}
