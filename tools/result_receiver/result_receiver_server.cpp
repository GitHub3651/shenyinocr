#include "result_receiver_server.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTcpServer>
#include <QTcpSocket>

ResultReceiverServer::ResultReceiverServer(QObject *parent)
    : QObject(parent),
      m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection,
            this, &ResultReceiverServer::handleNewConnection);
}

bool ResultReceiverServer::listen(quint16 port,
                                  const QString &outputDirectory,
                                  QString *errorMessage)
{
    close();
    if (errorMessage) {
        errorMessage->clear();
    }
    m_store.setOutputDirectory(outputDirectory);
    if (!m_server->listen(QHostAddress::AnyIPv4, port)) {
        if (errorMessage) {
            *errorMessage = m_server->errorString();
        }
        return false;
    }
    emit statusChanged(QStringLiteral("等待发送端连接"));
    return true;
}

void ResultReceiverServer::close()
{
    if (m_client) {
        m_client->disconnectFromHost();
        m_client->deleteLater();
        m_client = nullptr;
    }
    m_buffer.clear();
    if (m_server->isListening()) {
        m_server->close();
    }
    emit statusChanged(QStringLiteral("已停止"));
}

bool ResultReceiverServer::isListening() const
{
    return m_server->isListening();
}

bool ResultReceiverServer::hasClient() const
{
    return m_client != nullptr;
}

ResultReceiverStore &ResultReceiverServer::store()
{
    return m_store;
}

void ResultReceiverServer::handleNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *candidate = m_server->nextPendingConnection();
        if (m_client) {
            candidate->disconnectFromHost();
            candidate->deleteLater();
            continue;
        }
        m_client = candidate;
        connect(m_client, &QTcpSocket::readyRead,
                this, &ResultReceiverServer::handleReadyRead);
        connect(m_client, &QTcpSocket::disconnected,
                this, &ResultReceiverServer::handleClientDisconnected);
        emit statusChanged(QStringLiteral("发送端已连接"));
    }
}

void ResultReceiverServer::handleReadyRead()
{
    if (!m_client) {
        return;
    }
    m_buffer.append(m_client->readAll());
    int separator = -1;
    while ((separator = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_buffer.left(separator).trimmed();
        m_buffer.remove(0, separator + 1);
        if (!line.isEmpty()) {
            processLine(line);
        }
        if (!m_client) {
            return;
        }
    }
}

void ResultReceiverServer::handleClientDisconnected()
{
    if (m_client) {
        m_client->deleteLater();
        m_client = nullptr;
    }
    m_buffer.clear();
    if (m_server->isListening()) {
        emit statusChanged(QStringLiteral("等待发送端连接"));
    }
}

void ResultReceiverServer::processLine(const QByteArray &line)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError
            || !document.isObject()) {
        emit errorOccurred(QStringLiteral("收到坏 JSON，已关闭当前客户端。"));
        if (m_client) {
            m_client->abort();
        }
        return;
    }
    const QJsonObject object = document.object();
    if (object.value(QStringLiteral("ping")).toBool(false)) {
        sendJson(QJsonObject{{QStringLiteral("pong"), true}});
        return;
    }
    const QJsonValue idValue = object.value(QStringLiteral("id"));
    const QJsonValue timeValue = object.value(QStringLiteral("time"));
    const QJsonValue okValue = object.value(QStringLiteral("overallOk"));
    const QJsonValue contentValue = object.value(QStringLiteral("qrContent"));
    if (!idValue.isString() || idValue.toString().trimmed().isEmpty()
            || !timeValue.isString()
            || !okValue.isBool() || !contentValue.isString()) {
        emit errorOccurred(QStringLiteral("产品 JSON 字段无效，不返回 ACK。"));
        return;
    }
    const bool overallOk = okValue.toBool();
    const QString qrContent = contentValue.toString();
    if ((overallOk && qrContent.isEmpty())
            || (!overallOk && !qrContent.isEmpty())) {
        emit errorOccurred(QStringLiteral("产品 JSON 的 OK/NG 与 qrContent 组合无效。"));
        return;
    }
    bool duplicate = false;
    QString error;
    if (!m_store.acceptProduct(object, &duplicate, &error)) {
        emit errorOccurred(error.isEmpty()
                           ? QStringLiteral("JSONL 写入失败，不返回 ACK。")
                           : error);
        return;
    }
    QJsonObject ack;
    ack.insert(QStringLiteral("ack"), idValue.toString());
    sendJson(ack);
    emit productAccepted(idValue.toString(), duplicate);
}

void ResultReceiverServer::sendJson(const QJsonObject &object)
{
    if (!m_client) {
        return;
    }
    const QByteArray line = QJsonDocument(object).toJson(QJsonDocument::Compact)
            + '\n';
    m_client->write(line);
    m_client->flush();
}
