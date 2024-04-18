#pragma once

#ifndef	__FX_D3D11CONTEXT_H__
#define	__FX_D3D11CONTEXT_H__

#if	(PK_BUILD_WITH_D3D11_SUPPORT != 0)

#include "AEGP_Define.h"

#include "RenderApi/AEGP_BaseContext.h"

#include <pk_rhi/include/D3D11/D3D11BasicContext.h>

namespace AAePk {
	struct SAAEIOData;
}

PK_FORWARD_DECLARE(CD3D11ApiManager);

__AEGP_PK_BEGIN

PK_FORWARD_DECLARE(AsynchronousJob_CopyTextureTask);

struct	SD3D11PlatformContext;

class CAAED3D11Context : public CAAEBaseContext
{
public:
	static CAAEBaseContext		*GetInstance()
	{
		return PK_NEW(CAAED3D11Context);
	}

	CAAED3D11Context();
	virtual ~CAAED3D11Context();

	virtual bool	BeginFrame()			override;
	virtual bool	EndFrame()				override;
	virtual void	LogApiError()			override;


	virtual bool	InitIFN()				override;
	virtual bool	CreatePlatformContext(void *winHandle, void *deviceContext)	override;
	virtual bool	CreateRenderTarget(RHI::EPixelFormat format, CUint3 size)	override;

	virtual bool	SetAsCurrent(void *deviceContext) override;

	virtual bool	FillRenderBuffer(PRefCountedMemoryBuffer dstBuffer, RHI::PFrameBuffer srcBuffer, RHI::EPixelFormat format, u32 width, u32 height, u32 rowLength)	override;

	virtual bool	FillCompositingTexture(void* srcBuffer, RHI::EPixelFormat format, u32 width, u32 height, u32 rowLength) override;
	
	virtual TMemoryView<const RHI::PRenderTarget>		GetCurrentSwapChain()	override;

	RHI::SD3D11BasicContext		*m_D3D11Context = null;
private:
	bool	_LoadDynamicLibrary();
	bool	_CreateDevice();
	bool	_PickHardwareAdapter();

	RHI::CD3D11ApiManager		*m_D3D11Manager = null;
	SD3D11PlatformContext		*m_Context = null;

	ID3D11Texture2D					*m_Texture = null;
	ID3D11Texture2D					*m_StagingTexture = null;
	RHI::PD3D11RenderTarget			m_SwapChainRenderTarget;


	TArray<PAsynchronousJob_CopyTextureTask>	m_Tasks;
	u32											m_WorkerCount;

};

__AEGP_PK_END

#endif

#endif // __FX_D3D11CONTEXT_H__!
