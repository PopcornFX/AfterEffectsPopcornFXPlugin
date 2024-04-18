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
#include <pk_render_helpers/include/frame_collector/rh_frame_collector.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------
//
//	Frame collector
//
//----------------------------------------------------------------------------

// CFrameCollector is specialized with our policy to work with RHI.
// Create your own policy to use it in your custom engine.
class	CFrameCollector : public TFrameCollector<PKSample::CRHIParticleBatchTypes>
{
public:
	CFrameCollector();
	virtual ~CFrameCollector();

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
