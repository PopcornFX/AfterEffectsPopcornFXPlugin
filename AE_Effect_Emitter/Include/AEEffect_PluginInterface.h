//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef	__FX_CPluginInterface_H__
#define	__FX_CPluginInterface_H__


#include "PopcornFX_Define.h"
#include "PopcornFX_Suite.h"
#include "PopcornFX_BasePluginInterface.h"

#include <AE_Effect.h>
#include <AE_EffectCB.h>
#include <AE_Macros.h>
#include <Param_Utils.h>
#include <AE_EffectCBSuites.h>
#include <AE_GeneralPlug.h>
#include <AEFX_ChannelDepthTpl.h>
#include <AEGP_SuiteHandler.h>

#include <unordered_map>
#include <vector>

__AAEPK_BEGIN

//----------------------------------------------------------------------------

struct	SEmitterDesc;
struct	SEffectSequenceDataFlat;
struct	SAAEIOData;

//----------------------------------------------------------------------------

class CPluginInterface : public CBasePluginInterface
{
	struct SEffectData
	{
		//Memory Owned by effect
		SEmitterDesc	*m_Desc;
		int				m_LastRenderTime = -1;

		std::mutex		m_Lock;

		SEffectData()
			: m_Desc(nullptr)
		{

		}
	};
public:
	virtual ~CPluginInterface();
	static CPluginInterface		&Instance();

	PF_Err	About(SAAEIOData &AAEData, PF_ParamDef *params[], PF_LayerDef *output);
	PF_Err	GlobalSetup(SAAEIOData &AAEData, PF_ParamDef *params[], PF_LayerDef *output);
	PF_Err	ParamsSetup(SAAEIOData &AAEData, PF_ParamDef *params[], PF_LayerDef *output);
	PF_Err	GlobalSetdown(SAAEIOData &AAEData, PF_ParamDef *params[], PF_LayerDef *output);

	PF_Err	SequenceSetup(SAAEIOData &AAEData, PF_ParamDef *params[], PF_LayerDef *output);
	PF_Err	SequenceReSetup(SAAEIOData &AAEData, PF_ParamDef *params[], PF_LayerDef *output);
	PF_Err	SequenceFlatten(SAAEIOData &AAEData, PF_ParamDef *params[], PF_LayerDef *output);
	PF_Err	SequenceShutdown(SAAEIOData &AAEData, PF_ParamDef *params[], PF_LayerDef *output);

	PF_Err	PreRender(SAAEIOData &AAEData);
	PF_Err	SmartRender(SAAEIOData &AAEData);

	PF_Err	ParamValueChanged(SAAEIOData &AAEData, PF_ParamDef *params[]);
	PF_Err	UpdateParamsUI(SAAEIOData &AAEData, PF_ParamDef *params[]);
	PF_Err	HandleDataFromAEGP(SAAEIOData &AAEData, PF_ParamDef *params[]);
	PF_Err	QueryDynamicFlags(SAAEIOData &AAEData, PF_ParamDef *params[]);

	bool						GetEffectSequenceUID(SAAEIOData &AAEData, std::string &out);
	

private:
								CPluginInterface();
	static CPluginInterface		*m_Instance;
	static uint32_t				m_AttrUID;

	//void						_MakeParamCopy(PF_ParamDef *actual[], PF_ParamDef copy[]);
	bool						_RegisterEffectInstancePlugin(SAAEIOData &AAEData, PF_ParamDef *params[], SEffectSequenceDataFlat *sequenceData);
	bool						_UnRegisterEffectInstancePlugin(SAAEIOData &AAEData, PF_ParamDef *params[], SEffectSequenceDataFlat *sequenceData);

	//PF_Err						_UpdateEmitterName(SAAEIOData &AAEData, SEmitterDesc* desc);
	//PF_Err						_UpdateBackdropMeshPath(SAAEIOData &AAEData, SEmitterDesc* desc);

	std::unordered_map<std::string, SEffectData*>			m_EffectData;
	std::vector<SEffectData*>								m_QueuedEffectData;

};

//----------------------------------------------------------------------------

__AAEPK_END

#endif
