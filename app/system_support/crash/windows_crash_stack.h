// 文件作用：本文件用于收集Windows线程调用栈和模块信息，生成可排查的崩溃记录。
// 主要职责：收集Windows线程调用栈和模块信息，生成可排查的崩溃记录。
// 模块位置：系统支撑层；提供设置、日志、授权和崩溃诊断等基础能力。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#ifndef WINDOWS_CRASH_STACK_H
#define WINDOWS_CRASH_STACK_H

#include <windows.h>
#include <QString>


// 组件说明：WindowsCrashStack 组件封装本文件中与其名称对应的单一职责。
class WindowsCrashStack
{
private:
    PEXCEPTION_POINTERS m_pException;

private:
    QString moduleByReturnAddress(PBYTE Ret_Addr, PBYTE & Module_Addr);
    QString callStack(PEXCEPTION_POINTERS pException);
    QString versionString();

public:
    WindowsCrashStack(PEXCEPTION_POINTERS pException);

    QString exceptionInformation();
};

#endif // WINDOWS_CRASH_STACK_H
