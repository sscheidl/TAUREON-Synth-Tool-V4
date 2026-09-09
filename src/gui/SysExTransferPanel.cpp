#include "gui/SysExTransferPanel.hpp"

#include <QFileDialog>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

#include <chrono>
#include <type_traits>
#include <variant>

namespace taureon::gui {
namespace {

QString frame_status(const sysex::SysExFrame& frame) {
    if (frame.affected_by_data_loss) return QStringLiteral("Tainted / malformed");
    switch (frame.status) {
    case sysex::SysExFrameStatus::complete: return QStringLiteral("Complete");
    case sysex::SysExFrameStatus::incomplete: return QStringLiteral("Incomplete");
    case sysex::SysExFrameStatus::malformed: return QStringLiteral("Malformed");
    }
    return QStringLiteral("Unknown");
}

QString match_status(const profiles::ProfileMatchStatus status) {
    switch (status) {
    case profiles::ProfileMatchStatus::Explicit: return QStringLiteral("explicit");
    case profiles::ProfileMatchStatus::ConfidentSuggestion:
        return QStringLiteral("confident suggestion");
    case profiles::ProfileMatchStatus::Ambiguous: return QStringLiteral("ambiguous");
    case profiles::ProfileMatchStatus::NoMatch: return QStringLiteral("no match");
    case profiles::ProfileMatchStatus::Invalid: return QStringLiteral("invalid evidence");
    case profiles::ProfileMatchStatus::GenericFallback:
        return QStringLiteral("Generic fallback");
    }
    return QStringLiteral("unknown");
}

bool transfer_active(const transfer::TransferState state) {
    return state == transfer::TransferState::preparing ||
           state == transfer::TransferState::running ||
           state == transfer::TransferState::cancelling;
}

QString route_text(const std::optional<midi::MidiRouteIdentity>& route) {
    if (!route) return QStringLiteral("No exact TX route connected");
    return std::visit([](const auto& identity) -> QString {
        using Identity = std::decay_t<decltype(identity)>;
        if constexpr (std::is_same_v<Identity, midi::WmsRouteIdentity>) {
            return QStringLiteral("WMS %1 · group %2")
                .arg(QString::fromStdString(identity.endpoint_device_id))
                .arg(identity.group + 1);
        } else {
            return QStringLiteral("WinMM %1 · manufacturer %2 · product %3 · driver %4")
                .arg(QString::fromStdString(identity.port_name))
                .arg(identity.manufacturer_id)
                .arg(identity.product_id)
                .arg(identity.driver_version);
        }
    }, route->native);
}

QString bytes_hex(const std::vector<std::uint8_t>& bytes) {
    QByteArray raw(reinterpret_cast<const char*>(bytes.data()), static_cast<qsizetype>(bytes.size()));
    return QString::fromLatin1(raw.toHex(' ').toUpper());
}

} // namespace

SysExFrameModel::SysExFrameModel(QObject* parent) : QAbstractTableModel(parent) {}

int SysExFrameModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(frames_.size());
}

int SysExFrameModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : 5;
}

QVariant SysExFrameModel::data(const QModelIndex& index, const int role) const {
    if (!index.isValid() || role != Qt::DisplayRole || index.row() < 0 ||
        index.row() >= static_cast<int>(frames_.size())) return {};
    const auto& frame = frames_.at(static_cast<std::size_t>(index.row()));
    switch (index.column()) {
    case 0: return index.row() + 1;
    case 1: return frame_status(frame);
    case 2: return static_cast<qulonglong>(frame.bytes.size());
    case 3: return frame.group ? QVariant{static_cast<int>(*frame.group) + 1} : QVariant{"—"};
    case 4: return QString::fromStdString(frame.issue);
    default: return {};
    }
}

QVariant SysExFrameModel::headerData(const int section, const Qt::Orientation orientation,
                                     const int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    static const QStringList headers{"Frame", "State", "Bytes", "Group", "Issue"};
    return section >= 0 && section < headers.size() ? headers.at(section) : QVariant{};
}

void SysExFrameModel::set_frames(std::vector<sysex::SysExFrame> frames) {
    beginResetModel();
    frames_ = std::move(frames);
    endResetModel();
}

const sysex::SysExFrame* SysExFrameModel::frame(const int row) const noexcept {
    if (row < 0 || row >= static_cast<int>(frames_.size())) return nullptr;
    return &frames_.at(static_cast<std::size_t>(row));
}

SysExTransferPanel::SysExTransferPanel(app::ConnectionWorker& worker, QWidget* parent)
    : QWidget(parent), worker_(worker) {
    setObjectName("sysExTransferPanel");
    auto* root = new QVBoxLayout(this);
    auto* summary = new QGridLayout;
    source_label_ = new QLabel("No file or capture loaded", this);
    source_label_->setObjectName("sysExSource");
    evidence_label_ = new QLabel("Manufacturer / device: not identified — no verified evidence", this);
    evidence_label_->setWordWrap(true);
    profile_label_ = new QLabel("Active profile: Generic · match status: not evaluated", this);
    profile_label_->setWordWrap(true);
    profile_label_->setObjectName("sysExProfileMatch");
    counts_label_ = new QLabel("Frames 0 · bytes 0", this);
    integrity_label_ = new QLabel("Integrity: no data", this);
    integrity_label_->setObjectName("sysExIntegrity");
    route_label_ = new QLabel("Actual TX route: No exact TX route connected", this);
    route_label_->setObjectName("sysExTxRoute");
    route_label_->setWordWrap(true);
    pacing_label_ = new QLabel("Pacing: user-selected fixed inter-frame delay", this);
    pacing_label_->setWordWrap(true);
    summary->addWidget(source_label_, 0, 0, 1, 2);
    summary->addWidget(evidence_label_, 1, 0, 1, 2);
    summary->addWidget(profile_label_, 2, 0, 1, 2);
    summary->addWidget(counts_label_, 3, 0);
    summary->addWidget(integrity_label_, 3, 1);
    summary->addWidget(route_label_, 4, 0, 1, 2);
    summary->addWidget(pacing_label_, 5, 0, 1, 2);
    root->addLayout(summary);

    auto* actions = new QGridLayout;
    open_button_ = new QPushButton("Open .syx…", this);
    open_button_->setObjectName("sysExOpen");
    receive_button_ = new QPushButton("Receive", this);
    receive_button_->setObjectName("sysExReceive");
    send_button_ = new QPushButton("Raw Send", this);
    send_button_->setObjectName("sysExRawSend");
    send_button_->setToolTip("Explicit raw SysEx send to the exact displayed TX route; not a validated restore.");
    cancel_button_ = new QPushButton("Cancel", this);
    cancel_button_->setObjectName("sysExCancel");
    save_button_ = new QPushButton("Save received data…", this);
    save_button_->setObjectName("sysExSaveReceived");
    clear_button_ = new QPushButton("Clear", this);
    clear_button_->setObjectName("sysExClear");
    validated_restore_button_ = new QPushButton("Validated Restore", this);
    validated_restore_button_->setObjectName("sysExValidatedRestore");
    validated_restore_button_->setEnabled(false);
    validated_restore_button_->setToolTip(
        "Unavailable: no validated device restore protocol capability is implemented.");
    pacing_delay_ = new QSpinBox(this);
    pacing_delay_->setObjectName("sysExPacingDelay");
    pacing_delay_->setRange(0, 10'000);
    pacing_delay_->setSuffix(" ms between frames");
    actions->addWidget(open_button_, 0, 0);
    actions->addWidget(receive_button_, 0, 1);
    actions->addWidget(send_button_, 0, 2);
    actions->addWidget(cancel_button_, 0, 3);
    actions->addWidget(save_button_, 1, 0);
    actions->addWidget(clear_button_, 1, 1);
    actions->addWidget(validated_restore_button_, 1, 2);
    actions->addWidget(pacing_delay_, 1, 3);
    for (int column = 0; column < 4; ++column) actions->setColumnStretch(column, 1);
    root->addLayout(actions);

    progress_ = new QProgressBar(this);
    progress_->setRange(0, 1);
    progress_->setValue(0);
    root->addWidget(progress_);

    frame_model_ = new SysExFrameModel(this);
    frame_table_ = new QTableView(this);
    frame_table_->setObjectName("sysExFrameTable");
    frame_table_->setModel(frame_model_);
    frame_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    frame_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    frame_table_->horizontalHeader()->setStretchLastSection(true);
    root->addWidget(frame_table_, 2);

    raw_bytes_ = new QPlainTextEdit(this);
    raw_bytes_->setObjectName("sysExRawBytes");
    raw_bytes_->setReadOnly(true);
    raw_bytes_->setPlaceholderText("Select a frame to inspect its exact raw bytes.");
    raw_bytes_->setMaximumBlockCount(1);
    root->addWidget(raw_bytes_, 1);
    transfer_log_ = new QPlainTextEdit(this);
    transfer_log_->setObjectName("sysExTransferLog");
    transfer_log_->setReadOnly(true);
    transfer_log_->setMaximumBlockCount(100);
    root->addWidget(transfer_log_, 1);
    status_label_ = new QLabel("Idle — no automatic send is performed", this);
    status_label_->setObjectName("sysExStatus");
    root->addWidget(status_label_);

    connect(open_button_, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getOpenFileName(this, "Open SysEx file", {},
                                                       "SysEx files (*.syx);;All files (*)");
        if (!path.isEmpty()) request_load(path.toStdWString());
    });
    connect(receive_button_, &QPushButton::clicked, this, [this] {
        if (snapshot_.receiving) {
            set_pending(worker_.finish_sysex_receive(), PendingAction::finish_receive,
                        "Finishing receive capture…");
        } else {
            set_pending(worker_.begin_sysex_receive(), PendingAction::receive,
                        "Starting receive capture…");
        }
    });
    connect(send_button_, &QPushButton::clicked, this, [this] {
        set_pending(worker_.start_raw_sysex_send(
                        std::chrono::milliseconds{pacing_delay_->value()}),
                    PendingAction::send, "Raw Send requested by user…");
    });
    connect(cancel_button_, &QPushButton::clicked, this, [this] {
        set_pending(worker_.cancel_sysex_transfer(), PendingAction::cancel,
                    "Cancelling transfer…");
    });
    connect(save_button_, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getSaveFileName(this, "Save received SysEx as", {},
                                                       "SysEx files (*.syx)");
        if (!path.isEmpty()) request_save_received(path.toStdWString());
    });
    connect(clear_button_, &QPushButton::clicked, this, [this] {
        set_pending(worker_.clear_sysex(), PendingAction::clear, "Clearing transfer workspace…");
    });
    connect(frame_table_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex& current) { update_raw_inspector(current.row()); });

    poll_timer_ = new QTimer(this);
    poll_timer_->setInterval(25);
    connect(poll_timer_, &QTimer::timeout, this, [this] { poll_result(); });
    poll_timer_->start();
    apply_snapshot(snapshot_);
}

void SysExTransferPanel::request_load(const std::filesystem::path& path) {
    if (pending_) return;
    set_pending(worker_.load_sysex(path), PendingAction::load, "Loading SysEx read-only…");
}

bool SysExTransferPanel::request_load_document(
    sysex::SyxDocument document, std::string source_name, std::function<void(bool loaded)> completion) {
    if (pending_) {
        if (completion) completion(false);
        return false;
    }
    load_completion_ = std::move(completion);
    set_pending(worker_.load_sysex_document(std::move(document), std::move(source_name)),
                PendingAction::load, "Loading inspected Manager bytes read-only…");
    return true;
}

void SysExTransferPanel::request_save_received(const std::filesystem::path& path) {
    if (pending_) return;
    set_pending(worker_.save_received_sysex(path), PendingAction::save,
                "Saving verified received data to a new file…");
}

void SysExTransferPanel::request_select_temporary_profile(std::string profile_id) {
    if (pending_ || profile_id.empty()) return;
    set_pending(worker_.select_temporary_profile(std::move(profile_id)), PendingAction::select_profile,
                "Evaluating temporary profile against the existing SysEx data…");
}

void SysExTransferPanel::request_remember_overridden_manual_profile() {
    if (pending_) return;
    set_pending(worker_.remember_overridden_manual_profile(), PendingAction::remember_profile,
                "Promoting the displayed manual profile choice…");
}

void SysExTransferPanel::set_snapshot_observer(
    std::function<void(const app::SysExTransferSnapshot&)> observer) {
    snapshot_observer_ = std::move(observer);
    if (snapshot_observer_) snapshot_observer_(snapshot_);
}

bool SysExTransferPanel::has_required_controls() const noexcept {
    return open_button_ && receive_button_ && send_button_ && cancel_button_ && save_button_ &&
           clear_button_ && validated_restore_button_ && frame_table_ && raw_bytes_ &&
           transfer_log_;
}

void SysExTransferPanel::set_pending(
    std::future<midi::Result<app::SysExTransferSnapshot>> future,
    const PendingAction action, QString status) {
    if (pending_) return;
    pending_ = std::move(future);
    pending_action_ = action;
    status_label_->setText(std::move(status));
    open_button_->setEnabled(false);
    receive_button_->setEnabled(false);
    send_button_->setEnabled(false);
    cancel_button_->setEnabled(false);
    save_button_->setEnabled(false);
    clear_button_->setEnabled(false);
}

void SysExTransferPanel::poll_result() {
    using namespace std::chrono_literals;
    if (!pending_) {
        if (++idle_ticks_ >= 10) {
            idle_ticks_ = 0;
            set_pending(worker_.sysex_snapshot(), PendingAction::refresh, "Refreshing transfer state…");
        }
        return;
    }
    if (pending_->wait_for(0ms) != std::future_status::ready) return;
    auto result = pending_->get();
    const auto action = pending_action_;
    pending_.reset();
    auto completion = action == PendingAction::load ? std::move(load_completion_) :
                                                       std::function<void(bool loaded)>{};
    if (!result) {
        show_error(result.error());
        apply_snapshot(snapshot_);
        if (completion) completion(false);
        return;
    }
    apply_snapshot(result.value());
    if (action == PendingAction::load) {
        status_label_->setText("Loaded read-only — no automatic send was performed");
        if (completion) completion(true);
    } else if (action == PendingAction::select_profile) {
        status_label_->setText("Temporary profile evaluated — no route or send changed");
    } else if (action == PendingAction::remember_profile) {
        status_label_->setText("Profile binding promoted deliberately — no route or send changed");
    } else if (action == PendingAction::save) {
        status_label_->setText("Verified received data saved to a new file");
    } else if (action == PendingAction::clear) {
        status_label_->setText("Transfer workspace cleared");
    }
}

void SysExTransferPanel::apply_snapshot(const app::SysExTransferSnapshot& snapshot) {
    const auto selected_row = frame_table_->currentIndex().row();
    snapshot_ = snapshot;
    frame_model_->set_frames(snapshot.frames);
    source_label_->setText(snapshot.source_name.empty() ? "No file or capture loaded" :
                                                        QString::fromStdString(snapshot.source_name));
    if (snapshot.manufacturer || snapshot.model) {
        evidence_label_->setText(
            QStringLiteral("Manufacturer / device: %1 %2 · deterministic profile evidence")
                .arg(snapshot.manufacturer ? QString::fromStdString(*snapshot.manufacturer) :
                                             QStringLiteral("unknown"),
                     snapshot.model ? QString::fromStdString(*snapshot.model) : QString{}));
    } else {
        evidence_label_->setText(
            "Manufacturer / device: not identified — no verified device evidence");
    }
    const auto active_profile = snapshot.profile_display_name.empty() ?
        QStringLiteral("None") : QString::fromStdString(snapshot.profile_display_name);
    profile_label_->setText(
        QStringLiteral("Active profile: %1 · match status: %2 · profile transfer capability: %3")
            .arg(active_profile, match_status(snapshot.profile_match_status),
                 snapshot.profile_supports_transfer ? QStringLiteral("declared") :
                                                      QStringLiteral("not declared")));
    profile_label_->setToolTip(
        QString::fromStdString(snapshot.profile_match_message + [&] {
            std::string warnings;
            for (const auto& warning : snapshot.profile_warnings) warnings += "\n" + warning;
            return warnings;
        }()));
    counts_label_->setText(QStringLiteral("Frames %1 · bytes %2")
                               .arg(snapshot.frames.size())
                               .arg(snapshot.byte_count));
    integrity_label_->setText(
        QStringLiteral("Integrity: complete %1 · incomplete %2 · malformed %3 · tainted %4 · app drops %5")
            .arg(snapshot.complete_frames)
            .arg(snapshot.incomplete_frames)
            .arg(snapshot.malformed_frames)
            .arg(snapshot.tainted_frames)
            .arg(snapshot.application_dropped_events));
    route_label_->setText("Actual TX route: " + route_text(snapshot.transmit_route));
    pacing_label_->setText(QStringLiteral("Pacing: user-selected fixed inter-frame delay · %1 ms")
                               .arg(pacing_delay_->value()));
    progress_->setMaximum(static_cast<int>((std::max<std::uint64_t>)(1, snapshot.send_progress.bytes_total)));
    progress_->setValue(static_cast<int>((std::min<std::uint64_t>)(
        snapshot.send_progress.bytes_accepted,
        static_cast<std::uint64_t>(progress_->maximum()))));
    transfer_log_->setPlainText(QString::fromStdString([&] {
        std::string joined;
        for (const auto& line : snapshot.log) {
            if (!joined.empty()) joined += '\n';
            joined += line;
        }
        return joined;
    }()));
    const bool active = transfer_active(snapshot.send_progress.state);
    open_button_->setEnabled(!snapshot.receiving && !active);
    receive_button_->setEnabled(!active);
    receive_button_->setText(snapshot.receiving ? "Stop Receive" : "Receive");
    send_button_->setEnabled(snapshot.can_raw_send && snapshot.transmit_route && !active);
    send_button_->setToolTip(
        snapshot.profile_supports_transfer ?
            "Explicit raw SysEx send to the exact displayed TX route; not a validated restore." :
            "Generic Raw Send only: the active profile declares no transfer capability. "
            "This is not a validated restore and requires an explicit click.");
    cancel_button_->setEnabled(active);
    save_button_->setEnabled(snapshot.can_save_verified_received && !active);
    clear_button_->setEnabled(!snapshot.receiving && !active);
    if (selected_row >= 0 && selected_row < frame_model_->rowCount()) {
        frame_table_->selectRow(selected_row);
        update_raw_inspector(selected_row);
    } else if (frame_model_->rowCount() > 0) {
        frame_table_->selectRow(0);
        update_raw_inspector(0);
    } else {
        raw_bytes_->clear();
    }
    if (snapshot_observer_) snapshot_observer_(snapshot_);
    if (snapshot.send_error) {
        show_error(*snapshot.send_error);
    } else if (snapshot.receiving) {
        status_label_->setText("Receive capture active");
    } else {
        switch (snapshot.send_progress.state) {
        case transfer::TransferState::preparing:
        case transfer::TransferState::running:
            status_label_->setText("Raw Send active on the exact displayed TX route");
            break;
        case transfer::TransferState::cancelling:
            status_label_->setText("Raw Send cancellation requested");
            break;
        case transfer::TransferState::completed:
            status_label_->setText("Raw Send completed");
            break;
        case transfer::TransferState::cancelled:
            status_label_->setText("Raw Send cancelled");
            break;
        case transfer::TransferState::failed:
            status_label_->setText("Raw Send failed");
            break;
        case transfer::TransferState::idle:
            break;
        }
    }
}

void SysExTransferPanel::update_raw_inspector(const int row) {
    const auto* frame = frame_model_->frame(row);
    raw_bytes_->setPlainText(frame ? bytes_hex(frame->bytes) : QString{});
}

void SysExTransferPanel::show_error(const midi::MidiError& error) {
    status_label_->setText("Error: " + QString::fromStdString(error.message));
}

} // namespace taureon::gui
