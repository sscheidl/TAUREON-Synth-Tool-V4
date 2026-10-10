#include "gui/MidiMonitorFilterModel.hpp"

#include "gui/MidiMonitorModel.hpp"

#include <initializer_list>
#include <utility>

namespace taureon::gui {

MidiMonitorFilterModel::MidiMonitorFilterModel(QObject* parent) : QSortFilterProxyModel(parent) {
    setDynamicSortFilter(true);
}

void MidiMonitorFilterModel::set_direction(QString direction) {
    beginFilterChange();
    direction_ = std::move(direction);
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

void MidiMonitorFilterModel::set_backend(QString backend) {
    beginFilterChange();
    backend_ = std::move(backend);
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

void MidiMonitorFilterModel::set_channel(const int channel) {
    beginFilterChange();
    channel_ = channel;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

void MidiMonitorFilterModel::set_category_enabled(const MonitorEventCategory category,
                                                  const bool enabled) {
    const auto bit = 1u << static_cast<unsigned>(category);
    beginFilterChange();
    if (enabled) category_mask_ |= bit;
    else category_mask_ &= ~bit;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

bool MidiMonitorFilterModel::category_enabled(const MonitorEventCategory category) const noexcept {
    return (category_mask_ & (1u << static_cast<unsigned>(category))) != 0;
}

void MidiMonitorFilterModel::set_type_filter(QString type_filter) {
    beginFilterChange();
    type_filter_ = std::move(type_filter);
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

bool MidiMonitorFilterModel::filterAcceptsRow(const int source_row,
                                               const QModelIndex& source_parent) const {
    if (!sourceModel()) return false;
    const auto direction = sourceModel()->index(source_row, MidiMonitorModel::Direction, source_parent)
                               .data(Qt::DisplayRole).toString();
    if (!direction_.isEmpty() && direction != direction_) return false;
    const auto backend = sourceModel()->index(source_row, MidiMonitorModel::Backend, source_parent)
                           .data(Qt::DisplayRole).toString();
    if (!backend_.isEmpty() && backend != backend_) return false;
    const auto channel = sourceModel()->index(source_row, MidiMonitorModel::Channel, source_parent)
                             .data(Qt::DisplayRole).toInt();
    if (channel_ != 0 && channel != channel_) return false;
    const auto category = sourceModel()->index(source_row, MidiMonitorModel::Type, source_parent)
                              .data(MidiMonitorModel::CategoryRole).toInt();
    if ((category_mask_ & (1u << static_cast<unsigned>(category))) == 0) return false;
    if (type_filter_.isEmpty()) return true;
    for (const auto column : {MidiMonitorModel::Type, MidiMonitorModel::Event, MidiMonitorModel::Value}) {
        if (sourceModel()->index(source_row, column, source_parent)
                .data(Qt::DisplayRole).toString().contains(type_filter_, Qt::CaseInsensitive)) return true;
    }
    return false;
}

} // namespace taureon::gui
