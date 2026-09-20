#include "system_support/logging/application_logger.h"

#include "system_support/logging/log_categories.h"

#include <QCoreApplication>
#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QThread>
#include <QVector>
#include <QStringList>

#include <cstdio>
#include <cstdlib>
#include <algorithm>

namespace {

const qint64 kMebibyte = 1024LL * 1024LL;
const qint64 kPartSizeLimit = 32LL * kMebibyte;
const qint64 kTotalSizeLimit = 1024LL * kMebibyte;
const qint64 kTotalSizeBeforeNewPart =
        kTotalSizeLimit - kPartSizeLimit;
const int kRetentionDays = 60;
const QRegularExpression kNormalLogName(
            QStringLiteral(
                "^ShengYin_\\d{8}_\\d{6}_\\d{3}_\\d+_\\d{3}\\.log$"));

struct ManagedLogFile
{
    QString path;
    QString name;
    QDateTime lastModified;
    qint64 size = 0;
};

QFile g_logFile;
QString g_logDirectoryPath;
QString g_sessionName;
int g_partNumber = 0;
bool g_fileSinkFailed = false;
bool g_fileFailureReported = false;
QMutex g_logMutex;

void writeStderr(const QByteArray &text)
{
    if (!text.isEmpty()) {
        std::fwrite(text.constData(), 1,
                    static_cast<std::size_t>(text.size()), stderr);
    }
}

void writeLoggerWarning(const QString &message)
{
    writeStderr((QStringLiteral("ApplicationLogger warning: ")
                 + message + QLatin1Char('\n')).toUtf8());
}

QVector<ManagedLogFile> managedNormalLogs()
{
    QVector<ManagedLogFile> files;
    const QFileInfoList entries = QDir(g_logDirectoryPath).entryInfoList(
                QDir::Files | QDir::NoSymLinks,
                QDir::NoSort);
    for (const QFileInfo &entry : entries) {
        if (!kNormalLogName.match(entry.fileName()).hasMatch()) {
            continue;
        }
        ManagedLogFile file;
        file.path = entry.absoluteFilePath();
        file.name = entry.fileName();
        file.lastModified = entry.lastModified();
        file.size = entry.size();
        files.push_back(file);
    }
    return files;
}

void cleanupNormalLogs()
{
    const QDateTime retentionBoundary =
            QDateTime::currentDateTime().addDays(-kRetentionDays);
    QVector<ManagedLogFile> files = managedNormalLogs();
    QStringList failedNames;

    for (int index = files.size() - 1; index >= 0; --index) {
        if (files.at(index).lastModified >= retentionBoundary) {
            continue;
        }
        if (!QFile::remove(files.at(index).path)) {
            failedNames.append(files.at(index).name);
            continue;
        }
        files.remove(index);
    }

    qint64 totalSize = 0;
    for (const ManagedLogFile &file : files) {
        totalSize += file.size;
    }
    std::sort(files.begin(), files.end(),
              [](const ManagedLogFile &left, const ManagedLogFile &right) {
        return left.lastModified < right.lastModified;
    });
    for (const ManagedLogFile &file : files) {
        if (totalSize <= kTotalSizeBeforeNewPart) {
            break;
        }
        if (failedNames.contains(file.name)) {
            continue;
        }
        if (QFile::remove(file.path)) {
            totalSize -= file.size;
        } else {
            failedNames.append(file.name);
        }
    }

    if (!failedNames.isEmpty()) {
        failedNames.removeDuplicates();
        writeLoggerWarning(QStringLiteral(
            "normal log cleanup could not remove: %1")
            .arg(failedNames.join(QStringLiteral(","))));
    }
}

bool openNextPart(QString *errorMessage)
{
    ++g_partNumber;
    const QString fileName = QStringLiteral("ShengYin_%1_%2.log")
            .arg(g_sessionName)
            .arg(g_partNumber, 3, 10, QLatin1Char('0'));
    g_logFile.setFileName(QDir(g_logDirectoryPath).filePath(fileName));
    if (g_logFile.open(QIODevice::WriteOnly
                       | QIODevice::Text
                       | QIODevice::NewOnly)) {
        return true;
    }
    if (errorMessage) {
        *errorMessage = QStringLiteral("无法创建日志文件：%1（%2）")
                .arg(g_logFile.fileName(), g_logFile.errorString());
    }
    return false;
}

void disableFileSink(const QString &reason)
{
    if (g_logFile.isOpen()) {
        g_logFile.close();
    }
    g_fileSinkFailed = true;
    if (!g_fileFailureReported) {
        g_fileFailureReported = true;
        writeLoggerWarning(reason);
    }
}

const char *levelName(QtMsgType type)
{
    switch (type) {
    case QtInfoMsg:
        return "INFO";
    case QtWarningMsg:
        return "WARN";
    case QtCriticalMsg:
        return "ERROR";
    case QtFatalMsg:
        return "FATAL";
    case QtDebugMsg:
        break;
    }
    return nullptr;
}

QString localTimestamp()
{
    return QDateTime::currentDateTime().toString(
                QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
}

QByteArray formatLine(
    QtMsgType type,
    const QMessageLogContext &context,
    const QString &message)
{
    QString normalized = message;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\\n"));
    normalized.replace(QLatin1Char('\r'), QStringLiteral("\\n"));
    normalized.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
    const QString category = context.category && context.category[0]
            ? QString::fromLatin1(context.category)
            : QStringLiteral("default");
    const QString threadId = QString::number(
                reinterpret_cast<quintptr>(QThread::currentThreadId()), 16);
    return QStringLiteral("%1 %2 %3 tid=%4 %5\n")
            .arg(localTimestamp(),
                 QString::fromLatin1(levelName(type)),
                 category,
                 threadId,
                 normalized)
            .toUtf8();
}

void messageHandler(
    QtMsgType type,
    const QMessageLogContext &context,
    const QString &message)
{
    if (type == QtDebugMsg) {
        return;
    }
    const QByteArray line = formatLine(type, context, message);
    {
        QMutexLocker locker(&g_logMutex);
        if (!g_fileSinkFailed && g_logFile.isOpen()) {
            if (g_logFile.size() >= kPartSizeLimit) {
                g_logFile.flush();
                g_logFile.close();
                cleanupNormalLogs();
                QString rolloverError;
                if (!openNextPart(&rolloverError)) {
                    disableFileSink(rolloverError);
                }
            }
            if (!g_fileSinkFailed && g_logFile.isOpen()) {
                const qint64 written = g_logFile.write(line);
                if (written != line.size()) {
                    disableFileSink(QStringLiteral(
                        "日志文件写入失败：%1（%2）")
                        .arg(g_logFile.fileName(), g_logFile.errorString()));
                } else if (type == QtWarningMsg
                           || type == QtCriticalMsg
                           || type == QtFatalMsg) {
                    g_logFile.flush();
                }
            }
        }
    }

    writeStderr(line);
    if (type == QtFatalMsg) {
        std::abort();
    }
}

void clearState()
{
    g_logFile.setFileName(QString());
    g_logDirectoryPath.clear();
    g_sessionName.clear();
    g_partNumber = 0;
    g_fileSinkFailed = false;
    g_fileFailureReported = false;
}

} // namespace

bool ApplicationLogger::start(QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }

    QString openedFilePath;
    {
        QMutexLocker locker(&g_logMutex);
        clearState();
        g_logDirectoryPath = QDir(
                    QCoreApplication::applicationDirPath())
                .filePath(QStringLiteral("logs"));
        if (!QDir().mkpath(g_logDirectoryPath)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("无法创建日志目录：%1")
                        .arg(g_logDirectoryPath);
            }
            clearState();
            return false;
        }

        cleanupNormalLogs();
        const QDateTime startedAt = QDateTime::currentDateTime();
        g_sessionName = QStringLiteral("%1_%2")
                .arg(startedAt.toString(
                         QStringLiteral("yyyyMMdd_HHmmss_zzz")))
                .arg(QCoreApplication::applicationPid());
        if (!openNextPart(errorMessage)) {
            clearState();
            return false;
        }
        openedFilePath = g_logFile.fileName();
    }

    qInstallMessageHandler(messageHandler);
    qCInfo(logStartup).noquote()
            << QStringLiteral("event=log.started file=%1")
               .arg(QDir::toNativeSeparators(openedFilePath));
    return true;
}

void ApplicationLogger::stop()
{
    qInstallMessageHandler(nullptr);
    QMutexLocker locker(&g_logMutex);
    if (g_logFile.isOpen()) {
        g_logFile.flush();
        g_logFile.close();
    }
    clearState();
}
