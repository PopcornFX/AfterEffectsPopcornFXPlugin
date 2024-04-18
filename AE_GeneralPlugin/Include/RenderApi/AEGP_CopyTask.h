//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef	__FX_AEGP_COPYTASK_H__
#define	__FX_AEGP_COPYTASK_H__

#include "AEGP_Define.h"

#include "RenderApi/AEGP_BaseContext.h"

#include "pk_render_helpers/include/draw_requests/rh_tasks.h" // Task::CBase
#include "pk_render_helpers/include/draw_requests/rh_job_pools.h"

__AEGP_PK_BEGIN

//----------------------------------------------------------------------------

class CAsynchronousJob_CopyTextureTask : public CAsynchronousJob
{
public:
	TAtomic<u32>	*m_Counter;
	Threads::CEvent	*m_EndCB;

	u32				m_TargetCount;

	//				data
	u32				m_Height;
	u32				m_StartOffset;

	BYTE			*m_DestinationPtr;
	BYTE			*m_SourcePtr;

	u32				m_WidthSize;
	u32				m_RowPitch;

public:
	CAsynchronousJob_CopyTextureTask() { }
	~CAsynchronousJob_CopyTextureTask() { }

protected:
	virtual void		_VirtualLaunch(Threads::SThreadContext &) override { ImmediateExecute(); }

public:
	bool		Setup() { return true; }
	void		ImmediateExecute()
	{
		PK_NAMEDSCOPEDPROFILE("Copy Task");

		//Loop
		for (u32 i = 0; i < m_Height; ++i)
		{
			memcpy(m_DestinationPtr + (m_StartOffset + i) * m_WidthSize, m_SourcePtr + (m_StartOffset + i) * m_RowPitch, m_WidthSize);
		}

		u32 value = m_Counter->Inc();

		if (value == m_TargetCount)
			m_EndCB->Trigger();
	}
};
PK_DECLARE_REFPTRCLASS(AsynchronousJob_CopyTextureTask);

//----------------------------------------------------------------------------

__AEGP_PK_END


#endif	__FX_AEGP_COPYTASK_H__
