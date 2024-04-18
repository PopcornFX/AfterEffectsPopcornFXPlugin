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

#include "AWindowContext.h"

#include "ProfilerRenderer.h"	// PKSAMPLE_HAS_PROFILER_RENDERER

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

bool	CAbstractWindowContext::ProcessEvents()
{
	if (m_Controller == null)
		return true;
	if (m_Controller->IsButtonPressed(CAbstractController::ButtonStart, true))
		return false; // We quit when start is pressed

#if	PKSAMPLE_HAS_PROFILER_RENDERER
	if (m_Profiler != null)
	{
		if (m_Controller->IsButtonPressed(CAbstractController::ButtonGPadUp, true))
			m_Profiler->Enable(!m_Profiler->Enabled());
		if (m_Profiler->Enabled())
		{
			if (m_Controller->IsButtonPressed(CAbstractController::ButtonGPadLeft, true))
				m_Profiler->SetPaused(!m_Profiler->IsPaused());
		}
	}
#endif
	return true;
}

//----------------------------------------------------------------------------

void	CAbstractWindowContext::RegisterProfiler(CProfilerRenderer *profiler)
{
	(void)profiler;
#if	PKSAMPLE_HAS_PROFILER_RENDERER
	m_Profiler = profiler;
#endif
}

//----------------------------------------------------------------------------

const float	CAbstractController::kDeadZone = 0.01f;

//----------------------------------------------------------------------------

float		CAbstractController::FilterDeadZone(float value)
{
	return PKAbs(value) > kDeadZone ? value : 0.f;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
