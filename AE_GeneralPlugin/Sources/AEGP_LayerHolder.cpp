//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include <ae_precompiled.h>
#include "AEGP_LayerHolder.h"

#include "AEGP_Scene.h"

__AEGP_PK_BEGIN

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------

HBO_CLASS_DEFINITION_BEGIN(CGraphicOverride)
	.HBO_FIELD_DEFINITION(RendererID)
	.HBO_FIELD_DEFINITION(PropertyID)
	.HBO_FIELD_DEFINITION(Value)
HBO_CLASS_DEFINITION_END

CGraphicOverride::CGraphicOverride()
	: HBO_CONSTRUCT(CGraphicOverride)

{
}

//----------------------------------------------------------------------------

CGraphicOverride::~CGraphicOverride()
{
}

//----------------------------------------------------------------------------

bool	CGraphicOverride::operator==(const CGraphicOverride &other)
{
	if (other.m_RendererID == m_RendererID &&
		other.m_PropertyID == m_PropertyID)
		return true;
	return false;
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------

HBO_CLASS_DEFINITION_BEGIN(CLayerProperty)
	.HBO_FIELD_DEFINITION(CompName)
	.HBO_FIELD_DEFINITION(ID)
	.HBO_FIELD_DEFINITION(RendererProperties)
HBO_CLASS_DEFINITION_END

CLayerProperty::CLayerProperty()
	: HBO_CONSTRUCT(CLayerProperty)
{
}

//----------------------------------------------------------------------------

CLayerProperty::~CLayerProperty()
{
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------

SLayerHolder::SLayerHolder()
{

}

//----------------------------------------------------------------------------

SLayerHolder::~SLayerHolder()
{


}
//----------------------------------------------------------------------------

bool	SLayerHolder::Clear(SPBasicSuite* suite)
{
	AEGP_SuiteHandler	suites(suite);

	bool result = true;
	if (m_Scene->Quit() == false)
		result = false;
	m_Scene = null;

	for (s32 i = m_SPendingAttributes.Count() - 1; i >= 0; --i)
	{
		if (m_SPendingAttributes[i].m_AttributeEffectRef != null)
			suites.EffectSuite4()->AEGP_DisposeEffect(m_SPendingAttributes[i].m_AttributeEffectRef);
		m_SPendingAttributes[i].m_AttributeEffectRef = null;
		m_SPendingAttributes[i].m_Desc->m_IsDeleted = true;
		m_SPendingAttributes[i].m_Desc = null;
	}
	m_SPendingAttributes.Clear();

	for (auto &it : m_DeletedAttributes)
	{
		if (it.m_AttributeEffectRef != null)
			suites.EffectSuite4()->AEGP_DisposeEffect(it.m_AttributeEffectRef);
		it.m_AttributeEffectRef = null;
		it.m_Desc->m_IsDeleted = true;
		it.m_Desc = null;
	}
	m_DeletedAttributes.Clear();

	for (auto &it : m_SpawnedAttributes)
	{
		if (it.m_AttributeEffectRef != null)
			suites.EffectSuite4()->AEGP_DisposeEffect(it.m_AttributeEffectRef);
		it.m_AttributeEffectRef = null;
		it.m_Desc->m_IsDeleted = true;
		it.m_Desc = null;
	}
	m_SpawnedAttributes.Clear();

	for (auto &it : m_SpawnedAttributesSampler)
	{
		if (it.m_AttributeEffectRef != null)
			suites.EffectSuite4()->AEGP_DisposeEffect(it.m_AttributeEffectRef);
		it.m_AttributeEffectRef = null;
		it.m_Desc->m_IsDeleted = true;
		it.m_Desc = null;
	}
	m_SpawnedAttributes.Clear();

	for (auto &it : m_SPendingEmitters)
	{
		if (it.m_EffectHandle)
			it.m_EffectHandle = null;
		it.m_Desc->m_IsDeleted = true;
		it.m_Desc = null;
	}
	m_SPendingEmitters.Clear();

	if (m_SpawnedEmitter.m_EffectHandle)
		m_SpawnedEmitter.m_EffectHandle = null;
	m_SpawnedEmitter.m_Desc->m_IsDeleted = true;
	m_SpawnedEmitter.m_Desc = null;

	m_EffectLayer = null;
	m_CameraLayer = null;

	PK_SAFE_DELETE(m_BackdropAudioWaveform);
	PK_SAFE_DELETE(m_BackdropAudioSpectrum);
	return result;
}

//----------------------------------------------------------------------------

__AEGP_PK_END
