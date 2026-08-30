#include "gui/SysExManagerPanel.hpp"

#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace taureon::gui {
namespace {

struct ConfirmedSaveDestination {
    std::filesystem::path path;
    bool replace_existing{};
};

QString integrity_status(const app::SysExManagerItemSnapshot& item) {
    QString status = QStringLiteral("complete %1 · incomplete %2 · malformed %3 · tainted %4")
                         .arg(item.complete_frames)
                         .arg(item.incomplete_frames)
                         .arg(item.malformed_frames)
                         .arg(item.tainted_frames);
    if (item.exact_file_duplicate_of) {
        status += QStringLiteral(" · exact file duplicate of workspace item %1")
                      .arg(*item.exact_file_duplicate_of);
    }
    return status;
}

QString device_text(const app::SysExManagerItemSnapshot& item) {
    if (!item.manufacturer && !item.model) return QStringLiteral("Not identified");
    return QStringLiteral("%1 %2")
        .arg(item.manufacturer ? QString::fromStdString(*item.manufacturer) : QStringLiteral("Unknown"),
             item.model ? QString::fromStdString(*item.model) : QString{});
}

QString frame_status(const app::SysExManagerFrameSnapshot& frame) {
    if (frame.affected_by_data_loss) return QStringLiteral("Tainted / malformed");
    switch (frame.status) {
    case sysex::SysExFrameStatus::complete: return QStringLiteral("Complete");
    case sysex::SysExFrameStatus::incomplete: return QStringLiteral("Incomplete");
    case sysex::SysExFrameStatus::malformed: return QStringLiteral("Malformed");
    }
    return QStringLiteral("Unknown");
}

QString bytes_hex_preview(const std::vector<std::uint8_t>& bytes, const std::size_t limit) {
    const auto shown = (std::min)(bytes.size(), limit);
    const QByteArray raw(reinterpret_cast<const char*>(bytes.data()), static_cast<qsizetype>(shown));
    QString result = QStringLiteral("Showing %1 of %2 bytes")
                         .arg(shown)
                         .arg(bytes.size());
    if (shown < bytes.size()) result += QStringLiteral("; remaining bytes are not displayed.");
    result += QStringLiteral("\n") + QString::fromLatin1(raw.toHex(' ').toUpper());
    return result;
}

std::optional<ConfirmedSaveDestination> choose_save_destination(QWidget* parent,
                                                                 const QString& title) {
    QFileDialog dialog(parent, title);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setNameFilter(QStringLiteral("SysEx files (*.syx)"));
    dialog.setDefaultSuffix(QStringLiteral("syx"));
    // Qt's native save dialog asks before replacing an existing target.
    dialog.setOption(QFileDialog::DontConfirmOverwrite, false);
    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().empty()) return std::nullopt;
    const auto selected = dialog.selectedFiles().front();
    return ConfirmedSaveDestination{selected.toStdWString(), QFileInfo::exists(selected)};
}

} // namespace

SysExManagerItemModel::SysExManagerItemModel(QObject* parent) : QAbstractTableModel(parent) {}

int SysExManagerItemModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(items_.size());
}

int SysExManagerItemModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : 6;
}

QVariant SysExManagerItemModel::data(const QModelIndex& index, const int role) const {
    if (!index.isValid() || role != Qt::DisplayRole || index.row() < 0 ||
        index.row() >= static_cast<int>(items_.size())) return {};
    const auto& item = items_.at(static_cast<std::size_t>(index.row()));
    switch (index.column()) {
    case 0: return QString::fromStdString(item.source_name);
    case 1: return device_text(item);
    case 2: return QString::number(item.frames.size());
    case 3: return QString::number(item.byte_count);
    case 4: return integrity_status(item);
    case 5: return QString::fromStdString(item.file_hash);
    default: return {};
    }
}

QVariant SysExManagerItemModel::headerData(const int section, const Qt::Orientation orientation,
                                           const int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    static const QStringList headers{"File", "Device / Manufacturer", "Frames", "Size",
                                     "Status", "Deterministic hash"};
    return section >= 0 && section < headers.size() ? headers.at(section) : QVariant{};
}

void SysExManagerItemModel::set_items(std::vector<app::SysExManagerItemSnapshot> items) {
    beginResetModel();
    items_ = std::move(items);
    endResetModel();
}

const app::SysExManagerItemSnapshot* SysExManagerItemModel::item(const int row) const noexcept {
    if (row < 0 || row >= static_cast<int>(items_.size())) return nullptr;
    return &items_.at(static_cast<std::size_t>(row));
}

SysExManagerFrameModel::SysExManagerFrameModel(QObject* parent) : QAbstractTableModel(parent) {}

int SysExManagerFrameModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(frames_.size());
}

int SysExManagerFrameModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : 6;
}

QVariant SysExManagerFrameModel::data(const QModelIndex& index, const int role) const {
    if (!index.isValid() || role != Qt::DisplayRole || index.row() < 0 ||
        index.row() >= static_cast<int>(frames_.size())) return {};
    const auto& frame = frames_.at(static_cast<std::size_t>(index.row()));
    switch (index.column()) {
    case 0: return index.row() + 1;
    case 1: return frame_status(frame);
    case 2: return QString::number(frame.byte_count);
    case 3: return QString::fromStdString(frame.hash);
    case 4: return QString::fromStdString(frame.payload_hash);
    case 5: return QStringLiteral("exact frame %1 · payload %2")
                .arg(frame.exact_frame_duplicates.size())
                .arg(frame.exact_payload_duplicates.size());
    default: return {};
    }
}

QVariant SysExManagerFrameModel::headerData(const int section, const Qt::Orientation orientation,
                                            const int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    static const QStringList headers{"Frame", "State", "Bytes", "Frame hash", "Payload hash",
                                     "Exact duplicates"};
    return section >= 0 && section < headers.size() ? headers.at(section) : QVariant{};
}

void SysExManagerFrameModel::set_frames(std::vector<app::SysExManagerFrameSnapshot> frames) {
    beginResetModel();
    frames_ = std::move(frames);
    endResetModel();
}

const app::SysExManagerFrameSnapshot* SysExManagerFrameModel::frame(const int row) const noexcept {
    if (row < 0 || row >= static_cast<int>(frames_.size())) return nullptr;
    return &frames_.at(static_cast<std::size_t>(row));
}

SysExManagerPanel::SysExManagerPanel(std::shared_ptr<const profiles::ProfileRegistry> registry,
                                     OpenInTransfer open_in_transfer, QWidget* parent)
    : QWidget(parent), manager_(std::move(registry)), open_in_transfer_(std::move(open_in_transfer)) {
    setObjectName("sysExManagerPanel");
    auto* root = new QVBoxLayout(this);
    summary_label_ = new QLabel("Workspace is empty. Source files are read-only.", this);
    summary_label_->setObjectName("sysExManagerSummary");
    summary_label_->setWordWrap(true);
    root->addWidget(summary_label_);

    auto* actions = new QHBoxLayout;
    add_button_ = new QPushButton("Add .syx files…", this);
    add_button_->setObjectName("sysExManagerAdd");
    remove_button_ = new QPushButton("Remove from workspace", this);
    remove_button_->setObjectName("sysExManagerRemove");
    export_button_ = new QPushButton("Export selected frames…", this);
    export_button_->setObjectName("sysExManagerExport");
    merge_button_ = new QPushButton("Merge selected complete frames…", this);
    merge_button_->setObjectName("sysExManagerMerge");
    open_transfer_button_ = new QPushButton("Open valid item in Transfer", this);
    open_transfer_button_->setObjectName("sysExManagerOpenTransfer");
    actions->addWidget(add_button_);
    actions->addWidget(remove_button_);
    actions->addWidget(export_button_);
    actions->addWidget(merge_button_);
    actions->addWidget(open_transfer_button_);
    root->addLayout(actions);

    item_model_ = new SysExManagerItemModel(this);
    item_table_ = new QTableView(this);
    item_table_->setObjectName("sysExManagerFileTable");
    item_table_->setModel(item_model_);
    item_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    item_table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    item_table_->horizontalHeader()->setStretchLastSection(true);
    root->addWidget(item_table_, 2);

    frame_model_ = new SysExManagerFrameModel(this);
    frame_table_ = new QTableView(this);
    frame_table_->setObjectName("sysExManagerFrameTable");
    frame_table_->setModel(frame_model_);
    frame_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    frame_table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    frame_table_->horizontalHeader()->setStretchLastSection(true);
    root->addWidget(frame_table_, 2);

    raw_bytes_ = new QPlainTextEdit(this);
    raw_bytes_->setObjectName("sysExManagerRawBytes");
    raw_bytes_->setReadOnly(true);
    raw_bytes_->setPlaceholderText("Select a frame to inspect its exact raw bytes.");
    // One block holds the explicit preview count and one holds the displayed hex bytes.
    raw_bytes_->setMaximumBlockCount(2);
    root->addWidget(raw_bytes_, 1);

    status_label_ = new QLabel("No file has been opened and no transfer is active.", this);
    status_label_->setObjectName("sysExManagerStatus");
    status_label_->setWordWrap(true);
    root->addWidget(status_label_);

    connect(add_button_, &QPushButton::clicked, this, [this] {
        const auto paths = QFileDialog::getOpenFileNames(this, "Add SysEx files", {},
                                                          "SysEx files (*.syx)");
        for (const auto& path : paths) add_file(path.toStdWString());
    });
    connect(remove_button_, &QPushButton::clicked, this, [this] {
        if (selected_item_id_ == 0) return;
        const auto result = manager_.remove_item(selected_item_id_);
        if (!result) {
            show_error(result.error());
            return;
        }
        status_label_->setText("Removed from this workspace only; the source file was not deleted.");
        selected_item_id_ = 0;
        refresh();
    });
    connect(export_button_, &QPushButton::clicked, this, [this] {
        const auto references = selected_frame_references();
        if (references.empty()) return;
        const auto item_id = references.front().item_id;
        std::vector<std::size_t> indices;
        indices.reserve(references.size());
        for (const auto& reference : references) {
            if (reference.item_id != item_id) {
                status_label_->setText(
                    "Export selected frames requires one workspace file; use Merge for multiple files.");
                return;
            }
            indices.push_back(reference.frame_index);
        }
        const auto destination = choose_save_destination(this, "Export selected verified frames");
        if (!destination) return;
        const auto result = manager_.export_frames(item_id, indices, destination->path,
                                                   destination->replace_existing);
        if (!result) {
            show_error(result.error());
            return;
        }
        status_label_->setText("Exported selected verified frames atomically. Source unchanged.");
    });
    connect(merge_button_, &QPushButton::clicked, this, [this] {
        const auto references = selected_frame_references();
        if (references.empty()) return;
        const auto destination = choose_save_destination(this, "Merge selected verified frames");
        if (!destination) return;
        const auto result = manager_.merge_frames(references, destination->path,
                                                  destination->replace_existing);
        if (!result) {
            show_error(result.error());
            return;
        }
        status_label_->setText("Merged selected verified frames atomically. Source unchanged.");
    });
    connect(open_transfer_button_, &QPushButton::clicked, this, [this] {
        auto item = manager_.transferable_item(selected_item_id_);
        if (!item || !open_in_transfer_) {
            status_label_->setText(
                "Open in Transfer is unavailable until one complete, untainted workspace item is selected.");
            return;
        }
        auto completion_state = std::make_shared<TransferHandoffCompletionState>();
        pending_handoff_completion_ = completion_state;
        connect(completion_state.get(), &TransferHandoffCompletionState::completed, this,
                [this, completion = std::weak_ptr{completion_state}](const bool loaded) {
                    if (pending_handoff_completion_ != completion.lock()) return;
                    pending_handoff_completion_.reset();
                    on_transfer_handoff_result(loaded);
                },
                Qt::QueuedConnection);
        const bool accepted = open_in_transfer_(
            std::move(*item), [completion = std::weak_ptr{completion_state}](const bool loaded) {
                if (const auto state = completion.lock()) state->complete(loaded);
            });
        if (!accepted) {
            pending_handoff_completion_.reset();
            status_label_->setText(
                "Transfer handoff rejected: the Transfer workspace is busy; no document changed and no send started.");
        }
    });
    connect(item_table_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this] { refresh_selection(); });
    connect(frame_table_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex& current) { update_raw_inspector(current.row()); });
    refresh();
}

void SysExManagerPanel::add_file(const std::filesystem::path& path) {
    const auto result = manager_.add_file(path);
    if (!result) {
        show_error(result.error());
        return;
    }
    selected_item_id_ = result.value();
    status_label_->setText("Added read-only source file; no transfer was started.");
    refresh();
}

bool SysExManagerPanel::has_required_controls() const noexcept {
    return add_button_ && remove_button_ && export_button_ && merge_button_ &&
           open_transfer_button_ && item_table_ && frame_table_ && raw_bytes_;
}

void SysExManagerPanel::refresh() {
    const auto snapshot = manager_.snapshot();
    item_model_->set_items(snapshot.items);
    summary_label_->setText(
        QStringLiteral("%1 file(s) in this user-initiated read-only workspace. "
                       "Raw SysEx payload limit: %2 bytes per document, %3 bytes aggregate. "
                       "Hashes use deterministic FNV-1a 64.")
            .arg(snapshot.items.size())
            .arg(static_cast<qulonglong>(app::SysExManager::kMaxDocumentRawBytes))
            .arg(static_cast<qulonglong>(app::SysExManager::kMaxAggregateRawBytes)));
    int row = -1;
    for (int index = 0; index < item_model_->rowCount(); ++index) {
        if (const auto* item = item_model_->item(index); item && item->id == selected_item_id_) {
            row = index;
            break;
        }
    }
    if (row < 0 && item_model_->rowCount() > 0) row = 0;
    if (row >= 0) {
        item_table_->selectRow(row);
        refresh_selection();
    } else {
        selected_item_id_ = 0;
        frame_model_->set_frames({});
        raw_bytes_->clear();
        update_controls();
    }
}

void SysExManagerPanel::refresh_selection() {
    const auto selected = item_table_->selectionModel()->selectedRows();
    if (selected.empty()) {
        selected_item_id_ = 0;
        visible_frame_references_.clear();
        frame_model_->set_frames({});
        raw_bytes_->clear();
        update_controls();
        return;
    }
    std::vector<app::SysExManagerFrameSnapshot> frames;
    visible_frame_references_.clear();
    for (const auto& selection : selected) {
        const auto* item = item_model_->item(selection.row());
        if (!item) continue;
        for (std::size_t index = 0; index < item->frames.size(); ++index) {
            frames.push_back(item->frames.at(index));
            visible_frame_references_.push_back({item->id, index});
        }
    }
    const auto* current = item_model_->item(item_table_->currentIndex().row());
    selected_item_id_ = current ? current->id : 0;
    frame_model_->set_frames(std::move(frames));
    summary_label_->setText(
        QStringLiteral("%1 workspace file(s) selected; select frames below to export or merge.")
            .arg(selected.size()));
    if (frame_model_->rowCount() > 0) {
        frame_table_->selectRow(0);
        update_raw_inspector(0);
    } else {
        raw_bytes_->clear();
    }
    update_controls();
}

void SysExManagerPanel::update_raw_inspector(const int row) {
    if (row < 0 || row >= static_cast<int>(visible_frame_references_.size())) {
        raw_bytes_->clear();
        update_controls();
        return;
    }
    const auto bytes = manager_.frame_bytes(
        visible_frame_references_.at(static_cast<std::size_t>(row)));
    if (!bytes) {
        raw_bytes_->setPlainText("Frame bytes are unavailable.");
    } else {
        raw_bytes_->setPlainText(bytes_hex_preview(bytes.value(), kRawInspectorPreviewBytes));
    }
    update_controls();
}

void SysExManagerPanel::on_transfer_handoff_result(const bool loaded) {
    status_label_->setText(
        loaded ? "Opened inspected Manager bytes in SysEx Transfer; no send was started." :
                 "Transfer handoff was rejected; no document changed and no send started.");
}

std::vector<app::SysExManagerFrameReference> SysExManagerPanel::selected_frame_references() const {
    std::vector<app::SysExManagerFrameReference> result;
    const auto selected = frame_table_->selectionModel()->selectedRows();
    result.reserve(selected.size());
    for (const auto& index : selected) {
        if (index.row() >= 0 &&
            index.row() < static_cast<int>(visible_frame_references_.size())) {
            result.push_back(visible_frame_references_.at(static_cast<std::size_t>(index.row())));
        }
    }
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        return left.item_id == right.item_id ? left.frame_index < right.frame_index :
                                               left.item_id < right.item_id;
    });
    return result;
}

void SysExManagerPanel::show_error(const midi::MidiError& error) {
    status_label_->setText("Error: " + QString::fromStdString(error.message));
}

void SysExManagerPanel::update_controls() {
    const bool item_selected = selected_item_id_ != 0;
    const auto references = selected_frame_references();
    const bool frames_selected = !references.empty();
    const bool single_source = frames_selected &&
        std::all_of(references.begin(), references.end(), [&references](const auto& reference) {
            return reference.item_id == references.front().item_id;
        });
    remove_button_->setEnabled(item_selected);
    export_button_->setEnabled(frames_selected && single_source);
    merge_button_->setEnabled(frames_selected);
    open_transfer_button_->setEnabled(item_table_->selectionModel()->selectedRows().size() == 1 &&
                                      manager_.can_transfer(selected_item_id_));
    export_button_->setToolTip(
        "Writes selected complete, unaffected frames atomically to a destination; source files are unchanged.");
    merge_button_->setToolTip(
        "Merges selected complete, unaffected frames from one or more workspace files atomically.");
    open_transfer_button_->setToolTip(
        "Loads the inspected Manager bytes into SysEx Transfer. It does not send automatically.");
}

} // namespace taureon::gui
