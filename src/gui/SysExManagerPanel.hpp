#pragma once

#include "app/SysExManager.hpp"

#include <QAbstractTableModel>
#include <QWidget>

#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTableView;

namespace taureon::gui {

class SysExManagerItemModel final : public QAbstractTableModel {
public:
    explicit SysExManagerItemModel(QObject* parent = nullptr);
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index,
                                int role = Qt::DisplayRole) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                      int role = Qt::DisplayRole) const override;
    void set_items(std::vector<app::SysExManagerItemSnapshot> items);
    [[nodiscard]] const app::SysExManagerItemSnapshot* item(int row) const noexcept;

private:
    std::vector<app::SysExManagerItemSnapshot> items_;
};

class SysExManagerFrameModel final : public QAbstractTableModel {
public:
    explicit SysExManagerFrameModel(QObject* parent = nullptr);
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index,
                                int role = Qt::DisplayRole) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                      int role = Qt::DisplayRole) const override;
    void set_frames(std::vector<app::SysExManagerFrameSnapshot> frames);
    [[nodiscard]] const app::SysExManagerFrameSnapshot* frame(int row) const noexcept;

private:
    std::vector<app::SysExManagerFrameSnapshot> frames_;
};

class SysExManagerPanel final : public QWidget {
public:
    using OpenInTransfer = std::function<void(app::SysExManagerTransferItem)>;

    explicit SysExManagerPanel(std::shared_ptr<const profiles::ProfileRegistry> registry = {},
                               OpenInTransfer open_in_transfer = {}, QWidget* parent = nullptr);

    void add_file(const std::filesystem::path& path);
    [[nodiscard]] bool has_required_controls() const noexcept;

private:
    void refresh();
    void refresh_selection();
    void update_raw_inspector(int row);
    [[nodiscard]] std::vector<app::SysExManagerFrameReference> selected_frame_references() const;
    void show_error(const midi::MidiError& error);
    void update_controls();

    app::SysExManager manager_;
    OpenInTransfer open_in_transfer_;
    SysExManagerItemModel* item_model_{};
    SysExManagerFrameModel* frame_model_{};
    QTableView* item_table_{};
    QTableView* frame_table_{};
    QLabel* summary_label_{};
    QLabel* status_label_{};
    QPlainTextEdit* raw_bytes_{};
    QPushButton* add_button_{};
    QPushButton* remove_button_{};
    QPushButton* export_button_{};
    QPushButton* merge_button_{};
    QPushButton* open_transfer_button_{};
    std::uint64_t selected_item_id_{};
    std::vector<app::SysExManagerFrameReference> visible_frame_references_;
};

} // namespace taureon::gui
