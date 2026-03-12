# MinecraftLCE Xbox - Progress Session 3 (11/Mar/2026)
## Status: CRASH DUMP ANALYZED - ROOT CAUSE FOUND (different from expected!)

---

## 🔥 CRITICAL DISCOVERY FROM CRASH DUMP

The crash dump (`MinecraftLCE.exe.1140.dmp`, 414MB) revealed the **REAL** crash cause:

### It's NOT the USER32.dll delay-load crash we thought!

**Actual crash:** `FAST_FAIL_INVALID_ARG` (code `c0000409`, subcode `0x5`) in `ucrtbase!invoke_watson`
- A **NULL pointer was passed to `sprintf_s`** (a "safe" CRT string formatting function)
- `sprintf_s` detects invalid args and calls `invoke_watson` → `__fastfail(FAST_FAIL_INVALID_ARG)`
- This is an **uncatchable** crash (bypasses SEH `__except` handlers)
- Happened on **thread 33** (worker thread, not main thread!) — likely chunk generation/world loading

### Stack trace from dump (without PDB symbols):
```
ucrtbase!invoke_watson+0x18                          ← CRASH HERE (int 29h = __fastfail)
ucrtbase!_stdio_common_vswprintf+0x1241              ← sprintf_s detected NULL arg
ucrtbase!_stdio_common_vsprintf_s+0x324
MinecraftLCE+0x53E95  (MinecraftLCE!IsIconic+0x23cf5)  ← OUR CODE called sprintf_s with NULL
MinecraftLCE+0x3CEE07 (MinecraftLCE!IsIconic+0x37ec67)
MinecraftLCE+0x38B706 (MinecraftLCE!IsIconic+0x35b566)
MinecraftLCE+0x243A0E (MinecraftLCE!IsIconic+0x21386e)
MinecraftLCE+0x2830EE (MinecraftLCE!IsIconic+0x252f4e)
MinecraftLCE+0x343BA7 (MinecraftLCE!IsIconic+0x313a07)
MinecraftLCE+0x4027F  (MinecraftLCE!IsIconic+0x100df)
MinecraftLCE+0xF6634  (MinecraftLCE!IsIconic+0xc6494)
MinecraftLCE+0xF5D9A  (MinecraftLCE!IsIconic+0xc5bfa)
MinecraftLCE+0xF7228  (MinecraftLCE!IsIconic+0xc7088)
MinecraftLCE+0x3CF190 (MinecraftLCE!IsIconic+0x37eff0)
ntdll!RtlUserThreadStart+0x42
```

### Key info from dump:
- **OS:** Windows 10 Build 26100.7010 (`xb_flt_2602ge` = Xbox firmware!)
- **Process uptime:** 39 seconds (crashed shortly after launch)
- **MinecraftLCE.exe base:** `0x00007ff762320000` — `0x00007ff762aed000` (7.9MB range)
- **Loaded modules confirm:** mss64.dll ✓, iggy_w64.dll ✓, user32.dll ✓ (it loaded!), XINPUT1_4.dll ✓

### What this means:
1. **USER32.dll DID load on Xbox** — our delay-load approach works, the DLL exists on Xbox
2. Our 3 fixes (stubs.cpp, UIScene, KeyboardMouseInput) were probably unnecessary for the crash (but still good safety guards)
3. The crash is a **NULL string being passed to sprintf_s** somewhere in game code
4. It's on a worker thread → almost certainly during **world generation / chunk loading**

---

## 🔧 IMMEDIATE NEXT STEP: Rebuild with PDB symbols

We need PDB symbols to map the crash addresses to actual source code function names.

### CMake configure for PDB build (ALREADY DONE ✓):
```powershell
cd "C:\Users\hugod\Downloads\mc-console-oct2014\MinecraftConsoles-PC"
$env:WindowsSDKVersion = '10.0.22621.0\'
$env:WindowsSdkDir = 'C:\Program Files (x86)\Windows Kits\10\'
$env:UCRTVersion = '10.0.22621.0'
$env:UniversalCRTSdkDir = 'C:\Program Files (x86)\Windows Kits\10\'

# Clean and reconfigure
Remove-Item build -Recurse -Force -ErrorAction SilentlyContinue

& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" `
  -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_SYSTEM_NAME=WindowsStore `
  -DCMAKE_SYSTEM_VERSION="10.0.22621.0" `
  "-DCMAKE_CXX_FLAGS_RELEASE=/MD /O2 /Ob2 /DNDEBUG /Zi" `
  "-DCMAKE_C_FLAGS_RELEASE=/MD /O2 /Ob2 /DNDEBUG /Zi" `
  "-DCMAKE_EXE_LINKER_FLAGS_RELEASE=/DEBUG:FULL"
```

### Build command:
```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" `
  --build build --config Release -- /m:2
```

**Note:** The configure was done successfully. The build was attempted but may need re-running.
The build had only 17 OBJs when it stopped — might have been a transient issue or terminal collision.
**Just re-run the build command above.**

### After build succeeds:
1. PDB file will be at: `build\Release\MinecraftLCE.pdb`
2. Analyze dump with symbols:
```powershell
& "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe" `
  -y "C:\Users\hugod\Downloads\mc-console-oct2014\MinecraftConsoles-PC\build\Release" `
  -z "C:\Users\hugod\Downloads\MinecraftLCE.exe.1140.dmp" `
  -c ".ecxr; kP; q"
```
This will give us **exact function names, file names, and line numbers** for every frame in the crash stack.

---

## 📋 What was done this session:

### Code fixes applied (still in source, still valid as safety guards):
1. **stubs.cpp** `Mouse::getY()` — `#ifdef _UWP` guard using `g_iScreenHeight`
2. **UIScene_AbstractContainerMenu.cpp** `getMouseToSWFScale()` — null guard on `g_hWnd`
3. **KeyboardMouseInput.cpp** `Init()` — `#ifndef _UWP` around `RegisterRawInputDevices`
4. **UWP_App.cpp** `GameTick()` — verbose diagnostic logging for first 10 frames

### Build & Package:
- Built MinecraftLCE.exe (7.5MB) ✓
- Packaged as D:\MinecraftLCE_xbox.appx (454.6MB) ✓
- Signed with MinecraftLCE.pfx ✓
- User tested on Xbox → **still crashes** when creating world

### Crash dump analysis:
- Downloaded MinecraftLCE.exe.1140.dmp (414MB) from Xbox Device Portal
- Analyzed with cdb.exe → found real crash cause (sprintf_s NULL arg)
- Need PDB rebuild to get function names

---

## 📁 Important file locations:
- **Crash dump:** `C:\Users\hugod\Downloads\MinecraftLCE.exe.1140.dmp` (414MB)
- **Source code:** `C:\Users\hugod\Downloads\mc-console-oct2014\MinecraftConsoles-PC\`
- **Appx output:** `D:\MinecraftLCE_xbox.appx`
- **PFX cert:** `MinecraftConsoles-PC\appx_output\MinecraftLCE.pfx` (password: `minecraft`)
- **Xbox Device Portal:** `https://192.168.15.7:11443/`
- **Package ID:** `MinecraftLCE.XboxDevMode_1.6.0.0_x64__1q6a01qngb1pp`
- **Log download:** `curl.exe -k -o xbox_debug3.log "https://192.168.15.7:11443/ext/app/files?knownfolderid=LocalAppData&packagefullname=MinecraftLCE.XboxDevMode_1.6.0.0_x64__1q6a01qngb1pp&path=%5Cminecraft_log.txt"`

---

## 🎯 Game plan for next session:
1. **Rebuild with /Zi + /DEBUG:FULL** → get MinecraftLCE.pdb
2. **Re-analyze crash dump with PDB** → get exact function + line number of the sprintf_s call
3. **Fix the NULL string** being passed to sprintf_s (likely a path, filename, or format string)
4. **Rebuild, repackage, sign, test on Xbox**
5. If world loads → investigate no sound issue next

---

## 🔑 Build environment reminders:
- **MUST use VS-bundled cmake** at `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`
- **MUST set SDK env vars** before cmake configure (see above)
- **MUST use SDK 10.0.22621.0** (26100 has broken ucrtd.lib)
- Build takes ~15-20 minutes with `/m:2`
- After build: copy exe to appx_layout → makeappx pack → signtool sign
