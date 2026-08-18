// 文件作用：本文件用于安装Windows异常处理入口并在崩溃时触发诊断信息保存。
// 主要职责：安装Windows异常处理入口并在崩溃时触发诊断信息保存。
// 模块位置：系统支撑层；提供设置、日志、授权和崩溃诊断等基础能力。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "system_support/crash/windows_crash_handler.h"

#include <QtGlobal>

#ifdef Q_OS_WIN

#include "system_support/crash/windows_crash_stack.h"
#include "system_support/logging/application_logger.h"

#include <windows.h>

namespace {

// 函数说明：unhandledExceptionFilter 函数实现名称所表示的处理步骤。
LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS *exceptionPointers)
{
    WindowsCrashStack crashStack(exceptionPointers);
    ApplicationLogger::appendCrashInformation(
                crashStack.exceptionInformation());
    return EXCEPTION_EXECUTE_HANDLER;
}

} // namespace

#endif

// 函数说明：install 函数实现名称所表示的处理步骤。
void WindowsCrashHandler::install()
{
#ifdef Q_OS_WIN
    SetUnhandledExceptionFilter(unhandledExceptionFilter);
#endif
}
