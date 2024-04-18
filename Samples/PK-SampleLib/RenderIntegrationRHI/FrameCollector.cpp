//----------------------------------------------------------------------------
// This program is the property of Persistant Studios SARL.
//
// You may not redistribute it and/or modify it under any conditions
// without written permission from Persistant Studios SARL, unless
// otherwise stated in the latest Persistant Studios Code License.
//
// See the Persistant Studios Code License for further details.
//----------------------------------------------------------------------------

#include "precompiled.h"

#include "FrameCollector.h"

__PK_SAMPLE_API_BEGIN

//----------------------------------------------------------------------------

CFrameCollector::CFrameCollector()
{
}

//----------------------------------------------------------------------------

CFrameCollector::~CFrameCollector()
{
}

//----------------------------------------------------------------------------

bool	CFrameCollector::EarlyCull(const CAABB &bbox, const PCRendererDataBillboard &renderer) const
{
	return _Cull(bbox, renderer.Get());
}

//----------------------------------------------------------------------------

bool	CFrameCollector::EarlyCull(const CAABB &bbox, const PCRendererDataRibbon &renderer) const
{
	return _Cull(bbox, renderer.Get());
}

//----------------------------------------------------------------------------

bool	CFrameCollector::EarlyCull(const CAABB &bbox, const PCRendererDataMesh &renderer) const
{
	return _Cull(bbox, renderer.Get());
}

//----------------------------------------------------------------------------

bool	CFrameCollector::EarlyCull(const CAABB &bbox, const PCRendererDataTriangle &renderer) const
{
	return _Cull(bbox, renderer.Get());
}

//----------------------------------------------------------------------------

bool	CFrameCollector::EarlyCull(const CAABB &bbox, const PCRendererDataDecal &renderer) const
{
	return _Cull(bbox, renderer.Get());
}

//----------------------------------------------------------------------------

bool	CFrameCollector::EarlyCull(const CAABB &bbox, const PCRendererDataLight &renderer) const
{
	return _Cull(bbox, renderer.Get());
}

//----------------------------------------------------------------------------

bool	CFrameCollector::EarlyCull(const CAABB &bbox, const PCRendererDataSound &renderer) const
{
	return _Cull(bbox, renderer.Get());
}

//----------------------------------------------------------------------------

bool	CFrameCollector::_Cull(const CAABB &bbox, const CRendererDataBase *renderer) const
{
	const CRendererCacheInstance_UpdateThread *rdrCache = static_cast<const CRendererCacheInstance_UpdateThread *>(renderer->m_RendererCache.Get());

	// We do not cull the draw-call when shadow casting is active:
	// We want the shadow to be rendered even when the draw-call is not visible
	if (rdrCache->CastShadows())
		return false;

	// Can happen if bounds are not active
	if (!bbox.IsFinite() ||
		!bbox.Valid())
		return false;

	if (m_CullingFrustums.Empty())
		return false;

	const u32	viewCount = m_CullingFrustums.Count();
	for (u32 iView = 0; iView < viewCount; ++iView)
	{
		if (m_CullingFrustums[iView].Touches(bbox))
			return false;
	}
	return true;
}

//----------------------------------------------------------------------------

__PK_SAMPLE_API_END
