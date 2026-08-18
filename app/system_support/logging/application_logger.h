// 文件作用：本文件用于初始化应用日志、控制日志保留策略，并把运行信息稳定写入日志文件。
// 主要职责：初始化应用日志、控制日志保留策略，并把运行信息稳定写入日志文件。
// 模块位置：系统支撑层；提供设置、日志、授权和崩溃诊断等基础能力。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include <QString>

// 组件说明：ApplicationLogger 组件封装本文件中与其名称对应的单一职责。
class ApplicationLogger
{
public:
    static bool install(const QString &applicationDirectory);
    static void appendCrashInformation(const QString &information);
    static QString logDirectoryPath();
    static void shutdown();
};
