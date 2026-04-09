//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL.
// https://popcornfx.com/popcornfx-community-license/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"

#include "AEAttributeSampler_SequenceData.h"

#include <cstring>
#include <algorithm>

__AAEPK_BEGIN

//----------------------------------------------------------------------------

bool	SAttributeSamplerSequenceDataFlat::SetUUID(const char *uuid)
{
	m_AttributeUUIDLen = std::min(strlen(uuid) + 1, SequenceCST::UUID_LEN);
	if (m_AttributeUUIDLen != 0)
	{
		strncpy(m_AttributeUUID, uuid, m_AttributeUUIDLen);
	}
	return true;
}

//----------------------------------------------------------------------------

bool	SAttributeSamplerSequenceDataFlat::SetName(const char *name)
{
	m_AttributeNameLen = std::min(strlen(name) + 1, SequenceCST::MAX_NAME_LEN);
	if (m_AttributeNameLen != 0)
	{
		strncpy(m_AttributeName, name, m_AttributeNameLen);
	}
	return true;
}

//----------------------------------------------------------------------------

bool	SAttributeSamplerSequenceDataFlat::SetResourcePath(const char *path)
{
	m_ResourcePathLen = std::min(strlen(path) + 1, SequenceCST::MAX_PATH_LEN);
	if (m_ResourcePathLen != 0)
	{
		strncpy(m_ResourcePath, path, m_ResourcePathLen);
	}
	return true;
}

//----------------------------------------------------------------------------

bool	SAttributeSamplerSequenceDataFlat::SetLayerID(A_long id)
{
	m_LayerID = id;
	return true;
}

//----------------------------------------------------------------------------

__AAEPK_END
