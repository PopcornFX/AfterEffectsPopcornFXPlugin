//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef	__FX_RENDERCONTEXT_H__
#define	__FX_RENDERCONTEXT_H__

#include "AEGP_Define.h"

#include <AE_Effect.h>
#include <AEGP_SuiteHandler.h>
#include <AE_EffectPixelFormat.h>

#include <pk_rhi/include/FwdInterfaces.h>

#include <pk_kernel/include/kr_refptr.h>
#include <PK-SampleLib/RHIRenderParticleSceneHelpers.h>

//----------------------------------------------------------------------------

namespace AAePk
{
	struct SAAEIOData;
}

__AEGP_PK_BEGIN

class CAAEBaseContext;
PK_FORWARD_DECLARE(AAERenderContext);

//----------------------------------------------------------------------------

class CAAERenderContext : public CRefCountedObject
{
public:
	CAAERenderContext();
	~CAAERenderContext();

	bool	InitializeIFN(RHI::EGraphicalApi api, const CString &className);
	bool	InitGraphicContext(RHI::EPixelFormat rhiformat, u32 width, u32 height);
	bool	Destroy();

	CAAEBaseContext								*GetAEGraphicContext();

	PKSample::CRHIParticleSceneRenderHelper		*GetCurrentSceneRenderer();
	CUint2										GetViewportSize() const;
	void										*GetWindowHandle() { return m_WindowHandle; }

	void										LogGraphicsErrors();

	void										SetShaderLoader(PKSample::CShaderLoader *sl);
	PKSample::CShaderLoader						*GetShaderLoader();
	void										SetPostFXOptions(PKSample::SParticleSceneOptions &so);

	bool										SetAsCurrentContext();

	void										SetBackgroundOptions(bool isOverride, float alphaValue) { m_IsOverride = isOverride; m_AlphaValue = alphaValue; }
	void										SetGamma(float gamma) { m_Gamma = gamma; }

	bool	RenderFrameBegin(u32 width, u32 height);
	bool	RenderFrameEnd();

	bool	AERenderFrameBegin	(SAAEIOData &AAEData, bool getBackground = true);
	bool	AERenderFrameEnd	(SAAEIOData &AAEData);
	bool	RenderToSAAEWorld	(SAAEIOData &AAEData, AEGP_SuiteHandler &suiteHandler, PF_EffectWorld *inputWorld, PF_EffectWorld *effectWorld, PF_PixelFormat format);
	bool	GetCompositingBuffer(SAAEIOData &AAEData, AEGP_SuiteHandler &suiteHandler, PF_EffectWorld *inputWorld, PF_EffectWorld *effectWorld, PF_PixelFormat format);

	CUint2	GetContextSize();

private:
	bool		CreateInternalWindowIFN(const CString& className);

	void									*m_WindowHandle;
	void									*m_DeviceContext;

	u32										m_Width;
	u32										m_Height;

	RHI::EGraphicalApi						m_API;

	static Threads::CCriticalSection		m_AEGraphicContextLock;
	static CAAEBaseContext					*m_AEGraphicContext;
	
	RHI::EPixelFormat						m_Format;
	PF_PixelFormat							m_AAEFormat;

	bool									m_Initialized;

	float									m_Gamma = 1.0f;
	bool									m_IsOverride = false;
	float									m_AlphaValue = 1.0f;

	PKSample::CRHIParticleSceneRenderHelper	*m_RHIRendering;

	//Shared between rendercontexts
	PKSample::CShaderLoader					*m_ShaderLoader;

	PKSample::SParticleSceneOptions			m_SceneOptions;

	PRefCountedMemoryBuffer					m_UploadBuffer = null;
	u32										m_UploadBufferSize = 0;

	PRefCountedMemoryBuffer					m_DownloadBuffer = null;
	u32										m_DownloadBufferSize = 0;

private:
	void	ResetCheckedOutWorlds(SAAEIOData &AAEData);

	PF_EffectWorld		*m_InputWorld = null;
	PF_EffectWorld		*m_OutputWorld = null;

};
PK_DECLARE_REFPTRCLASS(AAERenderContext);

//----------------------------------------------------------------------------

__AEGP_PK_END

#endif
