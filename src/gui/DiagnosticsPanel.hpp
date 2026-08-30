#pragma once

#include "app/ConnectionWorker.hpp"
#include "app/Diagnostics.hpp"
#include "app/MonitorEventQueue.hpp"

#include <QWidget>

#include <future>
#include <memory>
#include <optional>

class QLabel;
class QPushButton;
class QPlainTextEdit;
class QTimer;

namespace taureon::gui {

class DiagnosticsPanel final : public QWidget {
public:
    explicit DiagnosticsPanel(app::ConnectionWorker& worker, app::MonitorEventQueue& monitor_queue,
                              std::shared_ptr<const app::DiagnosticExportPolicy> export_policy,
                              QWidget* parent = nullptr);

    [[nodiscard]] bool has_required_controls() const noexcept;
    [[nodiscard]] midi::Result<void> export_bundle(const std::filesystem::path& path) const;

private:
    [[nodiscard]] app::DiagnosticExportPolicy effective_export_policy() const;
    void refresh();
    void poll();
    void present();

    app::ConnectionWorker& worker_;
    app::MonitorEventQueue& monitor_queue_;
    std::shared_ptr<const app::DiagnosticExportPolicy> export_policy_;
    QPlainTextEdit* details_{};
    QLabel* status_{};
    QPushButton* export_button_{};
    QTimer* timer_{};
    std::optional<std::future<midi::Result<app::ConnectionSnapshot>>> pending_connection_;
    std::optional<std::future<midi::Result<app::SysExTransferSnapshot>>> pending_transfer_;
    app::ConnectionSnapshot connection_;
    app::SysExTransferSnapshot transfer_;
    bool have_connection_{};
    bool have_transfer_{};
};

} // namespace taureon::gui
