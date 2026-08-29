#pragma once

#include "app/MonitorEventQueue.hpp"
#include "app/ConnectionWorker.hpp"
#include "profiles/ProfileRegistry.hpp"

#include <QMainWindow>

#include <future>
#include <memory>
#include <optional>
#include <vector>

class QLabel;
class QComboBox;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTimer;

namespace taureon::app { class BoundedLog; }

namespace taureon::gui {

class DiagnosticsPanel;
class MidiMonitorModel;
class MonitorEventBridge;
class ProfileMatchPanel;
class SettingsPanel;
class SysExTransferPanel;
class SysExManagerPanel;

class MainWindow final : public QMainWindow {
public:
    MainWindow(app::MonitorEventQueue& monitor_queue, app::ConnectionWorker& connection_worker,
               std::shared_ptr<const profiles::ProfileRegistry> profile_registry = {});
    ~MainWindow() override;

    [[nodiscard]] bool has_expected_shell() const noexcept;

private:
    void select_workspace(int index);
    void begin_backend_selection(int index);
    void begin_connect_toggle();
    void poll_connection_result();
    void apply_connection_snapshot(const app::ConnectionSnapshot& snapshot, bool repopulate);
    void set_connection_busy(bool busy);

    enum class PendingConnectionAction { backend, connect, disconnect, snapshot };

    QListWidget* navigation_{};
    QStackedWidget* workspace_stack_{};
    QLabel* workspace_heading_{};
    DiagnosticsPanel* diagnostics_panel_{};
    MidiMonitorModel* monitor_model_{};
    MonitorEventBridge* monitor_bridge_{};
    ProfileMatchPanel* profile_panel_{};
    SettingsPanel* settings_panel_{};
    SysExTransferPanel* sysex_transfer_panel_{};
    std::shared_ptr<app::BoundedLog> diagnostics_log_;
    SysExManagerPanel* sysex_manager_panel_{};
    app::ConnectionWorker& connection_worker_;
    QComboBox* backend_selector_{};
    QComboBox* receive_selector_{};
    QComboBox* transmit_selector_{};
    QPushButton* connect_button_{};
    QTimer* connection_poll_timer_{};
    std::optional<std::future<midi::Result<app::ConnectionSnapshot>>> pending_connection_;
    PendingConnectionAction pending_action_{PendingConnectionAction::backend};
    std::vector<midi::MidiRouteIdentity> receive_routes_;
    std::vector<midi::MidiRouteIdentity> transmit_routes_;
    bool connected_{};
    int idle_poll_ticks_{};
};

} // namespace taureon::gui
