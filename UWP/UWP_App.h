#pragma once
// ============================================================================
// UWP_App.h — Xbox One Dev Mode UWP Application Framework
// Wraps the Minecraft LCE game loop into a UWP CoreApplication.
// ============================================================================

// D3D / DXGI — needed for swap chain creation
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <dxgi1_3.h>
// WRL ComPtr
#include <wrl.h>
#include <wrl/client.h>

// C++/CX projection namespaces (/ZW flag auto-generates these;
// we do NOT include the midl ABI headers <windows.ui.core.h> etc.
// because they cause ABI::Windows::UI::Color redefinition errors
// when mixed with /ZW projections).
using namespace Microsoft::WRL;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::UI::Core;
using namespace Windows::Foundation;

class Minecraft;

// ============================================================================
// Namespace must match the AppxManifest EntryPoint "MinecraftLCE.App"
// In C++/CX, a dot-separated EntryPoint maps to Namespace::Class.
// ============================================================================
namespace MinecraftLCE
{

// ============================================================================
// App : IFrameworkView
// ============================================================================
ref class App sealed : public Windows::ApplicationModel::Core::IFrameworkView
{
public:
    virtual void Initialize(CoreApplicationView^ applicationView);
    virtual void SetWindow(CoreWindow^ window);
    virtual void Load(Platform::String^ entryPoint);
    virtual void Run();
    virtual void Uninitialize();

internal:
    App();

private:
    void OnActivated(CoreApplicationView^ appView, IActivatedEventArgs^ args);
    void OnSuspending(Platform::Object^ sender, SuspendingEventArgs^ args);
    void OnResuming(Platform::Object^ sender, Platform::Object^ args);
    void OnWindowClosed(CoreWindow^ sender, CoreWindowEventArgs^ args);
    void OnWindowSizeChanged(CoreWindow^ sender, WindowSizeChangedEventArgs^ args);
    void OnVisibilityChanged(CoreWindow^ sender, VisibilityChangedEventArgs^ args);

    void CreateDeviceAndSwapChain();
    void GameTick();

    CoreWindow^ m_window;
    bool m_windowClosed;
    bool m_windowVisible;
    Minecraft* m_pMinecraft;
    bool       m_gameInitialized;
};

// ============================================================================
ref class AppSource sealed : public Windows::ApplicationModel::Core::IFrameworkViewSource
{
public:
    virtual Windows::ApplicationModel::Core::IFrameworkView^ CreateView();
};

} // namespace MinecraftLCE
