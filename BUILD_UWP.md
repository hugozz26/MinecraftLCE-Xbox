# 🎮 Minecraft LCE no Xbox One — Guia Completo de Build UWP

> **Minecraft Legacy Console Edition (TU19)** rodando no Xbox One via Dev Mode.
> Fork do [smartcmd/MinecraftConsoles](https://github.com/smartcmd/MinecraftConsoles) com adaptação para UWP.

---

## 📋 Índice

1. [Visão Geral](#-visão-geral)
2. [Pré-requisitos](#-pré-requisitos)
3. [Estrutura do Projeto](#-estrutura-do-projeto)
4. [Mudanças Técnicas (PC → UWP)](#-mudanças-técnicas-pc--uwp)
5. [Build Passo a Passo](#-build-passo-a-passo)
6. [Empacotamento (.appx)](#-empacotamento-appx)
7. [Deploy no Xbox One](#-deploy-no-xbox-one)
8. [Troubleshooting](#-troubleshooting)
9. [Arquitetura](#-arquitetura)
10. [Status Atual](#-status-atual)

---

## 🔍 Visão Geral

Este projeto adapta o build de PC (Win32) do Minecraft Legacy Console Edition para
rodar como aplicativo UWP no **Xbox One em Dev Mode**. O jogo usa as bibliotecas
pré-compiladas da 4J Studios (render, input, storage) e o middleware Iggy (UI em SWF).

### O que funciona

- ✅ Build completa sem erros (MSVC 14.44, x64 Release)
- ✅ Empacotamento .appx assinado
- ✅ Instalação via `Add-AppxPackage` no PC e Device Portal no Xbox
- ✅ D3D11 device + swap chain criados com sucesso
- ✅ Arquivo `.arc` (28 MB, 378 SWFs) carregado do pacote
- ✅ UI Iggy inicializada (fontes, skins, 5 UIGroups)
- ✅ Tela de título (Panorama) renderizando
- ✅ Game loop rodando (~214 MB working set, `Responding=True`)
- ✅ Gamertag do Xbox usada como nome do jogador

### O que ainda precisa de trabalho

- ⏳ Input por gamepad (XboxGamepadInput.h existe mas não está conectado ao loop)
- ⏳ Áudio (mss64.dll não carrega — Miles Sound System)
- ⏳ Multiplayer (WinsockNetLayer compila mas não testado no Xbox)
- ⏳ Save/Load (paths de save precisam apontar para LocalState)

---

## 📦 Pré-requisitos

### 1. Visual Studio 2022 Community

Download: https://visualstudio.microsoft.com/downloads/

Workloads necessários na instalação:
- ✅ **Desenvolvimento para Desktop com C++**
- ✅ **Desenvolvimento para Plataforma Universal do Windows**
  - Na barra lateral, marcar também:
    - ✅ Ferramentas C++ (v143) para Plataforma Universal do Windows
    - ✅ Windows 10 SDK (10.0.22621.0)

### 2. CMake 3.24+

Download: https://cmake.org/download/

Na instalação, escolha **"Add CMake to the system PATH"**

### 3. Windows SDK 10.0.22621.0

> ⚠️ **IMPORTANTE**: Se você tiver o SDK 10.0.26100.0 instalado, ele pode estar
> corrompido e causar erros. Use especificamente o **10.0.22621.0**.

### 4. Xbox One em Dev Mode (para deploy no Xbox)

- Pagar $19 USD uma vez pelo Xbox Dev Mode app
- Xbox precisa estar em **Developer Mode**
- PC e Xbox na **mesma rede**
- Anotar o IP do Xbox (mostrado no Dev Home)

---

## 📁 Estrutura do Projeto

Arquivos adicionados/modificados em relação ao build PC original:

```
MinecraftConsoles-PC/
├── CMakeLists.txt              ← Build system UWP (substitui o do PC)
├── BUILD_UWP.md                ← Este documento
├── cmake/
│   ├── WorldSources.cmake      ← Lista de .cpp do Minecraft.World
│   └── ClientSources.cmake     ← Lista de .cpp do Minecraft.Client
├── UWP/                        ← 📁 NOVOS — Arquivos UWP
│   ├── UWP_App.cpp             ← Entry point Win32 HWND + D3D11 + game loop
│   ├── UWP_App.h               ← Header com globals (device, swap chain, etc.)
│   ├── XboxGamepadInput.h      ← Wrapper Windows.Gaming.Input (gamepad)
│   ├── stdafx_uwp.h            ← Include principal que junta pre + post
│   ├── stdafx_uwp_pre.h        ← #undefs UNICODE, força Desktop API partition
│   ├── stdafx_uwp_post.h       ← Macros de compatibilidade pós-windows.h
│   ├── Package.appxmanifest    ← Identidade do pacote UWP
│   ├── generate_placeholder_logos.bat
│   └── Assets/                 ← PNGs de logo para o pacote
│       ├── Square44x44Logo.png
│       ├── Square150x150Logo.png
│       ├── Wide310x150Logo.png
│       ├── Square310x310Logo.png
│       ├── SplashScreen.png
│       └── StoreLogo.png
├── Minecraft.Client/
│   ├── crt_compat.cpp          ← NOVO: stubs CRT para 4J libs
│   ├── stdafx.h                ← MODIFICADO: inclui stdafx_uwp.h quando _UWP
│   └── ...                     ← (demais arquivos inalterados)
├── Minecraft.World/
│   ├── File.cpp                ← MODIFICADO: CreateFileA para UWP sandbox
│   ├── FileInputStream.cpp     ← MODIFICADO: CreateFileA + FILE_SHARE_READ
│   ├── FileOutputStream.cpp    ← MODIFICADO: CreateFileA para UWP
│   ├── StringHelpers.cpp       ← MODIFICADO: wstringtofilename() com path absoluto
│   ├── stdafx.h                ← MODIFICADO: inclui stdafx_uwp.h quando _UWP
│   └── ...
```

---

## 🔧 Mudanças Técnicas (PC → UWP)

### Problema 1: Entry Point

| PC (Win32) | UWP (Xbox) |
|------------|------------|
| `_tWinMain()` + HWND msg loop | `main()` + `CreateWindowExW` + `PeekMessage` loop |
| `D3D11CreateDeviceAndSwapChain` | `D3D11CreateDevice` + `CreateSwapChainForHwnd` (DXGI 1.2) |
| XInput9_1_0 | `xinput.lib` (UWP umbrella) |

> **Por que não CoreApplication::Run()?**
> Tentamos `IFrameworkView` + `CoreWindow`, mas o `EntryPoint` no manifest precisa
> ser `windows.fullTrustApplication` (para as 4J libs desktop). Isso é incompatível
> com `CoreApplication::Run()`. Solução: Win32 HWND direto, que funciona no Dev Mode.

### Problema 2: UNICODE implícito

CMake com `CMAKE_SYSTEM_NAME=WindowsStore` define automaticamente `UNICODE` e `_UNICODE`.
Isso faz `CreateFile` → `CreateFileW`, mas o codebase usa `char*` (ANSI). Resultado:
`ERROR_PATH_NOT_FOUND` em tudo.

**Solução**: `stdafx_uwp_pre.h` faz `#undef UNICODE` e `#undef _UNICODE` **antes** de
`<windows.h>` ser incluído, forçando todas as macros Win32 para variante ANSI (A).

### Problema 3: CRT Mismatch

As 4J libs foram compiladas com `/MT` (CRT estática). UWP com `/ZW` (C++/CX) exige `/MD`
(CRT dinâmica). Solução: compilar com `/MD`, suprimir as CRT estáticas com `NODEFAULTLIB`,
e binary-patch as 4J libs para não exigirem `/MT`.

### Problema 4: File I/O no sandbox UWP

`GetFileAttributes()` e `GetFileAttributesEx()` são restritos no sandbox UWP.
`fopen()` e `CreateFileA()` funcionam normalmente para arquivos dentro do pacote.

**Solução**: Em `File.cpp`, substituir `GetFileAttributes` por `CreateFileA` + `CloseHandle`
para `exists()`, `length()` e `isDirectory()`.

### Problema 5: Paths relativos

O CWD no UWP é `C:\WINDOWS\system32`, não a pasta do exe. Paths relativos falham.

**Solução**: `wstringtofilename()` em `StringHelpers.cpp` prepende o path de instalação
do pacote (`g_PackageRootPath`) para paths relativos sob `#ifdef _UWP`.

---

## 🔨 Build Passo a Passo

### 1. Clonar o repositório

```powershell
git clone https://github.com/hugozz26/MinecraftLCE-Xbox.git
cd MinecraftLCE-Xbox
```

### 2. Configurar variáveis do SDK

> ⚠️ Execute isso em **cada nova sessão** de PowerShell:

```powershell
$env:WindowsSdkDir = 'C:\Program Files (x86)\Windows Kits\10\'
$env:WindowsSDKVersion = '10.0.22621.0\'
$env:WindowsSDKLibVersion = '10.0.22621.0\'
$env:UCRTVersion = '10.0.22621.0'
$env:UniversalCRTSdkDir = 'C:\Program Files (x86)\Windows Kits\10\'
```

### 3. Configurar CMake

```powershell
cmake -S . -B build_uwp `
    -G "Visual Studio 17 2022" `
    -A x64 `
    -DCMAKE_SYSTEM_NAME=WindowsStore `
    -DCMAKE_SYSTEM_VERSION=10.0.22621.0
```

### 4. Binary-patch das 4J libs

As 4J libs têm diretivas `FAILIFMISMATCH` para CRT estática. Precisa patchear para `/MD`:

```powershell
# Backup primeiro!
$libs = @(
    "Minecraft.Client\Windows64\4JLibs\libs\4J_Input.lib",
    "Minecraft.Client\Windows64\4JLibs\libs\4J_Storage.lib",
    "Minecraft.Client\Windows64\4JLibs\libs\4J_Render_PC.lib"
)

foreach ($lib in $libs) {
    Copy-Item $lib "$lib.bak" -Force
    $bytes = [System.IO.File]::ReadAllBytes($lib)
    # Patch "RuntimeLibrary" → "xuntimeLibrary" para ignorar a verificação
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

O exe será gerado em `build_uwp\Release\MinecraftLCE.exe` (~8 MB).

---

## 📦 Empacotamento (.appx)

### 1. Criar a pasta de layout

```powershell
$layout = "appx_layout"
New-Item -ItemType Directory -Path $layout -Force

# Copiar o exe
Copy-Item "build_uwp\Release\MinecraftLCE.exe" "$layout\" -Force

# Copiar o manifest (IMPORTANTE: editar antes — ver passo 2)
Copy-Item "UWP\Package.appxmanifest" "$layout\AppxManifest.xml" -Force

# Copiar assets do pacote
Copy-Item "UWP\Assets" "$layout\Assets" -Recurse -Force

# Copiar DLLs necessárias
Copy-Item "x64\Release\iggy_w64.dll" "$layout\" -Force
Copy-Item "x64\Release\mss64.dll" "$layout\" -Force

# Copiar D3DCompiler (necessário para shaders)
$d3dc = "C:\Program Files (x86)\Windows Kits\10\Redist\D3D\x64\D3DCompiler_47.dll"
if (Test-Path $d3dc) { Copy-Item $d3dc "$layout\" -Force }

# Copiar game data (texturas, SWFs, etc.)
$buildDir = "build_uwp"
robocopy "$buildDir\Common" "$layout\Common" /S /MT /R:0 /W:0 /NP
robocopy "$buildDir\Windows64Media" "$layout\Windows64Media" /S /MT /R:0 /W:0 /NP
robocopy "$buildDir\res" "$layout\res" /S /MT /R:0 /W:0 /NP
robocopy "$buildDir\Effects" "$layout\Effects" /S /MT /R:0 /W:0 /NP
robocopy "$buildDir\Schematics" "$layout\Schematics" /S /MT /R:0 /W:0 /NP
```

### 2. Editar o AppxManifest.xml

O manifest em `appx_layout\AppxManifest.xml` precisa de ajustes:

```xml
<!-- Identity: escolha um nome e gere um Publisher que bata com seu certificado -->
<Identity
    Name="MinecraftLCE.DevMode"
    Publisher="CN=MinecraftLCE"
    Version="1.6.0.0"
    ProcessorArchitecture="x64" />

<!-- Application: EntryPoint DEVE ser windows.fullTrustApplication -->
<Application Id="App"
    Executable="MinecraftLCE.exe"
    EntryPoint="windows.fullTrustApplication">
```

> ⚠️ O `Publisher` no manifest **DEVE** ser idêntico ao Subject do certificado!

### 3. Criar certificado auto-assinado

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

# Instalar na Trusted People (necessário para Add-AppxPackage no PC)
Import-PfxCertificate -FilePath "appx_output\MinecraftLCE.pfx" `
    -CertStoreLocation "Cert:\LocalMachine\TrustedPeople" `
    -Password $password
```

### 4. Empacotar e assinar

```powershell
$sdkBin = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64"

# Empacotar
& "$sdkBin\makeappx.exe" pack /d appx_layout /p appx_output\MinecraftLCE.appx /o

# Assinar
& "$sdkBin\signtool.exe" sign /fd SHA256 /a `
    /f appx_output\MinecraftLCE.pfx `
    /p minecraft `
    appx_output\MinecraftLCE.appx
```

### 5. Instalar localmente (teste no PC)

```powershell
# Remover versão anterior
Get-AppxPackage -Name 'MinecraftLCE*' | Remove-AppxPackage

# Instalar
Add-AppxPackage appx_output\MinecraftLCE.appx
```

---

## 🎯 Deploy no Xbox One

### Método 1: Xbox Device Portal (Web)

1. No Xbox em Dev Mode, abra **Dev Home** → habilite **Device Portal**
2. No PC, abra o navegador: `https://<ip-do-xbox>:11443`
3. Login com as credenciais mostradas no Dev Home
4. **My games & apps → Add**
5. Upload do `MinecraftLCE.appx`
6. Clique **Install**

### Método 2: WDP REST API (PowerShell)

```powershell
$xboxIp = "192.168.1.XXX"  # Substitua pelo IP do seu Xbox
$cred = Get-Credential       # Credenciais do Device Portal

Invoke-WebRequest `
    -Uri "https://${xboxIp}:11443/api/app/packagemanager/package" `
    -Method POST `
    -Credential $cred `
    -InFile "appx_output\MinecraftLCE.appx" `
    -ContentType "application/octet-stream" `
    -SkipCertificateCheck
```

### Método 3: Visual Studio Remote Debugging

1. **Projeto → Propriedades → Debugging**
2. **Machine Name**: IP do Xbox
3. **Authentication Mode**: Universal (Unencrypted Protocol)
4. **F5** para deploy + debug

---

## 🔧 Troubleshooting

### O app abre e fecha imediatamente

**Causa provável**: `EntryPoint` no manifest não é `windows.fullTrustApplication`.

```xml
<!-- ❌ ERRADO -->
<Application EntryPoint="MinecraftLCE.App">

<!-- ✅ CORRETO -->
<Application EntryPoint="windows.fullTrustApplication">
```

### `ERROR_PATH_NOT_FOUND` (error 3) no CreateFile

**Causa**: `UNICODE` definido, `CreateFile` → `CreateFileW` recebendo `char*`.

**Solução**: Verificar que `stdafx_uwp_pre.h` tem `#undef UNICODE` e é incluído
**antes** de `<windows.h>` (via `stdafx.h`).

### Archive carrega 0 arquivos

**Causa**: `FileInputStream` falha ao abrir o `.arc`.

**Verificar**:
1. `File::exists()` retorna 1? Se não → `CreateFileA` vs `CreateFileW`
2. `FileInputStream` usa `FILE_SHARE_READ`? Se share=0, conflita com `File::length()`
3. Path absoluto? `wstringtofilename()` deve prepender `g_PackageRootPath`

### Texturas MipMapLevel2 falham ao carregar

**Esperado e inofensivo**. Os arquivos `*MipMapLevel2.png` não existem no pacote
de dados. O jogo funciona normalmente sem eles (usa só mipmap nível 0).

### mss64.dll não carrega (sem áudio)

O Miles Sound System (`mss64.dll`) está no pacote mas `GetModuleHandle` retorna NULL.
O `LoadLibrary` pode ser restrito no sandbox UWP — investigar alternativas.

### Como ler o log de debug

```powershell
# No PC:
$log = "$env:LOCALAPPDATA\Packages\MinecraftLCE.DevMode_1q6a01qngb1pp\LocalState\mc_debug.log"

# Filtrar spam de SetupFont:
Get-Content $log | Where-Object { $_ -notmatch 'SetupFont' } | Select-Object -Last 50
```

---

## 🏗️ Arquitetura

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
│  │  (lógica do jogo inalterada)              │   │
│  │                                            │   │
│  │  4J_Render_PC.lib  ← render D3D11         │   │
│  │  4J_Input.lib      ← input XInput         │   │
│  │  4J_Storage.lib    ← save/load            │   │
│  │  iggy_w64.lib      ← UI SWF (Scaleform)  │   │
│  └───────────────────────────────────────────┘   │
└──────────────────────────────────────────────────┘
```

### Defines de compilação

| Define | Propósito |
|--------|-----------|
| `_UWP` | Ativa todas as adaptações UWP (paths, file I/O, entry point) |
| `_WINDOWS64` | Build x64 Windows (herdado do PC) |
| `_CONTENT_PACKAGE` | Desabilita `__debugbreak()` em código de conteúdo |
| `_LARGE_WORLDS` | Suporte a mundos grandes (herdado) |
| `_CRT_SECURE_NO_WARNINGS` | Suprime warnings de funções C "inseguras" |

### Flags de compilação

| Flag | Motivo |
|------|--------|
| `/ZW` | Habilita C++/CX (ref classes, `^` pointers) — necessário para UWP APIs |
| `/MD` | CRT dinâmica (obrigatório com `/ZW`) |
| `/EHsc` | Exceções C++ padrão |
| `/bigobj` | .obj grandes (Minecraft.World tem muitos símbolos) |
| `NODEFAULTLIB:libcmt` | Suprime CRT estática que as 4J libs pedem |

---

## 📊 Status Atual

**Última build testada**: Março 2026

| Componente | Status |
|------------|--------|
| Build / Compilação | ✅ Zero erros |
| Empacotamento .appx | ✅ Funciona |
| D3D11 + Swap Chain | ✅ Funciona |
| Carregamento de texturas | ✅ PNGs via fopen |
| Arquivo .arc (378 SWFs) | ✅ 28 MB carregados |
| UI Iggy (fontes, menus) | ✅ Inicializada |
| Tela de título (Panorama) | ✅ Renderiza |
| Game loop | ✅ Estável (~214 MB) |
| Gamertag Xbox | ✅ Lida automaticamente |
| Input Gamepad | ⏳ Em desenvolvimento |
| Áudio (Miles) | ⏳ mss64.dll não carrega |
| Multiplayer | ⏳ Não testado |
| Save / Load | ⏳ Paths precisam adaptação |

---

## 🤝 Contribuindo

Contribuições são bem-vindas! Áreas que mais precisam de ajuda:

1. **Input por gamepad** — Conectar `XboxGamepadInput.h` ao `InputManager`
2. **Áudio** — Fazer o Miles Sound System funcionar ou substituir por XAudio2
3. **Save/Load** — Adaptar paths para `LocalState` do UWP
4. **Testes no Xbox real** — Validar tudo no hardware

---

## 📜 Créditos

- Código-fonte original: [Minecraft Legacy Console Edition](https://archive.org/details/minecraft-legacy-console-edition-source-code)
- Port para PC: [smartcmd/MinecraftConsoles](https://github.com/smartcmd/MinecraftConsoles)
- Adaptação UWP/Xbox: Este repositório
