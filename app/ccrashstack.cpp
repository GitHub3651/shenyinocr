#include "ccrashstack.h"
#include <tlhelp32.h>
#include <stdio.h>

#define _WIN32_DCOM
#include <comdef.h>
#include <Wbemidl.h>

//#include<base/constants.h>
#include "qdebug.h"

CCrashStack::CCrashStack(PEXCEPTION_POINTERS pException)
{
    m_pException = pException;
}

QString CCrashStack::GetModuleByRetAddr(PBYTE Ret_Addr, PBYTE & Module_Addr)
{
    MODULEENTRY32   M = {sizeof(M)};
    HANDLE  hSnapshot;

    wchar_t Module_Name[MAX_PATH] = {0};

    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);

    if ((hSnapshot != INVALID_HANDLE_VALUE) &&
            Module32First(hSnapshot, &M))
    {
            do
            {
                    if (DWORD(Ret_Addr - M.modBaseAddr) < M.modBaseSize)
                    {
                            lstrcpyn(Module_Name, M.szExePath, MAX_PATH);
                            Module_Addr = M.modBaseAddr;
                            break;
                    }
            } while (Module32Next(hSnapshot, &M));
    }

    CloseHandle(hSnapshot);

    QString sRet = QString::fromWCharArray(Module_Name);
    return sRet;
}

QString CCrashStack::GetCallStack(PEXCEPTION_POINTERS pException)
{
    PBYTE   Module_Addr_1;
    char buffer[256] = {0};
    QString sRet;

    typedef struct STACK
    {
#if defined(_WIN64)
        STACK * Rbp;
        PBYTE   Ret_Addr;
        DWORD64 Param[0];
#else
        STACK * Ebp;
        PBYTE   Ret_Addr;
        DWORD   Param[0];
#endif
    } STACK, * PSTACK;

    STACK   Stack = {0, 0};
    PSTACK  Ebp;

    if (pException)     // fake frame for exception address
    {
#if defined(_WIN64)
        Stack.Rbp = (PSTACK)pException->ContextRecord->Rbp;
#else
        Stack.Ebp = (PSTACK)pException->ContextRecord->Ebp;
#endif
        Stack.Ret_Addr = (PBYTE)pException->ExceptionRecord->ExceptionAddress;
        Ebp = &Stack;
    }
    else
    {
        Ebp = (PSTACK)&pException - 1;  // frame addr of Get_Call_Stack()

        // Skip frame of Get_Call_Stack().
        if (!IsBadReadPtr(Ebp, sizeof(PSTACK)))
#if defined(_WIN64)
            Ebp = Ebp->Rbp;     // caller rbp
#else
            Ebp = Ebp->Ebp;     // caller ebp
#endif
    }

    // Break trace on wrong stack frame.
    for (; !IsBadReadPtr(Ebp, sizeof(PSTACK)) && !IsBadCodePtr(FARPROC(Ebp->Ret_Addr));
#if defined(_WIN64)
            Ebp = Ebp->Rbp)
#else
            Ebp = Ebp->Ebp)
#endif
    {
        // If module with Ebp->Ret_Addr found.
        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "\n%p  ", Ebp->Ret_Addr);
        sRet.append(buffer);

        QString moduleName = this->GetModuleByRetAddr(Ebp->Ret_Addr, Module_Addr_1);
        if (moduleName.length() > 0)
        {
            sRet.append(moduleName);
        }
    }

    return sRet;
} // Get_Call_Stack

QString CCrashStack::GetVersionStr()
{
    OSVERSIONINFOEX V = {sizeof(OSVERSIONINFOEX)};  // EX for NT 5.0 and later

    if (!GetVersionEx((POSVERSIONINFO)&V))
    {
        ZeroMemory(&V, sizeof(V));
        V.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        GetVersionEx((POSVERSIONINFO)&V);
    }

    if (V.dwPlatformId != VER_PLATFORM_WIN32_NT)
        V.dwBuildNumber = LOWORD(V.dwBuildNumber);  // for 9x HIWORD(dwBuildNumber) = 0x04xx

    QString sRet;
    sRet.append(QString("Windows:  %1.%2.%3, SP %4.%5, Product Type %6\n")
        .arg(V.dwMajorVersion).arg(V.dwMinorVersion).arg(V.dwBuildNumber)
        .arg(V.wServicePackMajor).arg(V.wServicePackMinor).arg(V.wProductType));

    return sRet;
}

QString CCrashStack::GetExceptionInfo()
{
    WCHAR       Module_Name[MAX_PATH];
    PBYTE       Module_Addr;

    QString sRet;
    char buffer[512] = {0};

    QString sTmp = GetVersionStr();
    sRet.append(sTmp);
    sRet.append("Process:  ");

    GetModuleFileName(NULL, Module_Name, MAX_PATH);
    sRet.append(QString::fromWCharArray(Module_Name));
    sRet.append("\n");

    // If exception occurred.
    if (m_pException)
    {
        EXCEPTION_RECORD &  E = *m_pException->ExceptionRecord;
        CONTEXT &           C = *m_pException->ContextRecord;

        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "Exception Addr:  %p  ", E.ExceptionAddress);
        sRet.append(buffer);
        // If module with E.ExceptionAddress found - save its path and date.
        QString module = GetModuleByRetAddr((PBYTE)E.ExceptionAddress, Module_Addr);
        if (module.length() > 0)
        {
            sRet.append(" Module: ");
            sRet.append(module);
        }

        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "\nException Code:  %08X\n", E.ExceptionCode);
        sRet.append(buffer);

        if (E.ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
        {
            // Access violation type - Write/Read.
            memset(buffer, 0, sizeof(buffer));
            sprintf(buffer, "%s Address:  %p\n",
                (E.ExceptionInformation[0]) ? "Write" : "Read", (PVOID)E.ExceptionInformation[1]);
            sRet.append(buffer);
        }

        sRet.append("Instruction: ");
        for (int i = 0; i < 16; i++)
        {
            memset(buffer, 0, sizeof(buffer));
            sprintf(buffer, " %02X",  PBYTE(E.ExceptionAddress)[i]);
            sRet.append(buffer);
        }

        sRet.append("\nRegisters: ");

#if defined(_WIN64)
        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "\nRAX: %016llX  RBX: %016llX  RCX: %016llX  RDX: %016llX", C.Rax, C.Rbx, C.Rcx, C.Rdx);
        sRet.append(buffer);

        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "\nRSI: %016llX  RDI: %016llX  RSP: %016llX  RBP: %016llX", C.Rsi, C.Rdi, C.Rsp, C.Rbp);
        sRet.append(buffer);

        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "\nRIP: %016llX  EFlags: %08X", C.Rip, C.EFlags);
        sRet.append(buffer);
#else
        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "\nEAX: %08X  EBX: %08X  ECX: %08X  EDX: %08X", C.Eax, C.Ebx, C.Ecx, C.Edx);
        sRet.append(buffer);

        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "\nESI: %08X  EDI: %08X  ESP: %08X  EBP: %08X", C.Esi, C.Edi, C.Esp, C.Ebp);
        sRet.append(buffer);

        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "\nEIP: %08X  EFlags: %08X", C.Eip, C.EFlags);
        sRet.append(buffer);
#endif
    } // if (pException)

    sRet.append("\nCall Stack:");
    QString sCallstack = this->GetCallStack(m_pException);
    sRet.append(sCallstack);

    return sRet;
}
