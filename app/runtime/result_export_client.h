// 文件作用：定义当前进程二维码结果传输客户端及其本地运行时队列。
#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QByteArray>
#include <QList>
#include <QThread>

#include <mutex>

enum class ResultExportConnectionState
{
    Disconnected,
    Connecting,
    Connected,
    Checking
};

Q_DECLARE_METATYPE(ResultExportConnectionState)

struct ResultExportRunConfiguration
{
    bool enabled = false;
};

struct ResultExportRecord
{
    QString id;
    QDateTime eventTimeUtc;
    bool overallOk = false;
    QString qrContent;
};

class ResultExportNetworkWorker;

class ResultExportClient : public QObject
{
    Q_OBJECT

public:
    explicit ResultExportClient(QObject *parent = nullptr);
    ~ResultExportClient() override;

    int pendingCount() const;

    void enqueue(const ResultExportRecord &record);

    void requestConnect(const QString &ip, quint16 port);
    void requestConnectionCheck();
    void requestDisconnect();

    ResultExportConnectionState connectionState() const;
    double roundTripMs() const;

signals:
    void connectionStateChanged(ResultExportConnectionState state);
    void transportFailure(QString diagnostic);

private:
    void ensureWorker();
    QByteArray serializeRecord(const ResultExportRecord &record) const;
    void sendHeadIfPossible();
    void handleWorkerConnected();
    void handleWorkerDisconnected(QString reason);
    void handleWorkerAck();
    void handleWorkerFailure(QString reason);
    void handleWorkerPong(double roundTripMs);
    void setConnectionState(ResultExportConnectionState state);

    mutable std::mutex m_mutex;
    QList<ResultExportRecord> m_pendingRecords;
    ResultExportConnectionState m_state = ResultExportConnectionState::Disconnected;
    double m_roundTripMs = -1.0;
    bool m_sendInFlight = false;
    QThread *m_networkThread = nullptr;
    ResultExportNetworkWorker *m_worker = nullptr;
};
