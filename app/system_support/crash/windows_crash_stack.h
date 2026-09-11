#ifndef WINDOWS_CRASH_STACK_H
#define WINDOWS_CRASH_STACK_H

#include <windows.h>
#include <QString>


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
