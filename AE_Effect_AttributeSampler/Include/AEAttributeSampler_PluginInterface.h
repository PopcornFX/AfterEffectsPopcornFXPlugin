//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once


#ifndef	__FX_AEATTRIBUTESAMPLER_PLUGIN_INTERFACE_H__
#define	__FX_AEATTRIBUTESAMPLER_PLUGIN_INTERFACE_H__


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

//----------------------------------------------------------------------------

struct SAAEIOData;
struct SAttributeSamplerDesc;
struct SAttributeSamplerSequenceDataFlat;

//----------------------------------------------------------------------------

class	CPluginInterface : public CBasePluginInterface
{
	struct	SAttributeSamplerData
	{
		//Memory Owned by effect
		SAttributeSamplerDesc	*m_DescAttribute;

		std::string		m_ResourcePath;
		bool			m_IsDefault;

		SAttributeSamplerData()
			: m_DescAttribute(nullptr)
			, m_ResourcePath("")
			, m_IsDefault(true)
		{
		}
		~SAttributeSamplerData()
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

	PF_Err						HandleDataFromAEGP(SAAEIOData &AAEData, PF_ParamDef *params[]);

private:
								CPluginInterface();

	PF_Err						_UpdateParamsVisibility(SAAEIOData &AAEData, SAttributeSamplerData *AttrData);

	bool						_GetAttributeSequenceUID(SAAEIOData &AAEData, std::string &out);
	
	PF_Err						_RegisterAttributeInstancePlugin(SAAEIOData &AAEData, PF_ParamDef *params[], SAttributeSamplerSequenceDataFlat *sequenceData, bool setup);

	
	void						UpdateSamplerGeometry(SAAEIOData &AAEData, SAttributeSamplerDesc *descriptor);
	void						UpdateSamplerText(SAAEIOData &AAEData, SAttributeSamplerDesc *descriptor);
	void						UpdateSamplerImage(SAAEIOData &AAEData, SAttributeSamplerDesc *descriptor);
	void						UpdateSamplerVectorField(SAAEIOData &AAEData, SAttributeSamplerDesc *descriptor);

	static CPluginInterface										*m_Instance;
	static uint32_t												m_AttrUID;

	std::unordered_map<std::string, SAttributeSamplerData*>		m_AttributeData;

	std::thread::id												m_MainThreadID;
};

//----------------------------------------------------------------------------

__AAEPK_END

#endif
