# 🎮 Minecraft LCE on Xbox One — Full UWP Build Guide

> **Minecraft Legacy Console Edition (TU19)** running on Xbox One via Dev Mode.
> Fork of [smartcmd/MinecraftConsoles](https://github.com/smartcmd/MinecraftConsoles) with UWP adaptation.

---

## 📋 Table of Contents

1. [Overview](#-overview)
2. [Prerequisites](#-prerequisites)
3. [Project Structure](#-project-structure)
4. [Technical Changes (PC → UWP)](#-technical-changes-pc--uwp)
5. [Step-by-Step Build](#-step-by-step-build)
6. [Packaging (.appx)](#-packaging-appx)
7. [Deploy to Xbox One](#-deploy-to-xbox-one)
8. [Troubleshooting](#-troubleshooting)
9. [Architecture](#-architecture)
10. [Current Status](#-current-status)

---

## 🔍 Overview

This project adapts the PC (Win32) build of Minecraft Legacy Console Edition to
run as a UWP app on **Xbox One in Dev Mode**. The game uses pre-compiled 4J Studios
libraries (render, input, storage) and the Iggy middleware (SWF-based UI).

### What works

- ✅ Full build with zero errors (MSVC 14.44, x64 Release)
- ✅ Signed `.appx` packaging
- ✅ Installation via `Add-AppxPackage` on PC and Device Portal on Xbox
- ✅ D3D11 device + swap chain created successfully
- ✅ Archive file (`.arc` — 28 MB, 378 SWFs) loaded from package
- ✅ Iggy UI initialized (fonts, skins, 5 UIGroups)
- ✅ Title screen (Panorama) rendering
- ✅ Game loop running (~214 MB working set, `Responding=True`)
- ✅ Xbox gamertag used as player name

### What still needs work

- ⏳ Gamepad input (`XboxGamepadInput.h` exists but not connected to the loop)
- ⏳ Audio (`mss64.dll` won't load — Miles Sound System)
- ⏳ Multiplayer (WinsockNetLayer compiles but not tested on Xbox)
- ⏳ Save/Load (save paths need to point to LocalState)

---

## 📦 Prerequisites

### 1. Visual Studio 2022 Community

Download: https://visualstudio.microsoft.com/downloads/

Required workloads during installation:
- ✅ **Desktop development with C++**
- ✅ **Universal Windows Platform development**
  - In the sidebar, also check:
    - ✅ C++ (v143) Universal Windows Platform tools
    - ✅ Windows 10 SDK (10.0.22621.0)

### 2. CMake 3.24+

Download: https://cmake.org/download/

During installation, choose **"Add CMake to the system PATH"**

### 3. Windows SDK 10.0.22621.0

> ⚠️ **IMPORTANT**: If you have SDK 10.0.26100.0 installed, it may be
> corrupted and cause errors. Use specifically **10.0.22621.0**.

### 4. Xbox One in Dev Mode (for Xbox deployment)

- Pay $19 USD once for the Xbox Dev Mode app
- Xbox must be in **Developer Mode**
- PC and Xbox on the **same network**
- Note the Xbox IP (shown in Dev Home)

---

## 📁 Project Structure

Files added/modified relative to the original PC build:

```
MinecraftConsoles-PC/
├── CMakeLists.txt              ← UWP build system (replaces the PC one)
├── BUILD_UWP.md                ← This document
├── cmake/
│   ├── WorldSources.cmake      ← Minecraft.World .cpp file list
│   └── ClientSources.cmake     ← Minecraft.Client .cpp file list
├── UWP/                        ← 📁 NEW — UWP files
│   ├── UWP_App.cpp             ← Win32 HWND entry point + D3D11 + game loop
│   ├── UWP_App.h               ← Header with globals (device, swap chain, etc.)
│   ├── XboxGamepadInput.h      ← Windows.Gaming.Input wrapper (gamepad)
│   ├── stdafx_uwp.h            ← Main include that combines pre + post
│   ├── stdafx_uwp_pre.h        ← #undefs UNICODE, forces Desktop API partition
│   ├── stdafx_uwp_post.h       ← Compatibility macros post-windows.h
│   ├── Package.appxmanifest    ← UWP package identity
│   ├── generate_placeholder_logos.bat
│   └── Assets/                 ← Package logo PNGs
│       ├── Square44x44Logo.png
│       ├── Square150x150Logo.png
│       ├── Wide310x150Logo.png
│       ├── Square310x310Logo.png
│       ├── SplashScreen.png
│       └── StoreLogo.png
├── Minecraft.Client/
│   ├── crt_compat.cpp          ← NEW: CRT stubs for 4J libs
│   ├── stdafx.h                ← MODIFIED: includes stdafx_uwp.h when _UWP
│   └── ...                     ← (other files unchanged)
├── Minecraft.World/
│   ├── File.cpp                ← MODIFIED: CreateFileA for UWP sandbox
│   ├── FileInputStream.cpp     ← MODIFIED: CreateFileA + FILE_SHARE_READ
│   ├── FileOutputStream.cpp    ← MODIFIED: CreateFileA for UWP
│   ├── StringHelpers.cpp       ← MODIFIED: wstringtofilename() with absolute path
│   ├── stdafx.h                ← MODIFIED: includes stdafx_uwp.h when _UWP
│   └── ...
```

---

## 🔧 Technical Changes (PC → UWP)

### Problem 1: Entry Point

| PC (Win32) | UWP (Xbox) |
|------------|------------|
| `_tWinMain()` + HWND msg loop | `main()` + `CreateWindowExW` + `PeekMessage` loop |
| `D3D11CreateDeviceAndSwapChain` | `D3D11CreateDevice` + `CreateSwapChainForHwnd` (DXGI 1.2) |
| XInput9_1_0 | `xinput.lib` (UWP umbrella) |

> **Why not CoreApplication::Run()?**
> We tried `IFrameworkView` + `CoreWindow`, but the `EntryPoint` in the manifest must
> be `windows.fullTrustApplication` (required for the 4J desktop libs). This is incompatible
> with `CoreApplication::Run()`. Solution: direct Win32 HWND, which works in Dev Mode.

### Problem 2: Implicit UNICODE

CMake with `CMAKE_SYSTEM_NAME=WindowsStore` automatically defines `UNICODE` and `_UNICODE`.
This makes `CreateFile` → `CreateFileW`, but the codebase uses `char*` (ANSI). Result:
`ERROR_PATH_NOT_FOUND` on everything.

**Solution**: `stdafx_uwp_pre.h` does `#undef UNICODE` and `#undef _UNICODE` **before**
`<windows.h>` is included, forcing all Win32 macros to the ANSI (A) variant.

### Problem 3: CRT Mismatch

The 4J libs were compiled with `/MT` (static CRT). UWP with `/ZW` (C++/CX) requires `/MD`
(dynamic CRT). Solution: compile with `/MD`, suppress the static CRTs with `NODEFAULTLIB`,
and binary-patch the 4J libs so they don't enforce `/MT`.

### Problem 4: File I/O in UWP sandbox

`GetFileAttributes()` and `GetFileAttributesEx()` are restricted in the UWP sandbox.
`fopen()` and `CreateFileA()` work normally for files inside the package.

**Solution**: In `File.cpp`, replace `GetFileAttributes` with `CreateFileA` + `CloseHandle`
for `exists()`, `length()` and `isDirectory()`.

### Problem 5: Relative paths

The CWD in UWP is `C:\WINDOWS\system32`, not the exe folder. Relative paths fail.

**Solution**: `wstringtofilename()` in `StringHelpers.cpp` prepends the package installation
path (`g_PackageRootPath`) for relative paths under `#ifdef _UWP`.

---

## 🔨 Step-by-Step Build

### 1. Clone the repository

```powershell
git clone https://github.com/hugozz26/MinecraftLCE-Xbox.git
cd MinecraftLCE-Xbox
```

### 2. Set SDK environment variables

> ⚠️ Run this in **each new** PowerShell session:

```powershell
$env:WindowsSdkDir = 'C:\Program Files (x86)\Windows Kits\10\'
$env:WindowsSDKVersion = '10.0.22621.0\'
$env:WindowsSDKLibVersion = '10.0.22621.0\'
$env:UCRTVersion = '10.0.22621.0'
$env:UniversalCRTSdkDir = 'C:\Program Files (x86)\Windows Kits\10\'
```

### 3. Configure CMake

```powershell
cmake -S . -B build_uwp `
    -G "Visual Studio 17 2022" `
    -A x64 `
    -DCMAKE_SYSTEM_NAME=WindowsStore `
    -DCMAKE_SYSTEM_VERSION=10.0.22621.0
```

### 4. Binary-patch the 4J libs

The 4J libs have `FAILIFMISMATCH` directives for static CRT. They need to be patched for `/MD`:

```powershell
# Backup first!
$libs = @(
    "Minecraft.Client\Windows64\4JLibs\libs\4J_Input.lib",
    "Minecraft.Client\Windows64\4JLibs\libs\4J_Storage.lib",
    "Minecraft.Client\Windows64\4JLibs\libs\4J_Render_PC.lib"
)

foreach ($lib in $libs) {
    Copy-Item $lib "$lib.bak" -Force
    $bytes = [System.IO.File]::ReadAllBytes($lib)
    # Patch "RuntimeLibrary" → "xuntimeLibrary" to bypass the check
    for ($i = 0; $i -lt $bytes.Length - 14; $i++) {
        if ($bytes[$i] -eq 0x52 -and     # R
            $bytes[$i+1] -eq 0x75 -and   # u
            $bytes[$i+2] -eq 0x6E -and   # n
            $bytes[$i+3] -eq 0x74 -and   # t
            $bytes[$i+4] -eq 0x69 -and   # i
            $bytes[$i+5] -eq 0x6D -and   # m
            $bytes[$i+6] -eq 0x65 -and   # e
            $bytes[$i+7] -eq 0x4C) {     # L
            $bytes[$i] = 0x78  # R → x
            Write-Host "Patched at offset $i in $lib"
        }
    }
    [System.IO.File]::WriteAllBytes($lib, $bytes)
}
```

### 5. Build

```powershell
cmake --build build_uwp --config Release 2>&1 | Tee-Object -FilePath build_log.txt
```

The exe will be generated at `build_uwp\Release\MinecraftLCE.exe` (~8 MB).

---

## 📦 Packaging (.appx)

### 1. Create the layout folder

```powershell
$layout = "appx_layout"
New-Item -ItemType Directory -Path $layout -Force

# Copy the exe
Copy-Item "build_uwp\Release\MinecraftLCE.exe" "$layout\" -Force

# Copy the manifest (IMPORTANT: edit it first — see step 2)
Copy-Item "UWP\Package.appxmanifest" "$layout\AppxManifest.xml" -Force

# Copy package assets
Copy-Item "UWP\Assets" "$layout\Assets" -Recurse -Force

# Copy required DLLs
Copy-Item "x64\Release\iggy_w64.dll" "$layout\" -Force
Copy-Item "x64\Release\mss64.dll" "$layout\" -Force

# Copy D3DCompiler (needed for shaders)
$d3dc = "C:\Program Files (x86)\Windows Kits\10\Redist\D3D\x64\D3DCompiler_47.dll"
if (Test-Path $d3dc) { Copy-Item $d3dc "$layout\" -Force }

# Copy game data (textures, SWFs, etc.)
$buildDir = "build_uwp"
robocopy "$buildDir\Common" "$layout\Common" /S /MT /R:0 /W:0 /NP
robocopy "$buildDir\Windows64Media" "$layout\Windows64Media" /S /MT /R:0 /W:0 /NP
robocopy "$buildDir\res" "$layout\res" /S /MT /R:0 /W:0 /NP
robocopy "$buildDir\Effects" "$layout\Effects" /S /MT /R:0 /W:0 /NP
robocopy "$buildDir\Schematics" "$layout\Schematics" /S /MT /R:0 /W:0 /NP
```

### 2. Edit AppxManifest.xml

The manifest at `appx_layout\AppxManifest.xml` needs adjustments:

```xml
<!-- Identity: choose a name and generate a Publisher that matches your certificate -->
<Identity
    Name="MinecraftLCE.DevMode"
    Publisher="CN=MinecraftLCE"
    Version="1.6.0.0"
    ProcessorArchitecture="x64" />

<!-- Application: EntryPoint MUST be windows.fullTrustApplication -->
<Application Id="App"
    Executable="MinecraftLCE.exe"
    EntryPoint="windows.fullTrustApplication">
```

> ⚠️ The `Publisher` in the manifest **MUST** be identical to the certificate Subject!

### 3. Create a self-signed certificate

```powershell
$cert = New-SelfSignedCertificate `
    -Type Custom `
    -Subject "CN=MinecraftLCE" `
    -KeyUsage DigitalSignature `
    -FriendlyName "MinecraftLCE Dev Certificate" `
    -CertStoreLocation "Cert:\CurrentUser\My" `
    -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}")

$password = ConvertTo-SecureString -String "minecraft" -Force -AsPlainText
New-Item -ItemType Directory -Path "appx_output" -Force
Export-PfxCertificate -Cert $cert -FilePath "appx_output\MinecraftLCE.pfx" -Password $password

# Install to Trusted People (required for Add-AppxPackage on PC)
Import-PfxCertificate -FilePath "appx_output\MinecraftLCE.pfx" `
    -CertStoreLocation "Cert:\LocalMachine\TrustedPeople" `
    -Password $password
```

### 4. Package and sign

```powershell
$sdkBin = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64"

# Package
& "$sdkBin\makeappx.exe" pack /d appx_layout /p appx_output\MinecraftLCE.appx /o

# Sign
& "$sdkBin\signtool.exe" sign /fd SHA256 /a `
    /f appx_output\MinecraftLCE.pfx `
    /p minecraft `
    appx_output\MinecraftLCE.appx
```

### 5. Install locally (PC testing)

```powershell
# Remove previous version
Get-AppxPackage -Name 'MinecraftLCE*' | Remove-AppxPackage

# Install
Add-AppxPackage appx_output\MinecraftLCE.appx
```

---

## 🎯 Deploy to Xbox One

### Method 1: Xbox Device Portal (Web)

1. On the Xbox in Dev Mode, open **Dev Home** → enable **Device Portal**
2. On your PC, open browser: `https://<xbox-ip>:11443`
3. Log in with the credentials shown in Dev Home
4. **My games & apps → Add**
5. Upload `MinecraftLCE.appx`
6. Click **Install**

### Method 2: WDP REST API (PowerShell)

```powershell
$xboxIp = "192.168.1.XXX"  # Replace with your Xbox IP
$cred = Get-Credential       # Device Portal credentials

Invoke-WebRequest `
    -Uri "https://${xboxIp}:11443/api/app/packagemanager/package" `
    -Method POST `
    -Credential $cred `
    -InFile "appx_output\MinecraftLCE.appx" `
    -ContentType "application/octet-stream" `
    -SkipCertificateCheck
```

### Method 3: Visual Studio Remote Debugging

1. **Project → Properties → Debugging**
2. **Machine Name**: Xbox IP
3. **Authentication Mode**: Universal (Unencrypted Protocol)
4. **F5** to deploy + debug

---

## 🔧 Troubleshooting

### App opens and closes immediately

**Likely cause**: `EntryPoint` in the manifest is not `windows.fullTrustApplication`.

```xml
<!-- ❌ WRONG -->
<Application EntryPoint="MinecraftLCE.App">

<!-- ✅ CORRECT -->
<Application EntryPoint="windows.fullTrustApplication">
```

### `ERROR_PATH_NOT_FOUND` (error 3) in CreateFile

**Cause**: `UNICODE` defined, `CreateFile` → `CreateFileW` receiving `char*`.

**Solution**: Verify that `stdafx_uwp_pre.h` has `#undef UNICODE` and is included
**before** `<windows.h>` (via `stdafx.h`).

### Archive loads 0 files

**Cause**: `FileInputStream` fails to open the `.arc`.

**Check**:
1. Does `File::exists()` return 1? If not → `CreateFileA` vs `CreateFileW`
2. Does `FileInputStream` use `FILE_SHARE_READ`? If share=0, it conflicts with `File::length()`
3. Absolute path? `wstringtofilename()` should prepend `g_PackageRootPath`

### MipMapLevel2 textures fail to load

**Expected and harmless**. The `*MipMapLevel2.png` files don't exist in the
data package. The game works normally without them (uses only mipmap level 0).

### mss64.dll won't load (no audio)

The Miles Sound System (`mss64.dll`) is in the package but `GetModuleHandle` returns NULL.
`LoadLibrary` may be restricted in the UWP sandbox — investigating alternatives.

### How to read the debug log

```powershell
# On PC:
$log = "$env:LOCALAPPDATA\Packages\MinecraftLCE.DevMode_1q6a01qngb1pp\LocalState\mc_debug.log"

# Filter SetupFont spam:
Get-Content $log | Where-Object { $_ -notmatch 'SetupFont' } | Select-Object -Last 50
```

---

## 🏗️ Architecture

```
┌──────────────────────────────────────────────────┐
│            UWP_App.cpp (Win32 HWND)              │
│  ┌──────────────┐   ┌────────────────────────┐   │
│  │ CreateWindow  │   │ D3D11 Device           │   │
│  │ ExW + PeekMsg │   │ + SwapChainForHwnd     │   │
│  │ loop          │   │ (DXGI 1.2)             │   │
│  └──────┬───────┘   └───────────┬────────────┘   │
│         │                       │                 │
│  ┌──────▼───────────────────────▼────────────┐   │
│  │         GameTick_Win32()                   │   │
│  │  InputManager.Tick() → Minecraft::tick()   │   │
│  │  → UI::tick/render → Present               │   │
│  └───────────────────────────────────────────┘   │
│         │                                         │
│  ┌──────▼────────────────────────────────────┐   │
│  │  Minecraft.Client + Minecraft.World       │   │
│  │  (game logic unchanged)                   │   │
│  │                                            │   │
│  │  4J_Render_PC.lib  ← D3D11 rendering      │   │
│  │  4J_Input.lib      ← XInput input          │   │
│  │  4J_Storage.lib    ← save/load             │   │
│  │  iggy_w64.lib      ← SWF UI (Scaleform)   │   │
│  └───────────────────────────────────────────┘   │
└──────────────────────────────────────────────────┘
```

### Compilation defines

| Define | Purpose |
|--------|---------|
| `_UWP` | Enables all UWP adaptations (paths, file I/O, entry point) |
| `_WINDOWS64` | x64 Windows build (inherited from PC) |
| `_CONTENT_PACKAGE` | Disables `__debugbreak()` in content code |
| `_LARGE_WORLDS` | Large world support (inherited) |
| `_CRT_SECURE_NO_WARNINGS` | Suppresses "unsafe" C function warnings |

### Compiler flags

| Flag | Reason |
|------|--------|
| `/ZW` | Enables C++/CX (ref classes, `^` pointers) — required for UWP APIs |
| `/MD` | Dynamic CRT (mandatory with `/ZW`) |
| `/EHsc` | Standard C++ exceptions |
| `/bigobj` | Large .obj files (Minecraft.World has many symbols) |
| `NODEFAULTLIB:libcmt` | Suppresses static CRT that 4J libs request |

---

## 📊 Current Status

**Last tested build**: March 2026

| Component | Status |
|-----------|--------|
| Build / Compilation | ✅ Zero errors |
| .appx Packaging | ✅ Works |
| D3D11 + Swap Chain | ✅ Works |
| Texture loading | ✅ PNGs via fopen |
| Archive .arc (378 SWFs) | ✅ 28 MB loaded |
| Iggy UI (fonts, menus) | ✅ Initialized |
| Title screen (Panorama) | ✅ Renders |
| Game loop | ✅ Stable (~214 MB) |
| Xbox Gamertag | ✅ Read automatically |
| Gamepad Input | ⏳ Work in progress |
| Audio (Miles) | ⏳ mss64.dll won't load |
| Multiplayer | ⏳ Not tested |
| Save / Load | ⏳ Paths need adaptation |

---

## 🤝 Contributing

Contributions are welcome! Areas that need the most help:

1. **Gamepad input** — Connect `XboxGamepadInput.h` to `InputManager`
2. **Audio** — Get Miles Sound System working or replace with XAudio2
3. **Save/Load** — Adapt paths to UWP `LocalState`
4. **Real Xbox testing** — Validate everything on hardware

---

## 📜 Credits

- Original source code: [Minecraft Legacy Console Edition](https://archive.org/details/minecraft-legacy-console-edition-source-code)
- PC port: [smartcmd/MinecraftConsoles](https://github.com/smartcmd/MinecraftConsoles)
- UWP/Xbox adaptation: This repository
