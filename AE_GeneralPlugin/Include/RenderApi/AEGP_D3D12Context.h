//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef	__FX_D3D12CONTEXT_H__
#define	__FX_D3D12CONTEXT_H__

#if	(PK_BUILD_WITH_D3D12_SUPPORT != 0)

#include "AEGP_Define.h"

#include "RenderApi/AEGP_BaseContext.h"

#include <pk_rhi/include/D3D12/D3D12BasicContext.h>

//----------------------------------------------------------------------------

namespace AAePk {
	struct SAAEIOData;
}

PK_FORWARD_DECLARE(CD3D12ApiManager);

__AEGP_PK_BEGIN

PK_FORWARD_DECLARE(AsynchronousJob_CopyTextureTask);
struct	SD3D12PlatformContext;

//----------------------------------------------------------------------------

class CAAED3D12Context : public CAAEBaseContext
{

	static bool					m_Once;
	static CAAED3D12Context		*m_Instance;
public:
	static CAAEBaseContext		*GetInstance()
	{
		if (m_Instance == null)
			m_Instance = PK_NEW(CAAED3D12Context);
		return m_Instance;
	}

					CAAED3D12Context();
	virtual			~CAAED3D12Context();

	CAAED3D12Context(CAAED3D12Context &) = delete;
	CAAED3D12Context &operator=(CAAED3D12Context &) = delete;

	virtual bool	BeginFrame()			override;
	virtual bool	EndFrame()				override;
	virtual void	LogApiError()			override;


	virtual bool	InitIFN()				override;
	virtual bool	CreatePlatformContext(void *winHandle, void *deviceContext)	override;
	virtual bool	CreateRenderTarget(RHI::EPixelFormat format, CUint3 size)	override;


	virtual bool	SetAsCurrent(void *deviceContext) override;

	virtual bool	FillRenderBuffer(PRefCountedMemoryBuffer dstBuffer, RHI::PFrameBuffer srcBuffer, RHI::EPixelFormat format, u32 width, u32 height, u32 rowLength) override;

	virtual bool	FillCompositingTexture(void* srcBuffer, RHI::EPixelFormat format, u32 width, u32 height, u32 rowLength) override;

	bool			CreateCommandQueue();


	virtual TMemoryView<const RHI::PRenderTarget>		GetCurrentSwapChain()	override;

	RHI::SD3D12BasicContext		*m_D3D12Context;

	//Don't iterate over the different RT yet;
	static const u32			kFrameCount = 1;
private:
	bool					LoadDynamicLibrary();
	bool					CreateDevice();
	bool					PickHardwareAdapter();
	bool					CreateDescriptorAllocator();
	PRefCountedMemoryBuffer CreateBufferFromReadBackTexture(RHI::PCReadBackTexture readBackTexture) const;
	void					ClearContextSwapchainsRT();

	RHI::CD3D12ApiManager		*m_D3D12Manager;
	SD3D12PlatformContext		*m_Context;
	RHI::PD3D12Fence			m_Fence;
	u64							m_FrameCount = 0;


	ID3D12Resource					*m_Resources[CAAED3D12Context::kFrameCount];
	RHI::PD3D12RenderTarget			m_SwapChainRenderTarget;


	TArray<PAsynchronousJob_CopyTextureTask>	m_Tasks;
	u32											m_WorkerCount = 0;

};

//----------------------------------------------------------------------------

__AEGP_PK_END

#endif

#endif // __FX_D3D12CONTEXT_H__!
