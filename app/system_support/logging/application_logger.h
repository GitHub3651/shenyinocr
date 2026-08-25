// 文件作用：为Release应用提供简单的会话日志、分卷和保留清理。
#pragma once

#include <QString>

class ApplicationLogger
{
public:
    static bool start(QString *errorMessage = nullptr);
    static void stop();
};
