//----------------------------------------------------------------------------
// This program is the property of Persistant Studios SARL.
//
// You may not redistribute it and/or modify it under any conditions
// without written permission from Persistant Studios SARL, unless
// otherwise stated in the latest Persistant Studios Code License.
//
// See the Persistant Studios Code License for further details.
//----------------------------------------------------------------------------

#include "precompiled.h"

#include "ApiContextConfig.h"

#if (PK_BUILD_WITH_D3D11_SUPPORT != 0) && defined(PK_WINDOWS)

#include <pk_rhi/include/D3D11/D3D11RHI.h>
#include <dxgi1_2.h>

#include <initguid.h> // needed for d3d11
#include <dxgidebug.h>

#include <WindowContext/SdlContext/SdlContext.h>

#if	(PK_BUILD_WITH_SDL != 0)
#	if defined(PK_COMPILER_CLANG_CL)
#		pragma clang diagnostic push
#		pragma clang diagnostic ignored "-Wpragma-pack"
#	endif // defined(PK_COMPILER_CLANG_CL)
#	include <SDL_syswm.h>
#	if defined(PK_COMPILER_CLANG_CL)
#		pragma clang diagnostic pop
#	endif // defined(PK_COMPILER_CLANG_CL)
#endif // (PK_BUILD_WITH_SDL != 0)

#include "D3D11Context.h"
#include <pk_rhi/include/D3D11/D3D11RHI.h>
#include <pk_rhi/include/D3D11/D3D11RenderTarget.h>
#include <pk_rhi/include/D3D11/D3D11CommandBuffer.h>
#include <pk_rhi/include/D3DCommon/D3DPopcornEnumConversion.h>

#if defined(PK_DEBUG)
#	define		USE_DEBUG_DXGI			1
#	define		BREAK_ON_D3D_ERROR		0 // does not work anyway because "SetBreakOnSeverity" is ignored with dll-loaded libs
#	define		BREAK_ON_D3D_WARN		0 // does not work anyway because "SetBreakOnSeverity" is ignored with dll-loaded libs
#else
#	define		USE_DEBUG_DXGI			0
#	define		BREAK_ON_D3D_ERROR		0
#	define		BREAK_ON_D3D_WARN		0
#endif

#if (USE_DEBUG_DXGI != 0) || (BREAK_ON_D3D_WARN != 0) || (BREAK_ON_D3D_ERROR != 0)
#	include <dxgi1_3.h>	// debug
#	pragma comment(lib, "DXGI.lib")
#endif

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

#if (PK_BUILD_WITH_SDL != 0)
PK_DECLARE_REFPTRCLASS(SdlContext);
#endif

struct	SD3D11PlatformContext
{
	IDXGIFactory1				*m_Factory = null;
	IDXGIAdapter1				*m_HardwareAdapter = null;

	TArray<IDXGISwapChain*>		m_SwapChains;

#if (USE_DEBUG_DXGI != 0)
	IDXGIDebug					*m_Debug = null;
#endif

	PFN_D3D11_CREATE_DEVICE		m_CreateDeviceFunc;
	HMODULE						m_D3DModule = 0;
	HMODULE						m_DXGIModule = 0;

	SD3D11PlatformContext() {}
	~SD3D11PlatformContext()
	{
		for (IDXGISwapChain *swapChain : m_SwapChains)
		{
			if (swapChain != null)
				swapChain->Release();
		}
		m_SwapChains.Clear();

		if (m_Factory != null)
			m_Factory->Release();
		if (m_HardwareAdapter != null)
			m_HardwareAdapter->Release();
#if (USE_DEBUG_DXGI != 0)
		if (m_Debug != null)
		{
			m_Debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
			m_Debug->Release();
		}
#endif
		if (m_D3DModule != null)
			FreeLibrary(m_D3DModule);
		if (m_DXGIModule != null)
			FreeLibrary(m_DXGIModule);
	}
};

//----------------------------------------------------------------------------

CD3D11Context::CD3D11Context()
:	m_Context(PK_NEW(SD3D11PlatformContext))
{
}

//----------------------------------------------------------------------------

CD3D11Context::~CD3D11Context()
{
	m_ApiData.m_SwapChainRenderTargets.Clear();
	if (m_ApiData.m_ImmediateDeviceContext != null)
		m_ApiData.m_ImmediateDeviceContext->Release();
	if (m_ApiData.m_Device != null)
		m_ApiData.m_Device->Release();

	PK_DELETE(m_Context);
}

//----------------------------------------------------------------------------

bool	CD3D11Context::InitRenderApiContext(bool debug, PAbstractWindowContext windowApi)
{
	if (!InitContext(debug))
		return false;

	if (windowApi->GetContextApi() == PKSample::Context_Sdl)
	{
#if (PK_BUILD_WITH_SDL != 0)
		PSdlContext		sdlWindowApi = static_cast<CSdlContext*>(windowApi.Get());
		SDL_SysWMinfo	info;
		SDL_VERSION(&info.version);
		SDL_GetWindowWMInfo(sdlWindowApi->SdlGetWindow(), &info);
#if defined(PK_UWP)
		return AddSwapchain(info.info.winrt.window, sdlWindowApi->GetDrawableSize());
#elif defined(PK_WINDOWS)
		return AddSwapChain(info.info.win.window, sdlWindowApi->GetDrawableSize());
#endif	// defined(PK_UWP)
#else
		return false;
#endif	// (PK_BUILD_WITH_SDL != 0)
	}
	else if (windowApi->GetContextApi() == PKSample::Context_Offscreen)
	{
		return CreateOffscreenRenderTarget(windowApi->GetDrawableSize());
	}
	else
	{ // Does not implement another way to get window handle
		return false;
	}
}

//----------------------------------------------------------------------------

bool	CD3D11Context::LoadDynamicLibrary()
{
	PK_ASSERT(m_Context != null);
	m_Context->m_D3DModule = ::LoadLibraryA("d3d11.dll");
	if (m_Context->m_D3DModule == null)
		return false;
	m_Context->m_DXGIModule = ::LoadLibraryA("dxgi.dll");
	if (m_Context->m_DXGIModule == null)
		return false;
	m_Context->m_CreateDeviceFunc = (PFN_D3D11_CREATE_DEVICE)::GetProcAddress(m_Context->m_D3DModule, "D3D11CreateDevice");

	return m_Context->m_CreateDeviceFunc != null;
}

//----------------------------------------------------------------------------

bool	CD3D11Context::InitContext(bool debug)
{
	if (m_Context == null)
		return false;
	m_ApiData.m_Api = RHI::GApi_D3D11;
	m_ApiData.m_SwapChainCount = 0;

	if (!LoadDynamicLibrary())
		return false;

	// try grabbing CreateDXGIFactory1:
	typedef HRESULT	(WINAPI *FnCreateDXGIFactory1)(	REFIID				riid,
													_COM_Outptr_ void	**ppFactory);

	FnCreateDXGIFactory1	fnCreateDXGIFactory1 = (FnCreateDXGIFactory1)::GetProcAddress(m_Context->m_DXGIModule, "CreateDXGIFactory1");
	// if not found, this is not fatal, it will fallback (ie: on vista & xp)
	if (fnCreateDXGIFactory1 == null)
	{
		CLog::Log(PK_INFO, "DXGI API 'CreateDXGIFactory1' not found, cannot create D3D11 context.");
		return false;
	}

	if (!PK_D3D11_OK(fnCreateDXGIFactory1(IID_PPV_ARGS(&m_Context->m_Factory))) ||
		!CreateDevice(debug))
		return false;

	if (debug)
		EnableDebugLayer();

	return true;
}

//----------------------------------------------------------------------------

bool	CD3D11Context::WaitAllRenderFinished()
{
	return true;
}

//----------------------------------------------------------------------------

CGuid	CD3D11Context::BeginFrame()
{
	return BeginFrame(0u);
}

//----------------------------------------------------------------------------

bool	CD3D11Context::EndFrame(void *renderToWait)
{
	(void)renderToWait;
	return EndFrame(0u);
}

//----------------------------------------------------------------------------

CGuid	CD3D11Context::BeginFrame(u32 swapChainIdx)
{
	(void)swapChainIdx;
	return 0;	//m_Context->m_SwapChains[swapChainIdx]->GetCurrentBackBufferIndex();
}

//----------------------------------------------------------------------------

bool	CD3D11Context::EndFrame(u32 swapChainIdx)
{
	// offscreen context does not create swapchain instance
	if (m_Context->m_SwapChains[swapChainIdx] == null)
		return true;

	const HRESULT	hr = m_Context->m_SwapChains[swapChainIdx]->Present(0, 0);
//	if (FAILED(hr))
//		return false;
	if (hr == DXGI_ERROR_DEVICE_RESET ||
		hr == DXGI_ERROR_DEVICE_REMOVED)
		return false;
	if (hr == DXGI_STATUS_OCCLUDED)
		return true;

	return PK_D3D11_OK(hr);
}

//----------------------------------------------------------------------------

RHI::SApiContext	*CD3D11Context::GetRenderApiContext()
{
	return &m_ApiData;
}

//----------------------------------------------------------------------------

bool	CD3D11Context::RecreateSwapChain(const CUint2 &ctxSize)
{
	PK_ASSERT(m_ApiData.m_SwapChainRenderTargets.Count() == 1);
	if (!PK_VERIFY(!m_ApiData.m_SwapChainRenderTargets.Empty()))
		return false;
	return RecreateSwapChain(0, ctxSize);
}

//----------------------------------------------------------------------------

bool	CD3D11Context::RecreateSwapChain(u32 swapChainIdx, const CUint2 &ctxSize)
{
	if (!PK_VERIFY(swapChainIdx < m_Context->m_SwapChains.Count()))
		return false;
	m_ApiData.m_ImmediateDeviceContext->OMSetRenderTargets(0, null, null);
	m_ApiData.m_ImmediateDeviceContext->ClearState();
	RHI::PD3D11RenderTarget	rt = CastD3D11(m_ApiData.m_SwapChainRenderTargets[swapChainIdx]);
	PK_FOREACH(it, m_ApiData.m_DeferredCommandBuffer)
	{
		RHI::CD3D11CommandBuffer	*cmdBuff = it->Get();
		if (cmdBuff->IsBackBufferUsed(rt))
			cmdBuff->D3D11ReleaseCommandList();
	}
	rt->ReleaseResources();
	if (PK_D3D11_FAILED(m_Context->m_SwapChains[swapChainIdx]->ResizeBuffers(kFrameCount, ctxSize.x(), ctxSize.y(), DXGI_FORMAT_B8G8R8A8_UNORM, 0)))
	{
		return false;
	}
	if (!CreateRenderTargets(swapChainIdx, ctxSize, null))
		return false;
	return true;
}

//----------------------------------------------------------------------------

TMemoryView<const RHI::PRenderTarget>	CD3D11Context::GetCurrentSwapChain()
{
	return m_ApiData.m_SwapChainRenderTargets;
}

//----------------------------------------------------------------------------

bool	CD3D11Context::EnableDebugLayer()
{
	PK_ASSERT(m_ApiData.m_Device != null);

#if (USE_DEBUG_DXGI != 0)
	if (PK_D3D11_FAILED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&m_Context->m_Debug))))
		return false;
#endif

#if (BREAK_ON_D3D_ERROR != 0) || (BREAK_ON_D3D_WARN != 0)
	ID3D11InfoQueue	*infoQueue = null;
	m_ApiData.m_Device->QueryInterface(&infoQueue);
	if (infoQueue != null)
	{
		PK_D3D11_OK(infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE));
		PK_D3D11_OK(infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE));
#	if (BREAK_ON_D3D_WARN != 0)
		PK_D3D11_OK(infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING, TRUE));
#	endif
		infoQueue->Release();
	}
#endif
	return true;
}

//----------------------------------------------------------------------------

bool	CD3D11Context::CreateDevice(bool debug)
{
	D3D_FEATURE_LEVEL	featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};
	if (!PickHardwareAdapter())
	{
		CLog::Log(PK_ERROR, "D3D11: Couldn't pick hardware adapter");
		return false;
	}
	if (!PK_D3D11_OK(m_Context->m_CreateDeviceFunc(	m_Context->m_HardwareAdapter,
													D3D_DRIVER_TYPE_UNKNOWN,
													null,
													debug ? D3D11_CREATE_DEVICE_DEBUG : 0,
													featureLevels,
													_countof(featureLevels),
													D3D11_SDK_VERSION,
													&m_ApiData.m_Device,
													&m_ApiData.m_FeatureLevel,
													&m_ApiData.m_ImmediateDeviceContext)))
	{
		CLog::Log(PK_ERROR, "D3D12: Couldn't create device");
		return false;
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CD3D11Context::PickHardwareAdapter()
{
	IDXGIFactory1	*&factory = m_Context->m_Factory;
	IDXGIAdapter1	*&adapter = m_Context->m_HardwareAdapter;
	adapter = 0;
	for (u32 idx = 0; factory->EnumAdapters1(idx, &adapter) != DXGI_ERROR_NOT_FOUND; ++idx)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			// Don't select the Basic Render Driver adapter.
			// If you want a software adapter, pass in "/warp" on the command line.
			continue;
		}

		// Check to see if the adapter supports Direct3D 12, but don't create the
		// actual device yet.
		D3D_FEATURE_LEVEL featureLevels[] =
		{
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0,
		};
		if (SUCCEEDED(m_Context->m_CreateDeviceFunc(adapter, D3D_DRIVER_TYPE_UNKNOWN, null, 0, featureLevels, _countof(featureLevels), D3D11_SDK_VERSION, null, null, null)))
		{
			return true;
		}
	}
	adapter = 0;
	return false;
}

//----------------------------------------------------------------------------

bool	CD3D11Context::AddSwapChain(HWND winHandle, CUint2 winSize, TMemoryView<const RHI::PRenderTarget> *view /*= null*/)
{
	const CGuid		idx = m_Context->m_SwapChains.PushBack(null);

	if (!PK_VERIFY(idx.Valid()) ||
		!PK_VERIFY(m_ApiData.m_SwapChainRenderTargets.PushBack() == idx))
		return false;

	if (!CreateSwapChain(idx, winHandle, winSize, view))
		return false;

	m_ApiData.m_SwapChainCount++;

	return true;
}

//----------------------------------------------------------------------------

bool	CD3D11Context::CreateSwapChain(u32 swapChainIdx, HWND winHandle, CUint2 winSize, TMemoryView<const RHI::PRenderTarget> *view)
{
#if defined(PK_WINDOWS)
	IDXGISwapChain			*swapChain = null;
	DXGI_SWAP_CHAIN_DESC	swapChainDesc = {};
	swapChainDesc.BufferDesc.Width = winSize.x();
	swapChainDesc.BufferDesc.Height = winSize.y();
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_STRETCHED;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = kFrameCount;
	swapChainDesc.OutputWindow = winHandle;
	swapChainDesc.Windowed = TRUE;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

	if (PK_D3D11_FAILED(m_Context->m_Factory->CreateSwapChain(m_ApiData.m_Device, &swapChainDesc, &swapChain)))
		return false;
	m_Context->m_SwapChains[swapChainIdx] = swapChain;
	return CreateRenderTargets(swapChainIdx, winSize, view);
#else
	m_Context->m_SwapChains[swapChainIdx] = null;
	return false;
#endif
}

//----------------------------------------------------------------------------

bool	CD3D11Context::CreateRenderTargets(u32 swapChainIdx, CUint2 winSize, TMemoryView<const RHI::PRenderTarget> *view)
{
	// Offscreen rendering could leave a swapchain empty
	if (m_Context->m_SwapChains[swapChainIdx] == null)
		return false;
	m_ApiData.m_BufferingMode = RHI::ContextDoubleBuffering;// static_cast<RHI::EContextBufferingMode>(kFrameCount);

	ID3D11Resource	*rtResource = null;
	if (PK_D3D11_FAILED(m_Context->m_SwapChains[swapChainIdx]->GetBuffer(0, IID_PPV_ARGS(&rtResource))))
		return false;
	RHI::CD3D11RenderTarget * rhiRT = PK_NEW(RHI::CD3D11RenderTarget(RHI::SRHIResourceInfos("D3D11Context Swap Chain")));
	if (rhiRT == null)
		return false;
	rhiRT->D3D11SetRenderTarget(rtResource, RHI::FormatSrgb8BGRA, winSize, true);
	m_ApiData.m_SwapChainRenderTargets[swapChainIdx] = rhiRT;
	if (view != null)
		*view = TMemoryView<const RHI::PRenderTarget>(m_ApiData.m_SwapChainRenderTargets.Last());
	return true;
}

//----------------------------------------------------------------------------

bool	CD3D11Context::CreateOffscreenRenderTarget(const CUint2 &winSize)
{
	ID3D11Texture2D			*rtResource = null;
	D3D11_TEXTURE2D_DESC	textureDesc = {};
	textureDesc.Width = winSize.x();
	textureDesc.Height = winSize.y();
	textureDesc.ArraySize = 1;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET;

	if (PK_D3D11_FAILED(m_ApiData.m_Device->CreateTexture2D(&textureDesc, null, &rtResource)))
		return false;

	m_ApiData.m_BufferingMode = RHI::ContextDoubleBuffering;

	RHI::CD3D11RenderTarget		*rhiRT = PK_NEW(RHI::CD3D11RenderTarget(RHI::SRHIResourceInfos("D3D11Context Offscreen Swap Chain")));
	if (rhiRT == null)
		return false;

	// create a fake swapchain
	const CGuid		swapChainIdx = m_Context->m_SwapChains.PushBack(null);

	if (!PK_VERIFY(swapChainIdx.Valid()) ||
		!PK_VERIFY(m_ApiData.m_SwapChainRenderTargets.PushBack() == swapChainIdx))
		return false;
	m_ApiData.m_SwapChainCount++;

	// add render target
	rhiRT->D3D11SetRenderTarget(rtResource, RHI::FormatSrgb8BGRA, winSize);
	m_ApiData.m_SwapChainRenderTargets[swapChainIdx] = rhiRT;

	return true;
}

//----------------------------------------------------------------------------

#define	SAFE_RELEASE(_x)	do { if ((_x) != null) { (_x)->Release(); (_x) = null; } } while (0);

bool	CD3D11Context::DestroySwapChain(u32 swapChainIdx)
{
	if (!PK_VERIFY(swapChainIdx < m_Context->m_SwapChains.Count()))
		return false;
	m_ApiData.m_ImmediateDeviceContext->OMSetRenderTargets(0, null, null);
	m_ApiData.m_ImmediateDeviceContext->ClearState();

	RHI::PD3D11RenderTarget	rt = CastD3D11(m_ApiData.m_SwapChainRenderTargets[swapChainIdx]);
	PK_FOREACH(it, m_ApiData.m_DeferredCommandBuffer)
	{
		RHI::CD3D11CommandBuffer	*cmdBuff = it->Get();
		if (cmdBuff->IsBackBufferUsed(rt))
			cmdBuff->D3D11ReleaseCommandList();
	}
	rt->ReleaseResources();

	m_ApiData.m_SwapChainRenderTargets.Remove(swapChainIdx);
	SAFE_RELEASE(m_Context->m_SwapChains[swapChainIdx]);
	m_Context->m_SwapChains.Remove(swapChainIdx);
	m_ApiData.m_SwapChainCount--;
	return true;
}

#undef SAFE_RELEASE

//----------------------------------------------------------------------------

RHI::SD3D11BasicContext		*CD3D11Context::GetD3D11Context()
{
	return &m_ApiData;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

#endif
