// 文件作用：把Windows未处理异常写入独立崩溃文件。
#include "system_support/crash/windows_crash_handler.h"

#include <QtGlobal>

#ifdef Q_OS_WIN

#include "system_support/crash/windows_crash_stack.h"
#include "system_support/logging/log_categories.h"

#include <QCoreApplication>
#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>

#include <cstdio>
#include <windows.h>

namespace {

const int kCrashRetentionDays = 90;
const QRegularExpression kCrashFileName(
            QStringLiteral(
                "^crash_\\d{8}_\\d{6}_\\d{3}_\\d+\\.txt$"));
QString g_crashDirectoryPath;

void writeCrashWarning(const QString &message)
{
    const QByteArray text = (QStringLiteral("WindowsCrashHandler warning: ")
                             + message + QLatin1Char('\n')).toUtf8();
    std::fwrite(text.constData(), 1,
                static_cast<std::size_t>(text.size()), stderr);
}

void cleanupExpiredCrashFiles()
{
    const QDateTime boundary =
            QDateTime::currentDateTime().addDays(-kCrashRetentionDays);
    const QFileInfoList files = QDir(g_crashDirectoryPath).entryInfoList(
                QDir::Files | QDir::NoSymLinks,
                QDir::NoSort);
    QStringList failedNames;
    for (const QFileInfo &file : files) {
        if (!kCrashFileName.match(file.fileName()).hasMatch()
                || file.lastModified() >= boundary) {
            continue;
        }
        if (!QFile::remove(file.absoluteFilePath())) {
            failedNames.append(file.fileName());
        }
    }
    if (!failedNames.isEmpty()) {
        writeCrashWarning(QStringLiteral(
            "expired crash files could not be removed: %1")
            .arg(failedNames.join(QStringLiteral(","))));
    }
}

LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS *exceptionPointers)
{
    WindowsCrashStack crashStack(exceptionPointers);
    const QString fileName = QStringLiteral("crash_%1_%2.txt")
            .arg(QDateTime::currentDateTime().toString(
                     QStringLiteral("yyyyMMdd_HHmmss_zzz")))
            .arg(QCoreApplication::applicationPid());
    QFile file(QDir(g_crashDirectoryPath).filePath(fileName));
    if (file.open(QIODevice::WriteOnly
                  | QIODevice::Text
                  | QIODevice::NewOnly)) {
        QByteArray contents("Crash detected!\n");
        contents.append(crashStack.exceptionInformation().toUtf8());
        contents.append('\n');
        file.write(contents);
        file.flush();
        file.close();
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

} // namespace

#endif

void WindowsCrashHandler::install()
{
#ifdef Q_OS_WIN
    g_crashDirectoryPath = QDir(
                QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("logs"));
    QDir().mkpath(g_crashDirectoryPath);
    cleanupExpiredCrashFiles();
    SetUnhandledExceptionFilter(unhandledExceptionFilter);
    qCInfo(logStartup).noquote()
            << QStringLiteral("event=crash_handler.installed directory=%1 retentionDays=%2")
               .arg(QDir::toNativeSeparators(g_crashDirectoryPath))
               .arg(kCrashRetentionDays);
#endif
}
