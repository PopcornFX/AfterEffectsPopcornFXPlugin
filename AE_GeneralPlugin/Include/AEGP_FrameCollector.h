//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#include "PK-SampleLib/PKSample.h"

#include <pk_maths/include/pk_maths_primitives_frustum.h>
#include <pk_render_helpers/include/frame_collector/rh_frame_collector.h>

#include <PK-SampleLib/RenderIntegrationRHI/RHITypePolicy.h>

#include "AEGP_Define.h"

__AEGP_PK_BEGIN

//----------------------------------------------------------------------------
//
//	Frame collector
//
//----------------------------------------------------------------------------

// CFrameCollector is specialized with our policy to work with RHI.
// Create your own policy to use it in your custom engine.
class CFrameCollector : public TFrameCollector<PKSample::CRHIParticleBatchTypes>
{
public:
	CFrameCollector();
	virtual ~CFrameCollector();

	// Views to cull against (setup from update thread)
	TMemoryView<const CFrustum>		m_CullingFrustums;

private:
	// Early Cull: Culls an entire medium on the update thread (when collecting the frame)
	virtual bool	EarlyCull(const PopcornFX::CAABB &bbox) const override;

	// Late Cull: Cull draw requests or individual pages on the render thread (when collecting draw calls)
	// You can use this method if you don't have render thread views available from the update thread
	// Ideally, cull in EarlyCull, but you should implement both methods
	// Late cull also allows for finer culling (per draw request / per page)
	virtual bool	LateCull(const PopcornFX::CAABB &bbox) const override { return EarlyCull(bbox); }
};

//----------------------------------------------------------------------------
__AEGP_PK_END
