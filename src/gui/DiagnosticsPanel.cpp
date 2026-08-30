#include "gui/DiagnosticsPanel.hpp"

#include <QFileDialog>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <chrono>
#include <filesystem>
#include <future>
#include <string>
#include <utility>

#ifndef TAUREON_APP_VERSION
#define TAUREON_APP_VERSION "0.0.0"
#endif
#ifndef TAUREON_BUILD_REVISION
#define TAUREON_BUILD_REVISION "not embedded"
#endif

namespace taureon::gui {
namespace {

QString format_snapshot(const app::DiagnosticSnapshot& snapshot) {
    const auto line = [](const char* name, const auto& value) {
        return QString::fromLatin1(name) + ": " + QString::fromStdString(std::to_string(value)) + "\n";
    };
    QString result;
    const auto text = [&result](const char* name, const std::string& value) {
        result += QString::fromLatin1(name) + ": " + QString::fromStdString(value) + "\n";
    };
    text("Application version", snapshot.application_version);
    text("Build revision", snapshot.build_revision);
    text("OS", snapshot.operating_system);
    text("Process architecture", snapshot.process_architecture);
    text("Selected backend", snapshot.selected_backend);
    text("Active backend", snapshot.active_backend);
    text("Windows MIDI/WMS runtime", snapshot.windows_midi_runtime);
    text("RX route identity", snapshot.receive_route);
    text("TX route identity", snapshot.transmit_route);
    text("Connection state", snapshot.connection_state);
    text("Transfer state", snapshot.transfer_state);
    text("RX count", snapshot.receive_count);
    result += line("TX count", snapshot.transmit_count);
    result += line("Dropped count", snapshot.dropped_count);
    text("Overflow count", snapshot.overflow_count);
    text("Late-callback count", snapshot.late_callback_count);
    result += line("Queue current size", snapshot.queue_current_size);
    result += line("Queue high-water mark", snapshot.queue_high_water_mark);
    result += line("SysEx frame count", snapshot.sysex_frame_count);
    result += line("SysEx byte count", snapshot.sysex_byte_count);
    text("Last transport error", snapshot.last_transport_error);
    text("Last application/workflow error", snapshot.last_application_error);
    text("Disconnect/reconnect transition", snapshot.reconnect_transition);
    text("Active profile", snapshot.profile_id);
    text("Profile match status", snapshot.profile_match_status);
    text("Profile evidence", snapshot.profile_evidence);
    text("Profile validation", snapshot.profile_validation);
    return result;
}

} // namespace

DiagnosticsPanel::DiagnosticsPanel(app::ConnectionWorker& worker, app::MonitorEventQueue& monitor_queue,
                                   std::shared_ptr<const app::DiagnosticExportPolicy> export_policy, QWidget* parent)
    : QWidget(parent), worker_(worker), monitor_queue_(monitor_queue), export_policy_(std::move(export_policy)) {
    setObjectName("diagnosticsPanel");
    auto* layout = new QVBoxLayout(this);
    auto* explanation = new QLabel(
        "Observed application metadata only. Values without an existing safe snapshot contract are shown as not observed. "
        "Diagnostic bundles never contain SysEx/MIDI payloads, workspace bytes, file contents, or transfer logs.",
        this);
    explanation->setWordWrap(true);
    layout->addWidget(explanation);
    details_ = new QPlainTextEdit(this);
    details_->setObjectName("diagnosticsSnapshotText");
    details_->setReadOnly(true);
    layout->addWidget(details_, 1);
    export_button_ = new QPushButton("Export Diagnostic Bundle", this);
    export_button_->setObjectName("diagnosticsExportBundle");
    export_button_->setAccessibleName("Export Diagnostic Bundle");
    status_ = new QLabel("Refreshing safe diagnostics snapshots…", this);
    status_->setObjectName("diagnosticsStatus");
    layout->addWidget(export_button_);
    layout->addWidget(status_);

    connect(export_button_, &QPushButton::clicked, this, [this] {
        const auto selected = QFileDialog::getSaveFileName(
            this, "Export Diagnostic Bundle", "taureon-diagnostics.txt", "Text files (*.txt)");
        if (selected.isEmpty()) return;
        const auto saved = export_bundle(std::filesystem::path{selected.toStdWString()});
        status_->setText(saved ? "Diagnostic bundle exported without user payloads."
                               : "Diagnostic bundle export failed: " + QString::fromStdString(saved.error().message));
    });
    timer_ = new QTimer(this);
    timer_->setInterval(250);
    connect(timer_, &QTimer::timeout, this, [this] { poll(); });
    timer_->start();
    refresh();
}

bool DiagnosticsPanel::has_required_controls() const noexcept {
    return details_ && status_ && export_button_ && timer_;
}

midi::Result<void> DiagnosticsPanel::export_bundle(const std::filesystem::path& path) const {
    const auto snapshot = app::DiagnosticBundle::make_snapshot(
        connection_, transfer_, monitor_queue_.stats(), TAUREON_APP_VERSION, TAUREON_BUILD_REVISION, *export_policy_);
    return app::DiagnosticBundle::write(path, snapshot);
}

void DiagnosticsPanel::refresh() {
    if (!pending_connection_) pending_connection_ = worker_.snapshot();
    if (!pending_transfer_) pending_transfer_ = worker_.sysex_snapshot();
}

void DiagnosticsPanel::poll() {
    using namespace std::chrono_literals;
    if (pending_connection_ && pending_connection_->wait_for(0ms) == std::future_status::ready) {
        const auto result = pending_connection_->get();
        pending_connection_.reset();
        if (result) {
            connection_ = result.value();
            have_connection_ = true;
        }
    }
    if (pending_transfer_ && pending_transfer_->wait_for(0ms) == std::future_status::ready) {
        const auto result = pending_transfer_->get();
        pending_transfer_.reset();
        if (result) {
            transfer_ = result.value();
            have_transfer_ = true;
        }
    }
    present();
    refresh();
}

void DiagnosticsPanel::present() {
    const auto snapshot = app::DiagnosticBundle::make_snapshot(
        connection_, transfer_, monitor_queue_.stats(), TAUREON_APP_VERSION, TAUREON_BUILD_REVISION, *export_policy_);
    details_->setPlainText(format_snapshot(snapshot));
    if (have_connection_ || have_transfer_) {
        status_->setText("Safe snapshots refreshed; unavailable values are not inferred.");
    }
}

} // namespace taureon::gui
