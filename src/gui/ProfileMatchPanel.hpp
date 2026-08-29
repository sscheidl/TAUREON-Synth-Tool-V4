#pragma once

#include "profiles/DeviceProfile.hpp"

#include <QWidget>

#include <functional>
#include <string>
#include <vector>

class QComboBox;
class QLabel;
class QPushButton;

namespace taureon::gui {

class ProfileMatchPanel final : public QWidget {
public:
    explicit ProfileMatchPanel(QWidget* parent = nullptr);

    void set_available_profiles(const std::vector<profiles::DeviceProfile>& profiles);
    void present(const profiles::ProfileMatchResult& result);
    void set_select_temporary_action(std::function<void(std::string)> action);
    void set_remember_binding_action(std::function<void()> action);
    [[nodiscard]] bool override_is_visible() const;
    [[nodiscard]] bool remember_binding_is_enabled() const;
    void trigger_remember_binding();

private:
    void update_temporary_selection_enabled();

    QLabel* selected_{};
    QLabel* evidence_{};
    QLabel* override_{};
    QComboBox* temporary_selector_{};
    QPushButton* use_temporary_{};
    QPushButton* remember_{};
    std::function<void(std::string)> select_temporary_action_;
    std::function<void()> remember_action_;
};

} // namespace taureon::gui
