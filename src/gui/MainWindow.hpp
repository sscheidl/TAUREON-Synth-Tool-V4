#pragma once

#include "app/MonitorEventQueue.hpp"

#include <QMainWindow>

class QLabel;
class QListWidget;
class QStackedWidget;

namespace taureon::gui {

class MidiMonitorModel;
class MonitorEventBridge;
class ProfileMatchPanel;

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(app::MonitorEventQueue& monitor_queue);
    ~MainWindow() override;

    [[nodiscard]] bool has_expected_shell() const noexcept;

private:
    void select_workspace(int index);

    QListWidget* navigation_{};
    QStackedWidget* workspace_stack_{};
    QLabel* workspace_heading_{};
    MidiMonitorModel* monitor_model_{};
    MonitorEventBridge* monitor_bridge_{};
    ProfileMatchPanel* profile_panel_{};
};

} // namespace taureon::gui
