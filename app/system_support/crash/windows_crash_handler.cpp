#include "system_support/crash/windows_crash_handler.h"

#include <QtGlobal>

#ifdef Q_OS_WIN

#include "system_support/crash/windows_crash_stack.h"
#include "system_support/logging/application_logger.h"

#include <windows.h>

namespace {

LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS *exceptionPointers)
{
    WindowsCrashStack crashStack(exceptionPointers);
    ApplicationLogger::appendCrashInformation(
                crashStack.exceptionInformation());
    return EXCEPTION_EXECUTE_HANDLER;
}

} // namespace

#endif

void WindowsCrashHandler::install()
{
#ifdef Q_OS_WIN
    SetUnhandledExceptionFilter(unhandledExceptionFilter);
#endif
}
