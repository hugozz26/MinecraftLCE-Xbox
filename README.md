# 🎮 MinecraftLCE-Xbox

**Minecraft Legacy Console Edition (TU19) running on Xbox One via Dev Mode**

> Fork of [smartcmd/MinecraftConsoles](https://github.com/smartcmd/MinecraftConsoles)
> with a full UWP adaptation for Xbox One.

---

## ⚡ What is this?

This repository contains the source code of Minecraft Legacy Console Edition (v1.6.0560.0 / TU19)
adapted to compile and run as a **UWP** app on **Xbox One in Developer Mode**.

The original project is a PC port of the console version. This fork adds a UWP platform layer
that allows packaging the game as an `.appx` and sideloading it onto an Xbox One.

### 📸 Current Status

| | |
|---|---|
| 🏗️ Build | ✅ Compiles with zero errors (VS2022, x64 Release) |
| 📦 Package | ✅ Signed `.appx`, installable |
| 🎨 Rendering | ✅ D3D11 + title screen working |
| 🎭 UI (Iggy/SWF) | ✅ 378 SWFs loaded, menus render |
| 🔄 Game Loop | ✅ Running stable (~214 MB) |
| 🏷️ Gamertag | ✅ Xbox gamertag used as player name |
| 🎮 Gamepad | ⏳ Work in progress |
| 🔊 Audio | ⏳ Miles Sound System won't load |
| 💾 Save/Load | ⏳ Needs path adaptation |
| 🌐 Multiplayer | ⏳ Not tested on Xbox |

---

## 🚀 Quick Start

### Prerequisites

- **Visual Studio 2022** with C++ Desktop + UWP workloads
- **CMake 3.24+**
- **Windows SDK 10.0.22621.0**
- Xbox One in **Dev Mode** (for deployment — optional for PC build/testing)

### Build

```powershell
# 1. Clone
git clone https://github.com/hugozz26/MinecraftLCE-Xbox.git
cd MinecraftLCE-Xbox

# 2. Set SDK environment variables (run in each new PowerShell session)
$env:WindowsSdkDir = 'C:\Program Files (x86)\Windows Kits\10\'
$env:WindowsSDKVersion = '10.0.22621.0\'
$env:WindowsSDKLibVersion = '10.0.22621.0\'
$env:UCRTVersion = '10.0.22621.0'
$env:UniversalCRTSdkDir = 'C:\Program Files (x86)\Windows Kits\10\'

# 3. Configure CMake
cmake -S . -B build_uwp -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0.22621.0

# 4. Build
cmake --build build_uwp --config Release
```

> 📖 **Full guide with packaging, signing, and Xbox deployment:**
> See **[BUILD_UWP.md](BUILD_UWP.md)**

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────┐
│         UWP_App.cpp (Win32 HWND)            │
│  CreateWindowExW + PeekMessage loop         │
│  D3D11 Device + SwapChainForHwnd            │
├─────────────────────────────────────────────┤
│         GameTick_Win32()                    │
│  Input → Minecraft::tick() → UI → Present  │
├─────────────────────────────────────────────┤
│  Minecraft.Client + Minecraft.World         │
│  (game logic unchanged)                     │
│                                             │
│  4J_Render_PC.lib · 4J_Input.lib            │
│  4J_Storage.lib · iggy_w64.lib              │
└─────────────────────────────────────────────┘
```

The entire game (Client + World) is **unchanged**. Only the platform layer
(entry point, D3D11, input, file I/O) was adapted for UWP.

---

## 📁 UWP File Structure

```
UWP/
├── UWP_App.cpp           ← Entry point + D3D11 + game loop + logger
├── UWP_App.h             ← Globals (device, context, swap chain)
├── XboxGamepadInput.h    ← Windows.Gaming.Input wrapper
├── stdafx_uwp.h          ← Combines pre.h + <windows.h> + post.h
├── stdafx_uwp_pre.h      ← #undef UNICODE, forces Desktop API partition
├── stdafx_uwp_post.h     ← Compatibility macros
├── Package.appxmanifest  ← Package identity
└── Assets/               ← Package logos (placeholder)
```

---

## 🔧 Technical Challenges Solved

| Problem | Solution |
|---------|----------|
| `CoreApplication::Run()` incompatible with fullTrustApplication | Direct Win32 HWND (`CreateWindowExW`) |
| UNICODE implicitly defined by CMake WindowsStore | `#undef UNICODE` in `stdafx_uwp_pre.h` |
| CRT mismatch (4J libs `/MT` vs UWP `/MD`) | Binary-patch 4J libs + `NODEFAULTLIB` |
| `GetFileAttributes` restricted in UWP sandbox | Replaced with `CreateFileA` + `CloseHandle` |
| Relative paths fail (CWD = system32) | `wstringtofilename()` prepends package path |
| `__debugbreak()` crashes in release | `_CONTENT_PACKAGE` define disables it |

---

## 🤝 Contributing

Contributions are very welcome! Areas that need help:

1. 🎮 **Gamepad input** — Connect `XboxGamepadInput.h` to `InputManager`
2. 🔊 **Audio** — Get Miles Sound System working or replace with XAudio2
3. 💾 **Save/Load** — Adapt paths to UWP `LocalState` folder
4. 🧪 **Real Xbox testing** — Validate everything on hardware

---

## 📜 Credits

- Original source code: [Minecraft LCE](https://archive.org/details/minecraft-legacy-console-edition-source-code) (TU19)
- PC port: [smartcmd/MinecraftConsoles](https://github.com/smartcmd/MinecraftConsoles)
- Community: [Discord MinecraftConsoles](https://discord.gg/jrum7HhegA)
- UWP/Xbox adaptation: [@hugozz26](https://github.com/hugozz26)
