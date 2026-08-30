#include "TestSupport.hpp"

#include "app/Librarian.hpp"
#include "gui/LibrarianPanel.hpp"

#include <QApplication>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QTableView>

#include <memory>
#include <utility>

using namespace taureon;

namespace {

class InMemoryLibrarianProvider final : public app::ILibrarianProvider {
public:
    explicit InMemoryLibrarianProvider(app::LibrarianSnapshot snapshot)
        : snapshot_(std::move(snapshot)) {}

    [[nodiscard]] app::LibrarianSnapshot snapshot() const override { return snapshot_; }

private:
    app::LibrarianSnapshot snapshot_;
};

app::LibrarianSnapshot test_snapshot() {
    return {
        true,
        {},
        {{
            "collection.test", "Test collection",
            {
                {
                    "bank.alpha", "Alpha", app::LibrarianCapacity::known(3),
                    {
                        {"slot.alpha.1", "One", false,
                         app::LibrarianSemanticObject{"object.1", "Synthetic object", "Synthetic"}, {}},
                        {"slot.alpha.2", "Two", false, std::nullopt, {}},
                        {"slot.alpha.3", "Three", true, std::nullopt, "This slot is read-only."},
                    },
                    {true, false, false, false, "Rename is unavailable in the bounded foundation."},
                },
                {
                    "bank.empty", "Empty", app::LibrarianCapacity::known(0), {},
                    {true, false, false, false, "No semantic write operation is available."},
                },
                {
                    "bank.unknown", "Unknown", app::LibrarianCapacity::unknown(),
                    {
                        {"slot.unknown.1", "First observed", false, std::nullopt, {}},
                        {"slot.unknown.2", "Second observed", false, std::nullopt, {}},
                    },
                    {true, false, false, false, "Provider capacity is unknown."},
                },
            },
        }},
    };
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    return test::run([&] {
        const auto snapshot = test_snapshot();
        TAUREON_REQUIRE(app::librarian_snapshot_is_well_formed(snapshot));
        TAUREON_REQUIRE(snapshot.collections.size() == 1);
        TAUREON_REQUIRE(snapshot.collections.at(0).banks.size() == 3);
        TAUREON_REQUIRE(snapshot.collections.at(0).banks.at(0).slots.size() == 3);
        TAUREON_REQUIRE(snapshot.collections.at(0).banks.at(1).slots.empty());
        TAUREON_REQUIRE(snapshot.collections.at(0).banks.at(2).capacity.kind ==
                        app::LibrarianCapacityKind::unknown);

        auto provider = std::make_shared<InMemoryLibrarianProvider>(snapshot);
        gui::LibrarianPanel panel;
        panel.set_provider(provider);
        panel.show();
        QApplication::processEvents();

        auto* table = panel.findChild<QTableView*>("librarianSlotsTable");
        auto* capacity = panel.findChild<QLabel*>("librarianCapacityState");
        auto* support = panel.findChild<QLabel*>("librarianSupportState");
        auto* rename = panel.findChild<QPushButton*>("librarianRenameAction");
        TAUREON_REQUIRE(panel.has_required_controls());
        TAUREON_REQUIRE(table != nullptr && table->model()->rowCount() == 3);
        TAUREON_REQUIRE(table->model()->columnCount() == 4);
        TAUREON_REQUIRE(table->model()->index(-1, 0).data().isNull());
        TAUREON_REQUIRE(table->model()->index(0, 1).data().toString() == "Occupied");
        TAUREON_REQUIRE(table->model()->index(1, 1).data().toString() == "Empty");
        TAUREON_REQUIRE(table->model()->index(2, 3).data().toString() == "Read-only");
        TAUREON_REQUIRE(capacity != nullptr && capacity->text().contains("Known capacity: 3"));
        TAUREON_REQUIRE(support != nullptr && support->text().contains("available"));
        TAUREON_REQUIRE(rename != nullptr && !rename->isEnabled());

        const auto first = table->model()->index(0, 0);
        table->selectionModel()->setCurrentIndex(
            first, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        table->setFocus();
        QKeyEvent down(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
        QApplication::sendEvent(table, &down);
        QApplication::processEvents();
        TAUREON_REQUIRE(table->selectionModel()->currentIndex().row() == 1);

        QKeyEvent range(QEvent::KeyPress, Qt::Key_Down, Qt::ShiftModifier);
        QApplication::sendEvent(table, &range);
        QApplication::processEvents();
        TAUREON_REQUIRE(table->selectionModel()->selectedRows().size() >= 2);

        auto unavailable = std::make_shared<InMemoryLibrarianProvider>(
            app::LibrarianSnapshot{false,
                                   "Semantic Librarian support is unavailable for this profile.", {}});
        panel.set_provider(unavailable);
        QApplication::processEvents();
        TAUREON_REQUIRE(table->model()->rowCount() == 0);
        TAUREON_REQUIRE(support->text().contains("unavailable"));
        TAUREON_REQUIRE(!rename->isEnabled());

        // A valid, profile-recognized raw SysEx frame has no semantic provider in production.
        gui::LibrarianPanel production_panel;
        auto* production_table = production_panel.findChild<QTableView*>("librarianSlotsTable");
        auto* production_support = production_panel.findChild<QLabel*>("librarianSupportState");
        TAUREON_REQUIRE(production_table != nullptr && production_table->model()->rowCount() == 0);
        TAUREON_REQUIRE(production_support != nullptr &&
                        production_support->text().contains("unavailable"));
    });
}
