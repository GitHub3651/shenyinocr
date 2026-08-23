// 文件作用：本文件用于初始化应用日志、控制日志保留策略，并把运行信息稳定写入日志文件。
// 主要职责：初始化应用日志、控制日志保留策略，并把运行信息稳定写入日志文件。
// 模块位置：系统支撑层；提供设置、日志、授权和崩溃诊断等基础能力。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "system_support/logging/application_logger.h"

#include <QByteArray>
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

const QByteArray kUtf8Bom("\xEF\xBB\xBF", 3);

QFile g_logFile;
QString g_logDirectoryPath;
QDate g_currentLogDate;
QMutex g_logMutex;

// 函数说明：cleanupExpiredLogs 函数实现名称所表示的处理步骤。
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

// 函数说明：openLogFileForDate 函数创建、准备或启动对应流程。
bool openLogFileForDate(const QDate &date)
{
    if (g_logFile.isOpen()) {
        g_logFile.close();
    }

    const QString logPath = QDir(g_logDirectoryPath).absoluteFilePath(
                QStringLiteral("app_log_%1.txt").arg(
                    date.toString(QStringLiteral("yyyy-MM-dd"))));
    const QFileInfo existingLog(logPath);
    if (existingLog.exists() && existingLog.size() > 0) {
        QFile file(logPath);
        if (!file.open(QIODevice::ReadOnly)) {
            g_currentLogDate = QDate();
            return false;
        }
        const bool isUtf8 = file.read(kUtf8Bom.size()) == kUtf8Bom;
        file.close();
        if (!isUtf8) {
            const QString legacyPath = QDir(g_logDirectoryPath)
                    .absoluteFilePath(
                        QStringLiteral("app_log_%1_legacy_%2.txt")
                        .arg(date.toString(QStringLiteral("yyyy-MM-dd")),
                             QDateTime::currentDateTime().toString(
                                 QStringLiteral("HHmmss_zzz"))));
            if (!QFile::rename(logPath, legacyPath)) {
                g_currentLogDate = QDate();
                return false;
            }
        }
    }

    g_logFile.setFileName(logPath);
    if (!g_logFile.open(QIODevice::Append | QIODevice::Text)) {
        g_currentLogDate = QDate();
        return false;
    }
    if (g_logFile.size() == 0
            && g_logFile.write(kUtf8Bom) != kUtf8Bom.size()) {
        g_logFile.close();
        g_currentLogDate = QDate();
        return false;
    }

    g_currentLogDate = date;
    return true;
}

// 函数说明：messageHandler 函数实现名称所表示的处理步骤。
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

    const char *file = context.file ? context.file : "";
    const char *function = context.function ? context.function : "";
    const QString logMessage = QStringLiteral("%1 - %2 (%3:%4, %5)\n")
            .arg(QDateTime::currentDateTime().toString(
                     QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz")))
            .arg(message)
            .arg(QString::fromLocal8Bit(file))
            .arg(context.line)
            .arg(QString::fromLocal8Bit(function));

    if (g_logFile.isOpen()) {
        g_logFile.write(logMessage.toUtf8());
        g_logFile.flush();
    }
    std::fprintf(stderr, "%s", logMessage.toLocal8Bit().constData());

    if (type == QtFatalMsg) {
        std::abort();
    }
}

} // namespace

// 函数说明：install 函数实现名称所表示的处理步骤。
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

// 函数说明：appendCrashInformation 函数实现名称所表示的处理步骤。
void ApplicationLogger::appendCrashInformation(const QString &information)
{
    if (!g_logFile.isOpen()) {
        return;
    }
    g_logFile.write("\nCrash detected!\n");
    g_logFile.write(information.toUtf8());
    g_logFile.flush();
}

// 函数说明：logDirectoryPath 函数实现名称所表示的处理步骤。
QString ApplicationLogger::logDirectoryPath()
{
    QMutexLocker locker(&g_logMutex);
    return g_logDirectoryPath;
}

// 函数说明：shutdown 函数实现名称所表示的处理步骤。
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
