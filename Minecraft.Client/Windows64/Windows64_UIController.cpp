#include "stdafx.h"
#include "Windows64_UIController.h"

// Temp
#include "..\Minecraft.h"
#include "..\Textures.h"

#define _ENABLEIGGY

ConsoleUIController ui;

// Forward-declare the logger from UWP_App.cpp
extern void LogMsg(const char* fmt, ...);

void ConsoleUIController::init(ID3D11Device *dev, ID3D11DeviceContext *ctx, ID3D11RenderTargetView* pRenderTargetView, ID3D11DepthStencilView* pDepthStencilView, S32 w, S32 h)
{
#ifdef _ENABLEIGGY
	m_pRenderTargetView = pRenderTargetView;
	m_pDepthStencilView = pDepthStencilView;

	LogMsg("UICtrl: calling preInit(%d,%d)...\n", w, h);
	// Shared init
	preInit(w,h);
	LogMsg("UICtrl: preInit OK\n");

	LogMsg("UICtrl: calling gdraw_D3D11_CreateContext...\n");
	gdraw_funcs = gdraw_D3D11_CreateContext(dev, ctx, w, h);
	LogMsg("UICtrl: gdraw_D3D11_CreateContext returned %p\n", gdraw_funcs);

	if(!gdraw_funcs)
	{
		LogMsg("UICtrl: *** GDraw FAILED! ***\n");
		app.DebugPrintf("Failed to initialise GDraw!\n");
#ifndef _CONTENT_PACKAGE
		__debugbreak();
#endif
		app.FatalLoadError();
	}

	gdraw_D3D11_SetResourceLimits(GDRAW_D3D11_RESOURCE_vertexbuffer, 5000,  16 * 1024 * 1024);
	gdraw_D3D11_SetResourceLimits(GDRAW_D3D11_RESOURCE_texture     , 5000, 128 * 1024 * 1024);
	gdraw_D3D11_SetResourceLimits(GDRAW_D3D11_RESOURCE_rendertarget,   10,  64 * 1024 * 1024);
	LogMsg("UICtrl: resource limits set\n");

	/* GDraw is all set, so we'll point Iggy at it. */
	IggySetGDraw(gdraw_funcs);
	LogMsg("UICtrl: IggySetGDraw OK\n");

	// DISABLED for UWP — DirectSound is not available in packaged apps.
	// Game audio uses miniaudio, so Iggy embedded Flash audio is not needed.
	// IggyAudioUseDirectSound();

	LogMsg("UICtrl: calling postInit()...\n");
	// Shared init
	postInit();
	LogMsg("UICtrl: postInit OK\n");
#endif
}

void ConsoleUIController::render()
{
#ifdef _ENABLEIGGY
	/* Now that we've cleared, we need to tell GDraw which
	render target to use, what depth/stencil buffer to use,
	and where the origin should be.

	If we were using multisampling, we'd also need to give
	GDraw a render target view for a non-multisampled texture
	the size of main_rtv as a resolve target (this is the third
	parameter). But since we're not using multisampling in this
	example, no resolve targets are required. */
	gdraw_D3D11_SetTileOrigin( m_pRenderTargetView,
		m_pDepthStencilView,
		nullptr,
		0,
		0 );

	renderScenes();

	/* Finally we're ready to display the frame.  We call GDraw to
	let it know we're done rendering, so it can do any finalization
	it needs to do. */
	gdraw_D3D11_NoMoreGDrawThisFrame();
#endif
}

void ConsoleUIController::beginIggyCustomDraw4J(IggyCustomDrawCallbackRegion *region, CustomDrawData *customDrawRegion)
{
	PIXBeginNamedEvent(0,"Starting Iggy custom draw\n");

	PIXBeginNamedEvent(0,"Gdraw setup");
	// get the correct object-to-world matrix from GDraw, and set the render state to a normal state
	gdraw_D3D11_BeginCustomDraw_4J(region, customDrawRegion->mat);
	PIXEndNamedEvent();
}

CustomDrawData *ConsoleUIController::setupCustomDraw(UIScene *scene, IggyCustomDrawCallbackRegion *region)
{
	CustomDrawData *customDrawRegion = new CustomDrawData();
	customDrawRegion->x0 = region->x0;
	customDrawRegion->x1 = region->x1;
	customDrawRegion->y0 = region->y0;
	customDrawRegion->y1 = region->y1;

	PIXBeginNamedEvent(0,"Starting Iggy custom draw\n");
	// get the correct object-to-world matrix from GDraw, and set the render state to a normal state   
	gdraw_D3D11_BeginCustomDraw_4J(region, customDrawRegion->mat);

	setupCustomDrawGameStateAndMatrices(scene, customDrawRegion);

	return customDrawRegion;
}

CustomDrawData *ConsoleUIController::calculateCustomDraw(IggyCustomDrawCallbackRegion *region)
{
	CustomDrawData *customDrawRegion = new CustomDrawData();
	customDrawRegion->x0 = region->x0;
	customDrawRegion->x1 = region->x1;
	customDrawRegion->y0 = region->y0;
	customDrawRegion->y1 = region->y1;

	gdraw_D3D11_CalculateCustomDraw_4J(region, customDrawRegion->mat);

	return customDrawRegion;
}

void ConsoleUIController::endCustomDraw(IggyCustomDrawCallbackRegion *region)
{
	endCustomDrawGameStateAndMatrices();

	gdraw_D3D11_EndCustomDraw(region);
	PIXEndNamedEvent();
}

void ConsoleUIController::updateRenderTargets(ID3D11RenderTargetView* rtv, ID3D11DepthStencilView* dsv)
{
	m_pRenderTargetView = rtv;
	m_pDepthStencilView = dsv;
}

void ConsoleUIController::setTileOrigin(S32 xPos, S32 yPos)
{
	gdraw_D3D11_SetTileOrigin( m_pRenderTargetView,
		m_pDepthStencilView,
		nullptr,
		xPos,
		yPos );
}

GDrawTexture *ConsoleUIController::getSubstitutionTexture(int textureId)
{
	/* Create a wrapped texture from a shader resource view.
	A wrapped texture can be used to let Iggy draw using the contents of a texture
	you create and manage on your own. For example, you might render to this texture,
	or stream video into it. Wrapped textures take up a handle. They will never be
	freed or otherwise modified by GDraw; nor will GDraw change any reference counts.
	All this is up to the application. */
	ID3D11ShaderResourceView *tex = RenderManager.TextureGetTexture(textureId);
	ID3D11Resource *resource;
	tex->GetResource(&resource);
	ID3D11Texture2D  *tex2d = static_cast<ID3D11Texture2D *>(resource);
	D3D11_TEXTURE2D_DESC desc;
	tex2d->GetDesc(&desc);
	GDrawTexture *gdrawTex = gdraw_D3D11_WrappedTextureCreate(tex);
	return gdrawTex;
}

void ConsoleUIController::destroySubstitutionTexture(void *destroyCallBackData, GDrawTexture *handle)
{
	/* Destroys the GDraw wrapper for a wrapped texture object. This will free up
	a GDraw texture handle but not release the associated D3D texture; that is
	up to you. */
	gdraw_D3D11_WrappedTextureDestroy(handle);
}

void ConsoleUIController::shutdown()
{
#ifdef _ENABLEIGGY
	/* Destroy the GDraw context. This frees all resources, shaders etc.
	allocated by GDraw. Note this is only safe to call after all
	active Iggy player have been destroyed! */
	gdraw_D3D11_DestroyContext();
#endif
}