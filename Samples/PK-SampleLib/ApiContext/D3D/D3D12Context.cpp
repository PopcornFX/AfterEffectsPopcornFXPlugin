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

#if (PK_BUILD_WITH_D3D12_SUPPORT != 0) && defined(PK_WINDOWS)

#pragma warning(push)
#pragma warning(disable : 4668) // C4668 (level 4)	'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
#	include <d3d12.h>
#pragma warning(pop)

#include <dxgi1_4.h>

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

#include "D3D12Context.h"
#include <pk_rhi/include/D3D12/D3D12RHI.h>
#include <pk_rhi/include/D3D12/D3D12CommandBuffer.h>
#include <pk_rhi/include/D3D12/D3D12RenderTarget.h>
#include <pk_rhi/include/D3D12/D3D12DescriptorAllocator.h>
#include <pk_rhi/include/D3DCommon/D3DPopcornEnumConversion.h>

#if defined(PK_DEBUG)
#	define PK_ENABLE_DEBUG_LAYER	1
#else
#	define PK_ENABLE_DEBUG_LAYER	0 // disabled by default, set to 1 to enable debug layer controls in Release/Retail.
#endif // defined(PK_DEBUG)

#if	(PK_ENABLE_DEBUG_LAYER != 0)
#	define		USE_DEBUG_DXGI			1 // disabled by default, not available everywhere
#	define		USE_GPU_BASED_DEBUG		0 // disabled by default, not available everywhere
#	define		ENABLE_INFO_DEBUG_LVL	0 // disabled by default, change it for debug when specifically needed
#	define		USE_DRED				0 // disabled by default, not available everywhere
#	define		BREAK_ON_D3D_ERROR		0 // disabled by default, enable for break on D3D errors during API calls
#	define		BREAK_ON_D3D_WARN		0 // disabled by default, enable for break on D3D warnings during API calls
#	if (USE_DEBUG_DXGI != 0)
#		pragma comment(lib, "dxguid.lib")
#		include "d3d12sdklayers.h"
#	endif // (USE_DEBUG_DXGI != 0)
#else
#	define		USE_DEBUG_DXGI			0
#	define		USE_GPU_BASED_DEBUG		0
#	define		ENABLE_INFO_DEBUG_LVL	0 // useless to change this, debug layers shouldn't be enable
#	define		USE_DRED				0
#	define		BREAK_ON_D3D_ERROR		0
#	define		BREAK_ON_D3D_WARN		0
#endif

#if (USE_DEBUG_DXGI != 0)
#	include <dxgidebug.h>
#endif

#if (USE_DRED != 0)
#	include "pk_kernel/include/kr_string_unicode.h"
#endif

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

#if (PK_BUILD_WITH_SDL != 0)
PK_DECLARE_REFPTRCLASS(SdlContext);
#endif

class	CD3D12SwapChain : public RHI::ID3D12SwapChain
{
public:
	IDXGISwapChain3				*m_SwapChain;
	CGuid						m_BufferIndex;
	RHI::PD3D12RenderTarget		m_RenderTargets[CD3D12Context::kFrameCount];

	CD3D12SwapChain()
	:	m_SwapChain(null)
	{
	}

	~CD3D12SwapChain()
	{
		if (m_SwapChain != null)
			m_SwapChain->Release();
	}

	CGuid					BeginFrame(ID3D12CommandQueue *commandQueue)
	{
		(void)commandQueue;

		if (m_SwapChain != null)
		{
			m_BufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
			return m_BufferIndex;
		}

		// the case of offscreen rendering
		return 0;
	}

	virtual TMemoryView<const RHI::PD3D12RenderTarget>	GetD3D12RenderTargets() const
	{
		return TMemoryView<const RHI::PD3D12RenderTarget>(m_RenderTargets);
	}

	virtual TMemoryView<const RHI::PRenderTarget>		GetRenderTargets() const
	{
		return TMemoryView<const RHI::PRenderTarget>(GetD3D12RenderTargets());
	}
};

//----------------------------------------------------------------------------

struct	SD3D12PlatformContext
{
	IDXGIFactory4			*m_Factory;
	IDXGIAdapter1			*m_HardwareAdapter;

	TArray<CD3D12SwapChain*>	m_SwapChains;

#if (USE_DEBUG_DXGI != 0)
	IDXGIDebug1				*m_Debug;
#endif

	PFN_D3D12_CREATE_DEVICE			m_CreateDeviceFunc;
	PFN_D3D12_GET_DEBUG_INTERFACE	m_GetDebugInterfaceFunc;
	HMODULE							m_D3DModule;
	HMODULE							m_DXGIModule;

	SD3D12PlatformContext()
	:	m_Factory(null)
	,	m_HardwareAdapter(null)
#if (USE_DEBUG_DXGI != 0)
	,	m_Debug(null)
#endif
	,	m_D3DModule(0)
	,	m_DXGIModule(0)
	{
	}

	~SD3D12PlatformContext()
	{
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

CD3D12Context::CD3D12Context()
:	m_Context(PK_NEW(SD3D12PlatformContext))
{
}

//----------------------------------------------------------------------------

CD3D12Context::~CD3D12Context()
{
	m_ApiData.m_SwapChains.Clear();

	// Warning: clear swapchains before clear descriptor allocators
	if (m_Context != null)
	{
		for (u32 i = 0; i < m_Context->m_SwapChains.Count(); i++)
			PK_SAFE_DELETE(m_Context->m_SwapChains[i]);
		m_Context->m_SwapChains.Clear();
	}

	// Release allocator
	while (m_ApiData.m_DescriptorAllocators.Count())
	{
		PK_DELETE(m_ApiData.m_DescriptorAllocators.PopBack());
	}
	m_ApiData.m_DescriptorAllocators.Clean();

	if (m_ApiData.m_CommandQueue != null)
	{
		m_ApiData.m_CommandQueue->Release();
		m_ApiData.m_CommandQueue = null;
	}
	if (m_ApiData.m_CopyCommandQueue != null)
	{
		m_ApiData.m_CopyCommandQueue->Release();
		m_ApiData.m_CopyCommandQueue = null;
	}
	if (m_ApiData.m_ComputeCommandQueue != null)
	{
		m_ApiData.m_ComputeCommandQueue->Release();
		m_ApiData.m_ComputeCommandQueue = null;
	}
	if (m_ApiData.m_Device != null)
	{
#if (ENABLE_INFO_DEBUG_LVL != 0)
		ULONG	count = m_ApiData.m_Device->Release();
		if (!PK_VERIFY(count == 0))
			CLog::Log(PK_DBG, "ID3D12Device still referenced %ul times", count);
#else
		m_ApiData.m_Device->Release();
#endif
		m_ApiData.m_Device = null;
	}

	PK_DELETE(m_Context);
}

//----------------------------------------------------------------------------

bool	CD3D12Context::InitRenderApiContext(bool debug, PAbstractWindowContext windowApi)
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
#endif
#else
		return false;
#endif
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

bool	CD3D12Context::LoadDynamicLibrary()
{
	PK_ASSERT(m_Context != null);
	m_Context->m_D3DModule = ::LoadLibraryA("d3d12.dll");
	if (m_Context->m_D3DModule == null)
		return false;
	m_Context->m_DXGIModule = ::LoadLibraryA("dxgi.dll");
	if (m_Context->m_DXGIModule == null)
		return false;

	m_Context->m_CreateDeviceFunc = (PFN_D3D12_CREATE_DEVICE)::GetProcAddress(m_Context->m_D3DModule, "D3D12CreateDevice");
	m_Context->m_GetDebugInterfaceFunc = (PFN_D3D12_GET_DEBUG_INTERFACE)::GetProcAddress(m_Context->m_D3DModule, "D3D12GetDebugInterface");
	m_ApiData.m_SerializeRootSignatureFunc = (PFN_D3D12_SERIALIZE_ROOT_SIGNATURE)::GetProcAddress(m_Context->m_D3DModule, "D3D12SerializeRootSignature");

	return	m_Context->m_CreateDeviceFunc != null &&
			m_Context->m_GetDebugInterfaceFunc != null &&
			m_ApiData.m_SerializeRootSignatureFunc != null;
}

//----------------------------------------------------------------------------

bool	CD3D12Context::InitContext(bool debug)
{
	(void)debug;
	if (m_Context == null)
		return false;
	m_ApiData.m_Api = RHI::GApi_D3D12;
	m_ApiData.m_SwapChainCount = 0;
	m_ApiData.m_BufferingMode = RHI::ContextDoubleBuffering;

	if (!LoadDynamicLibrary())
		return false;

#if (PK_ENABLE_DEBUG_LAYER == 0)
	if (debug)
#endif // (PK_ENABLE_DEBUG_LAYER == 0)
		EnableDebugLayer();

	// try grabbing CreateDXGIFactory2:
	typedef HRESULT	(WINAPI *FnCreateDXGIFactory2)(	UINT				Flags,
													REFIID				riid,
													_COM_Outptr_ void	**ppFactory);

	FnCreateDXGIFactory2	fnCreateDXGIFactory2 = (FnCreateDXGIFactory2)::GetProcAddress(m_Context->m_DXGIModule, "CreateDXGIFactory2");
	// if not found, this is not fatal, it will fallback (ie: on vista & xp)
	if (fnCreateDXGIFactory2 == null)
	{
		CLog::Log(PK_INFO, "DXGI API 'CreateDXGIFactory2' not found, cannot create D3D12 context.");
		return false;
	}

	return	PK_D3D_OK(fnCreateDXGIFactory2(debug ? DXGI_CREATE_FACTORY_DEBUG : 0, IID_PPV_ARGS(&m_Context->m_Factory))) &&
			CreateDevice(debug) &&
			CreateCommandQueue() &&
			CreateDescriptorAllocator();
}

//----------------------------------------------------------------------------

bool	CD3D12Context::WaitAllRenderFinished()
{
	if (m_ApiData.m_Fence == null)
		return false;
	if (!m_ApiData.m_Fence->Reset())
		return false;
	// Note (Alex) : use different value than previously used to avoid to wait partially
	if (!m_ApiData.m_Fence->Signal(~0u))
		return false;
	if (!m_ApiData.m_Fence->Wait(~0u))
		return false;
	return true;
}

//----------------------------------------------------------------------------

CGuid	CD3D12Context::BeginFrame()
{
	return BeginFrame(0u);
}

//----------------------------------------------------------------------------

bool	CD3D12Context::EndFrame(void *renderToWait)
{
	(void)renderToWait;
	return EndFrame(0u);
}

//----------------------------------------------------------------------------

CGuid	CD3D12Context::BeginFrame(u32 swapChainIdx)
{
	CD3D12SwapChain		*swapchain = m_Context->m_SwapChains[swapChainIdx];
	PK_ASSERT(swapchain != null);
	return swapchain->BeginFrame(m_ApiData.m_CommandQueue);
}

//----------------------------------------------------------------------------

#if (USE_DRED != 0)
const char *KOp[] = {
	"OP_SETMARKER",
	"OP_BEGINEVENT",
	"OP_ENDEVENT",
	"OP_DRAWINSTANCED",
	"OP_DRAWINDEXEDINSTANCED",
	"OP_EXECUTEINDIRECT",
	"OP_DISPATCH",
	"OP_COPYBUFFERREGION",
	"OP_COPYTEXTUREREGION",
	"OP_COPYRESOURCE",
	"OP_COPYTILES",
	"OP_RESOLVESUBRESOURCE",
	"OP_CLEARRENDERTARGETVIEW",
	"OP_CLEARUNORDEREDACCESSVIEW",
	"OP_CLEARDEPTHSTENCILVIEW",
	"OP_RESOURCEBARRIER",
	"OP_EXECUTEBUNDLE",
	"OP_PRESENT",
	"OP_RESOLVEQUERYDATA",
	"OP_BEGINSUBMISSION",
	"OP_ENDSUBMISSION",
	"OP_DECODEFRAME",
	"OP_PROCESSFRAMES",
	"OP_ATOMICCOPYBUFFERUINT",
	"OP_ATOMICCOPYBUFFERUINT64",
	"OP_RESOLVESUBRESOURCEREGION",
	"OP_WRITEBUFFERIMMEDIATE",
	"OP_DECODEFRAME1",
	"OP_SETPROTECTEDRESOURCESESSION",
	"OP_DECODEFRAME2",
	"OP_PROCESSFRAMES1",
	"OP_BUILDRAYTRACINGACCELERATIONSTRUCTURE",
	"OP_EMITRAYTRACINGACCELERATIONSTRUCTUREPOSTBUIL",
	"OP_COPYRAYTRACINGACCELERATIONSTRUCTURE",
	"OP_DISPATCHRAYS",
	"OP_INITIALIZEMETACOMMAND",
	"OP_EXECUTEMETACOMMAND",
	"OP_ESTIMATEMOTION",
	"OP_RESOLVEMOTIONVECTORHEAP",
	"OP_SETPIPELINESTATE1",
	"OP_INITIALIZEEXTENSIONCOMMAND",
	"OP_EXECUTEEXTENSIONCOMMAND",
};
#endif

//----------------------------------------------------------------------------

bool	CD3D12Context::EndFrame(u32 swapchainIdx)
{
	CD3D12SwapChain		*swapchain = m_Context->m_SwapChains[swapchainIdx];
	PK_ASSERT(swapchain != null);
#if 1
	// offscreen rendering does not have swapchain instance
	if (swapchain->m_SwapChain == null)
		return true;

	const HRESULT	hr = swapchain->m_SwapChain->Present(0, 0);
	if (hr == DXGI_ERROR_DEVICE_RESET ||
		hr == DXGI_ERROR_DEVICE_REMOVED)
	{
		PK_D3D_OK(m_ApiData.m_Device->GetDeviceRemovedReason());

#if (USE_DRED != 0)

		// Gather extended device remove data
		ID3D12DeviceRemovedExtendedData	*dred = null;
		if (S_OK == m_ApiData.m_Device->QueryInterface(IID_PPV_ARGS(&dred)))
		{
			D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT DredAutoBreadcrumbsOutput;
			D3D12_DRED_PAGE_FAULT_OUTPUT DredPageFaultOutput;
			dred->GetAutoBreadcrumbsOutput(&DredAutoBreadcrumbsOutput);
			dred->GetPageFaultAllocationOutput(&DredPageFaultOutput);

			const D3D12_AUTO_BREADCRUMB_NODE * node = DredAutoBreadcrumbsOutput.pHeadAutoBreadcrumbNode;
			CLog::Log(PK_DBG, "AUTO BREADCRUMNS:");
			while (node != null)
			{
				const char *name = node->pCommandListDebugNameA;
				CString		tmp;
				if (name == null && node->pCommandListDebugNameW != null)
				{
					CStringUnicode	n(node->pCommandListDebugNameW);
					tmp = n.ToAscii();
					name = tmp.Data();
				}
				for (u32 i = 0; i < node->BreadcrumbCount; ++i)
				{
					const char *op = KOp[node->pCommandHistory[i]];
					CLog::Log(PK_DBG, "%s - %s", op, name);
				}
				node = node->pNext;
			}

			PK_BREAKPOINT();

			dred->Release();
		}
#endif

		return false;
	}
	if (hr == DXGI_STATUS_OCCLUDED)
		return true;

	return PK_D3D_OK(hr);//!FAILED(hr);
#else
	if (PK_D3D_FAILED(swapchain->m_SwapChain->Present(0, 0)))
		return false;
	return true;
#endif
}

//----------------------------------------------------------------------------

bool	CD3D12Context::RecreateSwapChain(const CUint2 &ctxSize)
{
	PK_ASSERT(m_ApiData.m_SwapChains.Count() == 1);
	if (!PK_VERIFY(!m_ApiData.m_SwapChains.Empty()))
		return false;
	return RecreateSwapChain(0, ctxSize);
}

//----------------------------------------------------------------------------

TMemoryView<const RHI::PRenderTarget>	CD3D12Context::GetCurrentSwapChain()
{
	if (PK_VERIFY(!m_ApiData.m_SwapChains.Empty()))
		return m_ApiData.m_SwapChains[0]->GetRenderTargets();

	return TMemoryView<const RHI::PRenderTarget>();
}

//----------------------------------------------------------------------------

bool	CD3D12Context::EnableDebugLayer()
{
	ID3D12Debug		*debugController = null;
	if (PK_D3D_FAILED(m_Context->m_GetDebugInterfaceFunc(IID_PPV_ARGS(&debugController))))
		return false;
	debugController->EnableDebugLayer();
#if (USE_DEBUG_DXGI != 0)
	// try grabbing DXGIGetDebugInterface1:
	typedef HRESULT (WINAPI *FnDXGIGetDebugInterface1)(UINT Flags, REFIID riid, _COM_Outptr_ void **pDebug);
	FnDXGIGetDebugInterface1	fnDXGIGetDebugInterface1 = (FnDXGIGetDebugInterface1)::GetProcAddress(m_Context->m_DXGIModule, "DXGIGetDebugInterface1");

	if (PK_D3D_FAILED(fnDXGIGetDebugInterface1(0, IID_PPV_ARGS(&m_Context->m_Debug))))
		return false;
	m_Context->m_Debug->EnableLeakTrackingForThread();
#endif
#if (USE_GPU_BASED_DEBUG != 0)
	ID3D12Debug1		*debugController1 = null;
	if (PK_D3D_FAILED(m_Context->m_GetDebugInterfaceFunc(IID_PPV_ARGS(&debugController1))))
		return false;
	debugController1->SetEnableGPUBasedValidation(TRUE);
#endif
#if (USE_DRED != 0)
	ID3D12DeviceRemovedExtendedDataSettings	*dredSettings = null;
	if (PK_D3D_FAILED(m_Context->m_GetDebugInterfaceFunc(IID_PPV_ARGS(&dredSettings))))
		return false;
	dredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
	dredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
	dredSettings->Release();
#endif
	return true;
}

//----------------------------------------------------------------------------

bool	CD3D12Context::CreateDevice(bool debug)
{
	(void)debug;
	if (!PickHardwareAdapter())
	{
		CLog::Log(PK_ERROR, "D3D12: Couldn't pick hardware adapter");
		return false;
	}
	if (!PK_D3D_OK(m_Context->m_CreateDeviceFunc(m_Context->m_HardwareAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_ApiData.m_Device))))
	{
		CLog::Log(PK_ERROR, "D3D12: Couldn't create device");
		return false;
	}

	D3D12_FEATURE_DATA_FEATURE_LEVELS	levelFeature = {};
	const D3D_FEATURE_LEVEL				levels[] =
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_12_1
	};
	levelFeature.NumFeatureLevels = PK_ARRAY_COUNT(levels);
	levelFeature.pFeatureLevelsRequested = levels;
	m_ApiData.m_Device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &levelFeature, sizeof(levelFeature));

	if (levelFeature.MaxSupportedFeatureLevel > D3D_FEATURE_LEVEL_11_0)
	{
		m_ApiData.m_Device->Release();
		m_ApiData.m_Device = null;
		if (!PK_D3D_OK(m_Context->m_CreateDeviceFunc(m_Context->m_HardwareAdapter, levelFeature.MaxSupportedFeatureLevel, IID_PPV_ARGS(&m_ApiData.m_Device))))
		{
			PK_ASSERT_NOT_REACHED_MESSAGE("Internal error when creating d3d12 device.");
			return false;
		}
	}

#if (PK_ENABLE_DEBUG_LAYER == 0)
	if (debug)
#endif // (PK_ENABLE_DEBUG_LAYER == 0)
		AdditionalFilterForDebugLayer();

	return true;
}

//----------------------------------------------------------------------------

void	CD3D12Context::AdditionalFilterForDebugLayer()
{
	ID3D12InfoQueue	*infoQueue = null;
	m_ApiData.m_Device->QueryInterface(&infoQueue);
	if (infoQueue == null)
		return;
	D3D12_INFO_QUEUE_FILTER	filter;
	Mem::Clear(filter);
#if		(ENABLE_INFO_DEBUG_LVL == 0)
	D3D12_MESSAGE_SEVERITY	denySeverity[] = { D3D12_MESSAGE_SEVERITY_INFO }; // deny info level
	filter.DenyList.NumSeverities = PK_ARRAY_COUNT(denySeverity);
	filter.DenyList.pSeverityList = denySeverity;
#endif
	D3D12_MESSAGE_ID		denyIDs[] = { D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE }; // deny clear warning on RT
	filter.DenyList.NumIDs = PK_ARRAY_COUNT(denyIDs);
	filter.DenyList.pIDList = denyIDs;
	PK_D3D_OK(infoQueue->PushStorageFilter(&filter));

#if (BREAK_ON_D3D_ERROR != 0) || (BREAK_ON_D3D_WARN != 0)
	PK_D3D_OK(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE));
	PK_D3D_OK(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
#	if (BREAK_ON_D3D_WARN != 0)
	PK_D3D_OK(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE));
#	endif	// (BREAK_ON_D3D_WARN != 0)
#endif // (BREAK_ON_D3D_ERROR != 0) || (BREAK_ON_D3D_WARN != 0)

	infoQueue->Release();
}

//----------------------------------------------------------------------------

bool	CD3D12Context::PickHardwareAdapter()
{
	IDXGIFactory4	*&factory = m_Context->m_Factory;
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
		if (SUCCEEDED(m_Context->m_CreateDeviceFunc(adapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), null)))
		{
			return true;
		}
	}
	adapter = 0;
	return false;
}

//----------------------------------------------------------------------------

bool	CD3D12Context::CreateDescriptorAllocator()
{
	// Create allocators if not exists or missing
	TArray<RHI::CD3D12DescriptorAllocator*> & allocators = m_ApiData.m_DescriptorAllocators;
	u32 count = allocators.Count();
	if (count < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES)
	{
		if (!allocators.Resize(D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES))
			return false;
		allocators[0] = PK_NEW(RHI::CD3D12DescriptorAllocator(m_ApiData.m_Device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 8192));
		if (allocators[0] == null)
			return false;
		allocators[1] = PK_NEW(RHI::CD3D12DescriptorAllocator(m_ApiData.m_Device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 512));
		if (allocators[1] == null)
			return false;
		allocators[2] = PK_NEW(RHI::CD3D12DescriptorAllocator(m_ApiData.m_Device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 32u));
		if (allocators[2] == null)
			return false;
		allocators[3] = PK_NEW(RHI::CD3D12DescriptorAllocator(m_ApiData.m_Device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 32u));
		if (allocators[3] == null)
			return false;
	}

	return true;
}

//----------------------------------------------------------------------------

bool	CD3D12Context::CreateCommandQueue()
{
	D3D12_COMMAND_QUEUE_DESC	graphicsCmdQueueDesc = {};
	graphicsCmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	graphicsCmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT; // Disable TDR for compute
	graphicsCmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;

	D3D12_COMMAND_QUEUE_DESC	copyCmdQueueDesc = {};
	copyCmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	copyCmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT; // Disable TDR for compute
	copyCmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;

	D3D12_COMMAND_QUEUE_DESC	computeCmdQueueDesc = {};
	computeCmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	computeCmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT; // Disable TDR for compute
	computeCmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;

	return	PK_D3D_OK(m_ApiData.m_Device->CreateCommandQueue(&graphicsCmdQueueDesc, IID_PPV_ARGS(&m_ApiData.m_CommandQueue))) &&
			PK_D3D_OK(m_ApiData.m_Device->CreateCommandQueue(&copyCmdQueueDesc, IID_PPV_ARGS(&m_ApiData.m_CopyCommandQueue))) &&
			PK_D3D_OK(m_ApiData.m_Device->CreateCommandQueue(&computeCmdQueueDesc, IID_PPV_ARGS(&m_ApiData.m_ComputeCommandQueue)));
}

//----------------------------------------------------------------------------

bool	CD3D12Context::RecreateSwapChain(u32 swapChainIdx, const CUint2 &ctxSize)
{
	WaitAllRenderFinished();
	if (!PK_VERIFY(swapChainIdx < m_Context->m_SwapChains.Count()))
		return false;

	CD3D12SwapChain	*swapchain = m_Context->m_SwapChains[swapChainIdx];
	PK_ASSERT(swapchain != null);

	for (u32 i = 0; i < kFrameCount; ++i)
	{
		RHI::PD3D12RenderTarget	&rt = swapchain->m_RenderTargets[i];
		PK_FOREACH(it, m_ApiData.m_DeferredCommandBuffer)
		{
			RHI::CD3D12CommandBuffer	*cmdBuff = it->Get();
			if (cmdBuff->IsBackBufferUsed(rt))
				cmdBuff->D3D12ReleaseCommandList();
		}
		rt->ReleaseResources();
	}

	if (PK_D3D_FAILED(swapchain->m_SwapChain->ResizeBuffers(kFrameCount, ctxSize.x(), ctxSize.y(), DXGI_FORMAT_B8G8R8A8_UNORM, 0)))
	{
		return false;
	}
	if (!CreateRenderTargets(swapChainIdx, ctxSize, null))
		return false;
	return true;
}

//----------------------------------------------------------------------------

bool	CD3D12Context::DestroySwapChain(u32 swapChainIdx)
{
	if (!PK_VERIFY(swapChainIdx < m_Context->m_SwapChains.Count()))
		return false;

	CD3D12SwapChain	*swapchain = m_Context->m_SwapChains[swapChainIdx];
	PK_ASSERT(swapchain != null);

	for (u32 i = 0; i < kFrameCount; ++i)
	{
		RHI::PD3D12RenderTarget	&rt = swapchain->m_RenderTargets[i];
		PK_FOREACH(it, m_ApiData.m_DeferredCommandBuffer)
		{
			RHI::CD3D12CommandBuffer	*cmdBuff = it->Get();
			if (cmdBuff->IsBackBufferUsed(rt))
				cmdBuff->D3D12ReleaseCommandList();
		}
		rt->ReleaseResources();
	}
	m_Context->m_SwapChains.Remove(swapChainIdx);
	PK_SAFE_DELETE(swapchain);
	m_ApiData.m_SwapChainCount--;
	m_ApiData.m_SwapChains = TMemoryView<RHI::ID3D12SwapChain*>(m_Context->m_SwapChains.ViewForWriting());
	return true;
}

//----------------------------------------------------------------------------

bool	CD3D12Context::AddSwapChain(HWND winHandle, CUint2 winSize, TMemoryView<const RHI::PRenderTarget> *view /*= null*/)
{
	CD3D12SwapChain	*swapchain = PK_NEW(CD3D12SwapChain());
	if (swapchain == null)
		return false;

	const CGuid	idx = m_Context->m_SwapChains.PushBack(swapchain);
	if (!PK_VERIFY(idx.Valid()))
	{
		PK_DELETE(swapchain);
		return false;
	}

	m_ApiData.m_SwapChainCount++;
	m_ApiData.m_SwapChains = TMemoryView<RHI::ID3D12SwapChain*>(m_Context->m_SwapChains.ViewForWriting());

	if (!CreateSwapChain(idx, winHandle, winSize, view))
	{
		// NOTE(Julien) : This feels sloppy. what behavior should a failure to create the swapchain have? doesn't seem clear
		// when 'AddSwapChain' returns false, in theory, there shouldn't be a new element in 'm_SwapChains'
		// it's the case currently if we don't run the code below.
		// If that's the behavior we want, it needs to be homogenized with the 'RecreateSwapChain' function above
		// to behave consistently across the different swapchain manipulation calls.
#if 0
		m_Context->m_SwapChains.Remove(idx);
		PK_SAFE_DELETE(swapchain);
		m_ApiData.m_SwapChainCount--;
		m_ApiData.m_SwapChains = TMemoryView<RHI::ID3D12SwapChain*>(m_Context->m_SwapChains.ViewForWriting());
#endif
		return false;
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CD3D12Context::CreateSwapChain(u32 swapChainIdx, HWND winHandle, CUint2 winSize, TMemoryView<const RHI::PRenderTarget> *view)
{
	CD3D12SwapChain	*swapchain = m_Context->m_SwapChains[swapChainIdx];
	PK_ASSERT(swapchain != null);

	DXGI_SWAP_CHAIN_DESC1	swapChainDesc = {};
	swapChainDesc.Width = winSize.x();
	swapChainDesc.Height = winSize.y();
	swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = kFrameCount;
	swapChainDesc.Scaling = DXGI_SCALING_NONE;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

	IDXGISwapChain1	*dxswapChain = null;
#if defined(PK_WINDOWS)
	if (PK_D3D_FAILED(m_Context->m_Factory->CreateSwapChainForHwnd(m_ApiData.m_CommandQueue, winHandle, &swapChainDesc, null, null, &dxswapChain)))
#endif
		return false;
	if (PK_D3D_FAILED(dxswapChain->QueryInterface(&swapchain->m_SwapChain)))
		return false;
	dxswapChain->Release();
	return CreateRenderTargets(swapChainIdx, winSize, view);
}

//----------------------------------------------------------------------------

bool	CD3D12Context::CreateRenderTargets(u32 swapChainIdx, CUint2 winSize, TMemoryView<const RHI::PRenderTarget> *view)
{
	CD3D12SwapChain	*swapchain = m_Context->m_SwapChains[swapChainIdx];
	PK_ASSERT(swapchain != null);

	for (u32 i = 0; i < kFrameCount; ++i)
	{
		ID3D12Resource	*rtResource = null;
		if (PK_D3D_FAILED(swapchain->m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&rtResource))))
			return false;
		RHI::CD3D12RenderTarget * rhiRT = swapchain->m_RenderTargets[i] != null ? swapchain->m_RenderTargets[i].Get() : PK_NEW(RHI::CD3D12RenderTarget(RHI::SRHIResourceInfos("D3D12Context Swap Chain")));
		if (rhiRT == null)
			return false;
		rhiRT->D3D12SetRenderTarget(rtResource, RHI::FormatSrgb8BGRA, winSize, true);
		rhiRT->D3D12SetResourceState(D3D12_RESOURCE_STATE_PRESENT);
		swapchain->m_RenderTargets[i] = rhiRT;
		RHI::D3D12SetDebugObjectName(rtResource, rhiRT->DebugName());
	}

	if (view != null)
		*view = swapchain->GetRenderTargets();
	return true;
}

//----------------------------------------------------------------------------

bool	CD3D12Context::CreateOffscreenRenderTarget(const CUint2 &winSize)
{
	D3D12_HEAP_PROPERTIES	heapProps = { D3D12_HEAP_TYPE_DEFAULT };
	D3D12_RESOURCE_DESC 	textureDesc = {};
	textureDesc.Width = winSize.x();
	textureDesc.Height = winSize.y();
	textureDesc.DepthOrArraySize = 1;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	// create a fake swapchain
	CD3D12SwapChain	*swapchain = PK_NEW(CD3D12SwapChain());
	PK_ASSERT(swapchain != null);

	const CGuid	idx = m_Context->m_SwapChains.PushBack(swapchain);
	if (!PK_VERIFY(idx.Valid()))
	{
		PK_DELETE(swapchain);
		return false;
	}

	m_ApiData.m_SwapChainCount++;
	m_ApiData.m_SwapChains = TMemoryView<RHI::ID3D12SwapChain*>(m_Context->m_SwapChains.ViewForWriting());
	m_ApiData.m_BufferingMode = RHI::ContextDoubleBuffering;

	for (u32 i = 0; i < kFrameCount; ++i)
	{
		RHI::CD3D12RenderTarget * rhiRT = swapchain->m_RenderTargets[i] != null ? swapchain->m_RenderTargets[i].Get() : PK_NEW(RHI::CD3D12RenderTarget(RHI::SRHIResourceInfos("D3D12Context Offscreen Swap Chain")));
		if (rhiRT == null)
			return false;

		ID3D12Resource			*rtResource = null;
		if (PK_D3D_FAILED(m_ApiData.m_Device->CreateCommittedResource(	&heapProps,
																		D3D12_HEAP_FLAG_NONE,
																		&textureDesc,
																		D3D12_RESOURCE_STATE_PRESENT,
																		null,
																		IID_PPV_ARGS(&rtResource))))
			return false;

		rhiRT->D3D12SetRenderTarget(rtResource, RHI::FormatSrgb8BGRA, winSize);
		rhiRT->D3D12SetResourceState(D3D12_RESOURCE_STATE_PRESENT);
		swapchain->m_RenderTargets[i] = rhiRT;
	}
	return true;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

#endif // (PK_BUILD_WITH_D3D12_SUPPORT != 0) && defined(PK_WINDOWS)
