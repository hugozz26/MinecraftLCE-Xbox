// ============================================================================
// stdafx_uwp.h — Legacy wrapper: includes both pre and post UWP headers
// ============================================================================
// Prefer using stdafx_uwp_pre.h (before windows.h) and stdafx_uwp_post.h
// (after windows.h) directly. This file is kept for backward compatibility.
// ============================================================================
#pragma once

#include "stdafx_uwp_pre.h"
// NOTE: stdafx_uwp_post.h requires Win32 types (BOOL, LPCSTR, etc.)
// It should only be included AFTER <windows.h>
// #include "stdafx_uwp_post.h"
