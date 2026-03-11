// StaticInitGuard.cpp — Installs a crash handler BEFORE any other static
// constructors run, using #pragma init_seg(compiler) so the CRT runs this
// first.  It writes a minidump + log file on any unhandled exception.
#include <windows.h>
#include <dbghelp.h>
#include <cstdio>

#pragma comment(lib, "dbghelp.lib")

// Force this TU's static init to run in the "compiler" phase —
// before any user-level (#pragma init_seg(lib) or default) globals.
#pragma init_seg(compiler)

static LONG WINAPI StaticInitCrashHandler(EXCEPTION_POINTERS *ep)
{
    // ---- write a text log ------------------------------------------------
    FILE *f = nullptr;
    fopen_s(&f, "static_init_crash.log", "w");
    if (f)
    {
        DWORD code = ep->ExceptionRecord->ExceptionCode;
        void *addr = ep->ExceptionRecord->ExceptionAddress;

        // Module that contains the faulting address
        HMODULE hMod = nullptr;
        char modName[MAX_PATH] = "<unknown>";
        if (GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                (LPCSTR)addr, &hMod))
        {
            GetModuleFileNameA(hMod, modName, MAX_PATH);
        }

        // Calculate RVA
        uintptr_t baseAddr = (uintptr_t)hMod;
        uintptr_t rva = (uintptr_t)addr - baseAddr;

        fprintf(f, "=== STATIC INIT CRASH ===\n");
        fprintf(f, "Exception code : 0x%08lX\n", code);
        fprintf(f, "Fault address  : %p\n", addr);
        fprintf(f, "Module         : %s\n", modName);
        fprintf(f, "Module base    : %p\n", (void *)baseAddr);
        fprintf(f, "RVA            : 0x%llX\n", (unsigned long long)rva);

        if (code == EXCEPTION_ACCESS_VIOLATION &&
            ep->ExceptionRecord->NumberParameters >= 2)
        {
            const char *op = ep->ExceptionRecord->ExceptionInformation[0] == 0
                                 ? "READ"
                                 : "WRITE";
            fprintf(f, "Access type    : %s\n", op);
            fprintf(f, "Target address : %p\n",
                    (void *)ep->ExceptionRecord->ExceptionInformation[1]);
        }

        // Walk the stack (best-effort, symbols may not be loaded)
        fprintf(f, "\nStack trace:\n");
        CONTEXT *ctx = ep->ContextRecord;
        STACKFRAME64 sf = {};
        sf.AddrPC.Offset = ctx->Rip;
        sf.AddrPC.Mode = AddrModeFlat;
        sf.AddrStack.Offset = ctx->Rsp;
        sf.AddrStack.Mode = AddrModeFlat;
        sf.AddrFrame.Offset = ctx->Rbp;
        sf.AddrFrame.Mode = AddrModeFlat;

        HANDLE hProc = GetCurrentProcess();
        HANDLE hThread = GetCurrentThread();
        SymInitialize(hProc, nullptr, TRUE);

        for (int i = 0; i < 64; ++i)
        {
            if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, hProc, hThread,
                             &sf, ctx, nullptr,
                             SymFunctionTableAccess64,
                             SymGetModuleBase64, nullptr))
                break;

            DWORD64 pc = sf.AddrPC.Offset;
            if (pc == 0) break;

            char symBuf[sizeof(SYMBOL_INFO) + 256];
            SYMBOL_INFO *sym = (SYMBOL_INFO *)symBuf;
            sym->SizeOfStruct = sizeof(SYMBOL_INFO);
            sym->MaxNameLen = 255;

            DWORD64 disp = 0;
            HMODULE hFrameMod = nullptr;
            char frameModName[MAX_PATH] = "";
            GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                (LPCSTR)pc, &hFrameMod);
            GetModuleFileNameA(hFrameMod, frameModName, MAX_PATH);
            uintptr_t frameRva = pc - (uintptr_t)hFrameMod;

            if (SymFromAddr(hProc, pc, &disp, sym))
            {
                fprintf(f, "  [%2d] %s!%s + 0x%llX  (RVA 0x%llX)\n",
                        i, frameModName, sym->Name,
                        (unsigned long long)disp,
                        (unsigned long long)frameRva);
            }
            else
            {
                fprintf(f, "  [%2d] %s + RVA 0x%llX\n",
                        i, frameModName, (unsigned long long)frameRva);
            }
        }

        SymCleanup(hProc);
        fclose(f);
    }

    // ---- write a minidump ------------------------------------------------
    HANDLE hDump = CreateFileA("static_init_crash.dmp",
                               GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hDump != INVALID_HANDLE_VALUE)
    {
        MINIDUMP_EXCEPTION_INFORMATION mei;
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = ep;
        mei.ClientPointers = FALSE;
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                          hDump, MiniDumpNormal, &mei, nullptr, nullptr);
        CloseHandle(hDump);
    }

    return EXCEPTION_CONTINUE_SEARCH; // let it crash normally after logging
}

// This object's constructor runs during CRT static init (compiler phase),
// BEFORE any other static constructors in the program.
struct StaticInitGuardInstaller
{
    StaticInitGuardInstaller()
    {
        SetUnhandledExceptionFilter(StaticInitCrashHandler);
    }
};

static StaticInitGuardInstaller s_guard;
