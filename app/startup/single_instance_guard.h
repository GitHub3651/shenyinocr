// 文件作用：本文件用于通过系统互斥和窗口激活机制保证同一时间只运行一个程序实例。
// 主要职责：通过系统互斥和窗口激活机制保证同一时间只运行一个程序实例。
// 模块位置：启动层；只负责进程初始化和对象组装，不放置业务规则。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include <QSharedMemory>
#include <QString>

// 组件说明：SingleInstanceGuard 组件封装本文件中与其名称对应的单一职责。
class SingleInstanceGuard
{
public:
    explicit SingleInstanceGuard(const QString &key);

    bool acquire();

private:
    QSharedMemory m_sharedMemory;
};
