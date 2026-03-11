// ============================================================================
// stdafx_uwp_post.h — Function stubs for UWP build (needs Win32 types)
// ============================================================================
// This file MUST be included AFTER <windows.h> because the inline stubs
// use BOOL, LPCSTR, HWND, OutputDebugStringA, etc., and some macros
// override Win32 function names that are declared in windows headers.
// ============================================================================
#pragma once

#ifdef _UWP

// ---- SetCurrentDirectoryA — UWP apps have a fixed working directory ------
#ifndef UWP_STUB_SETCURRENTDIR
#define UWP_STUB_SETCURRENTDIR
inline BOOL UWP_SetCurrentDirectoryA(LPCSTR) { return TRUE; }
#define SetCurrentDirectoryA UWP_SetCurrentDirectoryA
#endif

// SetProcessDPIAware — not needed on Xbox
#ifndef UWP_STUB_DPI
#define UWP_STUB_DPI
#define SetProcessDPIAware() ((void)0)
#endif

// DialogBox / GetDlgItemText / EndDialog — no modal dialogs on Xbox
// Must be after windows.h so they don't clobber the declarations in winuser.h
#ifndef UWP_STUB_DIALOG
#define UWP_STUB_DIALOG
#undef DialogBox
#define DialogBox(inst, res, hwnd, proc) ((INT_PTR)0)
#undef GetDlgItemText
#define GetDlgItemText(hwnd, id, buf, max) 0
#undef EndDialog
#define EndDialog(hwnd, code) ((void)0)
#endif

// ---- MessageBoxA replacement — write to debug output ---------------------
#ifndef UWP_STUB_MESSAGEBOX
#define UWP_STUB_MESSAGEBOX
inline int UWP_MessageBoxA(void*, const char* text, const char* caption, unsigned int)
{
    OutputDebugStringA("[MessageBox] ");
    if (caption) OutputDebugStringA(caption);
    OutputDebugStringA(": ");
    if (text) OutputDebugStringA(text);
    OutputDebugStringA("\n");
    return 0;
}
#define MessageBoxA(hwnd, text, caption, type) UWP_MessageBoxA(hwnd, text, caption, type)
#endif

// RegisterClassExW / CreateWindowExW — window management is done by CoreWindow
#ifndef UWP_STUB_WINDOW
#define UWP_STUB_WINDOW
#endif

// ShellScalingApi — not available on UWP
#ifndef UWP_STUB_SHELL
#define UWP_STUB_SHELL
#endif

// GetFocus — stubs for mouse grab logic (not applicable on Xbox)
#ifndef UWP_STUB_GETFOCUS
#define UWP_STUB_GETFOCUS
#endif

// GlobalMemoryStatus — deprecated but still present; no stub needed
#ifndef UWP_STUB_MEM
#define UWP_STUB_MEM
#endif

#endif // _UWP
