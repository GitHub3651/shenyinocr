// 文件作用：集中定义应用日志类别，不为单个类或操作增加类别。
#include "system_support/logging/log_categories.h"

Q_LOGGING_CATEGORY(logStartup, "app.startup", QtInfoMsg)
Q_LOGGING_CATEGORY(logRuntime, "app.runtime", QtInfoMsg)
Q_LOGGING_CATEGORY(logDevice, "app.device", QtInfoMsg)
Q_LOGGING_CATEGORY(logDetection, "app.detection", QtInfoMsg)
Q_LOGGING_CATEGORY(logTemplate, "app.template", QtInfoMsg)
Q_LOGGING_CATEGORY(logUi, "app.ui", QtInfoMsg)
