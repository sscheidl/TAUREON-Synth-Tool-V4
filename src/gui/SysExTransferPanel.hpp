#pragma once

#include "app/ConnectionWorker.hpp"

#include <QAbstractTableModel>
#include <QWidget>

#include <filesystem>
#include <future>
#include <optional>
#include <vector>

class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTableView;
class QTimer;

namespace taureon::gui {

class SysExFrameModel final : public QAbstractTableModel {
public:
    explicit SysExFrameModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index,
                                int role = Qt::DisplayRole) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                      int role = Qt::DisplayRole) const override;
    void set_frames(std::vector<sysex::SysExFrame> frames);
    [[nodiscard]] const sysex::SysExFrame* frame(int row) const noexcept;

private:
    std::vector<sysex::SysExFrame> frames_;
};

class SysExTransferPanel final : public QWidget {
public:
    explicit SysExTransferPanel(app::ConnectionWorker& worker, QWidget* parent = nullptr);

    void request_load(const std::filesystem::path& path);
    void request_load_document(sysex::SyxDocument document, std::string source_name);
    void request_save_received(const std::filesystem::path& path);
    [[nodiscard]] bool has_required_controls() const noexcept;

private:
    enum class PendingAction { load, receive, finish_receive, send, cancel, save, clear, refresh };

    void set_pending(std::future<midi::Result<app::SysExTransferSnapshot>> future,
                     PendingAction action, QString status);
    void poll_result();
    void apply_snapshot(const app::SysExTransferSnapshot& snapshot);
    void update_raw_inspector(int row);
    void show_error(const midi::MidiError& error);

    app::ConnectionWorker& worker_;
    SysExFrameModel* frame_model_{};
    QTableView* frame_table_{};
    QLabel* source_label_{};
    QLabel* evidence_label_{};
    QLabel* profile_label_{};
    QLabel* counts_label_{};
    QLabel* integrity_label_{};
    QLabel* route_label_{};
    QLabel* pacing_label_{};
    QLabel* status_label_{};
    QPlainTextEdit* raw_bytes_{};
    QPlainTextEdit* transfer_log_{};
    QProgressBar* progress_{};
    QSpinBox* pacing_delay_{};
    QPushButton* open_button_{};
    QPushButton* receive_button_{};
    QPushButton* send_button_{};
    QPushButton* cancel_button_{};
    QPushButton* save_button_{};
    QPushButton* clear_button_{};
    QPushButton* validated_restore_button_{};
    QTimer* poll_timer_{};
    std::optional<std::future<midi::Result<app::SysExTransferSnapshot>>> pending_;
    PendingAction pending_action_{PendingAction::refresh};
    app::SysExTransferSnapshot snapshot_;
    int idle_ticks_{};
};

} // namespace taureon::gui
