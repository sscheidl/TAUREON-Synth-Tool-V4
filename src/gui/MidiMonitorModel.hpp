#pragma once

#include "app/MonitorEventQueue.hpp"

#include <QAbstractTableModel>

#include <cstddef>
#include <deque>
#include <vector>

namespace taureon::gui {

class MidiMonitorModel final : public QAbstractTableModel {
public:
    enum Column { Time, Direction, Route, Group, Channel, Type, Event, Value, Raw, ColumnCount };

    explicit MidiMonitorModel(std::size_t history_limit, QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                      int role) const override;
    void append_batch(std::vector<app::MonitorEvent> events);
    void clear();
    [[nodiscard]] std::size_t history_limit() const noexcept { return history_limit_; }

private:
    const std::size_t history_limit_;
    std::deque<app::MonitorEvent> events_;
};

} // namespace taureon::gui
