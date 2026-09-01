#pragma once

#include <QWidget>

#include "result_receiver_server.h"

namespace Ui {
class ResultReceiverWindow;
}

class ResultReceiverWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ResultReceiverWindow(QWidget *parent = nullptr);
    ~ResultReceiverWindow() override;

private slots:
    void startListening();
    void stopListening();
    void synchronizeCsv();
    void updateStatus(const QString &status);

private:
    void loadSettings();
    void saveSettings();
    bool outputDirectoryReady(QString *errorMessage) const;

    Ui::ResultReceiverWindow *ui = nullptr;
    ResultReceiverServer m_server;
};
