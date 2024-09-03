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

#include "pk_render_helpers/include/render_features/rh_features_basic.h" // BasicRendererProperties
#include "pk_kernel/include/kr_string_unicode.h"
#include "pk_kernel/include/kr_sort.h"

#if (PK_BUILD_WITH_FMODEX_SUPPORT != 0)
#include <../SDK/Samples/External/fmodex-4.44.19/inc/fmod.hpp>
#endif

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------
//
//	Sound Resource:
//
//----------------------------------------------------------------------------

bool	CSoundResource::Load(FMOD::System *soundSystem)
{
	Release();

	if (soundSystem == null)
		return false;

	if (m_Path.Empty())
		return true;

#if (PK_BUILD_WITH_FMODEX_SUPPORT != 0)
	// Fix #4138: Audio files with filenames containing special unicode characters do not load & playback properly
	// With this version of fmod, we must explicitly pass it unicode strings.
	// Apparently from what I gathered from the online docs fmod studio now has removed all that,
	// and all file-related functions accept UTF-8 strings.
	const CStringUnicode	soundPathUTF16 = CStringUnicode::FromUTF8(m_Path.Data());
	FMOD_RESULT				res = soundSystem->createSound((const char*)soundPathUTF16.Data(), FMOD_LOOP_OFF | FMOD_3D | FMOD_UNICODE, 0, &m_SoundData);
	if (res != FMOD_OK)
	{
		CLog::Log(PK_ERROR, "FMOD: Failed to load sound \"%s\" (%d)", m_Path.Data(), res);
	}
	else
	{
		// get the sound length
		u32		soundDurationMs = 0;
		res = m_SoundData->getLength(&soundDurationMs, FMOD_TIMEUNIT_MS);
		if (res != FMOD_OK)
			CLog::Log(PK_ERROR, "FMOD: Failed to get sound length \"%s\" (%d)", m_Path.Data(), res);
		else
			m_Length = soundDurationMs * 0.001f;

		m_Frequency = 0.f; // cannot get the frequency from here
	}
	return (m_SoundData != null);
#else
	return false;
#endif
}

//----------------------------------------------------------------------------

void	CSoundResource::Release()
{
#if (PK_BUILD_WITH_FMODEX_SUPPORT != 0)
	if (m_SoundData != null)
	{
		m_SoundData->release();
		m_SoundData = null;
	}
#endif
}

//----------------------------------------------------------------------------

CString	CSoundResource::ResolveSoundPath(const PRendererDataBase &renderer)
{
	const SRendererFeaturePropertyValue	*soundProperty = renderer->m_Declaration.FindProperty(BasicRendererProperties::SID_Sound_SoundData());
	if (soundProperty != null)
		return soundProperty->ValuePath();
	return CString();
}

//----------------------------------------------------------------------------

PSoundResource	CSoundResourceManager::CreateOrFindSoundResource(FMOD::System *soundSystem, const PRendererDataBase &renderer, CStringView rootPath)
{
	CString	soundPath = CSoundResource::ResolveSoundPath(renderer);
	if (soundPath.Empty())	// no path: not an issue, just an empty resource
		return null;
	soundPath = rootPath / soundPath;
	return CreateOrFindSoundResource(soundSystem, soundPath.View());
}

//----------------------------------------------------------------------------

PSoundResource	CSoundResourceManager::CreateOrFindSoundResource(FMOD::System *soundSystem, CStringView pathFull)
{
	PK_SCOPEDLOCK(m_SoundResourcesLock);

	if (pathFull.Empty())	// no path: not an issue, just an empty resource
		return null;

	for (PSoundResource &r : m_SoundResources)
	{
		if (r->m_Path == pathFull)
			return r;
	}

	PSoundResource	sndres = PK_NEW(CSoundResource);
	if (!PK_VERIFY(sndres != null))
		return null;

	sndres->m_Path = pathFull.ToString();
	sndres->Load(soundSystem);

	return sndres;
}

//----------------------------------------------------------------------------

void	CSoundResourceManager::ReleaseAllSoundResources()
{
	PK_SCOPEDLOCK(m_SoundResourcesLock);
	m_SoundResources.Clear();
}

//----------------------------------------------------------------------------

void	CSoundResourceManager::BroadcastResourceChanged(CStringView pathVirtual)
{
	PK_SCOPEDLOCK(m_SoundResourcesLock);
	for (const PSoundResource &res : m_SoundResources)
	{
		if (res != null && res->m_Path.Contains(pathVirtual))
			res->m_NeedReload = true;
	}
}

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
	for (SSoundElement &soundElement : m_SoundPool)
	{
		if (soundElement.m_Resource->m_NeedReload)
		{
			if (soundElement.m_Handle != null)
			{
				soundElement.m_Handle->stop();
				soundElement.m_Handle = null;
			}
		}
		if (soundElement.m_Handle != null)
		{
			bool	isUsable = false;
			soundElement.m_Handle->isPlaying(&isUsable);
			if (!isUsable)
			{
				soundElement.m_Handle->stop();
				soundElement.m_Handle = null;
			}
		}
		if (soundElement.m_Handle != null && soundElement.m_Used)
		{
			u32	posMs = 0;
			soundElement.m_Handle->getPosition(&posMs, FMOD_TIMEUNIT_MS);
			const float playTime = posMs * 0.001f;
			if (soundElement.m_PlayTime - 0.010f <= playTime && playTime <= soundElement.m_PlayTime + 0.300f)
				soundElement.m_PlayTime = playTime;
			//else
			//	CLog::Log(PK_WARN, "FMod returned inconsistency time-position of sound slot %s (fmod=%f pk=%f)", soundElement.m_Resource->m_Path.Data(), playTime, soundElement.m_PlayTime);
		}
		else
		{
			soundElement.m_PlayTime = -1.f;
			soundElement.m_PerceivedVolume = 0.f;
		}
		soundElement.m_Used = false;
		soundElement.m_NeedResync = false;
	}
#endif
	// Reload the resource when needed
	for (SSoundElement &soundElement : m_SoundPool)
	{
		if (soundElement.m_Resource->m_NeedReload)
		{
			soundElement.m_Resource->Load(m_FmodSystem);
			soundElement.m_Resource->m_NeedReload = false;
		}
	}
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
	for (SSoundElement &soundElement : m_SoundPool)
	{
		if (!soundElement.m_Used)
		{
			if (soundElement.m_Handle != null)
				soundElement.m_Handle->setPaused(true);
			continue;
		}

		PK_ASSERT(soundElement.m_Handle != null || soundElement.m_NeedResync);
		PK_ASSERT(soundElement.m_Resource->m_SoundData != null); // if null, the sound-element should not be requested for playing.

		if (soundElement.m_NeedResync)
		{
			if (soundElement.m_Handle == null)
			{
				FMOD_RESULT		res = m_FmodSystem->playSound(FMOD_CHANNEL_FREE, soundElement.m_Resource->m_SoundData, true, &soundElement.m_Handle);
				if (res != FMOD_OK)
					CLog::Log(PK_ERROR, "FMOD cannot play the sound %s (%d)", soundElement.m_Resource->m_Path.Data(), res);
				else if (soundElement.m_Resource->m_Frequency == 0.f)
					soundElement.m_Handle->getFrequency(&soundElement.m_Resource->m_Frequency);
				PK_ASSERT(m_FmodChannelGroup != null);
				res = soundElement.m_Handle->setChannelGroup(m_FmodChannelGroup);
				if (res != FMOD_OK)
					CLog::Log(PK_ERROR, "FMOD failed to assign a channel group for sound %s (%d)", soundElement.m_Resource->m_Path.Data(), res);
			}
			if (soundElement.m_Handle == null)
				continue;
			soundElement.m_Handle->setPosition((u32)(soundElement.m_PlayTime * 1000), FMOD_TIMEUNIT_MS); // m_PlayTime is in seconds.
		}

		const FMOD_VECTOR		pos = { soundElement.m_Position.x(), soundElement.m_Position.y(), soundElement.m_Position.z() };
		const FMOD_VECTOR		vel = { soundElement.m_Velocity.x(), soundElement.m_Velocity.y(), soundElement.m_Velocity.z() };
		const bool				isPaused = (simulationSpeed == 0.f);

		FMOD_RESULT		res = FMOD_OK;

		if (!isPaused)
		{
			res = soundElement.m_Handle->set3DAttributes(&pos, &vel);
			if (res != FMOD_OK)
				CLog::Log(PK_ERROR, "FMOD Error set3DAttributes (%d)", res);
			res = soundElement.m_Handle->setVolume(soundElement.m_PerceivedVolume);
			if (res != FMOD_OK)
				CLog::Log(PK_ERROR, "FMOD Error setVolume (%d)", res);
			res = soundElement.m_Handle->set3DDopplerLevel(soundElement.m_DopplerLevel);
			if (res != FMOD_OK)
				CLog::Log(PK_ERROR, "FMOD Error set3DDopplerLevel (%d)", res);
			res = soundElement.m_Handle->setFrequency(soundElement.m_PlaySpeed * simulationSpeed * soundElement.m_Resource->m_Frequency);
			if (res != FMOD_OK)
				CLog::Log(PK_ERROR, "FMOD Error setFrequency (%d)", res);
		}

		res = soundElement.m_Handle->setPaused(isPaused);
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
