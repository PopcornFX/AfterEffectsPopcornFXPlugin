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

#include "OffscreenContext.h"

#define		DEFAULT_WINDOW_WIDTH	800
#define		DEFAULT_WINDOW_HEIGHT	600

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------
//
//	COffscreenContext
//
//----------------------------------------------------------------------------

COffscreenContext::COffscreenContext()
:	CAbstractWindowContext(Context_Offscreen)
,	m_WindowSize(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT)
,	m_PixelRatio(1.0f)
{
}

//----------------------------------------------------------------------------

bool	COffscreenContext::InitImgui(const RHI::PApiManager &manager)
{
	(void)manager;
	return true;
}

//----------------------------------------------------------------------------

bool	COffscreenContext::Init(RHI::EGraphicalApi api, const CString &title, bool allowHighDPI)
{
	(void)title;
	(void)allowHighDPI;
	m_UsedApi = api;

	return true;
}

//----------------------------------------------------------------------------

bool	COffscreenContext::ProcessEvents()
{
	return Super::ProcessEvents();
}

//----------------------------------------------------------------------------

__PK_SAMPLE_API_END
