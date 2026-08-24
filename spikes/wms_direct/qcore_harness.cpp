#include "wms_worker.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>

#include <iostream>
#include <string>

class CoreHarness final {
public:
    CoreHarness(QCoreApplication& app, const bool auto_close_active)
        : app_(app), auto_close_active_(auto_close_active) {
        worker_ = new WmsWorker;
        worker_->moveToThread(&worker_thread_);
        QObject::connect(&worker_thread_, &QThread::finished, worker_, &QObject::deleteLater);
        QObject::connect(worker_, &WmsWorker::countChanged, &app_, [this](const int count) {
            callback_count_ = count;
        });
        QObject::connect(worker_, &WmsWorker::initialized, &app_,
                         [this](qint64, const qulonglong worker_thread_id) {
            active_ = true;
            const auto core_thread_id = reinterpret_cast<qulonglong>(QThread::currentThreadId());
            std::cout << "{\"event\":\"qt_thread_boundary\",\"core_thread_id\":"
                      << core_thread_id << ",\"worker_thread_id\":" << worker_thread_id
                      << ",\"distinct\":"
                      << (core_thread_id != worker_thread_id ? "true" : "false") << "}"
                      << std::endl;
        });
        QObject::connect(worker_, &WmsWorker::stopped, &app_, [this](const int after_stop) {
            active_ = false;
            callbacks_after_stop_ = after_stop;
        });
        QObject::connect(worker_, &WmsWorker::failed, &app_, [](const QString& message) {
            std::cerr << "{\"event\":\"qt_worker_error\",\"message\":\""
                      << message.toStdString() << "\"}" << std::endl;
        });
        worker_thread_.start();

        core_tick_.setInterval(5);
        QObject::connect(&core_tick_, &QTimer::timeout, &app_, [this] { ++core_ticks_; });
        core_tick_.start();
    }

    ~CoreHarness() {
        const bool close_while_active = active_ || auto_close_active_;
        stopWorker();
        worker_thread_.quit();
        worker_thread_.wait();
        std::cout << "{\"event\":\"qt_shutdown\",\"qcoreapplication\":true,"
                  << "\"widgets_dependency\":false,\"close_while_worker_objects_exist\":"
                  << (close_while_active ? "true" : "false") << ",\"core_ticks\":"
                  << core_ticks_ << ",\"callback_count\":" << callback_count_
                  << ",\"callbacks_after_stop\":" << callbacks_after_stop_
                  << ",\"worker_joined\":true}" << std::endl;
    }

    void startWorker() {
        QElapsedTimer dispatch;
        dispatch.start();
        QMetaObject::invokeMethod(worker_, "start", Qt::QueuedConnection);
        const auto dispatch_us = dispatch.nsecsElapsed() / 1000;
        std::cout << "{\"event\":\"qt_start_dispatch\",\"core_dispatch_us\":"
                  << dispatch_us << ",\"non_blocking\":true}" << std::endl;
    }

    void stopWorker() {
        if (worker_thread_.isRunning() && worker_) {
            QMetaObject::invokeMethod(worker_, "stop", Qt::BlockingQueuedConnection);
        }
    }

private:
    QCoreApplication& app_;
    QThread worker_thread_;
    WmsWorker* worker_{};
    QTimer core_tick_;
    int core_ticks_{};
    int callback_count_{};
    int callbacks_after_stop_{};
    bool active_{};
    bool auto_close_active_{};
};

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    bool auto_test = false;
    bool auto_close_active = false;
    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        auto_test = auto_test || arg == "--auto-test";
        auto_close_active = auto_close_active || arg == "--auto-close-active";
    }

    CoreHarness harness(app, auto_close_active);
    QTimer::singleShot(0, &app, [&harness] { harness.startWorker(); });
    if (auto_close_active) {
        QTimer::singleShot(350, &app, &QCoreApplication::quit);
    } else if (auto_test) {
        QTimer::singleShot(300, &app, [&harness] { harness.stopWorker(); });
        QTimer::singleShot(450, &app, &QCoreApplication::quit);
    }
    return app.exec();
}
