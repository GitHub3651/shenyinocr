#include "runtime/result_export_client.h"

#include "runtime/result_export_network_worker.h"
#include "system_support/logging/log_categories.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QMetaObject>

namespace {

QString queueDirectoryPath()
{
    const QString root = QStandardPaths::writableLocation(
                QStandardPaths::AppDataLocation);
    return root.trimmed().isEmpty()
            ? QString()
            : QDir(root).filePath(QStringLiteral("result_export"));
}

}

ResultExportClient::ResultExportClient(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<ResultExportConnectionState>(
                "ResultExportConnectionState");
    qRegisterMetaType<ResultExportRecord>("ResultExportRecord");

    const QString directory = queueDirectoryPath();
    if (directory.isEmpty() || !QDir().mkpath(directory)) {
        m_startupError = QStringLiteral("无法创建结果传输本地队列目录。" );
        emit localQueueUnavailable(m_startupError);
        return;
    }
    m_outboxPath = QDir(directory).filePath(QStringLiteral("outbox.jsonl"));
    if (QFileInfo::exists(m_outboxPath)
            && !QFile::remove(m_outboxPath)) {
        m_startupError = QStringLiteral(
                    "上一进程的 outbox 无法清理：%1")
                .arg(QFileInfo(m_outboxPath).absoluteFilePath());
        emit localQueueUnavailable(m_startupError);
        return;
    }
    QString error;
    if (!persistSnapshot(QList<ResultExportRecord>(), &error)) {
        m_startupError = error;
        emit localQueueUnavailable(m_startupError);
        return;
    }
    m_startupReady = true;
}

ResultExportClient::~ResultExportClient()
{
    shutdown();
}

bool ResultExportClient::startupReady() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_startupReady;
}

QString ResultExportClient::startupError() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_startupError;
}

QString ResultExportClient::outboxPath() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_outboxPath;
}

int ResultExportClient::pendingCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_records.size();
}

bool ResultExportClient::hasPending() const
{
    return pendingCount() > 0;
}

bool ResultExportClient::enqueue(
    const ResultExportRecord &record,
    QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!record.isValid()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("结果传输记录无效。" );
        }
        return false;
    }
    QList<ResultExportRecord> next;
    {
        std::lock_guard<std::mutex> persistenceLock(m_persistenceMutex);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_startupReady || m_shutdown) {
                if (errorMessage) {
                    *errorMessage = m_startupError.isEmpty()
                            ? QStringLiteral("结果传输本地队列不可用。")
                            : m_startupError;
                }
                return false;
            }
            next = m_records;
            next.append(record);
        }
        QString persistError;
        if (!persistSnapshot(next, &persistError)) {
            if (errorMessage) {
                *errorMessage = persistError;
            }
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_records = next;
        }
    }
    emit pendingCountChanged(next.size());
    sendHeadIfPossible();
    return true;
}

bool ResultExportClient::clearOutbox(QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    {
        std::lock_guard<std::mutex> persistenceLock(m_persistenceMutex);
        QString persistError;
        if (!persistSnapshot(QList<ResultExportRecord>(), &persistError)) {
            if (errorMessage) {
                *errorMessage = persistError;
            }
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_records.clear();
            m_dispositionPending = false;
            m_sendInFlight = false;
        }
    }
    emit pendingCountChanged(0);
    return true;
}

bool ResultExportClient::discardOutboxForShutdown(QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    {
        std::lock_guard<std::mutex> persistenceLock(m_persistenceMutex);
        QString path;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            path = m_outboxPath;
        }
        if (!path.isEmpty() && QFileInfo::exists(path)
                && !QFile::remove(path)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("关闭时删除 outbox 失败：%1").arg(path);
            }
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_records.clear();
            m_dispositionPending = false;
            m_sendInFlight = false;
        }
    }
    emit pendingCountChanged(0);
    return true;
}

void ResultExportClient::requestConnect(const QString &ip, quint16 port)
{
    bool shuttingDown = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        shuttingDown = m_shutdown;
    }
    if (!startupReady() || shuttingDown) {
        emit localQueueUnavailable(startupError());
        return;
    }
    ensureWorker();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_receiverIp = ip.trimmed();
        m_receiverPort = port;
        m_roundTripMs = -1.0;
    }
    setDispositionPending(false);
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
    bool shuttingDown = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        shuttingDown = m_shutdown;
    }
    if (shuttingDown || connectionState() != ResultExportConnectionState::Connected
            || outboxDispositionPending()) {
        return;
    }
    ensureWorker();
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
    setDispositionPending(false);
    setConnectionState(ResultExportConnectionState::Disconnected);
    if (m_worker) {
        QMetaObject::invokeMethod(
                    m_worker,
                    [this]() { m_worker->disconnectFromHost(); },
                    Qt::QueuedConnection);
    }
}

void ResultExportClient::requestSynchronizeOutbox()
{
    if (!outboxDispositionPending() || !hasPending()) {
        return;
    }
    setDispositionPending(false);
    setConnectionState(ResultExportConnectionState::Syncing);
    sendHeadIfPossible();
}

void ResultExportClient::requestAbandonOutbox()
{
    if (!outboxDispositionPending() || !hasPending()) {
        return;
    }
    QString error;
    if (!clearOutbox(&error)) {
        emit localQueueUnavailable(error);
        emit transportFailure(error);
        return;
    }
    setConnectionState(ResultExportConnectionState::Connected);
    emit outboxDispositionFinished();
}

void ResultExportClient::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_shutdown) {
            return;
        }
        m_shutdown = true;
        m_roundTripMs = -1.0;
    }
    if (m_worker && m_networkThread) {
        QMetaObject::invokeMethod(
                    m_worker,
                    [this]() { m_worker->stop(); },
                    Qt::BlockingQueuedConnection);
        m_networkThread->quit();
        m_networkThread->wait();
        m_worker = nullptr;
        m_networkThread = nullptr;
    }
    setConnectionState(ResultExportConnectionState::Disconnected);
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

bool ResultExportClient::outboxDispositionPending() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_dispositionPending;
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

bool ResultExportClient::persistSnapshot(
    const QList<ResultExportRecord> &records,
    QString *errorMessage)
{
    QString path;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        path = m_outboxPath;
    }
    if (path.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("结果传输 outbox 路径不可用。" );
        }
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }
    for (const ResultExportRecord &record : records) {
        const QByteArray line = serializeRecord(record) + '\n';
        if (file.write(line) != line.size()) {
            if (errorMessage) {
                *errorMessage = file.errorString();
            }
            file.cancelWriting();
            return false;
        }
    }
    if (!file.flush() || !file.commit()) {
        if (errorMessage) {
            *errorMessage = file.errorString().isEmpty()
                    ? QStringLiteral("outbox 原子提交失败。")
                    : file.errorString();
        }
        return false;
    }
    return true;
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
        if (!m_worker || m_shutdown || m_dispositionPending
                || (m_state != ResultExportConnectionState::Connected
                    && m_state != ResultExportConnectionState::Syncing)
                || m_sendInFlight || m_records.isEmpty()) {
            return;
        }
        record = m_records.first();
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
    if (!reason.isEmpty() && !m_shutdown) {
        qCWarning(logRuntime).noquote()
                << QStringLiteral("event=result_export.disconnected reason=%1")
                   .arg(reason);
    }
}

void ResultExportClient::handleWorkerAck(QString id)
{
    QList<ResultExportRecord> next;
    QString persistError;
    bool persisted = false;
    {
        std::lock_guard<std::mutex> persistenceLock(m_persistenceMutex);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_sendInFlight || m_records.isEmpty()
                    || m_records.first().id != id) {
                m_sendInFlight = false;
                return;
            }
            next = m_records;
            next.removeFirst();
        }
        if (persistSnapshot(next, &persistError)) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_records = next;
            m_sendInFlight = false;
            persisted = true;
        } else {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_sendInFlight = false;
        }
    }
    if (!persisted) {
        emit localQueueUnavailable(persistError);
        emit transportFailure(QStringLiteral("ACK 已收到，但 outbox 清理失败：%1")
                              .arg(persistError));
        return;
    }
    emit pendingCountChanged(next.size());
    if (next.isEmpty()) {
        const bool wasSyncing = connectionState()
                == ResultExportConnectionState::Syncing;
        setConnectionState(ResultExportConnectionState::Connected);
        if (wasSyncing) {
            emit outboxDispositionFinished();
        }
    } else {
        sendHeadIfPossible();
    }
}

void ResultExportClient::handleWorkerFailure(QString reason)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sendInFlight = false;
        m_roundTripMs = -1.0;
    }
    if (!m_shutdown) {
        emit transportFailure(reason);
    }
    if (m_worker) {
        QMetaObject::invokeMethod(
                    m_worker,
                    [this]() { m_worker->disconnectFromHost(); },
                    Qt::QueuedConnection);
    }
    setConnectionState(ResultExportConnectionState::Disconnected);
}

void ResultExportClient::handleWorkerPong(double roundTripMs)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_roundTripMs = roundTripMs;
    }
    setConnectionState(ResultExportConnectionState::Connected);
    if (hasPending()) {
        setDispositionPending(true);
        emit outboxDispositionRequired(pendingCount());
        return;
    }
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

void ResultExportClient::setDispositionPending(bool pending)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_dispositionPending = pending;
}
