#include "TestSupport.hpp"

#include "app/ConnectionWorker.hpp"
#include "app/MonitorEventQueue.hpp"
#include "gui/MainWindow.hpp"
#include "gui/MidiMonitorModel.hpp"
#include "gui/ProfileMatchPanel.hpp"
#include "gui/SysExTransferPanel.hpp"
#include "gui/SysExManagerPanel.hpp"
#include <taureon/core/sysex/SysEx7.hpp>
#include "transports/fake/FakeMidiTransport.hpp"

#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QTableView>
#include <QTabWidget>
#include <QTabBar>
#include <QTimer>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

using namespace taureon;

namespace {

midi::MidiEndpointDescriptor endpoint(const midi::MidiDirection direction, std::string id) {
    midi::MidiRouteIdentity identity;
    identity.backend = midi::MidiBackend::windows_midi_services;
    identity.direction = direction;
    identity.native = midi::WmsRouteIdentity{std::move(id), 3};
    return {identity, direction == midi::MidiDirection::input ? "Fake RX" : "Fake TX",
            midi::MidiProtocol::midi1,
            {direction == midi::MidiDirection::input, direction == midi::MidiDirection::output,
             true, false},
            std::nullopt, std::string{"Test function block"}};
}

midi::MidiEndpointDescriptor winmm_endpoint(const midi::MidiDirection direction) {
    midi::MidiRouteIdentity identity;
    identity.backend = midi::MidiBackend::winmm;
    identity.direction = direction;
    identity.native = midi::WinmmRouteIdentity{
        direction == midi::MidiDirection::input ? "Fallback RX" : "Fallback TX", 1, 2, 3};
    return {identity, direction == midi::MidiDirection::input ? "Fallback RX" : "Fallback TX",
            midi::MidiProtocol::midi1,
            {direction == midi::MidiDirection::input, direction == midi::MidiDirection::output,
             true, false}, std::nullopt, std::nullopt};
}

template <typename Predicate>
bool process_until(Predicate&& predicate) {
    constexpr int iteration_limit = 100'000;
    for (int iteration = 0; iteration < iteration_limit; ++iteration) {
        if (predicate()) return true;
        QApplication::processEvents(QEventLoop::AllEvents);
        std::this_thread::yield();
    }
    return predicate();
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    return test::run([&] {
        const auto receive_endpoint = endpoint(midi::MidiDirection::input, "fake-rx-id");
        const auto transmit_endpoint = endpoint(midi::MidiDirection::output, "fake-tx-id");
        std::mutex transport_mutex;
        midi::FakeMidiTransport* transport_ptr = nullptr;
        auto profile_registry = std::make_shared<profiles::ProfileRegistry>();
        std::vector<profiles::ProfileLoadIssue> profile_issues;
        TAUREON_REQUIRE(profile_registry->load_directory(
            std::filesystem::path{TAUREON_SOURCE_DIR} / "resources" / "device_profiles",
            profile_issues));
        TAUREON_REQUIRE(profile_issues.empty());
        const auto* summit = profile_registry->find("novation.summit");
        TAUREON_REQUIRE(summit != nullptr);
        auto manual = *summit;
        manual.profile_id = "manual.other";
        manual.display_name = "Manual Other Profile";
        manual.recognition.sysex_fingerprints = {
            {"manual-other", 0, {0xF0, 0x7D, 0x55, 0x66}}};
        TAUREON_REQUIRE(profile_registry->register_profile(std::move(manual)));
        app::ConnectionWorker worker([&](const midi::MidiBackend backend) {
            TAUREON_REQUIRE(backend == midi::MidiBackend::windows_midi_services);
            auto transport = std::make_unique<midi::FakeMidiTransport>(
                backend, std::vector{receive_endpoint, transmit_endpoint});
            {
                std::scoped_lock lock(transport_mutex);
                transport_ptr = transport.get();
            }
            return transport;
        }, {}, profile_registry);
        app::MonitorEventQueue monitor_queue(32);
        gui::MainWindow window(monitor_queue, worker, profile_registry);
        // Offscreen font metrics differ from the native Windows platform; retain the
        // cross-platform guard against the original >1920-pixel minimum-size defect.
        TAUREON_REQUIRE(window.minimumSizeHint().width() <= 1920);
        TAUREON_REQUIRE(window.minimumSizeHint().height() <= 1080);
        window.show();

        auto* backend = window.findChild<QComboBox*>("backendSelector");
        auto* receive = window.findChild<QComboBox*>("receiveRouteSelector");
        auto* transmit = window.findChild<QComboBox*>("transmitRouteSelector");
        auto* connect = window.findChild<QPushButton*>("connectButton");
        TAUREON_REQUIRE(backend != nullptr);
        TAUREON_REQUIRE(window.windowTitle().contains("0.1.0-alpha.1"));
        TAUREON_REQUIRE(receive != nullptr);
        TAUREON_REQUIRE(transmit != nullptr);
        TAUREON_REQUIRE(connect != nullptr);
        int padded_midi_captions = 0;
        for (auto* caption : window.findChildren<QLabel*>("connectionCaption")) {
            if (caption->text() == "MIDI Input" || caption->text() == "MIDI Output") {
                TAUREON_REQUIRE(caption->contentsMargins().left() == 6);
                TAUREON_REQUIRE(caption->contentsMargins().right() == 6);
                ++padded_midi_captions;
            }
        }
        TAUREON_REQUIRE(padded_midi_captions == 2);

        TAUREON_REQUIRE(backend->currentIndex() == 0);
        TAUREON_REQUIRE(!connect->isEnabled());
        // Auto enumerates WMS first; the port fields never contain backend choices.
        TAUREON_REQUIRE(process_until([&] {
            return receive->count() == 2 && transmit->count() == 2 && receive->isEnabled() &&
                   backend->currentText() == "Auto (WMS)";
        }));
        TAUREON_REQUIRE(backend->currentIndex() == 0);
        TAUREON_REQUIRE(!receive->itemText(1).contains("Windows MIDI Services"));
        TAUREON_REQUIRE(receive->itemData(1, Qt::ToolTipRole).toString().contains("fake-rx-id"));
        TAUREON_REQUIRE(receive->itemText(1).contains("group 4"));
        TAUREON_REQUIRE(transmit->itemData(1, Qt::ToolTipRole).toString().contains("fake-tx-id"));

        const QString long_port = "M8U eX port 16 — very long descriptive hardware MIDI port name";
        receive->setItemText(1, long_port);
        receive->showPopup();
        TAUREON_REQUIRE(receive->view()->minimumWidth() >=
                        receive->fontMetrics().horizontalAdvance(long_port));
        receive->hidePopup();

        auto* monitor_table = window.findChild<QTableView*>("midiMonitorTable");
        auto* follow = window.findChild<QCheckBox*>("monitorFollowLatest");
        TAUREON_REQUIRE(monitor_table && follow && follow->isChecked());
        auto* proxy = dynamic_cast<QSortFilterProxyModel*>(monitor_table->model());
        TAUREON_REQUIRE(proxy != nullptr);
        auto* model = dynamic_cast<gui::MidiMonitorModel*>(proxy->sourceModel());
        TAUREON_REQUIRE(model != nullptr);
        const auto append_events = [model](int count) {
            std::vector<app::MonitorEvent> events;
            for (int index = 0; index < count; ++index) {
                events.push_back({static_cast<std::uint64_t>(index), midi::MidiDirection::input,
                    {midi::MidiBackend::winmm, midi::Midi1NativeMessage{{0x90, 60, 127}}, {}}});
            }
            model->append_batch(std::move(events));
        };
        append_events(100);
        auto* scroll = monitor_table->verticalScrollBar();
        TAUREON_REQUIRE(process_until([&] {
            return scroll->maximum() > 0 && scroll->value() == scroll->maximum();
        }));
        scroll->triggerAction(QAbstractSlider::SliderSingleStepSub);
        TAUREON_REQUIRE(!follow->isChecked());
        const int inspection_position = scroll->value();
        append_events(10);
        QApplication::processEvents(QEventLoop::AllEvents);
        TAUREON_REQUIRE(scroll->value() == inspection_position);
        follow->setChecked(true);
        TAUREON_REQUIRE(process_until([&] { return scroll->value() == scroll->maximum(); }));
        model->set_history_limit(50);
        append_events(100); // A history replacement resets the model; following must survive it.
        TAUREON_REQUIRE(process_until([&] {
            return model->rowCount() == 50 && scroll->value() == scroll->maximum();
        }));
        auto* filter_panel = window.findChild<QGroupBox*>("monitorFilterPanel");
        auto* notes = window.findChild<QCheckBox*>("monitorEventType_0");
        auto* clock = window.findChild<QCheckBox*>("monitorEventType_6");
        auto* receive_events = window.findChild<QCheckBox*>("monitorReceiveEvents");
        auto* transmit_events = window.findChild<QCheckBox*>("monitorTransmitEvents");
        TAUREON_REQUIRE(filter_panel && filter_panel->isVisible());
        TAUREON_REQUIRE(notes && clock && notes->isChecked() && clock->isChecked());
        TAUREON_REQUIRE(receive_events && transmit_events &&
                        receive_events->isChecked() && transmit_events->isChecked());
        for (auto* selector : filter_panel->findChildren<QComboBox*>()) {
            TAUREON_REQUIRE(selector->accessibleName() != "Monitor backend filter");
        }
        notes->setChecked(false);
        TAUREON_REQUIRE(proxy->rowCount() == 0 && model->rowCount() == 50);
        model->append_batch({{101, midi::MidiDirection::input,
                              {midi::MidiBackend::winmm, midi::Midi1NativeMessage{{0xF8}}, {}}}});
        TAUREON_REQUIRE(proxy->rowCount() == 1);
        clock->setChecked(false);
        TAUREON_REQUIRE(proxy->rowCount() == 0);
        notes->setChecked(true);
        clock->setChecked(true);
        TAUREON_REQUIRE(proxy->rowCount() == model->rowCount());
        receive_events->setChecked(false);
        TAUREON_REQUIRE(proxy->rowCount() == 0);
        receive_events->setChecked(true);
        TAUREON_REQUIRE(proxy->rowCount() == model->rowCount());

        // An explicit backend choice remains in the first selector only.
        backend->setCurrentIndex(1);
        TAUREON_REQUIRE(process_until([&] {
            return backend->currentIndex() == 1 && receive->count() == 2 && receive->isEnabled();
        }));
        transmit->setCurrentIndex(1);
        TAUREON_REQUIRE(backend->currentIndex() == 1);

        transmit->setCurrentIndex(0);
        receive->setCurrentIndex(1);
        TAUREON_REQUIRE(connect->isEnabled());
        connect->click();
        TAUREON_REQUIRE(process_until([&] { return connect->text() == "Disconnect"; }));
        TAUREON_REQUIRE(transmit->currentIndex() == 0);
        window.statusBar()->showMessage("MIDI operation failed: retained test message");
        QElapsedTimer status_check;
        status_check.start();
        while (status_check.elapsed() < 650) {
            QApplication::processEvents(QEventLoop::AllEvents);
            std::this_thread::yield();
        }
        TAUREON_REQUIRE(window.statusBar()->currentMessage() ==
                        "MIDI operation failed: retained test message");

        auto* transfer_panel = dynamic_cast<gui::SysExTransferPanel*>(
            window.findChild<QWidget*>("sysExTransferPanel"));
        auto* receive_sysex = window.findChild<QPushButton*>("sysExReceive");
        auto* open_sysex = window.findChild<QPushButton*>("sysExOpen");
        auto* clear_sysex = window.findChild<QPushButton*>("sysExClear");
        auto* transfer_status = window.findChild<QLabel*>("sysExStatus");
        auto* raw_send = window.findChild<QPushButton*>("sysExRawSend");
        auto* validated_restore = window.findChild<QPushButton*>("sysExValidatedRestore");
        auto* frame_table = window.findChild<QTableView*>("sysExFrameTable");
        auto* route_label = window.findChild<QLabel*>("sysExTxRoute");
        auto* profile_label = window.findChild<QLabel*>("sysExProfileMatch");
        TAUREON_REQUIRE(transfer_panel != nullptr);
        TAUREON_REQUIRE(transfer_panel->has_required_controls());
        TAUREON_REQUIRE(receive_sysex != nullptr);
        TAUREON_REQUIRE(open_sysex != nullptr && clear_sysex != nullptr &&
                        transfer_status != nullptr);
        TAUREON_REQUIRE(raw_send != nullptr);
        TAUREON_REQUIRE(validated_restore != nullptr);
        TAUREON_REQUIRE(frame_table != nullptr);
        auto* work_splitter = window.findChild<QSplitter*>("sysExWorkSplitter");
        auto* inspector_tabs = window.findChild<QTabWidget*>("sysExInspectorTabs");
        TAUREON_REQUIRE(work_splitter != nullptr && inspector_tabs != nullptr);
        TAUREON_REQUIRE(work_splitter->count() == 2 && inspector_tabs->count() == 2);
        TAUREON_REQUIRE(inspector_tabs->tabText(0) == "Raw bytes");
        TAUREON_REQUIRE(inspector_tabs->tabText(1) == "Transfer log");
        TAUREON_REQUIRE(window.findChild<QWidget*>("sysExTransferScrollArea") == nullptr);
        {
            gui::SysExTransferPanel compact_transfer(worker);
            compact_transfer.resize(700, 430);
            compact_transfer.show();
            QApplication::processEvents(QEventLoop::AllEvents);
            auto* compact_status = compact_transfer.findChild<QLabel*>("sysExStatus");
            auto* compact_table = compact_transfer.findChild<QTableView*>("sysExFrameTable");
            auto* compact_inspector = compact_transfer.findChild<QTabWidget*>("sysExInspectorTabs");
            TAUREON_REQUIRE(compact_status != nullptr && compact_table != nullptr &&
                            compact_inspector != nullptr);
            TAUREON_REQUIRE(compact_transfer.width() == 700 && compact_transfer.height() == 430);
            TAUREON_REQUIRE(compact_status->isVisible() &&
                            compact_status->geometry().bottom() < compact_transfer.height());
            TAUREON_REQUIRE(compact_table->height() > 40 && compact_inspector->height() > 40);
        }
        TAUREON_REQUIRE(route_label != nullptr);
        TAUREON_REQUIRE(profile_label != nullptr);
        TAUREON_REQUIRE(!validated_restore->isEnabled());
        int background_snapshots = 0;
        QObject::connect(frame_table->model(), &QAbstractItemModel::modelReset, &window,
                         [&] { ++background_snapshots; });
        QElapsedTimer refresh_check;
        refresh_check.start();
        while (refresh_check.elapsed() < 650) {
            QApplication::processEvents(QEventLoop::AllEvents);
            TAUREON_REQUIRE(open_sysex->isEnabled());
            TAUREON_REQUIRE(receive_sysex->isEnabled());
            TAUREON_REQUIRE(clear_sysex->isEnabled());
            TAUREON_REQUIRE(!transfer_status->text().contains("Refreshing transfer state"));
            std::this_thread::yield();
        }
        TAUREON_REQUIRE(background_snapshots == 0);
        auto* manager_panel = dynamic_cast<gui::SysExManagerPanel*>(
            window.findChild<QWidget*>("sysExManagerPanel"));
        TAUREON_REQUIRE(manager_panel != nullptr);
        TAUREON_REQUIRE(manager_panel->has_required_controls());
        const std::filesystem::path manager_fixture{TAUREON_TEST_FIXTURE_PATH};
        manager_panel->add_file(manager_fixture);
        auto* manager_open = window.findChild<QPushButton*>("sysExManagerOpenTransfer");
        auto* manager_status = window.findChild<QLabel*>("sysExManagerStatus");
        auto* navigation = window.findChild<QTabBar*>("workspaceNavigation");
        auto* source_label = window.findChild<QLabel*>("sysExSource");
        TAUREON_REQUIRE(manager_open != nullptr && manager_open->isEnabled());
        TAUREON_REQUIRE(manager_status != nullptr);
        TAUREON_REQUIRE(navigation != nullptr);
        TAUREON_REQUIRE(!navigation->isTabVisible(2));
        TAUREON_REQUIRE(source_label != nullptr);
        manager_open->click();
        TAUREON_REQUIRE(process_until([&] {
            return frame_table->model()->rowCount() == 1 && navigation->currentIndex() == 1;
        }));
        auto* raw_bytes = window.findChild<QPlainTextEdit*>("sysExRawBytes");
        TAUREON_REQUIRE(raw_bytes != nullptr);
        frame_table->selectRow(0);
        const auto inspected_bytes = raw_bytes->toPlainText();
        TAUREON_REQUIRE(!inspected_bytes.isEmpty());
        const int model_resets_after_load = background_snapshots;
        refresh_check.restart();
        while (refresh_check.elapsed() < 650) {
            QApplication::processEvents(QEventLoop::AllEvents);
            std::this_thread::yield();
        }
        TAUREON_REQUIRE(background_snapshots == model_resets_after_load);
        TAUREON_REQUIRE(frame_table->currentIndex().row() == 0);
        TAUREON_REQUIRE(raw_bytes->toPlainText() == inspected_bytes);
        {
            std::scoped_lock lock(transport_mutex);
            TAUREON_REQUIRE(transport_ptr->diagnostics().transmitted_messages == 0);
        }

        receive_sysex->click();
        TAUREON_REQUIRE(process_until([&] {
            return receive_sysex->text() == "Stop Receive" &&
                   frame_table->model()->rowCount() == 0;
        }));
        TAUREON_REQUIRE(source_label->text() == "Live capture");
        navigation->setCurrentIndex(2);
        manager_open->click();
        TAUREON_REQUIRE(process_until([&] {
            return manager_status->text().contains("rejected") && navigation->currentIndex() == 2;
        }));
        TAUREON_REQUIRE(frame_table->model()->rowCount() == 0);
        TAUREON_REQUIRE(source_label->text() == "Live capture");
        TAUREON_REQUIRE(!raw_send->isEnabled());
        {
            std::scoped_lock lock(transport_mutex);
            TAUREON_REQUIRE(transport_ptr->diagnostics().transmitted_messages == 0);
        }
        navigation->setCurrentIndex(1);
        const sysex::SysExFrame received_frame{sysex::SysExFrameStatus::complete,
                                               {0xF0, 0x7D, 0x22, 0xF7}, {},
                                               std::optional<std::uint8_t>{3}, false};
        const auto encoded = sysex::encode_sysex7(received_frame, 3);
        TAUREON_REQUIRE(encoded);
        midi::UmpNativeMessage ump;
        for (const auto& packet : encoded.value()) {
            ump.words.push_back(packet.word0);
            ump.words.push_back(packet.word1);
        }
        {
            std::scoped_lock lock(transport_mutex);
            transport_ptr->emit_received(
                {midi::MidiBackend::windows_midi_services, ump, std::nullopt});
        }
        receive_sysex->click();
        TAUREON_REQUIRE(process_until([&] {
            return receive_sysex->text() == "Receive" && frame_table->model()->rowCount() == 1;
        }));

        connect->click();
        TAUREON_REQUIRE(process_until([&] { return connect->text() == "Connect"; }));

        // Auto falls back before port selection if WMS cannot initialize.
        app::ConnectionWorker fallback_worker([](midi::MidiBackend selected_backend)
            -> std::unique_ptr<midi::IMidiTransport> {
            if (selected_backend == midi::MidiBackend::windows_midi_services) {
                throw std::runtime_error("WMS preview unavailable");
            }
            return std::make_unique<midi::FakeMidiTransport>(selected_backend,
                std::vector{winmm_endpoint(midi::MidiDirection::input),
                            winmm_endpoint(midi::MidiDirection::output)});
        });
        app::MonitorEventQueue fallback_queue(32);
        gui::MainWindow fallback_window(fallback_queue, fallback_worker);
        auto* fallback_backend = fallback_window.findChild<QComboBox*>("backendSelector");
        auto* fallback_input = fallback_window.findChild<QComboBox*>("receiveRouteSelector");
        auto* fallback_output = fallback_window.findChild<QComboBox*>("transmitRouteSelector");
        auto* fallback_connect = fallback_window.findChild<QPushButton*>("connectButton");
        TAUREON_REQUIRE(process_until([&] {
            return fallback_backend->currentText() == "Auto (WinMM)" &&
                   fallback_input->count() == 2 && fallback_output->count() == 2 &&
                   fallback_input->isEnabled();
        }));
        TAUREON_REQUIRE(fallback_backend->currentIndex() == 0);
        TAUREON_REQUIRE(fallback_backend->toolTip().contains("WMS preview unavailable"));
        TAUREON_REQUIRE(!fallback_connect->isEnabled());
        fallback_input->setCurrentIndex(1);
        TAUREON_REQUIRE(fallback_backend->currentIndex() == 0 && fallback_connect->isEnabled());

        // An available WMS backend with no routes also uses the WinMM fallback.
        app::ConnectionWorker empty_auto_worker([](midi::MidiBackend selected_backend) {
            return std::make_unique<midi::FakeMidiTransport>(selected_backend,
                selected_backend == midi::MidiBackend::winmm ?
                    std::vector{winmm_endpoint(midi::MidiDirection::input)} :
                    std::vector<midi::MidiEndpointDescriptor>{});
        });
        app::MonitorEventQueue empty_auto_queue(32);
        gui::MainWindow empty_auto_window(empty_auto_queue, empty_auto_worker);
        auto* empty_auto_backend = empty_auto_window.findChild<QComboBox*>("backendSelector");
        auto* empty_auto_input = empty_auto_window.findChild<QComboBox*>("receiveRouteSelector");
        TAUREON_REQUIRE(process_until([&] {
            return empty_auto_backend->currentText() == "Auto (WinMM)" &&
                   empty_auto_input->count() == 2 && empty_auto_input->isEnabled();
        }));
        TAUREON_REQUIRE(empty_auto_backend->toolTip().contains("no routes found"));

        // A failed backend must remain visibly failed, with no empty selectable port list or
        // background snapshot overwriting the initialization error.
        app::ConnectionWorker unavailable_worker([](midi::MidiBackend)
            -> std::unique_ptr<midi::IMidiTransport> {
            throw std::runtime_error("WMS SDK runtime test unavailable");
        });
        app::MonitorEventQueue unavailable_queue(32);
        gui::MainWindow unavailable_window(unavailable_queue, unavailable_worker);
        auto* unavailable_backend = unavailable_window.findChild<QComboBox*>("backendSelector");
        auto* unavailable_input = unavailable_window.findChild<QComboBox*>("receiveRouteSelector");
        TAUREON_REQUIRE(process_until([&] {
            return unavailable_window.statusBar()->currentMessage().contains("Auto failed:") &&
                   unavailable_window.statusBar()->currentMessage().contains("WinMM");
        }));
        unavailable_backend->setCurrentIndex(1);
        TAUREON_REQUIRE(process_until([&] {
            return unavailable_window.statusBar()->currentMessage().contains("MIDI operation failed:") &&
                   unavailable_window.statusBar()->currentMessage().contains("WMS SDK runtime test unavailable");
        }));
        TAUREON_REQUIRE(!unavailable_input->isEnabled());
        status_check.restart();
        while (status_check.elapsed() < 650) {
            QApplication::processEvents(QEventLoop::AllEvents);
            std::this_thread::yield();
        }
        TAUREON_REQUIRE(unavailable_window.statusBar()->currentMessage().contains("WMS SDK runtime test unavailable"));

        app::ConnectionWorker empty_worker([](midi::MidiBackend selected_backend) {
            return std::make_unique<midi::FakeMidiTransport>(selected_backend);
        });
        app::MonitorEventQueue empty_queue(32);
        gui::MainWindow empty_window(empty_queue, empty_worker);
        empty_window.findChild<QComboBox*>("backendSelector")->setCurrentIndex(1);
        TAUREON_REQUIRE(process_until([&] {
            return empty_window.statusBar()->currentMessage().contains("no MIDI routes found");
        }));

        receive->setCurrentIndex(0);
        transmit->setCurrentIndex(1);
        connect->click();
        TAUREON_REQUIRE(process_until([&] { return connect->text() == "Disconnect"; }));
        TAUREON_REQUIRE(receive->currentIndex() == 0);

        const std::filesystem::path fixture{TAUREON_TEST_FIXTURE_PATH};
        transfer_panel->request_load(fixture);
        TAUREON_REQUIRE(process_until([&] {
            return frame_table->model()->rowCount() == 1 && raw_send->isEnabled();
        }));
        {
            std::scoped_lock lock(transport_mutex);
            TAUREON_REQUIRE(transport_ptr->diagnostics().transmitted_messages == 0);
        }
        TAUREON_REQUIRE(route_label->text().contains("fake-tx-id"));
        TAUREON_REQUIRE(route_label->text().contains("group 4"));
        TAUREON_REQUIRE(profile_label->text().contains("Novation Summit"));
        TAUREON_REQUIRE(profile_label->text().contains("confident suggestion"));
        TAUREON_REQUIRE(profile_label->text().contains("not declared"));

        auto* profile_panel = dynamic_cast<gui::ProfileMatchPanel*>(
            window.findChild<QWidget*>("profileMatchPanel"));
        auto* temporary_profile = window.findChild<QComboBox*>("profileTemporarySelector");
        auto* use_temporary = window.findChild<QPushButton*>("profileUseTemporarily");
        auto* remember_binding = window.findChild<QPushButton*>("profileRememberBinding");
        auto* selected_profile = window.findChild<QLabel*>("profileSelectedProfile");
        TAUREON_REQUIRE(profile_panel != nullptr);
        TAUREON_REQUIRE(temporary_profile != nullptr);
        TAUREON_REQUIRE(use_temporary != nullptr);
        TAUREON_REQUIRE(remember_binding != nullptr);
        TAUREON_REQUIRE(selected_profile != nullptr);
        const auto temporary_index = temporary_profile->findData(QStringLiteral("manual.other"));
        TAUREON_REQUIRE(temporary_index >= 0);
        temporary_profile->setCurrentIndex(temporary_index);
        TAUREON_REQUIRE(use_temporary->isEnabled());
        use_temporary->click();
        TAUREON_REQUIRE(process_until([&] {
            return profile_panel->override_is_visible() &&
                   profile_panel->remember_binding_is_enabled();
        }));
        TAUREON_REQUIRE(selected_profile->text().contains("novation.summit"));
        {
            std::scoped_lock lock(transport_mutex);
            TAUREON_REQUIRE(transport_ptr->diagnostics().transmitted_messages == 0);
        }
        remember_binding->click();
        TAUREON_REQUIRE(process_until([&] {
            return !profile_panel->override_is_visible() &&
                   selected_profile->text().contains("manual.other");
        }));
        {
            std::scoped_lock lock(transport_mutex);
            TAUREON_REQUIRE(transport_ptr->diagnostics().transmitted_messages == 0);
        }
        bool confirmed_exact_route = false;
        QTimer::singleShot(0, &window, [&] {
            auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (!dialog) return;
            confirmed_exact_route = dialog->text().contains("fake-tx-id") &&
                                    dialog->text().contains("1 complete SysEx frame");
            for (auto* button : dialog->buttons()) {
                if (button->text() == "Send raw data") {
                    button->click();
                    break;
                }
            }
        });
        raw_send->click();
        TAUREON_REQUIRE(confirmed_exact_route);
        TAUREON_REQUIRE(process_until([&] {
            std::scoped_lock lock(transport_mutex);
            return transport_ptr->diagnostics().transmitted_messages == 1;
        }));

        {
            std::scoped_lock lock(transport_mutex);
            TAUREON_REQUIRE(transport_ptr != nullptr);
            transport_ptr->remove_endpoint(transmit_endpoint.identity);
        }
        // The connection snapshot is polled on a 250 ms timer. On a fast CI runner,
        // an iteration-only wait can finish before that timer fires even once.
        QElapsedTimer degraded_wait;
        degraded_wait.start();
        while (!window.statusBar()->currentMessage().startsWith("Degraded:") &&
               degraded_wait.elapsed() < 3000) {
            QApplication::processEvents(QEventLoop::AllEvents);
            std::this_thread::yield();
        }
        TAUREON_REQUIRE(window.statusBar()->currentMessage().startsWith("Degraded:"));
        TAUREON_REQUIRE(connect->text() == "Disconnect");

        connect->click();
        TAUREON_REQUIRE(process_until([&] { return connect->text() == "Connect"; }));

        // A failed connect (here: the selected output vanished) must keep the route
        // selectors and Connect usable for a retry without re-selecting the backend.
        TAUREON_REQUIRE(transmit->currentIndex() == 1);
        connect->click();
        TAUREON_REQUIRE(process_until([&] {
            return window.statusBar()->currentMessage().startsWith("MIDI operation failed:");
        }));
        TAUREON_REQUIRE(connect->text() == "Connect");
        TAUREON_REQUIRE(backend->isEnabled());
        TAUREON_REQUIRE(receive->isEnabled() && transmit->isEnabled());
        TAUREON_REQUIRE(connect->isEnabled());
        receive->setCurrentIndex(1);
        transmit->setCurrentIndex(0);
        TAUREON_REQUIRE(connect->isEnabled());
    });
}
