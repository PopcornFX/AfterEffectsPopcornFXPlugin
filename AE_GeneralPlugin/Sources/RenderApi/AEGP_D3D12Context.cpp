//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"
#include "RenderApi/AEGP_D3D12Context.h"

#if	(PK_BUILD_WITH_D3D12_SUPPORT != 0)

#include <pk_rhi/include/D3D12/D3D12RHI.h>
#include <pk_rhi/include/D3D12/D3D12ApiManager.h>
#include <pk_rhi/include/D3D12/D3D12BasicContext.h>
#include <pk_rhi/include/D3D12/D3D12Texture.h>
#include <pk_rhi/include/D3D12/D3D12RenderTarget.h>
#include "pk_rhi/include/D3D12/D3D12ReadBackTexture.h"

#include <pk_rhi/include/D3D12/D3D12PopcornEnumConversion.h>

#include <PK-SampleLib/ApiContext/D3D/D3D12Context.h>

#include <pk_kernel/include/kr_thread_pool_default.h>

#include "AEGP_World.h"
#include "RenderApi/AEGP_CopyTask.h"

#include <d3d12.h>

#include <dxgi1_4.h>
#include <wrl/client.h>

__AEGP_PK_BEGIN

//----------------------------------------------------------------------------

class	CD3D12SwapChainRT : public RHI::ID3D12SwapChain
{
public:
	CGuid						m_BufferIndex;

	RHI::PD3D12RenderTarget		m_RenderTargets[CAAED3D12Context::kFrameCount];
	RHI::PD3D12ReadBackTexture	m_ReadbackTextures[CAAED3D12Context::kFrameCount];


	CD3D12SwapChainRT()
		: m_BufferIndex(0)
	{
	}

	~CD3D12SwapChainRT()
	{
		m_BufferIndex = 0;
	}

	CGuid					BeginFrame()
	{
		m_BufferIndex = (m_BufferIndex + 1) % CAAED3D12Context::kFrameCount;
		return m_BufferIndex;
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

class	CD3D12SwapChain : public RHI::ID3D12SwapChain
{
public:
	IDXGISwapChain3				*m_SwapChain;
	CGuid						m_BufferIndex;
	RHI::PD3D12RenderTarget		m_RenderTargets[CAAED3D12Context::kFrameCount];

	CD3D12SwapChain()
		: m_SwapChain(null)
	{
	}

	~CD3D12SwapChain()
	{
		if (m_SwapChain != null)
			m_SwapChain->Release();
	}

	CGuid					BeginFrame()
	{
		m_BufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
		return m_BufferIndex;
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
	IDXGIFactory4				*m_Factory;
	IDXGIAdapter1				*m_HardwareAdapter;

	TArray<CD3D12SwapChainRT*>	m_SwapChainsRTs;


	PFN_D3D12_CREATE_DEVICE				m_CreateDeviceFunc;
	PFN_D3D12_GET_DEBUG_INTERFACE		m_GetDebugInterfaceFunc;

	HMODULE						m_D3DModule;
	HMODULE						m_DXGIModule;

	SD3D12PlatformContext()
		: m_Factory(null)
		, m_HardwareAdapter(null)
#if USE_DEBUG_DXGI
		, m_Debug(null)
#endif
		, m_D3DModule(0)
		, m_DXGIModule(0)
	{
	}

	~SD3D12PlatformContext()
	{
		if (m_Factory != null)
			m_Factory->Release();
		if (m_HardwareAdapter != null)
			m_HardwareAdapter->Release();
		if (m_D3DModule != null)
			FreeLibrary(m_D3DModule);
		if (m_DXGIModule != null)
			FreeLibrary(m_DXGIModule);
	}
};

//----------------------------------------------------------------------------

CAAED3D12Context	*CAAED3D12Context::m_Instance = null;
bool				CAAED3D12Context::m_Once = false;

//----------------------------------------------------------------------------

CAAED3D12Context::CAAED3D12Context()
	: m_Context(PK_NEW(SD3D12PlatformContext))
{
	for (u32 i = 0; i < kFrameCount; ++i)
	{
		m_Resources[i] = null;
	}
	m_D3D12Manager = PK_NEW(RHI::CD3D12ApiManager);
	m_D3D12Context = PK_NEW(RHI::SD3D12BasicContext);

	m_ApiContext = m_D3D12Context;
	m_ApiManager = m_D3D12Manager;
}

//----------------------------------------------------------------------------

CAAED3D12Context::~CAAED3D12Context()
{
	ClearContextSwapchainsRT();
	m_D3D12Manager->SwapChainRemoved(0);

	m_D3D12Manager = null;
	m_ApiManager = null;

	if (m_D3D12Context->m_Device != null)
		m_D3D12Context->m_Device->Release();
	PK_SAFE_DELETE(m_Context);
	m_ApiContext = null;
	PK_SAFE_DELETE(m_D3D12Context);
	m_Tasks.Clear();
}

//----------------------------------------------------------------------------

bool	CAAED3D12Context::InitIFN()
{
	if (m_Initialized || m_Once)
		return true;
	m_Initialized = true;
	m_Once = true;

	m_WorkerCount = CPopcornFXWorld::Instance().GetWorkerCount() + 1;
	m_Tasks.Resize(m_WorkerCount);

	m_ApiManager->InitApi(m_ApiContext);
	m_Fence = m_D3D12Manager->CreateFence(RHI::SRHIResourceInfos("Fence"));
	return true;
}

//----------------------------------------------------------------------------

bool	CAAED3D12Context::BeginFrame()
{
	if (m_Context->m_SwapChainsRTs.Count() > 0)
		m_ApiManager->BeginFrame(m_Context->m_SwapChainsRTs[0]->BeginFrame());
	else
		return false;
	LogApiError();
	return true;
}

//----------------------------------------------------------------------------

bool	CAAED3D12Context::EndFrame()
{
	LogApiError();

	m_FrameCount += 1;
	if (m_Context->m_SwapChainsRTs.Count() > 0)
	{
		RHI::PCommandBuffer cmdBuff = m_ApiManager->CreateCommandBuffer(RHI::SRHIResourceInfos("Command Buffer"), true);

		if (cmdBuff != null)
		{
			cmdBuff->Start();
			
			cmdBuff->ReadBackRenderTarget(m_Context->m_SwapChainsRTs[0]->m_RenderTargets[m_Context->m_SwapChainsRTs[0]->m_BufferIndex],
										  m_Context->m_SwapChainsRTs[0]->m_ReadbackTextures[m_Context->m_SwapChainsRTs[0]->m_BufferIndex]);
	
			cmdBuff->Stop();
			m_ApiManager->SubmitCommandBufferDirect(cmdBuff);
		}
		else
			CLog::Log(PK_ERROR, "D3D12: Create command buffer failed");
	}
	m_ApiManager->EndFrame();
	//Keep that after the endframe;
	m_Fence->Signal(m_FrameCount, m_D3D12Context->m_CommandQueue);
	return true;
}

//----------------------------------------------------------------------------

void	CAAED3D12Context::LogApiError()
{
}

//----------------------------------------------------------------------------

bool	CAAED3D12Context::CreateDescriptorAllocator()
{
	// Create allocators if not exists or missing
	TArray<RHI::CD3D12DescriptorAllocator*> & allocators = m_D3D12Context->m_DescriptorAllocators;
	u32 count = allocators.Count();
	if (count < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES)
	{
		if (!allocators.Resize(D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES))
			return false;
		for (u32 i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
		{
			u32	batchCount = (i >= D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER) ? 64u : 8192u;
			allocators[i] = PK_NEW(RHI::CD3D12DescriptorAllocator(m_D3D12Context->m_Device, static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i), batchCount));
			if (allocators[i] == null)
				return false;
		}
	}

	return true;
}

//----------------------------------------------------------------------------

bool	CAAED3D12Context::LoadDynamicLibrary()
{
	PK_ASSERT(m_Context != null);
	m_Context->m_D3DModule = ::LoadLibraryA("d3d12.dll");
	if (m_Context->m_D3DModule == null)
		return false;
	m_Context->m_DXGIModule = ::LoadLibraryA("dxgi.dll");
	if (m_Context->m_DXGIModule == null)
		return false;
	m_Context->m_CreateDeviceFunc = (PFN_D3D12_CREATE_DEVICE)::GetProcAddress(m_Context->m_D3DModule, "D3D12CreateDevice");
	m_D3D12Context->m_SerializeRootSignatureFunc = (PFN_D3D12_SERIALIZE_ROOT_SIGNATURE)::GetProcAddress(m_Context->m_D3DModule, "D3D12SerializeRootSignature");

	return	m_Context->m_CreateDeviceFunc != null &&
			m_D3D12Context->m_SerializeRootSignatureFunc != null;
}

//----------------------------------------------------------------------------

bool	CAAED3D12Context::CreateDevice()
{
	if (!PickHardwareAdapter())
		return false;
	if (!PK_D3D_OK(m_Context->m_CreateDeviceFunc(m_Context->m_HardwareAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_D3D12Context->m_Device))))
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
	m_D3D12Context->m_Device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &levelFeature, sizeof(levelFeature));

	if (levelFeature.MaxSupportedFeatureLevel > D3D_FEATURE_LEVEL_11_0)
	{
		m_D3D12Context->m_Device->Release();
		m_D3D12Context->m_Device = null;
		if (!PK_D3D_OK(m_Context->m_CreateDeviceFunc(m_Context->m_HardwareAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_D3D12Context->m_Device))))
		{
			PK_ASSERT_NOT_REACHED_MESSAGE("Internal error when creating d3d12 device.");
			return false;
		}
	}
	return true;
}

//----------------------------------------------------------------------------

//Duplicate of PK-Samples\PK-SampleLib\ApiContext\D3D\D3D12Context.h::PickHardwareAdapter
bool	CAAED3D12Context::PickHardwareAdapter()
{
	IDXGIFactory4	*&factory = m_Context->m_Factory;
	IDXGIAdapter1	*&adapter = m_Context->m_HardwareAdapter;
	adapter = 0;
	HRESULT			hres;
	for (u32 idx = 0; (hres = factory->EnumAdapters1(idx, &adapter)) != DXGI_ERROR_NOT_FOUND; ++idx)
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
		HRESULT hr = m_Context->m_CreateDeviceFunc(adapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), null);
		if (SUCCEEDED(hr))
		{
			return true;
		}
	}
	PK_ASSERT_NOT_REACHED();
	adapter = 0;
	return false;
}

//----------------------------------------------------------------------------

bool	CAAED3D12Context::CreatePlatformContext(void *winHandle, void *deviceContext)
{
	(void)winHandle;
	if (m_Once)
		return true;

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
	m_D3D12Context->m_Api = RHI::GApi_D3D12;
	m_D3D12Context->m_SwapChainCount = 0;

	if (!LoadDynamicLibrary())
		return false;
	// If you want to develop with the debug layer on:
	// -with DirectX Control panel(accessible via visual studio Debug->Graphics->DirectX Control panel)
	// - Click on Scope->Edit List...->Add : AfterFX.exe
	// - Then Direct3D / DXGI Debug Layer : Choose the `Force On` Option and voila !You got your debug logs.

#if 0
	Microsoft::WRL::ComPtr<ID3D12Debug> debugController;

	m_Context->m_GetDebugInterfaceFunc = (PFN_D3D12_GET_DEBUG_INTERFACE)::GetProcAddress(m_Context->m_D3DModule, "D3D12GetDebugInterface");
	if (PK_D3D_FAILED(m_Context->m_GetDebugInterfaceFunc(IID_PPV_ARGS(&debugController))))
		return false;
	debugController->EnableDebugLayer();
#endif

	// try grabbing CreateDXGIFactory1:
	typedef HRESULT(WINAPI *FnCreateDXGIFactory2)(UINT Flags, REFIID riid, _COM_Outptr_ void	**ppFactory);

	FnCreateDXGIFactory2	CreateDXGIFactory2 = (FnCreateDXGIFactory2)::GetProcAddress(m_Context->m_DXGIModule, "CreateDXGIFactory2");
	// if not found, this is not fatal, it will fallback (ie: on vista & xp)
	if (CreateDXGIFactory2 == null)
	{
		CLog::Log(PK_INFO, "DXGI API 'FnCreateDXGIFactory2' not found, cannot create D3D12 context.");
		return false;
	}

	UINT flags = 0;
	if (!PK_D3D_OK(CreateDXGIFactory2(flags, IID_PPV_ARGS(&m_Context->m_Factory))))
		return false;

	if (!CreateDevice())
		return false;

	return CreateCommandQueue() &&
		CreateDescriptorAllocator();;
}

//----------------------------------------------------------------------------

bool	CAAED3D12Context::CreateCommandQueue()
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

	return	PK_D3D_OK(m_D3D12Context->m_Device->CreateCommandQueue(&graphicsCmdQueueDesc, IID_PPV_ARGS(&m_D3D12Context->m_CommandQueue))) &&
			PK_D3D_OK(m_D3D12Context->m_Device->CreateCommandQueue(&copyCmdQueueDesc, IID_PPV_ARGS(&m_D3D12Context->m_CopyCommandQueue))) &&
			PK_D3D_OK(m_D3D12Context->m_Device->CreateCommandQueue(&computeCmdQueueDesc, IID_PPV_ARGS(&m_D3D12Context->m_ComputeCommandQueue)));
}

//----------------------------------------------------------------------------

bool	CAAED3D12Context::CreateRenderTarget(RHI::EPixelFormat format, CUint3 size)
{
	if (m_Context->m_SwapChainsRTs.Count() != 0)
	{
		ClearContextSwapchainsRT();
		m_D3D12Manager->SwapChainRemoved(0);
	}

	CD3D12SwapChainRT	*swapchainRT = PK_NEW(CD3D12SwapChainRT());
	const CGuid			idx = m_Context->m_SwapChainsRTs.PushBack(swapchainRT);
	if (!PK_VERIFY(idx.Valid()))
	{
		PK_DELETE(swapchainRT);
		return false;
	}
	m_D3D12Context->m_SwapChainCount = 1;
	m_D3D12Context->m_SwapChains = TMemoryView<RHI::ID3D12SwapChain*>(m_Context->m_SwapChainsRTs.ViewForWriting());

	for (u32 i = 0; i < CAAED3D12Context::kFrameCount; ++i)
	{
		RHI::PD3D12RenderTarget		rt = PK_NEW(RHI::CD3D12RenderTarget(RHI::SRHIResourceInfos("Render Target")));
		if (rt == null)
		{
			CLog::Log(PK_ERROR, "D3D12Context: CD3D12RenderTarget creation failure");
			return false;
		}
		ID3D12Resource			*resource = null;
		D3D12_HEAP_PROPERTIES	heapProps = { D3D12_HEAP_TYPE_DEFAULT };
		D3D12_RESOURCE_DESC		resDesc = {};
		D3D12_RESOURCE_STATES	initialState = (PKRHI_D3D12_SHADER_RESOURCE_STATE);
		D3D12_CLEAR_VALUE		defaultClearValue;

		resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resDesc.Alignment = 0;
		resDesc.Width = size.x();
		resDesc.Height = size.y();
		resDesc.DepthOrArraySize = 1;
		resDesc.MipLevels = 1;
		resDesc.Format = RHI::D3DConversion::PopcornToD3DPixelFormat(format);
		resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		resDesc.SampleDesc.Count = 1;
		resDesc.SampleDesc.Quality = 0;

		defaultClearValue.Format = RHI::D3DConversion::PopcornToD3DPixelFormat(format);
		for (u32 c = 0; c < 4; ++c)
			defaultClearValue.Color[c] = 0.0f;
		
		if (PK_D3D_FAILED(m_D3D12Context->m_Device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, initialState, null, PK_RHI_IID_PPV_ARGS(&resource))))
		{
			CLog::Log(PK_ERROR, "D3D12Context: CreateCommittedResource failure");
			m_D3D12Context->m_SwapChainCount = 0;
			m_D3D12Context->m_SwapChains = TMemoryView<RHI::ID3D12SwapChain*>();
			ClearContextSwapchainsRT();
			return false;
		}
		rt->D3D12SetRenderTarget(resource, format, size.xy(), true, null, RHI::SampleCount1);
		m_Resources[i] = resource;

		RHI::PReadBackTexture readbackTex = m_D3D12Manager->CreateReadBackTexture(RHI::SRHIResourceInfos("readback Texture"), rt);

		if (readbackTex == null)
		{
			CLog::Log(PK_ERROR, "D3D12Context: CreateReadBackTexture failure");
			return false;
		}
		m_Context->m_SwapChainsRTs[0]->m_RenderTargets[i] = rt;
		m_Context->m_SwapChainsRTs[0]->m_ReadbackTextures[i] = CastD3D12(readbackTex);
	}
	m_D3D12Manager->SwapChainAdded();

	return true;
}

//----------------------------------------------------------------------------

bool	CAAED3D12Context::SetAsCurrent(void *deviceContext)
{
	(void)deviceContext;
	return true;
}

//----------------------------------------------------------------------------

PRefCountedMemoryBuffer CAAED3D12Context::CreateBufferFromReadBackTexture(RHI::PCReadBackTexture readBackTexture) const
{
	PK_SCOPEDPROFILE();

	u32							size = RHI::PixelFormatHelpers::PixelFormatToPixelByteSize(readBackTexture->GetFormat()) * readBackTexture->GetSize().x() * readBackTexture->GetSize().y();
	PRefCountedMemoryBuffer		rawBuffer = CRefCountedMemoryBuffer::AllocAligned(size);

	if (rawBuffer == null)
	{
		CLog::Log(PK_ERROR, "D3D12Context: CreateBufferFromReadBackTexture alloc failure");
		return null;
	}

	RHI::PCD3D12ReadBackTexture	d3dReadBackTex = CastD3D12(readBackTexture);
	ID3D12Resource				*d3dResource = d3dReadBackTex->D3D12GetResource();
	void						*mappedData;

	if (PK_D3D_FAILED(d3dResource->Map(0, null, &mappedData)))
	{
		CLog::Log(PK_ERROR, "D3D12Context: map failure");
		return null;
	}

	const u32	rowcount = d3dReadBackTex->D3D12GetFootprint().m_RowCount;
	const u32	rowpitch = d3dReadBackTex->D3D12GetFootprint().m_Footprint.Footprint.RowPitch;
	const u64	rowsize = d3dReadBackTex->D3D12GetFootprint().m_RowSizeInBytes;

	for (u32 r = 0; r < rowcount; ++r)
	{
		Mem::Copy(Mem::AdvanceRawPointer(rawBuffer->Data<void>(), r * rowsize), Mem::AdvanceRawPointer(mappedData, r * rowpitch), rowsize);
	}

	d3dResource->Unmap(0, null);
	return rawBuffer;
}

//----------------------------------------------------------------------------

bool	CAAED3D12Context::FillRenderBuffer(PRefCountedMemoryBuffer dstBuffer, RHI::PFrameBuffer srcBuffer, RHI::EPixelFormat format, u32 width, u32 height, u32 rowLength)
{
	(void)rowLength;

	PK_SCOPEDPROFILE();

	{
		PK_NAMEDSCOPEDPROFILE("m_Fence->Wait");
		m_Fence->Wait(m_FrameCount);
	}
	RHI::PReadBackTexture	rbTexture = m_Context->m_SwapChainsRTs[0]->m_ReadbackTextures[m_Context->m_SwapChainsRTs[0]->m_BufferIndex];
	if (rbTexture == null)
	{
		CLog::Log(PK_ERROR, "D3D12Context: No readback texture in swap chain");
		return false;
	}
	

	{
		PK_NAMEDSCOPEDPROFILE("Copy Task Creation And Kick");

		PRefCountedMemoryBuffer			buffer = CreateBufferFromReadBackTexture(rbTexture);

		if (buffer == null)
		{
			CLog::Log(PK_ERROR, "D3D12Context: Create buffer from readback texture failed");
			return false;
		}

		BYTE			*sptr = buffer->Data< BYTE>();
		BYTE			*dptr = (BYTE*)dstBuffer->Data<BYTE*>();

		const u32		formatSize = RHI::PixelFormatHelpers::PixelFormatToPixelByteSize(format);
		const u32		widthSize = (formatSize * width);
		u32				taskRowNumbers = height / m_WorkerCount;
		u32				reminder = height % m_WorkerCount;
		TAtomic<u32>	counter = 0;
		Threads::CEvent	event;

		for (u32 i = 0; i < m_WorkerCount; ++i)
		{
			m_Tasks[i] = PK_NEW(CAsynchronousJob_CopyTextureTask);
			if (m_Tasks[i] == null)
			{

				CLog::Log(PK_ERROR, "D3D12Context: Create task copy texture failed");
				return false;
			}
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
			m_Tasks[i]->m_RowPitch = formatSize * width;
		}

		for (u32 i = 1; i < m_WorkerCount; ++i)
		{
			m_Tasks[i]->AddToPool(Scheduler::ThreadPool());
		}

		Scheduler::ThreadPool()->KickTasks(true);

		m_Tasks[0]->ImmediateExecute();

		{
			PK_NAMEDSCOPEDPROFILE("WaitMergeTask");

			event.Wait();

			for (u32 i = 0; i < m_WorkerCount; ++i)
			{
				if (!m_Tasks[i]->Done())
					i = 0;
			}
		}
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CAAED3D12Context::FillCompositingTexture(void *srcBuffer, RHI::EPixelFormat format, u32 width, u32 height, u32 rowLength)
{
	(void)rowLength;

	PK_SCOPEDPROFILE();

	CImageMap image(CUint3(width, height, 1), srcBuffer, RHI::PixelFormatHelpers::PixelFormatToPixelByteSize(format) * width * height);

	PK_TODO("Optim: Use GPU Swizzle instead of CPU");

	RHI::PTexture textureSrc = m_D3D12Manager->CreateTexture(RHI::SRHIResourceInfos("CompositingTexture"), TMemoryView<CImageMap>(image), format);

	m_CompositingTexture = textureSrc;
	return true;
}

//----------------------------------------------------------------------------

TMemoryView<const RHI::PRenderTarget>	CAAED3D12Context::GetCurrentSwapChain()
{
	if (PK_VERIFY(!m_D3D12Context->m_SwapChains.Empty()))
		return m_D3D12Context->m_SwapChains[0]->GetRenderTargets();

	return TMemoryView<const RHI::PRenderTarget>();
}

//----------------------------------------------------------------------------

void		CAAED3D12Context::ClearContextSwapchainsRT()
{
	for (u32 i = 0; i < m_Context->m_SwapChainsRTs.Count(); ++i)
	{
		PK_DELETE(m_Context->m_SwapChainsRTs[i]);
	}
	m_Context->m_SwapChainsRTs.Clear();
}

//----------------------------------------------------------------------------

__AEGP_PK_END

#endif
