//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"
#include "RenderApi/AEGP_BaseContext.h"

__AEGP_PK_BEGIN

//----------------------------------------------------------------------------

CAAEBaseContext::CAAEBaseContext()
	:	m_ApiManager(null)
	,	m_ApiContext(null)
	,	m_CompositingTexture(null)
{

}

//----------------------------------------------------------------------------

CAAEBaseContext::~CAAEBaseContext()
{
	PK_SAFE_DELETE(m_ApiContext);
	m_ApiManager = null;
	m_CompositingTexture = null;
}

//----------------------------------------------------------------------------

RHI::PApiManager	CAAEBaseContext::GetApiManager()
{
	PK_ASSERT(m_ApiManager != null);
	return m_ApiManager;
}

//----------------------------------------------------------------------------

RHI::SApiContext	*CAAEBaseContext::GetApiContext()
{
	PK_ASSERT(m_ApiContext != null);
	return m_ApiContext;
}

//----------------------------------------------------------------------------

RHI::PTexture CAAEBaseContext::GetCompositingTexture()
{
	PK_ASSERT(m_CompositingTexture != null);
	return m_CompositingTexture;
}

//----------------------------------------------------------------------------

__AEGP_PK_END
