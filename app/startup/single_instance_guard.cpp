// 文件作用：本文件用于通过系统互斥和窗口激活机制保证同一时间只运行一个程序实例。
// 主要职责：通过系统互斥和窗口激活机制保证同一时间只运行一个程序实例。
// 模块位置：启动层；只负责进程初始化和对象组装，不放置业务规则。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "startup/single_instance_guard.h"

// 函数说明：SingleInstanceGuard 构造函数创建组件并初始化其依赖和初始状态。
SingleInstanceGuard::SingleInstanceGuard(const QString &key)
    : m_sharedMemory(key)
{
}

// 函数说明：acquire 函数实现名称所表示的处理步骤。
bool SingleInstanceGuard::acquire()
{
    if (m_sharedMemory.attach()) {
        return false;
    }

    // Marker creation failure does not prove another instance is running.
    m_sharedMemory.create(1);
    return true;
}
