#pragma once

#include <QObject>

#include <memory>

class WmsWorker final : public QObject {
    Q_OBJECT

public:
    explicit WmsWorker(QObject* parent = nullptr);
    ~WmsWorker() override;

public slots:
    void start();
    void stop();
    void publishCount(int count);

signals:
    void initialized(qint64 durationMs, qulonglong workerThreadId);
    void countChanged(int count);
    void stopped(int callbacksAfterStop);
    void failed(const QString& message);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
