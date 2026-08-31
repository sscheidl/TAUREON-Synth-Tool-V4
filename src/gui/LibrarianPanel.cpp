#include "gui/LibrarianPanel.hpp"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

#include <array>
#include <utility>

namespace taureon::gui {
namespace {

constexpr auto kColumns = std::array{"Slot", "State", "Semantic object", "Access"};

QString capacity_text(const app::LibrarianCapacity& capacity) {
    switch (capacity.kind) {
    case app::LibrarianCapacityKind::known:
        return QStringLiteral("Known capacity: %1 slots").arg(capacity.known_slots);
    case app::LibrarianCapacityKind::unknown:
        return QStringLiteral("Capacity: unknown (no rows are invented)");
    case app::LibrarianCapacityKind::unavailable:
        return QStringLiteral("Capacity: unavailable");
    }
    return {};
}

QString slot_state(const app::LibrarianSlot& slot) {
    if (!slot.unavailable_reason.empty()) return QStringLiteral("Unavailable");
    return slot.semantic_object ? QStringLiteral("Occupied") : QStringLiteral("Empty");
}

} // namespace

LibrarianTableModel::LibrarianTableModel(QObject* parent) : QAbstractTableModel(parent) {}

int LibrarianTableModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid() || !bank_) return 0;
    return static_cast<int>(bank_->entries.size());
}

int LibrarianTableModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(kColumns.size());
}

QVariant LibrarianTableModel::data(const QModelIndex& index, const int role) const {
    if (!index.isValid() || !bank_ || index.row() < 0 ||
        index.row() >= static_cast<int>(bank_->entries.size())) return {};
    const auto& slot = bank_->entries.at(static_cast<std::size_t>(index.row()));
    if (role == Qt::ToolTipRole && !slot.unavailable_reason.empty()) {
        return QString::fromStdString(slot.unavailable_reason);
    }
    if (role != Qt::DisplayRole) return {};
    switch (index.column()) {
    case 0: return QString::fromStdString(slot.display_label);
    case 1: return slot_state(slot);
    case 2: return slot.semantic_object
        ? QString::fromStdString(slot.semantic_object->display_name)
        : QStringLiteral("—");
    case 3: return slot.read_only ? QStringLiteral("Read-only") : QStringLiteral("Writable");
    default: return {};
    }
}

QVariant LibrarianTableModel::headerData(const int section, const Qt::Orientation orientation,
                                         const int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section >= 0 &&
        section < static_cast<int>(kColumns.size())) return QString::fromLatin1(kColumns.at(section));
    return QAbstractTableModel::headerData(section, orientation, role);
}

void LibrarianTableModel::replace_bank(std::optional<app::LibrarianBank> bank) {
    beginResetModel();
    bank_ = std::move(bank);
    endResetModel();
}

LibrarianPanel::LibrarianPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("librarianPanel");
    auto* layout = new QVBoxLayout(this);
    support_ = new QLabel(this);
    support_->setObjectName("librarianSupportState");
    support_->setWordWrap(true);
    layout->addWidget(support_);

    auto* selectors = new QHBoxLayout;
    collection_selector_ = new QComboBox(this);
    collection_selector_->setObjectName("librarianCollectionSelector");
    collection_selector_->setAccessibleName("Librarian collection");
    bank_selector_ = new QComboBox(this);
    bank_selector_->setObjectName("librarianBankSelector");
    bank_selector_->setAccessibleName("Librarian bank");
    selectors->addWidget(new QLabel("Collection", this));
    selectors->addWidget(collection_selector_, 1);
    selectors->addWidget(new QLabel("Bank", this));
    selectors->addWidget(bank_selector_, 1);
    layout->addLayout(selectors);

    capacity_ = new QLabel(this);
    capacity_->setObjectName("librarianCapacityState");
    layout->addWidget(capacity_);

    model_ = new LibrarianTableModel(this);
    slots_ = new QTableView(this);
    slots_->setObjectName("librarianSlotsTable");
    slots_->setAccessibleName("Librarian slots");
    slots_->setModel(model_);
    slots_->setSelectionBehavior(QAbstractItemView::SelectRows);
    slots_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    slots_->setAlternatingRowColors(true);
    layout->addWidget(slots_, 1);

    rename_ = new QPushButton("Rename", this);
    rename_->setObjectName("librarianRenameAction");
    rename_->setEnabled(false);
    operation_reason_ = new QLabel(this);
    operation_reason_->setObjectName("librarianOperationReason");
    operation_reason_->setWordWrap(true);
    layout->addWidget(rename_);
    layout->addWidget(operation_reason_);

    connect(collection_selector_, &QComboBox::currentIndexChanged, this,
            [this](const int index) { select_collection(index); });
    connect(bank_selector_, &QComboBox::currentIndexChanged, this,
            [this](const int index) { select_bank(index); });
    set_provider({});
}

void LibrarianPanel::set_provider(std::shared_ptr<const app::ILibrarianProvider> provider) {
    provider_ = std::move(provider);
    rebuild_from_provider();
}

bool LibrarianPanel::has_required_controls() const noexcept {
    return model_ != nullptr && collection_selector_ != nullptr && bank_selector_ != nullptr &&
           slots_ != nullptr && support_ != nullptr && capacity_ != nullptr &&
           operation_reason_ != nullptr && rename_ != nullptr;
}

void LibrarianPanel::rebuild_from_provider() {
    snapshot_ = provider_ ? provider_->snapshot() : app::LibrarianSnapshot{
        false, "Semantic Librarian support is unavailable for this profile.", {}};
    const auto valid = app::librarian_snapshot_is_well_formed(snapshot_);
    const auto available = valid && snapshot_.semantic_support_available;
    support_->setText(available
        ? QStringLiteral("Semantic Librarian support is available from the active provider.")
        : !valid
            ? QStringLiteral("Semantic Librarian provider snapshot is malformed.")
            : QString::fromStdString(snapshot_.unavailable_reason.empty()
                ? "Semantic Librarian support is unavailable for this profile."
                : snapshot_.unavailable_reason));
    collection_selector_->blockSignals(true);
    collection_selector_->clear();
    for (const auto& collection : snapshot_.collections) {
        collection_selector_->addItem(QString::fromStdString(collection.display_name));
    }
    collection_selector_->setEnabled(available && !snapshot_.collections.empty());
    collection_selector_->blockSignals(false);
    select_collection(collection_selector_->count() > 0 ? 0 : -1);
}

void LibrarianPanel::select_collection(const int index) {
    collection_index_ = index;
    bank_selector_->blockSignals(true);
    bank_selector_->clear();
    if (index >= 0 && index < static_cast<int>(snapshot_.collections.size())) {
        for (const auto& bank : snapshot_.collections.at(static_cast<std::size_t>(index)).banks) {
            bank_selector_->addItem(QString::fromStdString(bank.display_name));
        }
    }
    bank_selector_->setEnabled(bank_selector_->count() > 0);
    bank_selector_->blockSignals(false);
    select_bank(bank_selector_->count() > 0 ? 0 : -1);
}

void LibrarianPanel::select_bank(const int index) {
    if (collection_index_ < 0 || collection_index_ >= static_cast<int>(snapshot_.collections.size()) ||
        index < 0 || index >= static_cast<int>(
            snapshot_.collections.at(static_cast<std::size_t>(collection_index_)).banks.size())) {
        model_->replace_bank(std::nullopt);
        capacity_->setText("Capacity: unavailable");
        operation_reason_->setText(provider_
            ? "Semantic actions are unavailable because no Librarian bank is selected."
            : "Semantic actions are unavailable because no semantic provider is active.");
        rename_->setEnabled(false);
        return;
    }
    const auto& bank = snapshot_.collections.at(static_cast<std::size_t>(collection_index_))
                           .banks.at(static_cast<std::size_t>(index));
    model_->replace_bank(bank);
    capacity_->setText(capacity_text(bank.capacity));
    const auto& reason = bank.operations.unavailable_reason;
    operation_reason_->setText(QString::fromStdString(reason.empty()
        ? "Rename is not implemented in this bounded foundation."
        : reason));
    rename_->setEnabled(false);
    rename_->setToolTip(operation_reason_->text());
}

} // namespace taureon::gui
