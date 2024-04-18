//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"
#include "RenderApi/AEGP_D3D11Context.h"

#if	(PK_BUILD_WITH_D3D11_SUPPORT != 0)

#include <pk_rhi/include/D3D11/D3D11ApiManager.h>
#include <pk_rhi/include/D3D11/D3D11BasicContext.h>
#include <pk_rhi/include/D3D11/D3D11Texture.h>
#include <pk_rhi/include/D3D11/D3D11RenderTarget.h>

#include <pk_rhi/include/D3D11/D3D11PopcornEnumConversion.h>

#include <PK-SampleLib/ApiContext/D3D/D3D11Context.h>

#include <pk_kernel/include/kr_thread_pool_default.h>

#include "AEGP_World.h"
#include "RenderApi/AEGP_CopyTask.h"

__AEGP_PK_BEGIN

//----------------------------------------------------------------------------

struct	SD3D11PlatformContext
{
	IDXGIFactory1				*m_Factory;
	IDXGIAdapter1				*m_HardwareAdapter;

	TArray<IDXGISwapChain*>		m_SwapChains;

#if USE_DEBUG_DXGI
	IDXGIDebug					*m_Debug;
#endif

	PFN_D3D11_CREATE_DEVICE		m_CreateDeviceFunc;
	HMODULE						m_D3DModule;
	HMODULE						m_DXGIModule;

	bool						m_Initialized = false;

	SD3D11PlatformContext()
		: m_Factory(null)
		, m_HardwareAdapter(null)
#if USE_DEBUG_DXGI
		, m_Debug(null)
#endif
		, m_D3DModule(0)
		, m_DXGIModule(0)
	{
	}

	~SD3D11PlatformContext()
	{
		if (m_Factory != null)
			m_Factory->Release();
		if (m_HardwareAdapter != null)
			m_HardwareAdapter->Release();
		PK_FOREACH(swapChain, m_SwapChains)
		{
			if (PK_VERIFY(*swapChain != null))
				(*swapChain)->Release();
		}
#if USE_DEBUG_DXGI
		if (m_Debug)
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

CAAED3D11Context::CAAED3D11Context()
	: m_Texture(null)
	, m_StagingTexture(null)
{
	m_D3D11Manager = PK_NEW(RHI::CD3D11ApiManager);
	m_D3D11Context = PK_NEW(RHI::SD3D11BasicContext);
	m_ApiManager = m_D3D11Manager;
	m_ApiContext = m_D3D11Context;

	if (m_Context == null)
		m_Context = (PK_NEW(SD3D11PlatformContext));
}

//----------------------------------------------------------------------------

CAAED3D11Context::~CAAED3D11Context()
{
	if (m_Texture != null)
	{
		m_Texture->Release();
		m_Texture = null;
	}
	if (m_StagingTexture != null)
	{
		m_StagingTexture->Release();
		m_StagingTexture = null;
	}
	m_D3D11Context->m_SwapChainRenderTargets.Clear();
	if (m_D3D11Context->m_ImmediateDeviceContext != null)
	{
		m_D3D11Context->m_ImmediateDeviceContext->Release();
		m_D3D11Context->m_ImmediateDeviceContext = null;
	}
	if (m_D3D11Context->m_Device != null)
	{
		m_D3D11Context->m_Device->Release();
		m_D3D11Context->m_Device = null;
	}

	PK_SAFE_DELETE(m_Context);
	PK_SAFE_DELETE(m_D3D11Context);

	m_D3D11Manager = null;
	m_ApiContext = null;
	m_ApiManager = null;

	m_Tasks.Clear();

}

//----------------------------------------------------------------------------

bool	CAAED3D11Context::InitIFN()
{
	if (m_Initialized)
		return true;
	m_Initialized = true;

	m_WorkerCount = CPopcornFXWorld::Instance().GetWorkerCount() + 1;
	m_Tasks.Resize(m_WorkerCount);

	m_ApiManager->InitApi(m_ApiContext);
	return true;
}

//----------------------------------------------------------------------------

bool	CAAED3D11Context::BeginFrame()
{
	m_ApiManager->BeginFrame(0);
	LogApiError();
	return true;
}

//----------------------------------------------------------------------------

bool	CAAED3D11Context::EndFrame()
{
	LogApiError();

	m_ApiManager->EndFrame();

	HRESULT	hr = 0;
	if (m_Context->m_SwapChains.Count() > 0)
	{
		hr = m_Context->m_SwapChains[0]->Present(0, 0);
		if (FAILED(hr))
			return false;
		if (hr == DXGI_ERROR_DEVICE_RESET ||
			hr == DXGI_ERROR_DEVICE_REMOVED)
			return false;
		if (hr == DXGI_STATUS_OCCLUDED)
			return true;
	}
	return PK_D3D11_OK(hr);
}

//----------------------------------------------------------------------------

void	CAAED3D11Context::LogApiError()
{
}

//----------------------------------------------------------------------------

bool	CAAED3D11Context::CreatePlatformContext(void *winHandle, void *deviceContext)
{	
	(void)winHandle;

	HDC		hdc = (HDC)deviceContext;

	int						PixelFormat;
	PIXELFORMATDESCRIPTOR	pfd;

	::ZeroMemory(&pfd, sizeof(pfd));

	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW; // | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cDepthBits = 8;

	PixelFormat = ChoosePixelFormat(hdc, &pfd);
	if (PixelFormat == 0)
		return false;
	if (!SetPixelFormat(hdc, PixelFormat, &pfd))
		return false;

	if (m_Context == null)
		return false;
	if (m_Context->m_Initialized)
		return true;

	m_D3D11Context->m_Api = RHI::GApi_D3D11;
	m_D3D11Context->m_SwapChainCount = 0;

	if (!_LoadDynamicLibrary())
		return false;

	// try grabbing CreateDXGIFactory1:
	typedef HRESULT(WINAPI *FnCreateDXGIFactory1)(REFIID riid, _COM_Outptr_ void	**ppFactory);

	FnCreateDXGIFactory1	fnCreateDXGIFactory1 = (FnCreateDXGIFactory1)::GetProcAddress(m_Context->m_DXGIModule, "CreateDXGIFactory1");
	// if not found, this is not fatal, it will fallback (ie: on vista & xp)
	if (fnCreateDXGIFactory1 == null)
	{
		CLog::Log(PK_INFO, "DXGI API 'CreateDXGIFactory1' not found, cannot create D3D11 context.");
		return false;
	}
	m_Context->m_Initialized  = true;
	return PK_D3D11_OK(fnCreateDXGIFactory1(IID_PPV_ARGS(&m_Context->m_Factory))) && _CreateDevice();
}

//----------------------------------------------------------------------------

bool	CAAED3D11Context::CreateRenderTarget(RHI::EPixelFormat format, CUint3 size)
{
	RHI::PD3D11RenderTarget		rt = PK_NEW(RHI::CD3D11RenderTarget(RHI::SRHIResourceInfos("Render Target")));
	ID3D11Texture2D				*texture2D = null;
	D3D11_TEXTURE2D_DESC		texDesc = {};

	texDesc.Width = size.x();
	texDesc.Height = size.y();
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = RHI::D3DConversion::PopcornToD3DPixelFormat(format);
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;

	if (PK_D3D11_FAILED(m_D3D11Context->m_Device->CreateTexture2D(&texDesc, null, &texture2D)))
		return false;
	texture2D->AddRef();
	rt->D3D11SetRenderTarget(texture2D, format, size.xy(), true, null, RHI::SampleCount1);

	if (m_Texture != null)
	{
		m_Texture->Release();
		m_Texture = null;
	}
		
	m_Texture = texture2D;
	
	if (m_D3D11Context->m_SwapChainCount != 0)
	{
		m_D3D11Context->m_SwapChainRenderTargets.Clear();
		m_D3D11Manager->SwapChainRemoved(0);
	}

	m_D3D11Context->m_SwapChainRenderTargets.PushBack(rt);
	m_D3D11Context->m_SwapChainCount = 1;

	m_D3D11Manager->SwapChainAdded();

	ID3D11Texture2D			*tex = null;
	D3D11_TEXTURE2D_DESC	textureStagingDesc = {};

	textureStagingDesc.Width = size.x();
	textureStagingDesc.Height = size.y();
	textureStagingDesc.MipLevels = 1;
	textureStagingDesc.ArraySize = 1;
	textureStagingDesc.Format = RHI::D3DConversion::PopcornToD3DPixelFormat(format);
	textureStagingDesc.SampleDesc.Count = 1;
	textureStagingDesc.Usage = D3D11_USAGE_STAGING;
	textureStagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	if (textureStagingDesc.Format == DXGI_FORMAT_UNKNOWN ||
		PK_D3D11_FAILED(m_D3D11Context->m_Device->CreateTexture2D(&textureStagingDesc, null, &tex)))
		return false;

	if (m_StagingTexture != null)
	{
		m_StagingTexture->Release();
		m_StagingTexture = null;
	}
	m_StagingTexture = tex;
	return true;
}

//----------------------------------------------------------------------------

bool	CAAED3D11Context::SetAsCurrent(void *deviceContext)
{
	(void)deviceContext;
	return true;
}

//----------------------------------------------------------------------------

bool	CAAED3D11Context::FillRenderBuffer(PRefCountedMemoryBuffer dstBuffer, RHI::PFrameBuffer srcBuffer, RHI::EPixelFormat format, u32 width, u32 height, u32 rowLength)
{
	(void)rowLength;

	PK_SCOPEDPROFILE();

	m_D3D11Context->m_ImmediateDeviceContext->CopyResource(m_StagingTexture, m_Texture);
	// Copy GPU Resource to CPU
	D3D11_TEXTURE2D_DESC		desc;
	D3D11_MAPPED_SUBRESOURCE	resource;
	UINT						subresource = D3D11CalcSubresource(0, 0, 0);

	m_StagingTexture->GetDesc(&desc);
	//Locking Map
	HRESULT hr = m_D3D11Context->m_ImmediateDeviceContext->Map(m_StagingTexture, subresource, D3D11_MAP_READ, 0, &resource);
	if (FAILED(hr))
	{
		CLog::Log(PK_ERROR, "D3D11Context map failure");
		return false;
	}

	BYTE	*sptr = reinterpret_cast<BYTE*>(resource.pData);
	BYTE	*dptr = (BYTE*)dstBuffer->Data<BYTE*>();

	const u32		formatSize = RHI::PixelFormatHelpers::PixelFormatToPixelByteSize(format);
	const u32		widthSize = (formatSize * width);

	PK_ASSERT(resource.RowPitch >= widthSize);

	u32				taskRowNumbers = height / m_WorkerCount;
	u32				reminder = height % m_WorkerCount;
	TAtomic<u32>	counter = 0;
	Threads::CEvent	event;

	for (u32 i = 0; i < m_WorkerCount;++i)
	{
		m_Tasks[i] = PK_NEW(CAsynchronousJob_CopyTextureTask);
		m_Tasks[i]->m_TargetCount = m_WorkerCount;

		m_Tasks[i]->m_Counter = &counter;
		m_Tasks[i]->m_EndCB = &event;

		if (i == (m_WorkerCount - 1))
			m_Tasks[i]->m_Height = taskRowNumbers + reminder;
		else
			m_Tasks[i]->m_Height = taskRowNumbers;
		m_Tasks[i]->m_StartOffset = i * taskRowNumbers;
		m_Tasks[i]->m_DestinationPtr = dptr;
		m_Tasks[i]->m_SourcePtr = sptr;
		m_Tasks[i]->m_WidthSize = widthSize;
		m_Tasks[i]->m_RowPitch = resource.RowPitch;
	}

	for (u32 i = 1; i < m_WorkerCount; ++i)
	{
		m_Tasks[i]->AddToPool(Scheduler::ThreadPool());
	}
	Scheduler::ThreadPool()->KickTasks(true);

	m_Tasks[0]->ImmediateExecute();
	{
		PK_SCOPEDLOGGEDPROFILE("WaitMergeTask");

		event.Wait();

		for (u32 i = 0; i < m_WorkerCount; ++i)
		{
			if (!m_Tasks[i]->Done())
				i = 0;
		}
	}

	m_D3D11Context->m_ImmediateDeviceContext->Unmap(m_StagingTexture, subresource);
	return true;
}

//----------------------------------------------------------------------------

bool	CAAED3D11Context::FillCompositingTexture(void *srcBuffer, RHI::EPixelFormat format, u32 width, u32 height, u32 rowLength)
{
	(void)rowLength;

	PK_SCOPEDPROFILE();

	CImageMap image(CUint3(width, height, 1), srcBuffer, RHI::PixelFormatHelpers::PixelFormatToPixelByteSize(format) * width * height);

	PK_TODO("Optim: Use GPU Swizzle instead of CPU");

	RHI::PTexture textureSrc = m_D3D11Manager->CreateTexture(RHI::SRHIResourceInfos("CompositingTexture"), TMemoryView<CImageMap>(image), format);

	m_CompositingTexture = textureSrc;
	return true;
}

//----------------------------------------------------------------------------

TMemoryView<const RHI::PRenderTarget>	CAAED3D11Context::GetCurrentSwapChain()
{
	return m_D3D11Context->m_SwapChainRenderTargets;
}

//----------------------------------------------------------------------------

bool	CAAED3D11Context::_LoadDynamicLibrary()
{
	if (m_Initialized)
		return true;
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

bool	CAAED3D11Context::_CreateDevice()
{
	D3D_FEATURE_LEVEL	featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};
	if (!_PickHardwareAdapter())
	{
		return false;
	}

#if	defined(PK_DEBUG)
	UINT	deviceFlags = (D3D11_CREATE_DEVICE_DEBUG);
#else
	UINT	deviceFlags = 0;
#endif

	if (!PK_D3D11_OK(m_Context->m_CreateDeviceFunc(	m_Context->m_HardwareAdapter,
													D3D_DRIVER_TYPE_UNKNOWN,
													null,
													deviceFlags,
													featureLevels,
													_countof(featureLevels),
													D3D11_SDK_VERSION,
													&m_D3D11Context->m_Device,
													&m_D3D11Context->m_FeatureLevel,
													&m_D3D11Context->m_ImmediateDeviceContext)))
	{
		return false;
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CAAED3D11Context::_PickHardwareAdapter()
{
	IDXGIFactory1	*&factory = m_Context->m_Factory;
	IDXGIAdapter1	*&adapter = m_Context->m_HardwareAdapter;

	adapter = 0;
	for (u32 idx = 0; factory->EnumAdapters1(idx, &adapter) != DXGI_ERROR_NOT_FOUND; ++idx)
	{
		DXGI_ADAPTER_DESC1	desc;
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

__AEGP_PK_END

#endif
