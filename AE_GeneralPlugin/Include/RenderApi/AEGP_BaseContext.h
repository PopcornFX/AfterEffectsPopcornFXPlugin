//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef	__FX_BASECONTEXT_H__
#define	__FX_BASECONTEXT_H__

#include "AEGP_Define.h"
#include <PK-SampleLib/RHIRenderParticleSceneHelpers.h>
#include "AEGP_RenderContext.h"

namespace AAePk {
	struct SAAEIOData;
}

//----------------------------------------------------------------------------

__AEGP_PK_BEGIN

PK_FORWARD_DECLARE(Texture);

//----------------------------------------------------------------------------

class CAAEBaseContext
{
public:
	static CAAEBaseContext		*GetInstance() { return null; }

	CAAEBaseContext();
	virtual			~CAAEBaseContext();

	virtual bool	BeginFrame()	{ return false; };
	virtual bool	EndFrame()		{ return m_ApiManager->EndFrame(); };

	virtual void	LogApiError()	{ return; };

	virtual bool	InitIFN()			{ return true; };

	virtual bool	CreatePlatformContext(void *winHandle, void *deviceContext) { (void)winHandle; (void)deviceContext; return false; };
	virtual bool	CreateRenderTarget(RHI::EPixelFormat format, CUint3 size) { (void)format; (void)size; return false; };

	virtual TMemoryView<const RHI::PRenderTarget>	GetCurrentSwapChain() { return TMemoryView<const RHI::PRenderTarget>(); };

	virtual bool	SetAsCurrent(void *deviceContext) { (void)deviceContext; return false; };

	virtual bool	FillRenderBuffer(PRefCountedMemoryBuffer dstBuffer, RHI::PFrameBuffer srcBuffer, RHI::EPixelFormat format, u32 width, u32 height, u32 rowLength) { (void)dstBuffer; (void)srcBuffer; (void)format; (void)width; (void)height; (void)rowLength;  return false; };

	virtual bool	FillCompositingTexture(void* srcBuffer, RHI::EPixelFormat format, u32 width, u32 height, u32 rowLength) { (void)srcBuffer; (void)format; (void)width; (void)height; (void)rowLength; return false; };

	RHI::PApiManager			GetApiManager();
	RHI::SApiContext			*GetApiContext();
	RHI::PTexture				GetCompositingTexture();

protected:
	bool						m_Initialized = false;
	RHI::PApiManager			m_ApiManager;
	RHI::SApiContext			*m_ApiContext;

	RHI::PTexture				m_CompositingTexture;
};

//----------------------------------------------------------------------------

__AEGP_PK_END


#endif // !
