#include "runtime/result_export_client.h"

#include "runtime/result_export_network_worker.h"
#include "system_support/logging/log_categories.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>

ResultExportClient::ResultExportClient(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<ResultExportConnectionState>(
                "ResultExportConnectionState");
}

ResultExportClient::~ResultExportClient()
{
    if (!m_worker || !m_networkThread) {
        return;
    }
    QMetaObject::invokeMethod(
                m_worker,
                [this]() { m_worker->stop(); },
                Qt::BlockingQueuedConnection);
    m_networkThread->quit();
    m_networkThread->wait();
}

int ResultExportClient::pendingCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pendingRecords.size();
}

void ResultExportClient::enqueue(const ResultExportRecord &record)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingRecords.push_back(record);
    }
    sendHeadIfPossible();
}

void ResultExportClient::requestConnect(const QString &ip, quint16 port)
{
    ensureWorker();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_roundTripMs = -1.0;
    }
    setConnectionState(ResultExportConnectionState::Connecting);
    const QString targetIp = ip.trimmed();
    QMetaObject::invokeMethod(
                m_worker,
                [this, targetIp, port]() {
        m_worker->connectToHost(targetIp, port);
    },
                Qt::QueuedConnection);
}

void ResultExportClient::requestConnectionCheck()
{
    if (connectionState() != ResultExportConnectionState::Connected) {
        return;
    }
    setConnectionState(ResultExportConnectionState::Checking);
    QMetaObject::invokeMethod(
                m_worker,
                [this]() { m_worker->checkConnection(); },
                Qt::QueuedConnection);
}

void ResultExportClient::requestDisconnect()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_roundTripMs = -1.0;
    }
    setConnectionState(ResultExportConnectionState::Disconnected);
    if (m_worker) {
        QMetaObject::invokeMethod(
                    m_worker,
                    [this]() { m_worker->disconnectFromHost(); },
                    Qt::QueuedConnection);
    }
}

ResultExportConnectionState ResultExportClient::connectionState() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

double ResultExportClient::roundTripMs() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_roundTripMs;
}

void ResultExportClient::ensureWorker()
{
    if (m_worker && m_networkThread) {
        return;
    }
    m_networkThread = new QThread(this);
    m_worker = new ResultExportNetworkWorker;
    m_worker->moveToThread(m_networkThread);
    connect(m_networkThread, &QThread::finished,
            m_worker, &QObject::deleteLater);
    connect(m_worker, &ResultExportNetworkWorker::connected,
            this, &ResultExportClient::handleWorkerConnected,
            Qt::QueuedConnection);
    connect(m_worker, &ResultExportNetworkWorker::disconnected,
            this, &ResultExportClient::handleWorkerDisconnected,
            Qt::QueuedConnection);
    connect(m_worker, &ResultExportNetworkWorker::ackReceived,
            this, &ResultExportClient::handleWorkerAck,
            Qt::QueuedConnection);
    connect(m_worker, &ResultExportNetworkWorker::failed,
            this, &ResultExportClient::handleWorkerFailure,
            Qt::QueuedConnection);
    connect(m_worker, &ResultExportNetworkWorker::pongReceived,
            this, &ResultExportClient::handleWorkerPong,
            Qt::QueuedConnection);
    m_networkThread->start();
}

QByteArray ResultExportClient::serializeRecord(
    const ResultExportRecord &record) const
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), record.id);
    object.insert(QStringLiteral("time"),
                  record.eventTimeUtc.toUTC().toString(Qt::ISODateWithMs));
    object.insert(QStringLiteral("overallOk"), record.overallOk);
    object.insert(QStringLiteral("qrContent"), record.qrContent);
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

void ResultExportClient::sendHeadIfPossible()
{
    ResultExportRecord record;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_worker
                || m_state != ResultExportConnectionState::Connected
                || m_sendInFlight || m_pendingRecords.isEmpty()) {
            return;
        }
        record = m_pendingRecords.first();
        m_sendInFlight = true;
    }
    const QByteArray line = serializeRecord(record) + '\n';
    const QString id = record.id;
    QMetaObject::invokeMethod(
                m_worker,
                [this, line, id]() {
        m_worker->sendRecord(line, id);
    },
                Qt::QueuedConnection);
}

void ResultExportClient::handleWorkerConnected()
{
    setConnectionState(ResultExportConnectionState::Checking);
    QMetaObject::invokeMethod(
                m_worker,
                [this]() { m_worker->checkConnection(); },
                Qt::QueuedConnection);
}

void ResultExportClient::handleWorkerDisconnected(QString reason)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sendInFlight = false;
        m_roundTripMs = -1.0;
    }
    setConnectionState(ResultExportConnectionState::Disconnected);
    if (!reason.isEmpty()) {
        qCWarning(logRuntime).noquote()
                << QStringLiteral("event=result_export.disconnected reason=%1")
                   .arg(reason);
    }
}

void ResultExportClient::handleWorkerAck()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingRecords.removeFirst();
        m_sendInFlight = false;
    }
    sendHeadIfPossible();
}

void ResultExportClient::handleWorkerFailure(QString reason)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sendInFlight = false;
        m_roundTripMs = -1.0;
    }
    emit transportFailure(reason);
    QMetaObject::invokeMethod(
                m_worker,
                [this]() { m_worker->disconnectFromHost(); },
                Qt::QueuedConnection);
    setConnectionState(ResultExportConnectionState::Disconnected);
}

void ResultExportClient::handleWorkerPong(double roundTripMs)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_roundTripMs = roundTripMs;
    }
    setConnectionState(ResultExportConnectionState::Connected);
    sendHeadIfPossible();
}

void ResultExportClient::setConnectionState(ResultExportConnectionState state)
{
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        changed = m_state != state;
        m_state = state;
    }
    if (changed) {
        emit connectionStateChanged(state);
    }
}
