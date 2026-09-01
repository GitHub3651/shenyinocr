#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>

class QTcpSocket;
class QTimer;

class ResultExportNetworkWorker : public QObject
{
    Q_OBJECT

public:
    explicit ResultExportNetworkWorker(QObject *parent = nullptr);

public slots:
    void connectToHost(const QString &ip, quint16 port);
    void disconnectFromHost();
    void sendRecord(const QByteArray &jsonLine, const QString &id);
    void checkConnection();
    void stop();

signals:
    void connected();
    void disconnected(QString reason);
    void ackReceived(QString id);
    void pongReceived(double roundTripMs);
    void failed(QString reason);

private:
    void failAndDisconnect(const QString &reason);
    void processLine(const QByteArray &line);
    void clearPending();

    QTcpSocket *m_socket = nullptr;
    QTimer *m_connectTimer = nullptr;
    QTimer *m_ackTimer = nullptr;
    QTimer *m_checkTimer = nullptr;
    QElapsedTimer m_pingElapsedTimer;
    QByteArray m_buffer;
    QString m_waitingAckId;
    bool m_waitingPong = false;
    bool m_stopping = false;
};
