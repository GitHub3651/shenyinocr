#include "system_support/logging/application_logger.h"

#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QStringList>

#include <cstdio>
#include <cstdlib>

namespace {

QFile g_logFile;
QString g_logDirectoryPath;
QDate g_currentLogDate;
QMutex g_logMutex;

void cleanupExpiredLogs()
{
    QDir logDirectory(g_logDirectoryPath);
    const QDate retentionBoundary = QDate::currentDate().addMonths(-3);
    const QFileInfoList logFiles = logDirectory.entryInfoList(
                QStringList() << QStringLiteral("app_log_*.txt")
                              << QStringLiteral("app_log.txt"),
                QDir::Files);
    for (const QFileInfo &logInfo : logFiles) {
        QDate logDate;
        if (logInfo.fileName() == QStringLiteral("app_log.txt")) {
            logDate = logInfo.lastModified().date();
        } else {
            const QString dateText = logInfo.fileName().mid(
                        QStringLiteral("app_log_").size(),
                        QStringLiteral("yyyy-MM-dd").size());
            logDate = QDate::fromString(
                        dateText, QStringLiteral("yyyy-MM-dd"));
        }

        if (logDate.isValid() && logDate < retentionBoundary) {
            QFile::remove(logInfo.absoluteFilePath());
        }
    }
}

bool openLogFileForDate(const QDate &date)
{
    if (g_logFile.isOpen()) {
        g_logFile.close();
    }

    const QString logPath = QDir(g_logDirectoryPath).absoluteFilePath(
                QStringLiteral("app_log_%1.txt").arg(
                    date.toString(QStringLiteral("yyyy-MM-dd"))));
    g_logFile.setFileName(logPath);
    if (!g_logFile.open(QIODevice::Append | QIODevice::Text)) {
        g_currentLogDate = QDate();
        return false;
    }

    g_currentLogDate = date;
    return true;
}

void messageHandler(QtMsgType type,
                    const QMessageLogContext &context,
                    const QString &message)
{
    QMutexLocker locker(&g_logMutex);
    const QDate today = QDate::currentDate();
    if (g_currentLogDate != today) {
        cleanupExpiredLogs();
        openLogFileForDate(today);
    }

    const QByteArray localMessage = message.toLocal8Bit();
    const char *file = context.file ? context.file : "";
    const char *function = context.function ? context.function : "";
    const QString logMessage = QStringLiteral("%1 - %2 (%3:%4, %5)\n")
            .arg(QDateTime::currentDateTime().toString(
                     QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz")))
            .arg(QString::fromLocal8Bit(localMessage))
            .arg(QString::fromLocal8Bit(file))
            .arg(context.line)
            .arg(QString::fromLocal8Bit(function));

    if (g_logFile.isOpen()) {
        g_logFile.write(logMessage.toLocal8Bit());
        g_logFile.flush();
    }
    std::fprintf(stderr, "%s", logMessage.toLocal8Bit().constData());

    if (type == QtFatalMsg) {
        std::abort();
    }
}

} // namespace

bool ApplicationLogger::install(const QString &applicationDirectory)
{
    QMutexLocker locker(&g_logMutex);
    g_logDirectoryPath = QDir(applicationDirectory).filePath(
                QStringLiteral("log"));
    if (!QDir().mkpath(g_logDirectoryPath)) {
        return false;
    }

    cleanupExpiredLogs();
    if (!openLogFileForDate(QDate::currentDate())) {
        return false;
    }
    qInstallMessageHandler(messageHandler);
    return true;
}

void ApplicationLogger::appendCrashInformation(const QString &information)
{
    if (!g_logFile.isOpen()) {
        return;
    }
    g_logFile.write("\nCrash detected!\n");
    g_logFile.write(information.toUtf8());
    g_logFile.flush();
}

QString ApplicationLogger::logDirectoryPath()
{
    QMutexLocker locker(&g_logMutex);
    return g_logDirectoryPath;
}

void ApplicationLogger::shutdown()
{
    qInstallMessageHandler(nullptr);
    QMutexLocker locker(&g_logMutex);
    if (g_logFile.isOpen()) {
        g_logFile.flush();
        g_logFile.close();
    }
    g_currentLogDate = QDate();
}
