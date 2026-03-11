// ============================================================================
// UWP_App.cpp — Xbox One Dev Mode UWP Application
// ============================================================================
// Replaces Windows64_Minecraft.cpp for the UWP/Xbox build.
//
// KEY DIFFERENCES vs Win32:
//   • CoreWindow instead of HWND for the D3D11 swap chain
//   • IFrameworkView instead of WinMain + message pump
//   • Xbox gamertag fetched from Windows.System.User API
//   • No keyboard/mouse input (controller only on Xbox)
//   • No XInput9_1_0 — 4J_Input handles gamepad natively
// ============================================================================

#include "stdafx.h"
#include "UWP_App.h"

#include <ppltasks.h>
#include <assert.h>
#include <fstream>
#include <cstdarg>
// NOTE: <collection.h> removed — it causes ABI::Windows::UI::Color redefinition
//       when mixed with game headers. Not actually needed.

// ============================================================================
// FILE-BASED CRASH LOGGER — writes to LocalState folder so we can see where
// the app crashes even without a debugger attached.
// ============================================================================
static std::ofstream g_logFile;
static void LogInit()
{
    if (g_logFile.is_open()) return;  // Already initialized
    try {
        auto localFolder = Windows::Storage::ApplicationData::Current->LocalFolder;
        auto pathW = localFolder->Path;
        char pathA[512];
        WideCharToMultiByte(CP_ACP, 0, pathW->Data(), -1, pathA, 512, nullptr, nullptr);
        strcat_s(pathA, "\\mc_debug.log");
        g_logFile.open(pathA, std::ios::out | std::ios::trunc);
        if (g_logFile.is_open())
            g_logFile << "=== MinecraftLCE Debug Log ===" << std::endl;
        OutputDebugStringA("UWP: Log file at: ");
        OutputDebugStringA(pathA);
        OutputDebugStringA("\n");
    } catch (...) {
        OutputDebugStringA("UWP: Could not open log file!\n");
    }
}
void LogMsg(const char* fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    OutputDebugStringA(buf);
    if (g_logFile.is_open()) {
        g_logFile << buf;
        g_logFile.flush();
    }
}

// Global crash handler — logs SEH exceptions before the process dies
static LONG WINAPI CrashFilter(EXCEPTION_POINTERS* ep)
{
    if (ep && ep->ExceptionRecord) {
        DWORD code = ep->ExceptionRecord->ExceptionCode;
        void* addr = ep->ExceptionRecord->ExceptionAddress;
        LogMsg("*** UNHANDLED EXCEPTION: Code=0x%08X Address=%p ***\n", code, addr);
        if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2) {
            ULONG_PTR rw = ep->ExceptionRecord->ExceptionInformation[0]; // 0=read, 1=write
            ULONG_PTR target = ep->ExceptionRecord->ExceptionInformation[1];
            LogMsg("*** ACCESS VIOLATION: %s at address 0x%p ***\n",
                   rw == 0 ? "READ" : (rw == 1 ? "WRITE" : "DEP"), (void*)target);
        }
    } else {
        LogMsg("*** UNHANDLED EXCEPTION (no info) ***\n");
    }
    g_logFile.flush();
    return EXCEPTION_CONTINUE_SEARCH; // let WER also handle it
}

// ---------------------------------------------------------------------------
// Game headers — stdafx.h already pulls in most things via the precompiled
// header chain.  We only need a few extras that Windows64_Minecraft.cpp
// explicitly includes.
// ---------------------------------------------------------------------------
// These use include-dir-relative paths (Minecraft.Client is an include dir).
#include "MinecraftServer.h"
#include "LocalPlayer.h"
#include "Minecraft.h"
#include "ChatScreen.h"
#include "User.h"
#include "StatsCounter.h"
#include "ConnectScreen.h"
#include "Tesselator.h"
#include "Options.h"
#include "Textures.h"
#include "Settings.h"
#include "GameRenderer.h"
#include "Common/PostProcesser.h"
#include "Windows64/Network/WinsockNetLayer.h"
#include "Common/UI/UI.h"

// Minecraft.World types used in InitialiseMinecraftRuntime_UWP / GameTick
#include "OldChunkStorage.h"
#include "Level.h"
#include "Tile.h"
#include "IntCache.h"
#include "AABB.h"
#include "Vec3.h"
#include "Compression.h"
// Windows64_App.h and Windows64_UIController.h are already included via stdafx.h

// 4J_Render internal renderer (defined in the pre-compiled .lib)
class Renderer;
extern Renderer InternalRenderManager;

// ============================================================================
// GLOBAL VARIABLES expected by the rest of the game code
// ============================================================================
// The game accesses these raw pointers everywhere. On Win32 they live in
// Windows64_Minecraft.cpp; since that file is excluded from the UWP build
// we must provide them here.
// ============================================================================

// Global package root path — used by BufferedImage, FileInputStream, etc.
char g_PackageRootPath[512] = {};

HINSTANCE               hMyInst = nullptr;
char                    chGlobalText[256] = {};
uint16_t                ui16GlobalText[256] = {};

BOOL                    g_bWidescreen = TRUE;

int                     g_iScreenWidth  = 1920;
int                     g_iScreenHeight = 1080;
int                     g_rScreenWidth  = 1920;
int                     g_rScreenHeight = 1080;
float                   g_iAspectRatio  = 1920.0f / 1080.0f;

char                    g_Win64Username[17]  = "XboxPlayer";
wchar_t                 g_Win64UsernameW[17] = L"XboxPlayer";

// D3D11 globals that InitialiseMinecraftRuntime() and many game functions use
HINSTANCE               g_hInst = nullptr;
HWND                    g_hWnd  = nullptr;   // stays nullptr on UWP — not used
D3D_DRIVER_TYPE         g_driverType   = D3D_DRIVER_TYPE_HARDWARE;
D3D_FEATURE_LEVEL       g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device*           g_pd3dDevice          = nullptr;
ID3D11DeviceContext*    g_pImmediateContext    = nullptr;
IDXGISwapChain*         g_pSwapChain          = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView   = nullptr;
ID3D11DepthStencilView* g_pDepthStencilView   = nullptr;
ID3D11Texture2D*        g_pDepthStencilBuffer = nullptr;

// Profile settings array (same as Windows64_Minecraft.cpp)
#define NUM_PROFILE_VALUES   5
#define NUM_PROFILE_SETTINGS 4
DWORD dwProfileSettingsA[NUM_PROFILE_VALUES] = { 0, 0, 0, 0, 0 };

// ---------------------------------------------------------------------------
// Stubs for Win32 functions referenced by game code but not on UWP
// ---------------------------------------------------------------------------
// GetGlobalText / SeedEditBox are called from game UI code
uint16_t* GetGlobalText()
{
    char* p = (char*)ui16GlobalText;
    for (int i = 0; i < 256; i++) p[i * 2] = chGlobalText[i];
    return ui16GlobalText;
}
void SeedEditBox() {}   // no dialog boxes on Xbox

// IsIconic / GetFocus stubs (Win32 desktop only)
#ifdef _UWP
extern "C" BOOL WINAPI IsIconic(HWND) { return FALSE; }
#endif

// MemSect — profiling stub. The real implementation (in Windows64_Minecraft.cpp)
// tracks memory allocations per section using PIX counters. On UWP we don't
// need it, so provide an empty stub.
void MemSect(int) {}

// ClearGlobalText — referenced by 4J_Input.lib. The real implementation
// (in Windows64_Minecraft.cpp) clears the global text buffers.
void ClearGlobalText()
{
    memset(chGlobalText, 0, 256);
    memset(ui16GlobalText, 0, 512);
}

// ============================================================================
// XBOX GAMERTAG FETCHER
// ============================================================================
// Uses Windows.System.User to get the current Xbox user's display name
// and copies it into g_Win64Username / g_Win64UsernameW so the game
// shows the real gamertag.
// ============================================================================
static void FetchXboxGamertag()
{
    // NOTE: Must use fully-qualified Windows::System::User to avoid
    //       conflict with the game's own User.h class.
    using namespace concurrency;

    try
    {
        // C++/CX IAsyncOperation^ doesn't have .get() —
        // must wrap in concurrency::create_task() first.
        auto users = create_task(
            Windows::System::User::FindAllAsync()).get();
        if (users->Size > 0)
        {
            auto user = users->GetAt(0);

            auto prop = create_task(
                user->GetPropertyAsync(
                    Windows::System::KnownUserProperties::DisplayName)).get();

            if (prop != nullptr)
            {
                Platform::String^ displayName = safe_cast<Platform::String^>(prop);
                if (displayName->Length() > 0)
                {
                    // Copy to wide buffer (max 16 chars)
                    int len = displayName->Length();
                    if (len > 16) len = 16;
                    wcsncpy_s(g_Win64UsernameW, 17, displayName->Data(), len);
                    g_Win64UsernameW[len] = L'\0';

                    // Copy to ANSI buffer
                    WideCharToMultiByte(CP_ACP, 0, g_Win64UsernameW, -1,
                                        g_Win64Username, 17, nullptr, nullptr);
                    g_Win64Username[16] = '\0';
                }
            }
        }
    }
    catch (Platform::Exception^)
    {
        OutputDebugStringA("UWP: Could not fetch Xbox gamertag, using default.\n");
        LogMsg("UWP: Could not fetch Xbox gamertag, using default.\n");
    }
}

// ============================================================================
// GAME INIT (mirrors InitialiseMinecraftRuntime from Windows64_Minecraft.cpp)
// ============================================================================
static Minecraft* InitialiseMinecraftRuntime_UWP()
{
    LogMsg("UWP: InitialiseMinecraftRuntime_UWP START\n");

    LogMsg("UWP: Calling app.loadMediaArchive()...\n");
    app.loadMediaArchive();
    LogMsg("UWP: loadMediaArchive OK\n");

    LogMsg("UWP: Skipping RenderManager.Initialise (already done in CreateDeviceAndSwapChain)\n");
    // RenderManager.Initialise already called in CreateDeviceAndSwapChain_HWND
    LogMsg("UWP: RenderManager.Initialise OK (skipped)\n");

    LogMsg("UWP: Calling app.loadStringTable()...\n");
    app.loadStringTable();
    LogMsg("UWP: loadStringTable OK\n");

    LogMsg("UWP: Calling ui.init() with device=%p ctx=%p rtv=%p dsv=%p %dx%d...\n",
           g_pd3dDevice, g_pImmediateContext, g_pRenderTargetView, g_pDepthStencilView,
           g_rScreenWidth, g_rScreenHeight);

    // Check if iggy_w64.dll is loaded
    {
        HMODULE hIggy = GetModuleHandleA("iggy_w64.dll");
        LogMsg("UWP: iggy_w64.dll module handle = %p\n", hIggy);
        HMODULE hMss = GetModuleHandleA("mss64.dll");
        LogMsg("UWP: mss64.dll module handle = %p\n", hMss);
    }

    g_logFile.flush(); // flush before risky call
    ui.init(g_pd3dDevice, g_pImmediateContext,
            g_pRenderTargetView, g_pDepthStencilView,
            g_rScreenWidth, g_rScreenHeight);
    LogMsg("UWP: ui.init OK\n");

    InputManager.Initialise(1, 3, MINECRAFT_ACTION_MAX, ACTION_MAX_MENU);
    // No KBM on Xbox — skip g_KBMInput.Init() / DefineActions() that need HWND
    InputManager.SetJoypadMapVal(0, 0);
    InputManager.SetKeyRepeatRate(0.3f, 0.2f);
    LogMsg("UWP: InputManager OK\n");

    ProfileManager.Initialise(TITLEID_MINECRAFT,
                              app.m_dwOfferID,
                              PROFILE_VERSION_10,
                              NUM_PROFILE_VALUES,
                              NUM_PROFILE_SETTINGS,
                              dwProfileSettingsA,
                              app.GAME_DEFINED_PROFILE_DATA_BYTES * XUSER_MAX_COUNT,
                              &app.uiGameDefinedDataChangedBitmask);
    ProfileManager.SetDefaultOptionsCallback(
        &CConsoleMinecraftApp::DefaultOptionsCallback, (LPVOID)&app);
    LogMsg("UWP: ProfileManager OK\n");

    LogMsg("UWP: Calling g_NetworkManager.Initialise()...\n");
    g_NetworkManager.Initialise();
    LogMsg("UWP: NetworkManager OK\n");

    // Set up local player gamertags — player 0 gets Xbox gamertag
    for (int i = 0; i < MINECRAFT_NET_MAX_PLAYERS; i++)
    {
        IQNet::m_player[i].m_smallId     = static_cast<BYTE>(i);
        IQNet::m_player[i].m_isRemote    = false;
        IQNet::m_player[i].m_isHostPlayer = (i == 0);
        swprintf_s(IQNet::m_player[i].m_gamertag, 32, L"Player%d", i);
    }
    wcscpy_s(IQNet::m_player[0].m_gamertag, 32, g_Win64UsernameW);

    WinsockNetLayer::Initialize();
    ProfileManager.SetDebugFullOverride(true);

    // Thread-local storage (same as Win32 path)
    Tesselator::CreateNewThreadStorage(1024 * 1024);
    AABB::CreateNewThreadStorage();
    Vec3::CreateNewThreadStorage();
    IntCache::CreateNewThreadStorage();
    Compression::CreateNewThreadStorage();
    OldChunkStorage::CreateNewThreadStorage();
    Level::enableLightingCache();
    Tile::CreateNewThreadStorage();

    LogMsg("UWP: Calling Minecraft::main()...\n");
    Minecraft::main();
    LogMsg("UWP: Minecraft::main() returned, getting instance...\n");
    Minecraft* pMinecraft = Minecraft::GetInstance();
    if (!pMinecraft)
    {
        LogMsg("UWP: ERROR — Minecraft::GetInstance() returned NULL!\n");
        return nullptr;
    }

    LogMsg("UWP: Calling app.InitGameSettings()...\n");
    app.InitGameSettings();
    LogMsg("UWP: Calling app.InitialiseTips()...\n");
    app.InitialiseTips();
    LogMsg("UWP: InitialiseMinecraftRuntime_UWP DONE\n");
    return pMinecraft;
}

// ============================================================================
// AppSource
// ============================================================================
namespace MinecraftLCE
{

IFrameworkView^ AppSource::CreateView() { LogMsg("UWP: CreateView() called\n"); return ref new App(); }

// ============================================================================
// App — constructor
// ============================================================================
App::App()
    : m_windowClosed(false)
    , m_windowVisible(true)
    , m_pMinecraft(nullptr)
    , m_gameInitialized(false)
{
    LogMsg("UWP: App::App() constructor\n");
}

// ============================================================================
// IFrameworkView::Initialize
// ============================================================================
void App::Initialize(CoreApplicationView^ applicationView)
{
    LogInit();
    SetUnhandledExceptionFilter(CrashFilter);
    LogMsg("UWP: Initialize() START\n");

    // ---- Package root path (needed by BufferedImage, FileInputStream, etc.) ----
    {
        char cwd[512] = {};
        GetCurrentDirectoryA(512, cwd);
        LogMsg("UWP: CWD = %s\n", cwd);

        auto pkg = Windows::ApplicationModel::Package::Current;
        auto installPath = pkg->InstalledLocation->Path;
        char installPathA[512] = {};
        WideCharToMultiByte(CP_ACP, 0, installPath->Data(), -1, installPathA, 512, nullptr, nullptr);
        LogMsg("UWP: Package install path = %s\n", installPathA);

        strncpy(g_PackageRootPath, installPathA, 511);
        g_PackageRootPath[511] = '\0';

        SetCurrentDirectoryA(installPathA);
        LogMsg("UWP: CWD set to package install path\n");
    }

    applicationView->Activated +=
        ref new TypedEventHandler<CoreApplicationView^, IActivatedEventArgs^>(
            this, &App::OnActivated);

    CoreApplication::Suspending +=
        ref new EventHandler<SuspendingEventArgs^>(this, &App::OnSuspending);

    CoreApplication::Resuming +=
        ref new EventHandler<Platform::Object^>(this, &App::OnResuming);

    LogMsg("UWP: Initialize() DONE\n");
}

// ============================================================================
// IFrameworkView::SetWindow
// ============================================================================
void App::SetWindow(CoreWindow^ window)
{
    LogMsg("UWP: SetWindow() START\n");
    m_window = window;

    window->SizeChanged +=
        ref new TypedEventHandler<CoreWindow^, WindowSizeChangedEventArgs^>(
            this, &App::OnWindowSizeChanged);
    window->VisibilityChanged +=
        ref new TypedEventHandler<CoreWindow^, VisibilityChangedEventArgs^>(
            this, &App::OnVisibilityChanged);
    window->Closed +=
        ref new TypedEventHandler<CoreWindow^, CoreWindowEventArgs^>(
            this, &App::OnWindowClosed);
    LogMsg("UWP: SetWindow() DONE\n");
}

// ============================================================================
// IFrameworkView::Load — create D3D, fetch gamertag
// ============================================================================
void App::Load(Platform::String^ /*entryPoint*/)
{
    LogMsg("UWP: Load() START\n");

    // Fetch the Xbox gamertag BEFORE creating any game objects
    LogMsg("UWP: Calling FetchXboxGamertag...\n");
    FetchXboxGamertag();
    LogMsg("UWP: Gamertag = %s\n", g_Win64Username);

    // Create D3D11 device + CoreWindow swap chain
    LogMsg("UWP: Calling CreateDeviceAndSwapChain...\n");
    CreateDeviceAndSwapChain();
    LogMsg("UWP: Load() DONE\n");
}

// ============================================================================
// IFrameworkView::Run — main game loop (replaces Win32 message pump)
// ============================================================================
void App::Run()
{
    LogMsg("UWP: Run() START\n");

    // Initialise all game subsystems (same order as Win32 path)
    LogMsg("UWP: Calling InitialiseMinecraftRuntime_UWP...\n");
    m_pMinecraft = InitialiseMinecraftRuntime_UWP();
    if (!m_pMinecraft)
    {
        LogMsg("UWP: FATAL — InitialiseMinecraftRuntime_UWP failed!\n");
        return;
    }
    m_gameInitialized = true;
    LogMsg("UWP: Entering game loop\n");

    while (!m_windowClosed && !app.m_bShutdown)
    {
        if (m_windowVisible)
        {
            // Process all pending UWP events (replaces PeekMessage/DispatchMessage)
            CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(
                CoreProcessEventsOption::ProcessAllIfPresent);

            GameTick();
        }
        else
        {
            // Suspended / not visible — block until an event wakes us
            CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(
                CoreProcessEventsOption::ProcessOneAndAllPending);
        }
    }
}

// ============================================================================
// IFrameworkView::Uninitialize
// ============================================================================
void App::Uninitialize()
{
    if (g_pImmediateContext) g_pImmediateContext->ClearState();
    // Resources are released by the game's own shutdown path
}

// ============================================================================
// GameTick — ONE frame (faithful reproduction of the Win32 main loop)
// ============================================================================
void App::GameTick()
{
    if (!m_gameInitialized || !m_pMinecraft) return;

    RenderManager.StartFrame();

    app.UpdateTime();

    // Input (4J_Input handles XInput/Windows.Gaming.Input internally)
    InputManager.Tick();

    StorageManager.Tick();
    RenderManager.Tick();

    g_NetworkManager.DoWork();

    // Game logic
    if (app.GetGameStarted())
    {
        m_pMinecraft->run_middle();
        app.SetAppPaused(
            g_NetworkManager.IsLocalGame() &&
            g_NetworkManager.GetPlayerCount() == 1 &&
            ui.IsPauseMenuDisplayed(ProfileManager.GetPrimaryPad()));
    }
    else
    {
        m_pMinecraft->soundEngine->tick(nullptr, 0.0f);
        m_pMinecraft->textures->tick(true, false);
        IntCache::Reset();
        if (app.GetReallyChangingSessionType())
            m_pMinecraft->tickAllConnections();
    }

    m_pMinecraft->soundEngine->playMusicTick();

    ui.tick();
    ui.render();

    m_pMinecraft->gameRenderer->ApplyGammaPostProcess();

    // Present
    RenderManager.Present();

    ui.CheckMenuDisplayed();

    app.HandleXuiActions();

    // Trial timer
    if (!ProfileManager.IsFullVersion())
    {
        if (app.GetGameStarted())
        {
            if (app.IsAppPaused())
                app.UpdateTrialPausedTimer();
            ui.UpdateTrialTimer(ProfileManager.GetPrimaryPad());
        }
    }

    Vec3::resetPool();
}

// ============================================================================
// D3D11 Device + CoreWindow SwapChain (UWP / Xbox compatible)
// ============================================================================
void App::CreateDeviceAndSwapChain()
{
    LogMsg("UWP: CreateDeviceAndSwapChain() START\n");
    HRESULT hr = S_OK;

    // ---------- Device creation (no HWND needed) ----------
    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    ComPtr<ID3D11Device>        device;
    ComPtr<ID3D11DeviceContext>  context;

    hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevels, ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        device.GetAddressOf(),
        &g_featureLevel,
        context.GetAddressOf());

    if (FAILED(hr))
    {
        LogMsg("UWP: Hardware D3D failed (hr=0x%08X), trying WARP...\n", hr);
        // Fallback to WARP
        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            createDeviceFlags,
            featureLevels, ARRAYSIZE(featureLevels),
            D3D11_SDK_VERSION,
            device.GetAddressOf(),
            &g_featureLevel,
            context.GetAddressOf());
    }
    LogMsg("UWP: D3D11CreateDevice hr=0x%08X featureLevel=0x%X\n", hr, g_featureLevel);
    assert(SUCCEEDED(hr));

    // Store raw pointers in the globals the game expects
    g_pd3dDevice       = device.Get();
    g_pImmediateContext = context.Get();
    // AddRef so they stay alive (the ComPtrs will go out of scope)
    g_pd3dDevice->AddRef();
    g_pImmediateContext->AddRef();
    g_driverType = D3D_DRIVER_TYPE_HARDWARE;

    // ---------- CoreWindow swap chain (UWP + Xbox) ----------
    // Xbox One runs at 1920×1080; we respect the actual CoreWindow bounds.
    auto bounds = m_window->Bounds;
    int width  = static_cast<int>(bounds.Width);
    int height = static_cast<int>(bounds.Height);
    LogMsg("UWP: CoreWindow bounds: %dx%d\n", width, height);
    if (width  < 1920) width  = 1920;
    if (height < 1080) height = 1080;
    g_iScreenWidth  = 1920;
    g_iScreenHeight = 1080;
    g_rScreenWidth  = width;
    g_rScreenHeight = height;
    g_iAspectRatio  = static_cast<float>(width) / height;

    DXGI_SWAP_CHAIN_DESC1 scd = {};
    scd.Width              = width;
    scd.Height             = height;
    scd.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.SampleDesc.Count   = 1;
    scd.SampleDesc.Quality = 0;
    scd.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
    scd.BufferCount        = 2;
    scd.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    scd.Scaling            = DXGI_SCALING_STRETCH;

    ComPtr<IDXGIDevice1>  dxgiDevice;
    ComPtr<IDXGIAdapter>  dxgiAdapter;
    ComPtr<IDXGIFactory2> dxgiFactory;

    device.As(&dxgiDevice);
    dxgiDevice->GetAdapter(dxgiAdapter.GetAddressOf());
    dxgiAdapter->GetParent(IID_PPV_ARGS(dxgiFactory.GetAddressOf()));

    ComPtr<IDXGISwapChain1> swapChain1;
    LogMsg("UWP: Creating swap chain for CoreWindow...\n");
    hr = dxgiFactory->CreateSwapChainForCoreWindow(
        g_pd3dDevice,
        reinterpret_cast<IUnknown*>(m_window),
        &scd,
        nullptr,
        swapChain1.GetAddressOf());
    LogMsg("UWP: CreateSwapChainForCoreWindow hr=0x%08X\n", hr);
    assert(SUCCEEDED(hr));

    // The game uses IDXGISwapChain* — QI for the base interface
    hr = swapChain1.As(&swapChain1);  // already IDXGISwapChain1
    g_pSwapChain = swapChain1.Detach();   // transfer ownership to global

    // ---------- Render target view ----------
    ComPtr<ID3D11Texture2D> backBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
    hr = g_pd3dDevice->CreateRenderTargetView(backBuffer.Get(), nullptr, &g_pRenderTargetView);
    assert(SUCCEEDED(hr));

    // ---------- Depth-stencil ----------
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width      = width;
    depthDesc.Height     = height;
    depthDesc.MipLevels  = 1;
    depthDesc.ArraySize  = 1;
    depthDesc.Format     = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage      = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags  = D3D11_BIND_DEPTH_STENCIL;

    hr = g_pd3dDevice->CreateTexture2D(&depthDesc, nullptr, &g_pDepthStencilBuffer);
    assert(SUCCEEDED(hr));

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format             = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension      = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;
    hr = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencilBuffer, &dsvDesc, &g_pDepthStencilView);
    assert(SUCCEEDED(hr));

    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);

    // ---------- Viewport ----------
    D3D11_VIEWPORT vp = {};
    vp.Width    = static_cast<float>(width);
    vp.Height   = static_cast<float>(height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    g_pImmediateContext->RSSetViewports(1, &vp);

    // ---------- 4J RenderManager ----------
    RenderManager.Initialise(g_pd3dDevice, g_pSwapChain);
    PostProcesser::GetInstance().Init();

    LogMsg("UWP: D3D11 device + CoreWindow swap chain created OK\n");
}

// ============================================================================
// Event handlers
// ============================================================================
void App::OnActivated(CoreApplicationView^, IActivatedEventArgs^)
{
    CoreWindow::GetForCurrentThread()->Activate();
}

void App::OnSuspending(Platform::Object^, SuspendingEventArgs^ args)
{
    auto deferral = args->SuspendingOperation->GetDeferral();

    // Xbox requirement: Trim DXGI resources on suspend
    if (g_pd3dDevice)
    {
        ComPtr<IDXGIDevice3> dxgiDev3;
        ComPtr<ID3D11Device> dev(g_pd3dDevice);
        if (SUCCEEDED(dev.As(&dxgiDev3)))
            dxgiDev3->Trim();
    }

    deferral->Complete();
}

void App::OnResuming(Platform::Object^, Platform::Object^)
{
    // Nothing special — the game loop resumes automatically
}

void App::OnWindowClosed(CoreWindow^, CoreWindowEventArgs^)
{
    m_windowClosed = true;
}

void App::OnWindowSizeChanged(CoreWindow^, WindowSizeChangedEventArgs^ args)
{
    g_rScreenWidth  = static_cast<int>(args->Size.Width);
    g_rScreenHeight = static_cast<int>(args->Size.Height);
    g_iAspectRatio  = static_cast<float>(g_rScreenWidth) / g_rScreenHeight;
    // Note: a full swap chain resize would go here if needed
}

void App::OnVisibilityChanged(CoreWindow^, VisibilityChangedEventArgs^ args)
{
    m_windowVisible = args->Visible;
}

} // namespace MinecraftLCE

// ============================================================================
// UWP entry point — CoreApplication (Xbox compatible)
// ============================================================================
[Platform::MTAThread]
int main(Platform::Array<Platform::String^>^)
{
    CoreApplication::Run(ref new MinecraftLCE::AppSource());
    return 0;
}
