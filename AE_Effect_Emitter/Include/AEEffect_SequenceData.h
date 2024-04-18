//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef	__AAEFFECT_SEQUENCEDATA_H__
#define __AAEFFECT_SEQUENCEDATA_H__

#include <ae_precompiled.h>
#include <A.h>
#include <string>

__AAEPK_BEGIN

//----------------------------------------------------------------------------

namespace	SequenceCST
{
	static const size_t	MAX_PATH_LEN = 1024;
	static const size_t	MAX_NAME_LEN = 100;
	static const size_t	UUID_LEN = 32;
}

//----------------------------------------------------------------------------

struct	SEffectSequenceDataFlat
{
	//Set is useless, as we do not create directly the struct. Just a friendly reminder.
	bool		m_IsFlat = true;

	char		m_EffectUUID[SequenceCST::UUID_LEN];

	char		m_EffectName[SequenceCST::MAX_NAME_LEN];
	size_t		m_EffectNameLen;

	char		m_EffectPath[SequenceCST::MAX_PATH_LEN];
	size_t		m_EffectPathLen;

	char		m_EffectPathSource[SequenceCST::MAX_PATH_LEN];
	size_t		m_EffectPathSourceLen;

	char		m_EffectBackdropMeshPath[SequenceCST::MAX_PATH_LEN];
	size_t		m_EffectBackdropMeshPathLen;

	char		m_EffectEnvironmentMapPath[SequenceCST::MAX_PATH_LEN];
	size_t		m_EffectEnvironmentMapPathLen;

	A_long		m_LayerID;

	bool		SetEffectName(const char *name);
	bool		SetEffectPathSource(const char *name);
	bool		SetEffectBackdropMeshPath(const char *name);
	bool		SetEffectEnvironmentMapPath(const char *name);
	bool		SetUUID(const std::string &uuid);
	bool		SetUUID(const char *uuid);
	void		SetLayerID(A_long id);
	bool		CopyFrom(SEffectSequenceDataFlat *src);
};

//----------------------------------------------------------------------------

__AAEPK_END

#endif // !__AAEFFECT_SEQUENCEDATA_H__

