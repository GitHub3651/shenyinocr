// 文件作用：定义当前进程二维码结果传输客户端及其本地运行时队列。
#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QByteArray>
#include <QList>
#include <QThread>

#include <memory>
#include <mutex>

enum class ResultExportConnectionState
{
    Disconnected,
    Connecting,
    Connected,
    Checking,
    Syncing
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

    bool isValid() const
    {
        return !id.trimmed().isEmpty()
                && eventTimeUtc.isValid()
                && eventTimeUtc.timeSpec() == Qt::UTC
                && ((overallOk && !qrContent.isEmpty())
                    || (!overallOk && qrContent.isEmpty()));
    }
};

Q_DECLARE_METATYPE(ResultExportRecord)

class ResultExportNetworkWorker;

class ResultExportClient : public QObject
{
    Q_OBJECT

public:
    explicit ResultExportClient(QObject *parent = nullptr);
    ~ResultExportClient() override;

    bool startupReady() const;
    QString startupError() const;
    QString outboxPath() const;
    int pendingCount() const;
    bool hasPending() const;

    bool enqueue(const ResultExportRecord &record, QString *errorMessage = nullptr);
    bool clearOutbox(QString *errorMessage = nullptr);
    bool discardOutboxForShutdown(QString *errorMessage = nullptr);

    void requestConnect(const QString &ip, quint16 port);
    void requestConnectionCheck();
    void requestDisconnect();
    void requestSynchronizeOutbox();
    void requestAbandonOutbox();
    void shutdown();

    ResultExportConnectionState connectionState() const;
    double roundTripMs() const;
    bool outboxDispositionPending() const;

signals:
    void connectionStateChanged(ResultExportConnectionState state);
    void pendingCountChanged(int count);
    void outboxDispositionRequired(int count);
    void transportFailure(QString diagnostic);
    void localQueueUnavailable(QString diagnostic);
    void outboxDispositionFinished();

private:
    void ensureWorker();
    bool persistSnapshot(const QList<ResultExportRecord> &records,
                         QString *errorMessage);
    QByteArray serializeRecord(const ResultExportRecord &record) const;
    void sendHeadIfPossible();
    void handleWorkerConnected();
    void handleWorkerDisconnected(QString reason);
    void handleWorkerAck(QString id);
    void handleWorkerFailure(QString reason);
    void handleWorkerPong(double roundTripMs);
    void setConnectionState(ResultExportConnectionState state);
    void setDispositionPending(bool pending);

    mutable std::mutex m_mutex;
    std::mutex m_persistenceMutex;
    QString m_outboxPath;
    QString m_startupError;
    QList<ResultExportRecord> m_records;
    ResultExportConnectionState m_state = ResultExportConnectionState::Disconnected;
    double m_roundTripMs = -1.0;
    bool m_startupReady = false;
    bool m_dispositionPending = false;
    bool m_sendInFlight = false;
    bool m_shutdown = false;
    QString m_receiverIp;
    quint16 m_receiverPort = 0;
    QThread *m_networkThread = nullptr;
    ResultExportNetworkWorker *m_worker = nullptr;
};
