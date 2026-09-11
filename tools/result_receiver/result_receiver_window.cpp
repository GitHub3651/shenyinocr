#include "result_receiver_window.h"
#include "ui_result_receiver_window.h"

#include <QDir>
#include <QFileDialog>
#include <QDebug>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>

ResultReceiverWindow::ResultReceiverWindow(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::ResultReceiverWindow)
{
    ui->setupUi(this);

    connect(ui->pushButton_browse, &QPushButton::clicked, this, [this]() {
        const QString selected = QFileDialog::getExistingDirectory(
                    this, QStringLiteral("选择结果保存文件夹"),
                    ui->lineEdit_outputDirectory->text());
        if (!selected.isEmpty()) {
            ui->lineEdit_outputDirectory->setText(QDir::cleanPath(selected));
        }
    });
    connect(ui->pushButton_startListening, &QPushButton::clicked,
            this, &ResultReceiverWindow::startListening);
    connect(ui->pushButton_stopListening, &QPushButton::clicked,
            this, &ResultReceiverWindow::stopListening);
    connect(ui->pushButton_sync, &QPushButton::clicked,
            this, &ResultReceiverWindow::synchronizeCsv);
    connect(&m_server, &ResultReceiverServer::statusChanged,
            this, &ResultReceiverWindow::updateStatus);
    connect(&m_server, &ResultReceiverServer::errorOccurred,
            this, [this](const QString &message) {
        ui->textEdit_log->append(message);
    });
    connect(&m_server, &ResultReceiverServer::productAccepted,
            this, [this](const QString &id, bool duplicate) {
        ui->textEdit_log->append(
                    QStringLiteral("%1 %2")
                    .arg(duplicate ? QStringLiteral("重复记录")
                                   : QStringLiteral("已接收"), id));
    });
    loadSettings();
    updateStatus(QStringLiteral("已停止"));
}

ResultReceiverWindow::~ResultReceiverWindow()
{
    delete ui;
}

void ResultReceiverWindow::startListening()
{
    QString error;
    if (!outputDirectoryReady(&error)) {
        QMessageBox::warning(this, QStringLiteral("无法监听"), error);
        return;
    }
    saveSettings();
    if (!m_server.listen(
                static_cast<quint16>(ui->spinBox_port->value()),
                ui->lineEdit_outputDirectory->text(), &error)) {
        QMessageBox::warning(
                    this, QStringLiteral("监听失败"),
                    QStringLiteral("无法开始接收，请检查端口是否被占用。"));
        return;
    }
    ui->spinBox_port->setEnabled(false);
    ui->lineEdit_outputDirectory->setEnabled(false);
    ui->pushButton_sync->setEnabled(true);
}

void ResultReceiverWindow::stopListening()
{
    m_server.close();
    ui->spinBox_port->setEnabled(true);
    ui->lineEdit_outputDirectory->setEnabled(true);
}

void ResultReceiverWindow::synchronizeCsv()
{
    QStringList success;
    QStringList failed;
    QString error;
    const bool allOk = m_server.store().synchronizeAllCsv(
                &success, &failed, &error);
    QString message = QStringLiteral("成功日期：%1")
            .arg(success.isEmpty() ? QStringLiteral("无") : success.join(
                      QStringLiteral(", ")));
    if (!failed.isEmpty()) {
        message += QStringLiteral("\n失败日期：%1")
                .arg(failed.join(QStringLiteral(", ")));
    }
    if (!error.isEmpty()) {
        message += QStringLiteral("\n%1").arg(error);
    }
    ui->textEdit_log->append(message);
    if (!allOk) {
        QMessageBox::warning(this, QStringLiteral("部分 CSV 文件未能生成"), message);
    }
}

void ResultReceiverWindow::updateStatus(const QString &status)
{
    ui->label_status->setText(status);
    const bool listening = m_server.isListening();
    ui->pushButton_startListening->setEnabled(!listening);
    ui->pushButton_stopListening->setEnabled(listening);
}

void ResultReceiverWindow::loadSettings()
{
    QSettings settings(QStringLiteral("OCRGangYin"),
                       QStringLiteral("ResultReceiver"));
    ui->spinBox_port->setValue(
                settings.value(QStringLiteral("network/port"), 35680).toInt());
    ui->lineEdit_outputDirectory->setText(
                settings.value(QStringLiteral("storage/outputDirectory"),
                               QString()).toString());
}

void ResultReceiverWindow::saveSettings()
{
    QSettings settings(QStringLiteral("OCRGangYin"),
                       QStringLiteral("ResultReceiver"));
    settings.setValue(QStringLiteral("network/port"),
                      ui->spinBox_port->value());
    settings.setValue(QStringLiteral("storage/outputDirectory"),
                      ui->lineEdit_outputDirectory->text().trimmed());
}

bool ResultReceiverWindow::outputDirectoryReady(QString *errorMessage) const
{
    const QString path = ui->lineEdit_outputDirectory->text().trimmed();
    if (path.isEmpty() || !QDir(path).isAbsolute()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("首次使用前，请先选择结果保存文件夹。" );
        }
        return false;
    }
    if (!QDir().mkpath(path)) {
        qWarning().noquote()
                << QStringLiteral("result receiver output directory creation failed: %1")
                   .arg(path);
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法创建结果保存文件夹，请检查文件夹权限。" );
        }
        return false;
    }
    return true;
}
