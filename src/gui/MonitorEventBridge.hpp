#pragma once

#include "app/MonitorEventQueue.hpp"

#include <QObject>

class QTimer;

namespace taureon::gui {

class MidiMonitorModel;

class MonitorEventBridge final : public QObject {
public:
    MonitorEventBridge(app::MonitorEventQueue& queue, MidiMonitorModel& model,
                       QObject* parent = nullptr);
    ~MonitorEventBridge() override;

    void drain_once();
    void shutdown() noexcept;

private:
    app::MonitorEventQueue& queue_;
    MidiMonitorModel& model_;
    QTimer* timer_{};
    bool accepting_gui_updates_{true};
};

} // namespace taureon::gui
