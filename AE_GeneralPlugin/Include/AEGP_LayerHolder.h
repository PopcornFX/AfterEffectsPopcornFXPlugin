//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once
#include "AEGP_Define.h"

#include "AEGP_Attribute.h"

#include <AEConfig.h>

#include <entry.h>
#include <AE_GeneralPlug.h>
#include <A.h>
#include <SPSuites.h>
#include <AE_Macros.h>

#include <popcornfx.h>

#include <pk_base_object/include/hbo_object.h>
#include <pk_kernel/include/kr_refptr.h>

#include <pk_kernel/include/kr_string_id.h>
#include <pk_kernel/include/kr_containers_hash.h>
#include <pk_kernel/include/kr_file.h>

namespace AAePk
{
	struct	SAAEIOData;
	struct	SAttributeDesc;
	struct	SAttributeSamplerDesc;
	struct	SEmitterDesc;
};

__AEGP_PK_BEGIN

PK_FORWARD_DECLARE(AAEScene);
PK_FORWARD_DECLARE(RendererProperties);

//----------------------------------------------------------------------------

class HBO_CLASS(CGraphicOverride), public CBaseObject
{
public:
	HBO_FIELD(u32,		RendererID);
	HBO_FIELD(u32,		PropertyID);

	HBO_FIELD(CString,	Value);
public:
	CGraphicOverride();
	~CGraphicOverride();

	bool	operator==(const CGraphicOverride &other);

	HBO_CLASS_DECLARATION();
};
PK_DECLARE_REFPTRCLASS(GraphicOverride);

//----------------------------------------------------------------------------

class HBO_CLASS(CLayerProperty), public CBaseObject
{
public:
	HBO_FIELD(CString, 						CompName);
	HBO_FIELD(u32,							ID);
	HBO_FIELD(TArray<CGraphicOverride*>,	RendererProperties);

public:
	CLayerProperty();
	~CLayerProperty();

	HBO_CLASS_DECLARATION();
};
PK_DECLARE_REFPTRCLASS(LayerProperty);

//----------------------------------------------------------------------------

struct SPendingEmitter
{
	PF_ProgPtr	m_EffectHandle;

	//Data Owned by AEEffect. Do not free in AEGP
	SEmitterDesc	*m_Desc;

	SPendingEmitter(PF_ProgPtr ptr = null, SEmitterDesc *desc = null)
		: m_EffectHandle(ptr)
		, m_Desc(desc)
	{

	}

	SPendingEmitter(const SPendingEmitter& other)
	{
		m_EffectHandle = other.m_EffectHandle;
		m_Desc = other.m_Desc;
	}

	~SPendingEmitter()
	{
		m_EffectHandle = null;
		m_Desc = null;
	}
};

//----------------------------------------------------------------------------

struct SLayerHolder
{
	u32												ID = 0;

	u32												m_TimeScale = 0;
	u32												m_TimeStep = 0;
	u32												m_CurrentTime = 0;
	CFloat4x4										m_ViewMatrix = CFloat4x4::IDENTITY;
	CFloat4											m_CameraPos = CFloat4::ZERO;

	AEGP_LayerH										m_EffectLayer = null;
	AEGP_LayerH										m_CameraLayer = null;
	PAAEScene										m_Scene = null;

	TArray<SPendingEmitter>							m_SPendingEmitters;
	SPendingEmitter									m_SpawnedEmitter;

	TArray<SPendingAttribute>						m_SPendingAttributes;
	THashMap<SPendingAttribute, CStringId>			m_DeletedAttributes;
	THashMap<SPendingAttribute, CStringId>			m_SpawnedAttributes;

	THashMap<SPendingAttribute, CStringId>			m_DeletedAttributesSampler;
	THashMap<SPendingAttribute, CStringId>			m_SpawnedAttributesSampler;

	SSamplerAudio									*m_BackdropAudioWaveform = null;
	SSamplerAudio									*m_BackdropAudioSpectrum = null;

	float											m_ScaleFactor = 1;
	bool											m_ForceRender = false;
	bool											m_Deleted = false;

	Threads::CCriticalSection						m_LayerLock;

	CString											m_SourcePackPath;
	PFilePack										m_BakedPack = null;

	CString											m_LayerName;
	CString											m_CompositionName;
	PLayerProperty									m_LayerProperty = null;

	SLayerHolder();
	~SLayerHolder();

	bool	Clear(SPBasicSuite* suite);
};

__AEGP_PK_END
