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

#include "pk_render_helpers/include/frame_collector/rh_particle_render_data_factory.h"

// Forward declaration of fmod
namespace FMOD
{
	class System;
	class Channel;
	class ChannelGroup;
	class Sound;
}
typedef unsigned int	FMOD_MODE;

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------
//
// Sound resource
// (interfaced with the frame collector's rendering cache)
//
//----------------------------------------------------------------------------

class	CSoundResource : public CRendererCacheBase
{
public:
	CString			m_Path;
	FMOD::Sound		*m_SoundData;
	float			m_Frequency;
	float			m_Length;
	bool			m_NeedReload;

	CSoundResource()
	:	m_Path()
	,	m_SoundData(null)
	,	m_Frequency(0.0f)
	,	m_Length(0.0f)
	,	m_NeedReload(false)
	{}

	virtual ~CSoundResource() { Release(); }

	virtual void	UpdateThread_BuildBillboardingFlags(const PRendererDataBase &) override { }

	static	CString	ResolveSoundPath(const PRendererDataBase &renderer);

	bool		Load(FMOD::System *soundSystem);
	void		Release();
};
PK_DECLARE_REFPTRCLASS(SoundResource);

//----------------------------------------------------------------------------

// Simple resousrce-manager for the sound resources:
// It locks when loading resources (Note: FMOD can handle asynch. loading, to check)
// The runtime CResourceManager could be used, but it would require to create a new "plugin".
class	CSoundResourceManager
{
protected:
	TArray<PSoundResource>		m_SoundResources;
	Threads::CCriticalSection	m_SoundResourcesLock;

public:
	CSoundResourceManager() {}
	~CSoundResourceManager() { ReleaseAllSoundResources(); }

	PSoundResource	CreateOrFindSoundResource(FMOD::System *soundSystem, const PRendererDataBase &renderer, CStringView rootPath);
	PSoundResource	CreateOrFindSoundResource(FMOD::System *soundSystem, CStringView pathFull);

	void			ReleaseAllSoundResources();

	void			BroadcastResourceChanged(CStringView pathVirtual);
};

//----------------------------------------------------------------------------
//
// Sound player
//
//----------------------------------------------------------------------------

struct	SSoundElement
{
	PSoundResource	m_Resource;		// audio-data to use
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
	CSoundPoolCache() : m_FmodSystem(null), m_FmodChannelGroup(null), m_PoolSize(0) {}
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

// Custom render context containing few additionnal info for the sound playing
// Only used in ***::EmitDrawCall
struct	SAudioRenderContext : public PopcornFX::SRenderContext
{
	float	m_SimSpeed;
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
