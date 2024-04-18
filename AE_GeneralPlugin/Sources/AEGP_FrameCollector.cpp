//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"

#include "AEGP_FrameCollector.h"

__AEGP_PK_BEGIN

//----------------------------------------------------------------------------

CFrameCollector::CFrameCollector()
{
}

//----------------------------------------------------------------------------

CFrameCollector::~CFrameCollector()
{
}

//----------------------------------------------------------------------------

bool	CFrameCollector::EarlyCull(const CAABB &bbox) const
{
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

__AEGP_PK_END
