#pragma once

#include "app/MonitorEventQueue.hpp"

#include <QObject>

#include <QMetaObject>

class QTimer;

namespace taureon::gui {

class MidiMonitorModel;

class MonitorEventBridge final : public QObject {
public:
    struct PresentationStats {
        std::uint64_t displayed{};
        std::uint64_t discarded_while_paused{};
        bool paused{};
    };

    MonitorEventBridge(app::MonitorEventQueue& queue, MidiMonitorModel& model,
                       QObject* parent = nullptr);
    ~MonitorEventBridge() override;

    void drain_once();
    void set_paused(bool paused) noexcept;
    [[nodiscard]] PresentationStats presentation_stats() const noexcept;
    void shutdown() noexcept;

private:
    app::MonitorEventQueue& queue_;
    MidiMonitorModel& model_;
    MidiMonitorModel* model_{};
    QTimer* timer_{};
    QMetaObject::Connection model_destroyed_connection_;
    bool accepting_gui_updates_{true};
    PresentationStats stats_;
};

} // namespace taureon::gui
