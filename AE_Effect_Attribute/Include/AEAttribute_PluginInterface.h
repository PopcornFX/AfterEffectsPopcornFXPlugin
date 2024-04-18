//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef	__FX_CPluginInterface_H__
#define	__FX_CPluginInterface_H__

#include "PopcornFX_Define.h"
#include "PopcornFX_BasePluginInterface.h"

#include <AE_Effect.h>
#include <AE_EffectCB.h>
#include <AE_Macros.h>
#include <Param_Utils.h>
#include <AE_EffectCBSuites.h>
#include <AE_GeneralPlug.h>
#include <AEFX_ChannelDepthTpl.h>
#include <AEGP_SuiteHandler.h>

#include <PopcornFX_Suite.h>

#include <unordered_map>
#include <thread>

__AAEPK_BEGIN

struct SAAEIOData;
struct SAttributeDesc;
struct SAttributeSequenceDataFlat;

//----------------------------------------------------------------------------

class	CPluginInterface : public CBasePluginInterface
{
	struct	SAttributeData
	{
		//Memory Owned by effect
		SAttributeDesc	*m_DescAttribute;

		bool			m_UIVisibility[__Attribute_Parameters_Count];
		bool			m_IsDefault;

		SAttributeData()
			: m_DescAttribute(nullptr)
			, m_IsDefault(true)
		{
		}
		~SAttributeData()
		{
			m_DescAttribute = nullptr;
		}
	};
public:
								~CPluginInterface();
	static CPluginInterface		&Instance();

	PF_Err						About(SAAEIOData &AAEData, PF_ParamDef *params[], PF_LayerDef *output);
	PF_Err						GlobalSetup(SAAEIOData &AAEData, PF_ParamDef *params[], PF_LayerDef *output);
	PF_Err						ParamsSetup(SAAEIOData &AAEData, PF_ParamDef *params[], PF_LayerDef *output);
	PF_Err						GlobalSetdown(SAAEIOData &AAEData, PF_ParamDef *params[], PF_LayerDef *output);

	PF_Err						SequenceSetup(SAAEIOData &AAEData, PF_ParamDef *params[], PF_LayerDef *output);
	PF_Err						SequenceReSetup(SAAEIOData &AAEData, PF_ParamDef *params[], PF_LayerDef *output);
	PF_Err						SequenceFlatten(SAAEIOData &AAEData, PF_ParamDef *params[], PF_LayerDef *output);
	PF_Err						SequenceShutdown(SAAEIOData &AAEData, PF_ParamDef *params[], PF_LayerDef *output);

	PF_Err						PreRender(SAAEIOData &AAEData);
	PF_Err						SmartRender(SAAEIOData &AAEData);
	PF_Err						UpdateParams(SAAEIOData &AAEData, PF_ParamDef *params[]);
	PF_Err						UpdateParamsUI(SAAEIOData &AAEData, PF_ParamDef *params[]);

	PF_Err						SetDefaultValueIFN(SAAEIOData &AAEData, PF_ParamDef *params[], SAttributeData *AttrData);

	PF_Err						HandleDataFromAEGP(SAAEIOData &AAEData, PF_ParamDef *params[]);

	void						UpdateBoolAttribute(SAAEIOData &AAEData, SAttributeDesc *descriptor, bool *uiVisibility);
	void						UpdateIntAttribute(SAAEIOData &AAEData, SAttributeDesc *descriptor, bool *uiVisibility);
	void						UpdateFloatAttribute(SAAEIOData &AAEData, SAttributeDesc *descriptor, bool *uiVisibility);
	
private:
								CPluginInterface();
	static CPluginInterface		*m_Instance;
	static uint32_t				m_AttrUID;

	bool						_GetAttributeSequenceUID(SAAEIOData &AAEData, std::string &out);
	
	PF_Err						_RegisterAttributeInstancePlugin(SAAEIOData &AAEData, PF_ParamDef *params[], SAttributeSequenceDataFlat *sequenceData, bool setup);

	std::unordered_map<std::string, SAttributeData*>		m_AttributeData;

	std::thread::id											m_MainThreadID;
};

//----------------------------------------------------------------------------

__AAEPK_END

#endif
