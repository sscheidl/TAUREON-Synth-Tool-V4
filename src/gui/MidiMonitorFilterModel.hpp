#pragma once

#include <QSortFilterProxyModel>

#include <optional>

namespace taureon::gui {

class MidiMonitorFilterModel final : public QSortFilterProxyModel {
public:
    explicit MidiMonitorFilterModel(QObject* parent = nullptr);

    void set_direction(QString direction);
    void set_type_filter(QString type_filter);

protected:
    [[nodiscard]] bool filterAcceptsRow(int source_row,
                                        const QModelIndex& source_parent) const override;

private:
    QString direction_;
    QString type_filter_;
};

} // namespace taureon::gui
