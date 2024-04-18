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
#include "SoundPoolCache.h"

#include "../Runtime/pk_kernel/include/kr_sort.h"

#include "PK-SampleLib/RenderIntegrationRHI/RendererCache.h"

#if (PK_BUILD_WITH_FMODEX_SUPPORT != 0)
#include <../SDK/Samples/External/fmodex-4.44.19/inc/fmod.hpp>
#endif

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------
//
//	Sound Pool:
//
//----------------------------------------------------------------------------

void	CSoundPoolCache::SetSoundSystem(FMOD::System *soundSystem, FMOD::ChannelGroup *FmodChannelGroup)
{
	m_FmodSystem = soundSystem;
	m_FmodChannelGroup = FmodChannelGroup;
}

//----------------------------------------------------------------------------

void	CSoundPoolCache::PreparePool()
{
#if (PK_BUILD_WITH_FMODEX_SUPPORT != 0)
	// Reset the sound-pool and update the time cursor from the sound system
	for (u32 spoolIdx = 0; spoolIdx < m_SoundPool.Count(); ++spoolIdx)
	{
		if (m_SoundPool[spoolIdx].m_Handle != null)
		{
			bool	isUsable = false;
			m_SoundPool[spoolIdx].m_Handle->isPlaying(&isUsable);
			if (!isUsable)
			{
				m_SoundPool[spoolIdx].m_Handle->stop();
				m_SoundPool[spoolIdx].m_Handle = null;
			}
		}
		if (m_SoundPool[spoolIdx].m_Handle != null && m_SoundPool[spoolIdx].m_Used)
		{
			u32	posMs = 0;
			m_SoundPool[spoolIdx].m_Handle->getPosition(&posMs, FMOD_TIMEUNIT_MS);
			const float playTime = posMs * 0.001f;
			if (m_SoundPool[spoolIdx].m_PlayTime - 0.010f <= playTime && playTime <= m_SoundPool[spoolIdx].m_PlayTime + 0.300f)
				m_SoundPool[spoolIdx].m_PlayTime = playTime;
			//else
			//	CLog::Log(PK_WARN, "FMod returned inconsistency time-position of sound slot %d (fmod=%f pk=%f)", spoolIdx, playTime, m_SoundPool[spoolIdx].m_PlayTime);
		}
		else
		{
			m_SoundPool[spoolIdx].m_PlayTime = -1.f;
			m_SoundPool[spoolIdx].m_PerceivedVolume = 0.f;
		}
		m_SoundPool[spoolIdx].m_Used = false;
		m_SoundPool[spoolIdx].m_NeedResync = false;
	}
#endif
}

//----------------------------------------------------------------------------

void	CSoundPoolCache::PlayPool(const u32 maxSoundsPlayed, const float simulationSpeed)
{
	(void)simulationSpeed;
	m_PoolSize = maxSoundsPlayed;
#if (PK_BUILD_WITH_FMODEX_SUPPORT != 0)
	if (!PK_VERIFY(m_FmodSystem != null)) // We shouldn't be here
		return;
	// Retrench the pool
	if (m_SoundPool.Count() > m_PoolSize)
	{
		QuickSort(m_SoundPool);
		for (u32 spoolIdx = m_PoolSize; spoolIdx < m_SoundPool.Count(); ++spoolIdx)
		{
			if (m_SoundPool[spoolIdx].m_Handle != null)
			{
				m_SoundPool[spoolIdx].m_Handle->stop();
				m_SoundPool[spoolIdx].m_Handle = null;
			}
		}
		m_SoundPool.Resize(m_PoolSize);
	}

	// Update the sound-pool on the sound-system
	for (u32 spoolIdx = 0; spoolIdx < m_SoundPool.Count(); ++spoolIdx)
	{
		if (!m_SoundPool[spoolIdx].m_Used)
		{
			if (m_SoundPool[spoolIdx].m_Handle != null)
				m_SoundPool[spoolIdx].m_Handle->setPaused(true);
			continue;
		}

		PK_ASSERT(m_SoundPool[spoolIdx].m_Handle != null || m_SoundPool[spoolIdx].m_NeedResync);

		if (m_SoundPool[spoolIdx].m_NeedResync)
		{
			if (m_SoundPool[spoolIdx].m_Handle == null)
			{
				FMOD_RESULT		res = m_FmodSystem->playSound(FMOD_CHANNEL_FREE, m_SoundPool[spoolIdx].m_Resource->m_SoundData, true, &m_SoundPool[spoolIdx].m_Handle);
				if (res != FMOD_OK)
					CLog::Log(PK_ERROR, "FMOD cannot play the sound %d (%d)", spoolIdx, res);
				else if (m_SoundPool[spoolIdx].m_Resource->m_Frequency == 0.f)
					m_SoundPool[spoolIdx].m_Handle->getFrequency(&m_SoundPool[spoolIdx].m_Resource->m_Frequency);
				PK_ASSERT(m_FmodChannelGroup != null);
				res = m_SoundPool[spoolIdx].m_Handle->setChannelGroup(m_FmodChannelGroup);
				if (res != FMOD_OK)
					CLog::Log(PK_ERROR, "FMOD failed to assign a channel group for sound %d (%d)", spoolIdx, res);
			}
			if (m_SoundPool[spoolIdx].m_Handle == null)
				continue;
			m_SoundPool[spoolIdx].m_Handle->setPosition((u32)(m_SoundPool[spoolIdx].m_PlayTime * 1000), FMOD_TIMEUNIT_MS); // m_PlayTime is in seconds.
		}

		const FMOD_VECTOR		pos = { m_SoundPool[spoolIdx].m_Position.x(), m_SoundPool[spoolIdx].m_Position.y(), m_SoundPool[spoolIdx].m_Position.z() };
		const FMOD_VECTOR		vel = { m_SoundPool[spoolIdx].m_Velocity.x(), m_SoundPool[spoolIdx].m_Velocity.y(), m_SoundPool[spoolIdx].m_Velocity.z() };
		const bool				isPaused = (simulationSpeed == 0.f);

		FMOD_RESULT		res = FMOD_OK;

		if (!isPaused)
		{
			res = m_SoundPool[spoolIdx].m_Handle->set3DAttributes(&pos, &vel);
			if (res != FMOD_OK)
				CLog::Log(PK_ERROR, "FMOD Error set3DAttributes (%d)", res);
			res = m_SoundPool[spoolIdx].m_Handle->setVolume(m_SoundPool[spoolIdx].m_PerceivedVolume);
			if (res != FMOD_OK)
				CLog::Log(PK_ERROR, "FMOD Error setVolume (%d)", res);
			res = m_SoundPool[spoolIdx].m_Handle->set3DDopplerLevel(m_SoundPool[spoolIdx].m_DopplerLevel);
			if (res != FMOD_OK)
				CLog::Log(PK_ERROR, "FMOD Error set3DDopplerLevel (%d)", res);
			res = m_SoundPool[spoolIdx].m_Handle->setFrequency(m_SoundPool[spoolIdx].m_PlaySpeed * simulationSpeed * m_SoundPool[spoolIdx].m_Resource->m_Frequency);
			if (res != FMOD_OK)
				CLog::Log(PK_ERROR, "FMOD Error setFrequency (%d)", res);
		}

		res = m_SoundPool[spoolIdx].m_Handle->setPaused(isPaused);
		if (res != FMOD_OK)
			CLog::Log(PK_ERROR, "FMOD Error setPaused (%d)", res);

	}
#endif
}

//----------------------------------------------------------------------------

s32	CSoundPoolCache::FindBestMatchingSoundSlot(CSoundResource *resource, float maxDt, const float curtime)
{
	s32	sPoolMatchIdx = -1;
	for (u32 spoolIdx = 0; spoolIdx < m_SoundPool.Count(); ++spoolIdx)
	{
		const SSoundElement	&element = m_SoundPool[spoolIdx];
		if (element.m_Used ||
			element.m_Resource != resource ||
			element.m_Handle == null ||
			element.m_PlayTime < 0.f)
			continue;

		const float		deltaTimeAbs = PKAbs(element.m_PlayTime - curtime);
		if (deltaTimeAbs < maxDt)
		{
			sPoolMatchIdx = spoolIdx;
			maxDt = deltaTimeAbs;
		}
	}
	return sPoolMatchIdx;
}

//----------------------------------------------------------------------------

CGuid	CSoundPoolCache::GetFreeSoundSlot(CSoundResource *resource)
{
	// just find one not used (do not override sounds from other EmitDrawCall requests)
	for (u32 spoolIdx = 0; spoolIdx < m_SoundPool.Count(); ++spoolIdx)
	{
		SSoundElement	&element = m_SoundPool[spoolIdx];
		if (element.m_PlayTime < 0 && (element.m_Resource == resource || element.m_Resource == null))
		{
			return spoolIdx;
		}
	}

	return m_SoundPool.PushBack();
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
