#pragma once

#include "profiles/DeviceProfile.hpp"

#include <QWidget>

#include <cstddef>
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
    void set_profile_load_issue_count(std::size_t count);
    void present(const profiles::ProfileMatchResult& result);
    void set_select_temporary_action(std::function<void(std::string)> action);
    void set_remember_binding_action(std::function<void()> action);
    [[nodiscard]] bool override_is_visible() const;
    [[nodiscard]] bool remember_binding_is_enabled() const;
    void trigger_remember_binding();

private:
    void update_temporary_selection_enabled();
    void show_profile_details();

    QLabel* selected_{};
    QLabel* evidence_{};
    QLabel* override_{};
    QLabel* load_issues_{};
    QComboBox* temporary_selector_{};
    QPushButton* use_temporary_{};
    QPushButton* remember_{};
    QPushButton* details_{};
    std::vector<profiles::DeviceProfile> profiles_;
    profiles::ProfileMatchResult current_match_;
    std::function<void(std::string)> select_temporary_action_;
    std::function<void()> remember_action_;
};

} // namespace taureon::gui
