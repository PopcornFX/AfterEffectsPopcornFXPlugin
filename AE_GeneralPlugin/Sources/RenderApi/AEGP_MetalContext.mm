#include "ae_precompiled.h"
#include "RenderApi/AEGP_MetalContext.h"

#if	defined(PK_MACOSX)
#if	(PK_BUILD_WITH_METAL_SUPPORT != 0)

#include "AEGP_World.h"

#include <pk_rhi/include/metal/MetalApiManager.h>
#include <pk_rhi/include/metal/MetalBasicContext.h>
#include <pk_rhi/include/metal/MetalTexture.h>
#include <pk_rhi/include/metal/MetalRenderTarget.h>

#include <pk_rhi/include/metal/MetalPopcornEnumConversion.h>

#include <PK-SampleLib/ApiContext/Metal/MetalContext.h>

#include <pk_kernel/include/kr_thread_pool_default.h>

__AEGP_PK_BEGIN

CAAEMetalContext	*CAAEMetalContext::m_Instance = null;

struct SMetalPlatformContext
{
	RHI::SMetalBasicContext					*m_MetalContext;
	RHI::CMetalApiManager					*m_MetalManager;
	RHI::PMetalRenderTarget 				m_MetalFinalRT;
	PKSample::CMetalContext::SFinalBlit		m_FinalBlit;

	SMetalPlatformContext()
	{
	}

	~SMetalPlatformContext()
	{
		m_MetalContext = null;
		m_MetalManager = null;
	}
};

CAAEMetalContext::CAAEMetalContext()
:	m_Data(null)
{
	m_ApiManager = PK_NEW(RHI::CMetalApiManager);
	m_ApiContext = PK_NEW(RHI::SMetalBasicContext);
	m_Data = PK_NEW(SMetalPlatformContext);
	m_Data->m_MetalContext = static_cast<RHI::SMetalBasicContext*>(m_ApiContext);
	m_Data->m_MetalManager = static_cast<RHI::CMetalApiManager*>(m_ApiManager.Get());
}

CAAEMetalContext::~CAAEMetalContext()
{
	PK_SAFE_DELETE(m_Data);
	PK_SAFE_DELETE(m_ApiContext);
	m_ApiManager = null;
}

bool	CAAEMetalContext::BeginFrame()
{
	m_ApiManager->BeginFrame(0);
	LogApiError();
	return true;
}

bool	CAAEMetalContext::EndFrame()
{
	void	*syncRenderData = m_ApiManager->EndFrame();
	if (syncRenderData == null)
		return false;
	m_LastFrameSyncInfo = reinterpret_cast<RHI::SWaitAllSwapChains*>(syncRenderData);
	PKSample::CMetalContext::EndFrame(	m_Data->m_MetalContext->m_Queue,
										m_Data->m_MetalContext->m_SwapChains,
										m_Data->m_FinalBlit,
										m_LastFrameSyncInfo,
										true,
										m_Data->m_MetalFinalRT);
		return false;
	return true;
}

void	CAAEMetalContext::LogApiError()
{
}

bool	CAAEMetalContext::InitIFN()
{
	if (m_Initialized)
		return true;
	m_Initialized = true;
	m_ApiContext->m_Api = RHI::GApi_Metal;
	id<MTLDevice>			device = MTLCreateSystemDefaultDevice();
	if (!PK_VERIFY(device != null))
		return false;
	id<MTLCommandQueue>		queue = [device newCommandQueue];
	if (!PK_VERIFY(queue != null))
		return false;
	m_Data->m_MetalContext->m_Device = device;
	m_Data->m_MetalContext->m_Queue = queue;
	if (!m_Data->m_MetalManager->InitApi(m_Data->m_MetalContext))
		return false;
	if (!PKSample::CMetalContext::CreateFinalBlitData(device, m_Data->m_FinalBlit, RHI::FormatFloat32RGBA))
		return false;
	return true;
}

bool	CAAEMetalContext::CreateRenderTarget(RHI::EPixelFormat format, CUint3 size)
{
	RHI::PMetalRenderTarget		mtlRt = PK_NEW(RHI::CMetalRenderTarget(RHI::SRHIResourceInfos("Render Target"), null));

	if (!PK_VERIFY(mtlRt != null))
		return false;
	if (!PK_VERIFY(mtlRt->MetalCreateRenderTarget(size.xy(), format, true, RHI::SampleCount1, m_Data->m_MetalContext->m_Device)))
		return false;

	m_Data->m_MetalFinalRT = PKSample::CMetalContext::MetalCreateRenderTarget(m_Data->m_MetalContext->m_Device, size.xy(), RHI::FormatFloat32RGBA, true);

	if (m_Data->m_MetalContext->m_SwapChainCount != 0)
	{
		m_Data->m_MetalContext->m_SwapChains.Clear();
		m_Data->m_MetalManager->SwapChainRemoved(0);
	}
	m_Data->m_MetalContext->m_SwapChains.PushBack().Valid();
	m_Data->m_MetalContext->m_SwapChains.Last().m_RenderTarget = mtlRt;
	m_Data->m_MetalContext->m_SwapChainCount = 1;

	if (!m_Data->m_MetalManager->SwapChainAdded())
		return false;
	return true;
}

bool	CAAEMetalContext::FillRenderBuffer(PRefCountedMemoryBuffer dstBuffer, RHI::PFrameBuffer srcBuffer, RHI::EPixelFormat format, u32 width, u32 height, u32 rowLength)
{
	(void)rowLength;
	(void)srcBuffer;
	if (!PK_VERIFY(m_LastFrameSyncInfo != null))
		return false;

	const u32		formatSize = RHI::PixelFormatHelpers::PixelFormatToPixelByteSize(format);
	const u32		widthSize = (formatSize * width);
	const u32		bytesPerImage = RHI::PixelFormatHelpers::GetTextureBufferSize(format, CUint2(width, height));
	u8 				*dstBuffBytes = (u8*)dstBuffer->Data<u8*>();
	id<MTLTexture>	mtlTexture = m_Data->m_MetalFinalRT->MetalGetTexture();

	PK_ASSERT(dstBuffer->DataSizeInBytes() >= bytesPerImage);

	// Synchronize the command buffer:
	{
		PK_SCOPEDLOCK(m_LastFrameSyncInfo->m_SwapChainDoneCountLock);
		if (m_LastFrameSyncInfo->m_SwapChainDoneCount < m_LastFrameSyncInfo->m_SwapChainToWait)
			m_LastFrameSyncInfo->m_CondVar.Wait(m_LastFrameSyncInfo->m_SwapChainDoneCountLock);
		m_LastFrameSyncInfo->m_SwapChainDoneCount = 0;
		m_LastFrameSyncInfo->m_SwapChainToWait = 0;
	}

	[mtlTexture getBytes:dstBuffBytes bytesPerRow:widthSize bytesPerImage:bytesPerImage fromRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0 slice:0];
	return true;
}

bool	CAAEMetalContext::FillCompositingTexture(void *srcBuffer, RHI::EPixelFormat format, u32 width, u32 height, u32 rowLength)
{
	(void)rowLength;
	PK_SCOPEDPROFILE();

	CImageMap image(CUint3(width, height, 1), srcBuffer, RHI::PixelFormatHelpers::PixelFormatToPixelByteSize(format) * width * height);

	RHI::PTexture textureSrc = m_Data->m_MetalManager->CreateTexture(RHI::SRHIResourceInfos("Texture"), TMemoryView<CImageMap>(image), format);

	m_CompositingTexture = textureSrc;
	return true;
}

TMemoryView<const RHI::PRenderTarget>	CAAEMetalContext::GetCurrentSwapChain()
{
	return TMemoryView<RHI::PRenderTarget>(m_Data->m_MetalContext->m_SwapChains.Last().m_RenderTarget);
}

bool	CAAEMetalContext::CreatePlatformContext(void *window, void *deviceContext)
{
	(void)window; (void)deviceContext;
	return true;
}

__AEGP_PK_END


#endif //PK_BUILD_WITH_METAL_SUPPORT != 0
#endif //PK_MACOSX