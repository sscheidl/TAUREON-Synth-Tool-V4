#pragma once

#include "profiles/DeviceProfile.hpp"

#include <QWidget>

#include <functional>

class QLabel;
class QPushButton;

namespace taureon::gui {

class ProfileMatchPanel final : public QWidget {
public:
    explicit ProfileMatchPanel(QWidget* parent = nullptr);

    void present(const profiles::ProfileMatchResult& result);
    void set_remember_binding_action(std::function<void()> action);
    [[nodiscard]] bool override_is_visible() const;
    [[nodiscard]] bool remember_binding_is_enabled() const;
    void trigger_remember_binding();

private:
    QLabel* selected_{};
    QLabel* evidence_{};
    QLabel* override_{};
    QPushButton* remember_{};
    std::function<void()> remember_action_;
};

} // namespace taureon::gui
