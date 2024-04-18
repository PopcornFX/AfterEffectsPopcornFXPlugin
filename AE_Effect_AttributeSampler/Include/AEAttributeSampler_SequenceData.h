//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef	__AEATTRIBUTESAMPLER_SEQUENCEDATA_H__
#define __AEATTRIBUTESAMPLER_SEQUENCEDATA_H__

#include <ae_precompiled.h>
#include <A.h>
#include <string>

__AAEPK_BEGIN

//----------------------------------------------------------------------------

namespace	SequenceCST
{
	static const size_t	MAX_PATH_LEN = 4096;
	static const size_t	MAX_NAME_LEN = 100;
	static const size_t	UUID_LEN = 64;
}

//----------------------------------------------------------------------------

struct	SAttributeSamplerSequenceDataFlat
{
	bool		m_IsFlat = true;

	char		m_AttributeUUID[SequenceCST::UUID_LEN];
	size_t		m_AttributeUUIDLen;
	
	char		m_AttributeName[SequenceCST::MAX_NAME_LEN];
	size_t		m_AttributeNameLen;

	char		m_ResourcePath[SequenceCST::MAX_PATH_LEN];
	size_t		m_ResourcePathLen;

	A_long		m_LayerID;

	bool	SetUUID(const char *uuid);
	bool	SetName(const char *name);
	bool	SetResourcePath(const char * path);
	bool	SetLayerID(A_long id);
};

//----------------------------------------------------------------------------

__AAEPK_END

#endif // !__AAEFFECT_SEQUENCEDATA_H__

