#pragma once

#include <QMainWindow>

class QLabel;
class QListWidget;
class QStackedWidget;

namespace taureon::gui {

class MainWindow final : public QMainWindow {
public:
    MainWindow();

    [[nodiscard]] bool has_expected_shell() const noexcept;

private:
    void select_workspace(int index);

    QListWidget* navigation_{};
    QStackedWidget* workspace_stack_{};
    QLabel* workspace_heading_{};
};

} // namespace taureon::gui
