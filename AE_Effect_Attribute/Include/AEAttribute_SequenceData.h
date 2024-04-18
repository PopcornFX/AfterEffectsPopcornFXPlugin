//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef	__AEATTRIBUTE_SEQUENCEDATA_H__
#define __AEATTRIBUTE_SEQUENCEDATA_H__

#include <ae_precompiled.h>
#include <A.h>
#include <string>

__AAEPK_BEGIN

//----------------------------------------------------------------------------

namespace	SequenceCST
{
	static const size_t	MAX_PATH_LEN = 1024;
	static const size_t	MAX_NAME_LEN = 100;
	static const size_t	UUID_LEN = 64;
};

//----------------------------------------------------------------------------

struct	SAttributeSequenceDataFlat
{
	bool		m_IsFlat = true;

	char		m_AttributeUUID[SequenceCST::UUID_LEN];
	size_t		m_AttributeUUIDLen;

	char		m_AttributeName[SequenceCST::MAX_NAME_LEN];
	size_t		m_AttributeNameLen;

	bool		m_IsDefault = true;

	A_long		m_LayerID;

	void	CopyFrom(SAttributeSequenceDataFlat* src);

	void	SetIsDefaultValue(bool value);
	bool	SetUUID(const char *uuid);
	bool	SetName(const char *name);
	bool	SetLayerID(bool value);
};

//----------------------------------------------------------------------------

__AAEPK_END

#endif // !__AAEFFECT_SEQUENCEDATA_H__
