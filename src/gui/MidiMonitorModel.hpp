#pragma once

#include "app/MonitorEventQueue.hpp"

#include <QAbstractTableModel>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace taureon::gui {

enum class MonitorEventCategory : std::uint8_t {
    notes, controllers, program_change, pitch_bend, aftertouch,
    sysex, clock, active_sensing, other, count
};

class MidiMonitorModel final : public QAbstractTableModel {
public:
    enum Column { Time, Direction, Route, Channel, Type, Event, Value, Raw, ColumnCount };
    enum Role { CategoryRole = Qt::UserRole + 1 };

    explicit MidiMonitorModel(std::size_t history_limit, QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                      int role) const override;
    void append_batch(std::vector<app::MonitorEvent> events);
    void clear();
    void set_history_limit(std::size_t history_limit);
    [[nodiscard]] std::size_t history_limit() const noexcept { return history_limit_; }

private:
    std::size_t history_limit_;
    std::deque<app::MonitorEvent> events_;
};

} // namespace taureon::gui
