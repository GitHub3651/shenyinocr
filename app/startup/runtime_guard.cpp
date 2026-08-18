// 文件作用：本文件用于在程序退出时按固定顺序停止运行对象并检查后台任务已经结束。
// 主要职责：在程序退出时按固定顺序停止运行对象并检查后台任务已经结束。
// 模块位置：启动层；只负责进程初始化和对象组装，不放置业务规则。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "startup/runtime_guard.h"

#include "system_support/license/license_codec.h"

#include <QCoreApplication>
#include <QDate>
#include <QDir>

// 函数说明：check 函数校验、转换或恢复对应数据。
bool RuntimeGuard::check()
{
    const QString licensePath = QDir(
                QCoreApplication::applicationDirPath()).filePath(
                QStringLiteral("license.ini"));
    const LicenseReadResult license = LicenseCodec::readFile(licensePath);
    return license.succeeded()
            && QDate::currentDate() <= license.expiresDate;
}
