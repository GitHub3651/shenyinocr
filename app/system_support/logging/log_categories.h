// 文件作用：声明第一版日志系统唯一允许使用的六个固定类别。
#pragma once

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logStartup)
Q_DECLARE_LOGGING_CATEGORY(logRuntime)
Q_DECLARE_LOGGING_CATEGORY(logDevice)
Q_DECLARE_LOGGING_CATEGORY(logDetection)
Q_DECLARE_LOGGING_CATEGORY(logTemplate)
Q_DECLARE_LOGGING_CATEGORY(logUi)
