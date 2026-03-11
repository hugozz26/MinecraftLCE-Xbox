# 🎮 MinecraftLCE-Xbox

**Minecraft Legacy Console Edition (TU19) rodando no Xbox One via Dev Mode**

> Fork do [smartcmd/MinecraftConsoles](https://github.com/smartcmd/MinecraftConsoles)
> com adaptação completa para UWP / Xbox One.

---

## ⚡ O que é isso?

Este repositório contém o código-fonte do Minecraft Legacy Console Edition (v1.6.0560.0 / TU19)
adaptado para compilar e rodar como aplicativo **UWP** no **Xbox One em Developer Mode**.

O projeto original é um port para PC da versão de console. Este fork adiciona uma camada UWP
que permite empacotar o jogo como `.appx` e instalar no Xbox One.

### 📸 Status

| | |
|---|---|
| 🏗️ Build | ✅ Compila sem erros (VS2022, x64 Release) |
| 📦 Package | ✅ `.appx` assinado e instalável |
| 🎨 Renderização | ✅ D3D11 + tela de título funcionando |
| 🎭 UI (Iggy/SWF) | ✅ 378 SWFs carregados, menus renderizam |
| 🔄 Game Loop | ✅ Rodando estável (~214 MB) |
| 🏷️ Gamertag | ✅ Nome do Xbox usado automaticamente |
| 🎮 Gamepad | ⏳ Em desenvolvimento |
| 🔊 Áudio | ⏳ Miles Sound System não carrega |
| 💾 Save/Load | ⏳ Precisa adaptação de paths |
| 🌐 Multiplayer | ⏳ Não testado no Xbox |

---

## 🚀 Quick Start

### Pré-requisitos

- **Visual Studio 2022** com workloads C++ Desktop + UWP
- **CMake 3.24+**
- **Windows SDK 10.0.22621.0**
- Xbox One em **Dev Mode** (para deploy — opcional para build/teste no PC)

### Build

```powershell
# 1. Clonar
git clone https://github.com/hugozz26/MinecraftLCE-Xbox.git
cd MinecraftLCE-Xbox

# 2. Configurar SDK (rodar em cada sessão PowerShell)
$env:WindowsSdkDir = 'C:\Program Files (x86)\Windows Kits\10\'
$env:WindowsSDKVersion = '10.0.22621.0\'
$env:WindowsSDKLibVersion = '10.0.22621.0\'
$env:UCRTVersion = '10.0.22621.0'
$env:UniversalCRTSdkDir = 'C:\Program Files (x86)\Windows Kits\10\'

# 3. Configurar CMake
cmake -S . -B build_uwp -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0.22621.0

# 4. Build
cmake --build build_uwp --config Release
```

> 📖 **Guia completo com empacotamento, assinatura e deploy no Xbox:**
> Veja **[BUILD_UWP.md](BUILD_UWP.md)**

---

## 🏗️ Arquitetura

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
│  (lógica do jogo inalterada)                │
│                                             │
│  4J_Render_PC.lib · 4J_Input.lib            │
│  4J_Storage.lib · iggy_w64.lib              │
└─────────────────────────────────────────────┘
```

O jogo inteiro (Client + World) é **inalterado**. Apenas a camada de plataforma
(entry point, D3D11, input, file I/O) foi adaptada para UWP.

---

## 📁 Estrutura dos Arquivos UWP

```
UWP/
├── UWP_App.cpp           ← Entry point + D3D11 + game loop + logger
├── UWP_App.h             ← Globals (device, context, swap chain)
├── XboxGamepadInput.h    ← Windows.Gaming.Input wrapper
├── stdafx_uwp.h          ← Junta pre.h + <windows.h> + post.h
├── stdafx_uwp_pre.h      ← #undef UNICODE, força Desktop API partition
├── stdafx_uwp_post.h     ← Macros de compatibilidade
├── Package.appxmanifest  ← Identidade do pacote
└── Assets/               ← Logos do pacote (placeholder)
```

---

## 🔧 Desafios Técnicos Resolvidos

| Problema | Solução |
|----------|---------|
| `CoreApplication::Run()` incompatível com fullTrustApplication | Win32 HWND direto (`CreateWindowExW`) |
| UNICODE implícito pelo CMake WindowsStore | `#undef UNICODE` em `stdafx_uwp_pre.h` |
| CRT mismatch (4J libs `/MT` vs UWP `/MD`) | Binary-patch das 4J libs + `NODEFAULTLIB` |
| `GetFileAttributes` restrito no UWP | Substituído por `CreateFileA` + `CloseHandle` |
| Paths relativos falham (CWD = system32) | `wstringtofilename()` prepende path do pacote |
| `__debugbreak()` crasha em release | `_CONTENT_PACKAGE` define desabilita |

---

## 🤝 Contribuindo

Contribuições são muito bem-vindas! Áreas que precisam de ajuda:

1. 🎮 **Input por gamepad** — Conectar `XboxGamepadInput.h` ao `InputManager`
2. 🔊 **Áudio** — Fazer Miles Sound System funcionar ou substituir por XAudio2
3. 💾 **Save/Load** — Adaptar paths para `LocalState` do pacote UWP
4. 🧪 **Testes no Xbox real** — Validar tudo no hardware

---

## 📜 Créditos

- Código-fonte original: [Minecraft LCE](https://archive.org/details/minecraft-legacy-console-edition-source-code) (TU19)
- Port para PC: [smartcmd/MinecraftConsoles](https://github.com/smartcmd/MinecraftConsoles)
- Comunidade: [Discord MinecraftConsoles](https://discord.gg/jrum7HhegA)
- Adaptação UWP/Xbox: [@hugozz26](https://github.com/hugozz26)
