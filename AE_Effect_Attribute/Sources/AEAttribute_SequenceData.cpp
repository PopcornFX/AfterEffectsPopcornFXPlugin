//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"

#include "AEAttribute_SequenceData.h"

#include <cstring>
#include <algorithm>

__AAEPK_BEGIN

//----------------------------------------------------------------------------

void SAttributeSequenceDataFlat::CopyFrom(SAttributeSequenceDataFlat *src)
{
	m_IsFlat = true;
	SetUUID(src->m_AttributeUUID);
	SetName(src->m_AttributeName);
	SetIsDefaultValue(src->m_IsDefault);
}

//----------------------------------------------------------------------------

void	SAttributeSequenceDataFlat::SetIsDefaultValue(bool value)
{
	m_IsDefault = value;
}

//----------------------------------------------------------------------------

bool	SAttributeSequenceDataFlat::SetUUID(const char *uuid)
{
	m_AttributeUUIDLen = strlen(uuid) + 1;

	strncpy(m_AttributeUUID, uuid, SequenceCST::UUID_LEN);
	return true;
}

//----------------------------------------------------------------------------

bool	SAttributeSequenceDataFlat::SetName(const char *name)
{
	m_AttributeNameLen = strlen(name) + 1;

	strncpy(m_AttributeName, name, SequenceCST::MAX_NAME_LEN);
	return true;
}

//----------------------------------------------------------------------------

bool	SAttributeSequenceDataFlat::SetLayerID(bool value)
{
	m_LayerID = value;
	return true;
}

//----------------------------------------------------------------------------
__AAEPK_END
