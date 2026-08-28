#include "gui/SysExManagerPanel.hpp"

#include <QFileDialog>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>

namespace taureon::gui {
namespace {

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

QString frame_status(const sysex::SysExFrame& frame) {
    if (frame.affected_by_data_loss) return QStringLiteral("Tainted / malformed");
    switch (frame.status) {
    case sysex::SysExFrameStatus::complete: return QStringLiteral("Complete");
    case sysex::SysExFrameStatus::incomplete: return QStringLiteral("Incomplete");
    case sysex::SysExFrameStatus::malformed: return QStringLiteral("Malformed");
    }
    return QStringLiteral("Unknown");
}

QString bytes_hex(const std::vector<std::uint8_t>& bytes) {
    const QByteArray raw(reinterpret_cast<const char*>(bytes.data()),
                         static_cast<qsizetype>(bytes.size()));
    return QString::fromLatin1(raw.toHex(' ').toUpper());
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
    case 3: return QString::number(item.raw_bytes.size());
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
    case 1: return frame_status(frame.frame);
    case 2: return QString::number(frame.frame.bytes.size());
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
    item_table_->setSelectionMode(QAbstractItemView::SingleSelection);
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
    raw_bytes_->setMaximumBlockCount(1);
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
        const auto indices = selected_frame_indices();
        if (selected_item_id_ == 0 || indices.empty()) return;
        const auto destination = QFileDialog::getSaveFileName(
            this, "Export selected verified frames", {}, "SysEx files (*.syx)");
        if (destination.isEmpty()) return;
        const auto result = manager_.export_frames(selected_item_id_, indices,
                                                    destination.toStdWString());
        if (!result) {
            show_error(result.error());
            return;
        }
        status_label_->setText("Exported selected verified frames to a new file. Source unchanged.");
    });
    connect(merge_button_, &QPushButton::clicked, this, [this] {
        const auto indices = selected_frame_indices();
        if (selected_item_id_ == 0 || indices.empty()) return;
        std::vector<app::SysExManagerFrameReference> frames;
        frames.reserve(indices.size());
        for (const auto index : indices) frames.push_back({selected_item_id_, index});
        const auto destination = QFileDialog::getSaveFileName(
            this, "Merge selected verified frames", {}, "SysEx files (*.syx)");
        if (destination.isEmpty()) return;
        const auto result = manager_.merge_frames(frames, destination.toStdWString());
        if (!result) {
            show_error(result.error());
            return;
        }
        status_label_->setText("Merged selected verified frames to a new file. Source unchanged.");
    });
    connect(open_transfer_button_, &QPushButton::clicked, this, [this] {
        const auto source = manager_.transferable_source(selected_item_id_);
        if (!source || !open_in_transfer_) {
            status_label_->setText(
                "Open in Transfer is unavailable until one complete, untainted workspace item is selected.");
            return;
        }
        open_in_transfer_(*source);
        status_label_->setText("Opened the selected valid source in SysEx Transfer; no send was started.");
    });
    connect(item_table_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex& current) { select_item(current.row()); });
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
        QStringLiteral("%1 file(s) in this read-only workspace. Hashes use deterministic FNV-1a 64.")
            .arg(snapshot.items.size()));
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
        select_item(row);
    } else {
        selected_item_id_ = 0;
        frame_model_->set_frames({});
        raw_bytes_->clear();
        update_controls();
    }
}

void SysExManagerPanel::select_item(const int row) {
    const auto* item = item_model_->item(row);
    if (!item) {
        selected_item_id_ = 0;
        frame_model_->set_frames({});
        raw_bytes_->clear();
        update_controls();
        return;
    }
    selected_item_id_ = item->id;
    frame_model_->set_frames(item->frames);
    summary_label_->setText(
        QStringLiteral("%1 · %2 · %3")
            .arg(QString::fromStdString(item->source_name), device_text(*item),
                 QString::fromStdString(item->recognition_message)));
    if (frame_model_->rowCount() > 0) {
        frame_table_->selectRow(0);
        update_raw_inspector(0);
    } else {
        raw_bytes_->clear();
    }
    update_controls();
}

void SysExManagerPanel::update_raw_inspector(const int row) {
    const auto* frame = frame_model_->frame(row);
    raw_bytes_->setPlainText(frame ? bytes_hex(frame->frame.bytes) : QString{});
    update_controls();
}

std::vector<std::size_t> SysExManagerPanel::selected_frame_indices() const {
    std::vector<std::size_t> result;
    const auto selected = frame_table_->selectionModel()->selectedRows();
    result.reserve(selected.size());
    for (const auto& index : selected) {
        if (index.row() >= 0) result.push_back(static_cast<std::size_t>(index.row()));
    }
    std::sort(result.begin(), result.end());
    return result;
}

void SysExManagerPanel::show_error(const midi::MidiError& error) {
    status_label_->setText("Error: " + QString::fromStdString(error.message));
}

void SysExManagerPanel::update_controls() {
    const bool item_selected = selected_item_id_ != 0;
    const bool frames_selected = !selected_frame_indices().empty();
    remove_button_->setEnabled(item_selected);
    export_button_->setEnabled(item_selected && frames_selected);
    merge_button_->setEnabled(item_selected && frames_selected);
    open_transfer_button_->setEnabled(manager_.transferable_source(selected_item_id_).has_value());
    export_button_->setToolTip(
        "Writes selected complete, unaffected frames to a new destination; source files are unchanged.");
    merge_button_->setToolTip(
        "Writes the selected complete, unaffected frames in visible order to a new destination.");
    open_transfer_button_->setToolTip(
        "Loads one complete, untainted source file into SysEx Transfer. It does not send automatically.");
}

} // namespace taureon::gui
