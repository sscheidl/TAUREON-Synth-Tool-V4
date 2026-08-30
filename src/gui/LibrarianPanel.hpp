#pragma once

#include "app/Librarian.hpp"

#include <QAbstractTableModel>
#include <QWidget>

#include <memory>
#include <optional>

class QComboBox;
class QLabel;
class QPushButton;
class QTableView;

namespace taureon::gui {

class LibrarianTableModel final : public QAbstractTableModel {
public:
    explicit LibrarianTableModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void replace_bank(std::optional<app::LibrarianBank> bank);

private:
    std::optional<app::LibrarianBank> bank_;
};

class LibrarianPanel final : public QWidget {
public:
    explicit LibrarianPanel(QWidget* parent = nullptr);

    void set_provider(std::shared_ptr<const app::ILibrarianProvider> provider);
    [[nodiscard]] bool has_required_controls() const noexcept;

private:
    void rebuild_from_provider();
    void select_collection(int index);
    void select_bank(int index);

    std::shared_ptr<const app::ILibrarianProvider> provider_;
    app::LibrarianSnapshot snapshot_;
    LibrarianTableModel* model_{};
    QComboBox* collection_selector_{};
    QComboBox* bank_selector_{};
    QTableView* slots_{};
    QLabel* support_{};
    QLabel* capacity_{};
    QLabel* operation_reason_{};
    QPushButton* rename_{};
    int collection_index_{-1};
};

} // namespace taureon::gui
