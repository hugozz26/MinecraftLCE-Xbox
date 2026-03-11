// ============================================================================
// stdafx_uwp_pre.h — Preprocessor-only overrides for UWP build
// ============================================================================
// This file MUST be included BEFORE <windows.h>.
// It contains ONLY preprocessor defines — no types, no functions, no macros
// that override Win32 function names (those go in stdafx_uwp_post.h).
// ============================================================================
#pragma once

#ifdef _UWP

// ---------------------------------------------------------------------------
// Force Desktop API partition so Win32 headers expose CreateFile, MoveFile,
// GetFileSize, GlobalMemoryStatus, etc.  These symbols ARE present in
// WindowsApp.lib — the headers just hide them under WINAPI_FAMILY_APP.
// ---------------------------------------------------------------------------
#ifndef WINAPI_PARTITION_DESKTOP
#define WINAPI_PARTITION_DESKTOP 1
#endif

#ifdef WINAPI_FAMILY
#undef WINAPI_FAMILY
#endif
#define WINAPI_FAMILY WINAPI_FAMILY_DESKTOP_APP

// ---------------------------------------------------------------------------
// Force ANSI (non-Unicode) Win32 API macros.
// CMake's WindowsStore target implicitly defines UNICODE and _UNICODE, which
// makes CreateFile → CreateFileW, GetFileAttributes → GetFileAttributesW, etc.
// This codebase was written for ANSI char* paths (via wstringtofilename()),
// so we undefine UNICODE to get CreateFile → CreateFileA, etc.
// ---------------------------------------------------------------------------
#ifdef UNICODE
#undef UNICODE
#endif
#ifdef _UNICODE
#undef _UNICODE
#endif

#endif // _UWP
