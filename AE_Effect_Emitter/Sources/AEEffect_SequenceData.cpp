//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"

#include "AEEffect_SequenceData.h"

#include <cstring>
#include <algorithm>

__AAEPK_BEGIN

//----------------------------------------------------------------------------

bool	SEffectSequenceDataFlat::SetEffectName(const char* name)
{
	strncpy(m_EffectName, name, SequenceCST::MAX_NAME_LEN);

	m_EffectNameLen = strlen(name) + 1;
	return true;
}

//----------------------------------------------------------------------------

bool	SEffectSequenceDataFlat::SetEffectPathSource(const char *path)
{
	m_EffectPathSourceLen = strlen(path) + 1;
	strncpy(m_EffectPathSource, path, SequenceCST::MAX_PATH_LEN);
	return true;
}

//----------------------------------------------------------------------------

bool	SEffectSequenceDataFlat::SetEffectBackdropMeshPath(const char *path)
{
	m_EffectBackdropMeshPathLen = strlen(path) + 1;
	strncpy(m_EffectBackdropMeshPath, path, SequenceCST::MAX_PATH_LEN);
	return true;
}

//----------------------------------------------------------------------------

bool	SEffectSequenceDataFlat::SetEffectEnvironmentMapPath(const char *path)
{
	m_EffectEnvironmentMapPathLen = strlen(path) + 1;
	strncpy(m_EffectEnvironmentMapPath, path, SequenceCST::MAX_PATH_LEN);
	return true;
}

//----------------------------------------------------------------------------

bool	SEffectSequenceDataFlat::SetUUID(const char *uuid)
{
	strncpy(m_EffectUUID, uuid, SequenceCST::UUID_LEN);
	return true;
}

//----------------------------------------------------------------------------

bool	SEffectSequenceDataFlat::SetUUID(const std::string &uuid)
{
	strncpy(m_EffectUUID, uuid.c_str(), SequenceCST::UUID_LEN);
	return true;
}

//----------------------------------------------------------------------------

void	SEffectSequenceDataFlat::SetLayerID(A_long id)
{
	m_LayerID = id;
}

//----------------------------------------------------------------------------

bool	SEffectSequenceDataFlat::CopyFrom(SEffectSequenceDataFlat *src)
{
	SetEffectName(src->m_EffectName);
	SetEffectPathSource(src->m_EffectPathSource);
	SetEffectBackdropMeshPath(src->m_EffectBackdropMeshPath);
	SetEffectEnvironmentMapPath(src->m_EffectEnvironmentMapPath);
	SetUUID(src->m_EffectUUID);
	SetLayerID(src->m_LayerID);
	return true;
}

//----------------------------------------------------------------------------
__AAEPK_END

