#pragma once

//----------------------------------------------------------------------------
// This program is the property of Persistant Studios SARL.
//
// You may not redistribute it and/or modify it under any conditions
// without written permission from Persistant Studios SARL, unless
// otherwise stated in the latest Persistant Studios Code License.
//
// See the Persistant Studios Code License for further details.
//----------------------------------------------------------------------------

#include "PK-SampleLib/PKSample.h"
#include "PK-SampleLib/RenderIntegrationRHI/RHITypePolicy.h"

#include <pk_maths/include/pk_maths_primitives_frustum.h>
#include <pk_render_helpers/include/frame_collector/legacy/rh_frame_collector_legacy.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------
//
//	Specialization types to RHI (legacy)
//
//----------------------------------------------------------------------------

struct	SRHIRenderContextLegacy
{
	enum	EPass
	{
		EPass_PostUpdateFence,
		EPass_RenderThread
	};

	EPass					Pass() const { return m_Pass; }
	bool					IsPostUpdateFencePass() const { return m_Pass == EPass_PostUpdateFence; }
	bool					IsRenderThreadPass() const { return m_Pass == EPass_RenderThread; }
	RHI::PApiManager		ApiManager() const { return m_ApiManager; }

	SRHIRenderContextLegacy(EPass pass, RHI::PApiManager apiManager)
		:	m_Pass(pass)
		,	m_ApiManager(apiManager)
	{
	}

private:
	EPass					m_Pass;
	RHI::PApiManager		m_ApiManager;
};

//----------------------------------------------------------------------------

struct	SRHISceneView
{
	CFloat4x4	m_InvViewMatrix;		// Billboarding matrix (Right now, CPU billboarding tasks expect a RH_YUP billboarding matrix. You can flip m_InvViewMatrix.StrippedZAxis() for LH_YUP)
	bool		m_NeedsSortedIndices;	// Defaults to true, set it to false for views that don't require sorted indices (ie. shadows)
	u32			m_MaxSliceCount;		// If slicing is enabled, maximum slice count per view.

	SRHISceneView()
		:	m_InvViewMatrix(CFloat4x4::IDENTITY)
		,	m_NeedsSortedIndices(true)
		,	m_MaxSliceCount(5)
	{
	}
};

//----------------------------------------------------------------------------

class	CRHIParticleBatchTypes
{
public:
	typedef SRHIRenderContextLegacy	CRenderContext;
	typedef SRHIDrawOutputs			CFrameOutputData;
	typedef SRHISceneView			CViewUserData;

	enum { kMaxQueuedCollectedFrame = 2U };
};

//----------------------------------------------------------------------------
//
//	Frame collector (legacy)
//
//----------------------------------------------------------------------------

// CFrameCollector is specialized with our policy to work with RHI.
// Create your own policy to use it in your custom engine.
class	CFrameCollector : public TFrameCollector<PKSample::CRHIParticleBatchTypes>
{
public:
	PK_DEPRECATED("v2.20.0: PKSample::CFrameCollector is no longer maintained. Use PopcornFX::CFrameCollector instead (see v2.20 upgrade guidelines).")
	CFrameCollector() {}
	virtual ~CFrameCollector() {}

	// Views to cull against (setup from update thread)
	TMemoryView<const CFrustum>		m_CullingFrustums;

private:
	virtual bool	EarlyCull(const CAABB &bbox, const PCRendererDataBillboard &renderer) const override;
	virtual bool	EarlyCull(const CAABB &bbox, const PCRendererDataRibbon &renderer) const override;
	virtual bool	EarlyCull(const CAABB &bbox, const PCRendererDataMesh &renderer) const override;
	virtual bool	EarlyCull(const CAABB &bbox, const PCRendererDataTriangle &renderer) const override;
	virtual bool	EarlyCull(const CAABB &bbox, const PCRendererDataDecal &renderer) const override;
	virtual bool	EarlyCull(const CAABB &bbox, const PCRendererDataLight &renderer) const override;
	virtual bool	EarlyCull(const CAABB &bbox, const PCRendererDataSound &renderer) const override;

	bool			_Cull(const CAABB &bbox, const CRendererDataBase *renderer) const;
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
