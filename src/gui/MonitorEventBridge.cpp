#include "gui/MonitorEventBridge.hpp"

#include "gui/MidiMonitorModel.hpp"

#include <QTimer>

#include <limits>

namespace taureon::gui {

MonitorEventBridge::MonitorEventBridge(app::MonitorEventQueue& queue, MidiMonitorModel& model,
                                       QObject* parent)
    : QObject(parent), queue_(queue), model_(&model), timer_(new QTimer(this)) {
    // The model's same-thread destruction closes this bridge before its storage is released.
    // This is an explicit lifetime gate, not QPointer or an asynchronous raw-pointer handoff.
    model_destroyed_connection_ = connect(&model, &QObject::destroyed, this, [this] {
        model_ = nullptr;
        shutdown();
    });
    timer_->setInterval(16);
    timer_->setTimerType(Qt::CoarseTimer);
    connect(timer_, &QTimer::timeout, this, [this] { drain_once(); });
    timer_->start();
}

MonitorEventBridge::~MonitorEventBridge() { shutdown(); }

void MonitorEventBridge::drain_once() {
    if (!accepting_gui_updates_ || !model_) return;
    auto events = queue_.drain(512);
    if (stats_.paused) {
        stats_.discarded_while_paused += events.size();
        return;
    }
    stats_.displayed += events.size();
    model_->append_batch(std::move(events));
}

void MonitorEventBridge::set_paused(const bool paused) noexcept { stats_.paused = paused; }

MonitorEventBridge::PresentationStats MonitorEventBridge::presentation_stats() const noexcept {
    return stats_;
}

void MonitorEventBridge::shutdown() noexcept {
    if (!accepting_gui_updates_) return;
    accepting_gui_updates_ = false;
    timer_->stop();
    queue_.close_acceptance();
    // A closed presentation discards already queued model work deterministically.
    static_cast<void>(queue_.drain(std::numeric_limits<std::size_t>::max()));
}

} // namespace taureon::gui
