#include "result_receiver_server.h"

#include <QHostAddress>
#include <QDebug>
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
        qWarning().noquote()
                << QStringLiteral("result receiver listen failed: port=%1 error=%2")
                   .arg(port)
                   .arg(m_server->errorString());
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "无法开始接收，请检查端口是否被占用。");
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
        qWarning().noquote()
                << QStringLiteral("invalid result JSON: offset=%1 error=%2")
                   .arg(parseError.offset)
                   .arg(parseError.errorString());
        emit errorOccurred(QStringLiteral(
                               "收到的结果数据格式不正确，当前连接已断开。"));
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
        qWarning().noquote()
                << QStringLiteral(
                    "invalid product result fields: idType=%1 idEmpty=%2 timeType=%3 overallOkType=%4 qrContentType=%5")
                   .arg(static_cast<int>(idValue.type()))
                   .arg(idValue.toString().trimmed().isEmpty())
                   .arg(static_cast<int>(timeValue.type()))
                   .arg(static_cast<int>(okValue.type()))
                   .arg(static_cast<int>(contentValue.type()));
        emit errorOccurred(QStringLiteral(
                               "收到的产品结果格式不正确，未确认接收。"));
        return;
    }
    const bool overallOk = okValue.toBool();
    const QString qrContent = contentValue.toString();
    if ((overallOk && qrContent.isEmpty())
            || (!overallOk && !qrContent.isEmpty())) {
        qWarning().noquote()
                << QStringLiteral(
                    "inconsistent product result: id=%1 overallOk=%2 qrContentEmpty=%3")
                   .arg(idValue.toString())
                   .arg(overallOk)
                   .arg(qrContent.isEmpty());
        emit errorOccurred(QStringLiteral(
                               "收到的产品判定与二维码内容不一致，未确认接收。"));
        return;
    }
    bool duplicate = false;
    QString error;
    if (!m_store.acceptProduct(object, &duplicate, &error)) {
        emit errorOccurred(error.isEmpty()
                           ? QStringLiteral("结果记录保存失败，未确认接收。")
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
