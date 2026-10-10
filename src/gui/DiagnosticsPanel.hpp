#pragma once

#include "app/ConnectionWorker.hpp"
#include "app/Diagnostics.hpp"
#include "app/MonitorEventQueue.hpp"
#include "profiles/ProfileRegistry.hpp"

#include <QWidget>

class QHideEvent;
class QShowEvent;

#include <future>
#include <memory>
#include <optional>
#include <vector>

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
    void set_profile_load_issues(std::vector<profiles::ProfileLoadIssue> issues);
    [[nodiscard]] midi::Result<void> export_bundle(const std::filesystem::path& path) const;

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    [[nodiscard]] app::DiagnosticExportPolicy effective_export_policy() const;
    [[nodiscard]] app::DiagnosticSnapshot current_snapshot() const;
    void check_engines();
    void refresh();
    void poll();
    void present();

    app::ConnectionWorker& worker_;
    app::MonitorEventQueue& monitor_queue_;
    std::shared_ptr<const app::DiagnosticExportPolicy> export_policy_;
    QPlainTextEdit* details_{};
    QLabel* status_{};
    QPushButton* export_button_{};
    QPushButton* check_engines_button_{};
    QTimer* timer_{};
    std::optional<std::future<midi::Result<app::ConnectionSnapshot>>> pending_connection_;
    std::optional<std::future<midi::Result<app::SysExTransferSnapshot>>> pending_transfer_;
    std::optional<std::future<midi::Result<app::BackendProbeReport>>> pending_engines_;
    std::string engine_summary_{"not checked: use Check MIDI engines (enumeration only, no port opened)"};
    app::ConnectionSnapshot connection_;
    app::SysExTransferSnapshot transfer_;
    bool have_connection_{};
    bool have_transfer_{};
    bool export_status_latched_{};
    std::vector<profiles::ProfileLoadIssue> profile_load_issues_;
};

} // namespace taureon::gui
