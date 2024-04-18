//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef __AEGP_METALCONTEXT_H__
#define __AEGP_METALCONTEXT_H__

#if defined (PK_MACOSX)


#include "AEGP_Define.h"
#include "RenderApi/AEGP_BaseContext.h"

#include "pk_render_helpers/include/draw_requests/rh_tasks.h" // Task::CBase
#include "pk_render_helpers/include/draw_requests/rh_job_pools.h"

//----------------------------------------------------------------------------

#if	(PK_BUILD_WITH_METAL_SUPPORT != 0)

namespace AAePk {
	struct SAAEIOData;
}

__PK_RHI_API_BEGIN
	struct	SMetalBasicContext;
	struct 	SWaitAllSwapChains;
	class	CMetalApiManager;
__PK_RHI_API_END

PK_FORWARD_DECLARE(CMetalApiManager);

__AEGP_PK_BEGIN
	struct SMetalPlatformContext;
__AEGP_PK_END


__AEGP_PK_BEGIN

//----------------------------------------------------------------------------

class CAAEMetalContext : public CAAEBaseContext
{
public:
	static CAAEMetalContext 	*GetInstance()
	{
		if (m_Instance == null)
			m_Instance = PK_NEW(CAAEMetalContext);
		return m_Instance;
	}

	CAAEMetalContext();
	virtual			~CAAEMetalContext();

	virtual bool	BeginFrame()			override;
	virtual bool	EndFrame()				override;
	virtual void	LogApiError()			override;


	virtual bool	InitIFN()				override;

	virtual bool	SetAsCurrent(void *deviceContext) { (void)deviceContext; return true; };

	virtual bool	CreateRenderTarget(RHI::EPixelFormat format, CUint3 size) override;

	virtual bool	FillRenderBuffer(PRefCountedMemoryBuffer dstBuffer, RHI::PFrameBuffer srcBuffer, RHI::EPixelFormat format, u32 width, u32 height, u32 rowLength) override;

	virtual bool	FillCompositingTexture(void *srcBuffer, RHI::EPixelFormat format, u32 width, u32 height, u32 rowLength) override;

	virtual TMemoryView<const RHI::PRenderTarget>		GetCurrentSwapChain() override;

	virtual bool	CreatePlatformContext(void *winHandle, void *deviceContext) override;

private:
	SMetalPlatformContext					*m_Data;
	bool									m_Parented;
	static CAAEMetalContext 				*m_Instance;
	RHI::SWaitAllSwapChains 				*m_LastFrameSyncInfo = null;
};

__AEGP_PK_END

//----------------------------------------------------------------------------

#endif //PK_BUILD_WITH_METAL_SUPPORT != 0

#endif //PK_MACOSX

#endif //__AEGP_METALCONTEXT_H__
