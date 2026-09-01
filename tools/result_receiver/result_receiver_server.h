#pragma once

#include <QObject>
#include <QString>

#include "result_receiver_store.h"

class QTcpServer;
class QTcpSocket;

class ResultReceiverServer : public QObject
{
    Q_OBJECT

public:
    explicit ResultReceiverServer(QObject *parent = nullptr);

    bool listen(quint16 port, const QString &outputDirectory,
                QString *errorMessage);
    void close();
    bool isListening() const;
    bool hasClient() const;
    ResultReceiverStore &store();

signals:
    void statusChanged(QString status);
    void errorOccurred(QString diagnostic);
    void productAccepted(QString id, bool duplicate);

private:
    void handleNewConnection();
    void handleReadyRead();
    void handleClientDisconnected();
    void processLine(const QByteArray &line);
    void sendJson(const QJsonObject &object);

    QTcpServer *m_server = nullptr;
    QTcpSocket *m_client = nullptr;
    QByteArray m_buffer;
    ResultReceiverStore m_store;
};
