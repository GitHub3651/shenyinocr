#include "widget.h"
#include <QDateTime>
#include <QMessageBox>
#include <QDebug>
#include <QApplication>
#include <QSharedMemory>
#include <QDebug>
#include <QTextCodec>
#include <iostream>
#include <memory>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QLoggingCategory>
#include <QTimer>
#include "RuntimeGuard.h"

#ifdef Q_OS_WIN
#include "ccrashstack.h"
#endif
QFile logFile;
using namespace  std;
#define APP_VERSION "5.14.2"
#pragma execution_character_set("utf-8")

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QByteArray localMsg = msg.toLocal8Bit();
    const char *file = context.file ? context.file : "";
    const char *function = context.function ? context.function : "";
    QString logMessage = QString("%1 - %2 (%3:%4, %5)\n")
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
            .arg(localMsg.constData())
            .arg(file)
            .arg(context.line)
            .arg(function);

    if (logFile.isOpen()) {
        logFile.write(logMessage.toLocal8Bit());
        logFile.flush();
    }

    fprintf(stderr, "%s", logMessage.toLocal8Bit().constData());

    if (type == QtFatalMsg) {
        abort();
    }
}


#ifdef Q_OS_WIN
long __stdcall callback(_EXCEPTION_POINTERS* excp)
{
    CCrashStack crashStack(excp);
    QString sCrashInfo = crashStack.GetExceptionInfo();

    if (logFile.isOpen()) {
        logFile.write("\nCrash detected!\n");
        logFile.write(sCrashInfo.toUtf8());
        logFile.flush();
    }

    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

void setupLogging()
{
    QString logDirPath = QCoreApplication::applicationDirPath() + "/log";
    QDir logDir(logDirPath);
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }

    QString logPath = logDirPath + "/app_log.txt";
    logFile.setFileName(logPath);
    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
        qDebug() << "Logging to" << logPath;
    } else {
        qWarning() << "Failed to open log file" << logPath;
    }

    qInstallMessageHandler(messageHandler);
}

void showRuntimeGuardExitMessage(const QString &message)
{
    QMessageBox msgBox(QMessageBox::Critical, "提示", message, QMessageBox::NoButton);
    msgBox.addButton("确认退出", QMessageBox::AcceptRole);
    msgBox.setWindowModality(Qt::ApplicationModal);
    msgBox.exec();
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    qRegisterMetaType<cv::Mat>("cv::Mat");

    // 确保最后一个窗口关闭时立即退出
    QApplication::setQuitOnLastWindowClosed(true);

    // 当应用即将退出时，强制清理
    QObject::connect(&a, &QCoreApplication::aboutToQuit, [](){
        cv::destroyAllWindows();  // 关闭所有OpenCV窗口
        QThread::msleep(100);     // 给一点时间清理
    });
    if (!RuntimeGuard::check()) {
        showRuntimeGuardExitMessage("系统初始化失败，请联系供应商。");
        return -1;
    }

    QTimer runtimeGuardTimer;
    QObject::connect(&runtimeGuardTimer, &QTimer::timeout, [](){
        if (!RuntimeGuard::check()) {
            showRuntimeGuardExitMessage("程序已失效，即将退出，请联系供应商。");
            QCoreApplication::quit();
        }
    });
    runtimeGuardTimer.start(24 * 60 * 60 * 1000);

    // 初始化一个共享内存对象，用于检查程序是否已被打开
    QSharedMemory shared("ecust");

    // 检查共享内存是否已附加，如果已附加则说明程序已运行，防止重复打开
    if (shared.attach())//共享内存被占用则直接返回
    {
        QMessageBox::warning(NULL, QStringLiteral("Warning"), "程序运行中避免重复打开");
        return 0;
    }

    // 创建共享内存，长度为1字节，用于标记程序的运行状态
    shared.create(1);

    Widget w;
#ifdef Q_OS_WIN
    SetUnhandledExceptionFilter(callback);
#endif
    setupLogging();
    w.showMaximized();
    //return a.exec();
    int result = a.exec();

    // 确保所有资源释放
    cv::destroyAllWindows();
    exit(result);
}
