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

// Foward declaration of fmod
namespace FMOD
{
	class System;
	class Channel;
	class ChannelGroup;
	class Sound;
}

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

class	CSoundResource;

struct	SSoundElement
{
	CSoundResource	*m_Resource;	// audio-data to use
	FMOD::Channel	*m_Handle;		// audio-handle that controls the playback
	float			m_PlayTime;
	float			m_PerceivedVolume;
	float			m_PlaySpeed;
	float			m_DopplerLevel;
	CFloat3			m_Position;		// position relative to the listener
	CFloat3			m_Velocity;		// velocity relative to the listener
	bool			m_Used;
	bool			m_NeedResync;

	SSoundElement()
	:	m_Resource(null)
	,	m_Handle(null)
	,	m_PlayTime(0.f)
	,	m_PerceivedVolume(0.f)
	,	m_PlaySpeed(1.f)
	,	m_DopplerLevel(1.f)
	,	m_Used(false)
	,	m_NeedResync(false)
	{}

	bool operator <  (const SSoundElement &other) const { return m_PerceivedVolume >  other.m_PerceivedVolume; }
	bool operator <= (const SSoundElement &other) const { return m_PerceivedVolume >= other.m_PerceivedVolume; }
	bool operator == (const SSoundElement &other) const { return m_PerceivedVolume == other.m_PerceivedVolume; }
};

//----------------------------------------------------------------------------

class	CSoundPoolCache
{
public:
	CSoundPoolCache() : m_FmodSystem(null), m_PoolSize(0) {}
	~CSoundPoolCache() {}

	void					SetSoundSystem(FMOD::System *soundSystem, FMOD::ChannelGroup *FmodChannelGroup);

	void					PreparePool();
	void					CleanPool() { m_SoundPool.Clean(); }
	void					PlayPool(const u32 maxSoundsPlayed, const float simulationSpeed);

	s32						FindBestMatchingSoundSlot(CSoundResource *resource, float maxDt, const float curtime);
	CGuid					GetFreeSoundSlot(CSoundResource *resource);

	SSoundElement			&operator [] (u32 id) { return m_SoundPool[id]; }
	const SSoundElement		&operator [] (u32 id) const { return m_SoundPool[id]; }
	u32						Count() const { return m_SoundPool.Count(); }

private:
	FMOD::System			*m_FmodSystem;
	FMOD::ChannelGroup		*m_FmodChannelGroup;
	TArray<SSoundElement>	m_SoundPool;
	u32						m_PoolSize;
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
