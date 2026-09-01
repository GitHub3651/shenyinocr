#include "runtime/result_export_network_worker.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTimer>
#include <QTcpSocket>

namespace {

const int kConnectTimeoutMs = 3000;
const int kAckTimeoutMs = 1500;
const int kCheckTimeoutMs = 3000;

}

ResultExportNetworkWorker::ResultExportNetworkWorker(QObject *parent)
    : QObject(parent),
      m_connectTimer(new QTimer(this)),
      m_ackTimer(new QTimer(this)),
      m_checkTimer(new QTimer(this))
{
    m_connectTimer->setSingleShot(true);
    m_ackTimer->setSingleShot(true);
    m_checkTimer->setSingleShot(true);

    connect(m_connectTimer, &QTimer::timeout, this, [this]() {
        failAndDisconnect(QStringLiteral("TCP 连接超时（3秒）。"));
    });
    connect(m_ackTimer, &QTimer::timeout, this, [this]() {
        failAndDisconnect(QStringLiteral("产品 ACK 超时（1.5秒）。"));
    });
    connect(m_checkTimer, &QTimer::timeout, this, [this]() {
        failAndDisconnect(QStringLiteral("PING/PONG 检查超时（3秒）。"));
    });
}

void ResultExportNetworkWorker::connectToHost(const QString &ip, quint16 port)
{
    if (m_stopping) {
        return;
    }
    if (m_socket && (m_socket->state() == QAbstractSocket::ConnectedState
                     || m_socket->state() == QAbstractSocket::ConnectingState)) {
        return;
    }
    if (m_socket) {
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    clearPending();
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, [this]() {
        m_connectTimer->stop();
        emit connected();
    });
    connect(m_socket, &QTcpSocket::readyRead, this, [this]() {
        m_buffer.append(m_socket->readAll());
        int separator = -1;
        while ((separator = m_buffer.indexOf('\n')) >= 0) {
            const QByteArray line = m_buffer.left(separator).trimmed();
            m_buffer.remove(0, separator + 1);
            if (!line.isEmpty()) {
                processLine(line);
            }
        }
    });
    connect(m_socket, &QTcpSocket::disconnected, this, [this]() {
        m_connectTimer->stop();
        m_ackTimer->stop();
        m_checkTimer->stop();
        clearPending();
        emit disconnected(QStringLiteral("TCP 连接已断开。"));
    });
    connect(m_socket,
            QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this,
            [this](QAbstractSocket::SocketError) {
        const QString reason = m_socket
                ? m_socket->errorString()
                : QStringLiteral("TCP socket 错误。");
        emit failed(reason);
    });
    m_connectTimer->start(kConnectTimeoutMs);
    m_socket->connectToHost(ip, port);
}

void ResultExportNetworkWorker::disconnectFromHost()
{
    m_connectTimer->stop();
    m_ackTimer->stop();
    m_checkTimer->stop();
    clearPending();
    if (!m_socket || m_socket->state() == QAbstractSocket::UnconnectedState) {
        emit disconnected(QStringLiteral("已断开连接。"));
        return;
    }
    m_socket->disconnectFromHost();
}

void ResultExportNetworkWorker::sendRecord(
    const QByteArray &jsonLine,
    const QString &id)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        emit failed(QStringLiteral("TCP 尚未连接，无法发送产品结果。"));
        return;
    }
    if (!m_waitingAckId.isEmpty() || m_waitingPong) {
        emit failed(QStringLiteral("当前已有待确认的网络请求。"));
        return;
    }
    const qint64 written = m_socket->write(jsonLine);
    if (written != jsonLine.size()) {
        emit failed(QStringLiteral("产品 JSON 未能完整交给 QTcpSocket::write()。"));
        return;
    }
    m_waitingAckId = id;
    m_ackTimer->start(kAckTimeoutMs);
}

void ResultExportNetworkWorker::checkConnection()
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        emit failed(QStringLiteral("TCP 尚未连接，无法执行连接检查。"));
        return;
    }
    if (!m_waitingAckId.isEmpty() || m_waitingPong) {
        emit failed(QStringLiteral("当前已有待确认的网络请求。"));
        return;
    }
    const QByteArray ping = QByteArrayLiteral("{\"ping\":true}\n");
    m_pingElapsedTimer.invalidate();
    if (m_socket->write(ping) != ping.size()) {
        emit failed(QStringLiteral("PING 未能完整写入 socket。"));
        return;
    }
    m_pingElapsedTimer.start();
    m_waitingPong = true;
    m_checkTimer->start(kCheckTimeoutMs);
}

void ResultExportNetworkWorker::stop()
{
    m_stopping = true;
    m_connectTimer->stop();
    m_ackTimer->stop();
    m_checkTimer->stop();
    clearPending();
    if (m_socket) {
        m_socket->disconnectFromHost();
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->abort();
        }
        m_socket->deleteLater();
        m_socket = nullptr;
    }
}

void ResultExportNetworkWorker::failAndDisconnect(const QString &reason)
{
    emit failed(reason);
    if (m_socket) {
        m_socket->abort();
    }
}

void ResultExportNetworkWorker::processLine(const QByteArray &line)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError
            || !document.isObject()) {
        failAndDisconnect(QStringLiteral("收到无法解析的 JSON 响应。"));
        return;
    }
    const QJsonObject object = document.object();
    if (object.value(QStringLiteral("ack")).isString()) {
        const QString id = object.value(QStringLiteral("ack")).toString();
        if (m_waitingAckId.isEmpty() || id != m_waitingAckId) {
            failAndDisconnect(QStringLiteral("收到不匹配的产品 ACK。"));
            return;
        }
        m_ackTimer->stop();
        m_waitingAckId.clear();
        emit ackReceived();
        return;
    }
    if (object.value(QStringLiteral("pong")).toBool(false)) {
        if (!m_waitingPong) {
            failAndDisconnect(QStringLiteral("收到未请求的 PONG。"));
            return;
        }
        const double roundTripMs =
                m_pingElapsedTimer.nsecsElapsed() / 1000000.0;
        m_checkTimer->stop();
        m_waitingPong = false;
        m_pingElapsedTimer.invalidate();
        emit pongReceived(roundTripMs);
        return;
    }
    failAndDisconnect(QStringLiteral("收到未知的 JSON 响应。"));
}

void ResultExportNetworkWorker::clearPending()
{
    m_waitingAckId.clear();
    m_waitingPong = false;
    m_pingElapsedTimer.invalidate();
    m_buffer.clear();
}
