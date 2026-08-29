#pragma once

#include "app/BoundedLog.hpp"
#include "app/ConnectionController.hpp"
#include "app/Settings.hpp"

#include <QWidget>

#include <filesystem>
#include <memory>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace taureon::gui {

class SettingsPanel final : public QWidget {
public:
    explicit SettingsPanel(std::filesystem::path path, std::shared_ptr<app::BoundedLog> log,
                           QWidget* parent = nullptr);

    void set_connection_snapshot(const app::ConnectionSnapshot& snapshot);
    [[nodiscard]] midi::Result<void> save();
    [[nodiscard]] const app::Settings& settings() const noexcept;
    [[nodiscard]] bool has_required_controls() const noexcept;

private:
    void populate_controls();
    void read_controls();
    void update_route_labels();
    void set_status(std::string text);

    std::filesystem::path path_;
    std::shared_ptr<app::BoundedLog> log_;
    app::Settings settings_;
    app::ConnectionSnapshot connection_;
    QLabel* status_{};
    QLabel* receive_route_{};
    QLabel* transmit_route_{};
    QComboBox* theme_{};
    QSpinBox* ui_scale_{};
    QLineEdit* standard_path_{};
    QCheckBox* restore_session_{};
    QComboBox* backend_{};
    QComboBox* reconnect_{};
    QCheckBox* monitor_paused_{};
    QSpinBox* monitor_history_{};
    QSpinBox* pacing_{};
    QComboBox* confirmation_{};
    QCheckBox* stop_on_loss_{};
    QComboBox* log_level_{};
    QLineEdit* log_destination_{};
    QSpinBox* log_rotation_{};
    QCheckBox* include_routes_{};
    QPushButton* capture_routes_{};
    QPushButton* save_button_{};
};

} // namespace taureon::gui
