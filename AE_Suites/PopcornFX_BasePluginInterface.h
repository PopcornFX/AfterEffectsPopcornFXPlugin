//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef	__FX_BASEPLUGININTERFACE_H__
#define	__FX_BASEPLUGININTERFACE_H__

#include "PopcornFX_Define.h"
#include "PopcornFX_Suite.h"

#include <AEGP_SuiteHandler.h>

__AAEPK_BEGIN

//----------------------------------------------------------------------------

const float	k_MinFloat = -100000.0f;
const float	k_MaxFloat = 100000.0f;

//----------------------------------------------------------------------------

class CBasePluginInterface
{
public:
	virtual ~CBasePluginInterface() { }

#if defined(PK_WINDOWS)
#pragma warning( push )
#pragma warning( disable : 4100 ) //Disable unused parameters
#endif

	virtual PF_Err	About(SAAEIOData &AAEData, PF_ParamDef *params[], PF_LayerDef *output) { (void)AAEData; (void)params; (void)output; return A_Err_NONE; };
	virtual PF_Err	GlobalSetup(SAAEIOData &AAEData, PF_ParamDef *params[], PF_LayerDef *output) { (void)AAEData; (void)params; (void)output; return A_Err_NONE; };
	virtual PF_Err	ParamsSetup(SAAEIOData &AAEData, PF_ParamDef *params[], PF_LayerDef *output) { (void)AAEData; (void)params; (void)output; return A_Err_NONE; };
	virtual PF_Err	GlobalSetdown(SAAEIOData &AAEData, PF_ParamDef *params[], PF_LayerDef *output) { (void)AAEData; (void)params; (void)output; return A_Err_NONE; };
	virtual PF_Err	SequenceSetup(SAAEIOData &AAEData, PF_ParamDef *params[], PF_LayerDef *output) { (void)AAEData; (void)params; (void)output; return A_Err_NONE; };
	virtual PF_Err	SequenceReSetup(SAAEIOData &AAEData, PF_ParamDef *params[], PF_LayerDef *output) { (void)AAEData; (void)params; (void)output; return A_Err_NONE; };
	virtual PF_Err	SequenceFlatten(SAAEIOData &AAEData, PF_ParamDef *params[], PF_LayerDef *output) { (void)AAEData; (void)params; (void)output; return A_Err_NONE; };
	virtual PF_Err	SequenceShutdown(SAAEIOData &AAEData, PF_ParamDef *params[], PF_LayerDef *output) { (void)AAEData; (void)params; (void)output; return A_Err_NONE; };
	virtual PF_Err	PreRender(SAAEIOData &AAEData) { (void)AAEData; return A_Err_NONE; };
	virtual PF_Err	SmartRender(SAAEIOData &AAEData) { (void)AAEData; return A_Err_NONE; };
	virtual PF_Err	ParamValueChanged(SAAEIOData &AAEData, PF_ParamDef *params[]) { (void)AAEData; (void)params; return A_Err_NONE; };
	virtual PF_Err	UpdateParamsUI(SAAEIOData &AAEData, PF_ParamDef *params[]) { (void)AAEData; (void)params; return A_Err_NONE; };
	virtual PF_Err	HandleDataFromAEGP(SAAEIOData &AAEData, PF_ParamDef *params[]) { (void)AAEData; (void)params; return A_Err_NONE; };
	virtual PF_Err	QueryDynamicFlags(SAAEIOData &AAEData, PF_ParamDef *params[]) { (void)AAEData; (void)params; return A_Err_NONE; };

#if defined(PK_WINDOWS)
#pragma warning( pop )
#endif
	const int					*GetParametersIndexes()
	{
		return m_ParametersIndexes;
	};

	PF_Err			SetEffectName(SAAEIOData &AAEData, std::string &name, AEGP_EffectRefH effect = nullptr)
	{
		AEGP_SuiteHandler	suites(AAEData.m_InData->pica_basicP);
		AEGP_EffectRefH		effectRef = effect;
		A_UTF16Char			nameUTF[64];
		AEGP_StreamRefH		streamRef = nullptr;
		AEGP_StreamRefH		effectStreamRef = nullptr;
		PF_Err				result = A_Err_NONE;

		AE_VERIFY(name.size() < 64);
		if (effectRef == nullptr)
		{
			result = suites.PFInterfaceSuite1()->AEGP_GetNewEffectForEffect(m_AAEID, AAEData.m_InData->effect_ref, &effectRef);
			if (!AE_VERIFY(result == A_Err_NONE))
				return result;
		}

		result |= suites.StreamSuite2()->AEGP_GetNewEffectStreamByIndex(m_AAEID, effectRef, 1, &streamRef);
		if (!AE_VERIFY(result == A_Err_NONE))
			return result;
		CopyCharToUTF16(name.data(), nameUTF);

		result |= suites.DynamicStreamSuite4()->AEGP_GetNewParentStreamRef(m_AAEID, streamRef, &effectStreamRef);
		result |= suites.StreamSuite2()->AEGP_DisposeStream(streamRef);
		streamRef = nullptr;
		if (!AE_VERIFY(result == A_Err_NONE))
			return result;
		result |= suites.DynamicStreamSuite4()->AEGP_SetStreamName(effectStreamRef, nameUTF);
		result |= suites.StreamSuite2()->AEGP_DisposeStream(effectStreamRef);

		effectStreamRef = nullptr;
		if (effect == nullptr)
		{
			result |= suites.EffectSuite4()->AEGP_DisposeEffect(effectRef);
		}
		return result;
	}

	PF_Err	SetParameterStreamName(SAAEIOData &AAEData, std::string &str, unsigned int index, AEGP_EffectRefH effect = nullptr)
	{
		AEGP_SuiteHandler	suites(AAEData.m_InData->pica_basicP);
		PF_Err				result = PF_Err_NONE;
		A_UTF16Char			strUTF[256];
		AEGP_StreamRefH		streamRef = nullptr;
		AEGP_EffectRefH		effectRef = effect;

		AE_VERIFY(str.size() < 256);
		if (effectRef == nullptr)
		{
			result = suites.PFInterfaceSuite1()->AEGP_GetNewEffectForEffect(m_AAEID, AAEData.m_InData->effect_ref, &effectRef);
			if (!AE_VERIFY(result == A_Err_NONE))
				return result;
		}
		CopyCharToUTF16(str.data(), strUTF);

		result |= suites.StreamSuite2()->AEGP_GetNewEffectStreamByIndex(m_AAEID, effectRef, index, &streamRef);
		result |= suites.DynamicStreamSuite4()->AEGP_SetStreamName(streamRef, strUTF);
		result |= suites.StreamSuite2()->AEGP_DisposeStream(streamRef);

		AAEData.m_OutData->out_flags |= PF_OutFlag_REFRESH_UI;
		if (effect == nullptr)
		{
			result |= suites.EffectSuite4()->AEGP_DisposeEffect(effectRef);
		}
		return result;
	}

	PF_Err	GetParamsSequenceUID(SAAEIOData &AAEData, std::string &out, unsigned int index, AEGP_EffectRefH effect = nullptr)
	{
		AEGP_SuiteHandler	suites(AAEData.m_InData->pica_basicP);
		AEGP_StreamRefH		streamRef = nullptr;
		AEGP_EffectRefH		effectRef = effect;
		PF_Err				result = PF_Err_NONE;

		AEGP_MemHandle		nameHandle;
		aechar_t			*wname;

		out.clear();
		if (effectRef == nullptr)
		{
			result |= suites.PFInterfaceSuite1()->AEGP_GetNewEffectForEffect(m_AAEID, AAEData.m_InData->effect_ref, &effectRef);
			if (!AE_VERIFY(result == A_Err_NONE))
				return result;
		}
		result |= suites.StreamSuite2()->AEGP_GetNewEffectStreamByIndex(m_AAEID, effectRef, index, &streamRef);

		result |= suites.StreamSuite5()->AEGP_GetStreamName(m_AAEID, streamRef, false, &nameHandle);
		result |= suites.StreamSuite2()->AEGP_DisposeStream(streamRef);
		streamRef = nullptr;

		result |= suites.MemorySuite1()->AEGP_LockMemHandle(nameHandle, reinterpret_cast<void**>(&wname));

		WCharToString(wname, &out);

		result |= suites.MemorySuite1()->AEGP_UnlockMemHandle(nameHandle);
		result |= suites.MemorySuite1()->AEGP_FreeMemHandle(nameHandle);

		if (effect == nullptr)
			result |= suites.EffectSuite4()->AEGP_DisposeEffect(effectRef);
		effectRef = nullptr;
		return result;
	}

	void	MakeParamCopy(PF_ParamDef *actual[], PF_ParamDef copy[], A_short arraySize)
	{
		for (A_short iS = 0; iS < arraySize; ++iS) {
			AEFX_CLR_STRUCT(copy[iS]);
			copy[iS] = *actual[iS];
		}
	}

	PF_Err	AddAngleParameter(PF_InData *in_data, const char *name, unsigned int id, float defaultValue = 0.0f, PF_ParamFlags flags = 0, PF_ParamUIFlags uiFlags = 0)
	{
		PF_ParamDef		def;

		AEFX_CLR_STRUCT(def);
		def.flags = flags;
		def.ui_flags = uiFlags;
		PF_ADD_ANGLE(name, defaultValue, id);

		m_ParametersIndexes[id] = ++m_CurrentIndex;
		return PF_Err_NONE;
	}

	PF_Err	AddPercentParameter(PF_InData *in_data, const char *name, unsigned int id, int defaultValue = 0, PF_ParamFlags flags = 0, PF_ParamUIFlags uiFlags = 0)
	{
		PF_ParamDef		def;

		AEFX_CLR_STRUCT(def);
		def.flags = flags;
		def.ui_flags = uiFlags;
		PF_ADD_PERCENT(name, defaultValue, id);

		m_ParametersIndexes[id] = ++m_CurrentIndex;
		return PF_Err_NONE;
	}

	PF_Err	AddCheckBoxParameter(PF_InData *in_data, const char *name, unsigned int id, bool defaultValue = false, PF_ParamFlags flags = 0, PF_ParamUIFlags uiFlags = 0)
	{
		PF_ParamDef		def;

		AEFX_CLR_STRUCT(def);
		def.flags = flags;
		def.ui_flags = uiFlags;
		PF_ADD_CHECKBOX(name, "", defaultValue, flags, id);

		m_ParametersIndexes[id] = ++m_CurrentIndex;
		return PF_Err_NONE;
	}

	PF_Err	AddFloatParameter(PF_InData *in_data, const char *name, unsigned int id, float defaultValue = 0.0f, float min = 0.0f, float max = 0.0f, PF_ParamFlags flags = 0, PF_ParamUIFlags uiFlags = 0)
	{
		PF_ParamDef		def;

		AEFX_CLR_STRUCT(def);
		def.flags = flags;
		def.ui_flags = uiFlags;
		PF_ADD_FLOAT_SLIDER(name, min, max, min, max,
			0/*Curve tolerance*/, defaultValue, 2/*float*/, 0/*display_flags*/, 0/*want phase*/, id);

		m_ParametersIndexes[id] = ++m_CurrentIndex;
		return PF_Err_NONE;
	}

	PF_Err	AddFloatParameterUnbound(PF_InData *in_data, const char *name,  unsigned int id, float defaultValue = 0.0f, PF_ParamFlags flags = 0, PF_ParamUIFlags uiFlags = 0)
	{
		PF_ParamDef		def;

		AEFX_CLR_STRUCT(def);
		def.flags = flags;
		def.ui_flags = uiFlags;
		PF_ADD_FLOAT_SLIDER(name, k_MinFloat, k_MaxFloat, k_MinFloat, k_MaxFloat,
			0/*Curve tolerance*/, defaultValue, 2/*float*/, 0/*display_flags*/, 0/*want phase*/, id);

		m_ParametersIndexes[id] = ++m_CurrentIndex;
		return PF_Err_NONE;
	}

	PF_Err	AddIntParameter(PF_InData *in_data, const char *name, unsigned int id, int defaultValue = 0, int min = 0, int max = 0, PF_ParamFlags flags = 0, PF_ParamUIFlags uiFlags = 0)
	{
		PF_ParamDef		def;

		AEFX_CLR_STRUCT(def);
		def.flags = flags;
		def.ui_flags = uiFlags;
		PF_ADD_FLOAT_SLIDER(name, (PF_FpShort)min, (PF_FpShort)max, (PF_FpShort)min, (PF_FpShort)max,
			0/*Curve tolerance*/, defaultValue, 0/*int*/, 0/*display_flags*/, 0/*want phase*/, id);

		m_ParametersIndexes[id] = ++m_CurrentIndex;
		return PF_Err_NONE;
	}

	PF_Err	AddIntParameterUnbound(PF_InData *in_data, const char *name, unsigned int id, int defaultValue = 0, PF_ParamFlags flags = 0, PF_ParamUIFlags uiFlags = 0)
	{
		PF_ParamDef		def;

		AEFX_CLR_STRUCT(def);
		def.flags = flags;
		def.ui_flags = uiFlags;
		PF_ADD_FLOAT_SLIDER(name, k_MinFloat, k_MaxFloat, k_MinFloat, k_MaxFloat,
			0/*Curve tolerance*/, defaultValue, 0/*int*/, 0/*display_flags*/, 0/*want phase*/, id);

		m_ParametersIndexes[id] = ++m_CurrentIndex;
		return PF_Err_NONE;
	}

	PF_Err	StartParameterCategory(PF_InData *in_data, const char *name, unsigned int id)
	{
		PF_ParamDef		def;

		AEFX_CLR_STRUCT(def);
		PF_ADD_TOPIC(name, id);
		m_ParametersIndexes[id] = ++m_CurrentIndex;
		return PF_Err_NONE;
	}

	PF_Err	EndParameterCategory(PF_InData *in_data,  unsigned int id)
	{
		PF_ParamDef		def;

		AEFX_CLR_STRUCT(def);
		PF_END_TOPIC(id);
		m_ParametersIndexes[id] = ++m_CurrentIndex;
		return PF_Err_NONE;
	}

protected:
	AEGP_PluginID	m_AAEID;
	int				*m_ParametersIndexes = nullptr;
	int				m_CurrentIndex = 0;
};

//----------------------------------------------------------------------------

__AAEPK_END

#endif

