#include "gui/MonitorEventBridge.hpp"

#include "gui/MidiMonitorModel.hpp"

#include <QTimer>

namespace taureon::gui {

MonitorEventBridge::MonitorEventBridge(app::MonitorEventQueue& queue, MidiMonitorModel& model,
                                       QObject* parent)
    : QObject(parent), queue_(queue), model_(model), timer_(new QTimer(this)) {
    timer_->setInterval(16);
    timer_->setTimerType(Qt::CoarseTimer);
    connect(timer_, &QTimer::timeout, this, [this] { drain_once(); });
    timer_->start();
}

MonitorEventBridge::~MonitorEventBridge() { shutdown(); }

void MonitorEventBridge::drain_once() {
    if (!accepting_gui_updates_) return;
    auto events = queue_.drain(512);
    if (stats_.paused) {
        stats_.discarded_while_paused += events.size();
        return;
    }
    stats_.displayed += events.size();
    model_.append_batch(std::move(events));
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
}

} // namespace taureon::gui
