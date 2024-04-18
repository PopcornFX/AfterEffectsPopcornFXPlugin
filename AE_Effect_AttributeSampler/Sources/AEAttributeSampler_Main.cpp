//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"

#include "AEAttributeSampler_Main.h"

#include "AEAttributeSampler_PluginInterface.h"
#include "PopcornFX_Suite.h"

#include <assert.h>

#include <PopcornFX_Define.h>

//----------------------------------------------------------------------------

extern "C"
{
	DllExport	PF_Err PluginDataEntryFunction(	PF_PluginDataPtr	inPtr,
												PF_PluginDataCB		inPluginDataCallBackPtr,
												SPBasicSuite		*inSPBasicSuitePtr,
												const char			*inHostName,
												const char			*inHostVersion)
	{
		(void)inSPBasicSuitePtr;
		(void)inHostName;
		(void)inHostVersion;

		PF_Err	result = PF_Err_INVALID_CALLBACK;

		result = PF_REGISTER_EFFECT(
			inPtr,								// Infos must match the PopcornFXPiPL.r
			inPluginDataCallBackPtr,			// 
			"Attribute Sampler",				// Name
			"ADBE PopcornFX Sampler",	// Match Name
			"PopcornFX",						// Category
			AE_RESERVED_INFO);					// Reserved Info
		return result;
	}
}

//----------------------------------------------------------------------------

PF_Err	EffectMain(	PF_Cmd			cmd,
					PF_InData		*in_data,
					PF_OutData		*out_data,
					PF_ParamDef		*params[],
					PF_LayerDef		*output,
					void			*extra)
{
	PF_Err						result = PF_Err_NONE;
	AAePk::CPluginInterface		&AEPlugin = AAePk::CPluginInterface::Instance();
	AAePk::SAAEIOData			AAEData{ cmd, in_data, out_data, extra, AEPlugin.GetParametersIndexes() };

#if _DEBUG
	try
	{
#endif

		assert(in_data != nullptr);
		assert(out_data != nullptr);
		if (in_data == nullptr)
			return PF_Err_BAD_CALLBACK_PARAM;
	
		if (AAEData.m_InData->appl_id == 'PrMr')
		{
			//User Tried to load plugin in Premiere Pro.
			//There is surely a better way to handle this.
			return PF_Err_UNRECOGNIZED_PARAM_TYPE;
		}
	
		switch (cmd)
		{
		// Called once
		case	PF_Cmd_ABOUT:
			//Mandatory
			//Version and General Infos about the plugin
			result = AEPlugin.About(AAEData, params, output);
			break;
		case	PF_Cmd_GLOBAL_SETUP:
			//Mandatory
			//Startup run
			result = AEPlugin.GlobalSetup(AAEData, params, output);
			break;
		case	PF_Cmd_PARAMS_SETUP:
			//Mandatory
			//Setup AAE UI.
			result = AEPlugin.ParamsSetup(AAEData, params, output);
			break;
		case	PF_Cmd_GLOBAL_SETDOWN:
			//Mandatory
			result = AEPlugin.GlobalSetdown(AAEData, params, output);
			break;
		// Sequence Handling
		//UI thread
		case	PF_Cmd_SEQUENCE_SETUP:
			//Each time the user adds the effect to a layer
			result = AEPlugin.SequenceSetup(AAEData, params, output);
			break;
		case	PF_Cmd_SEQUENCE_RESETUP:
			//Load or Duplicate
			result = AEPlugin.SequenceReSetup(AAEData, params, output);
			break;
			//UI thread
		case	PF_Cmd_SEQUENCE_FLATTEN:
			//Effect Saved, copied, duplicated..
			result = AEPlugin.SequenceFlatten(AAEData, params, output);
			break;
		case	PF_Cmd_SEQUENCE_SETDOWN:
			//Effect Deleted
			result = AEPlugin.SequenceShutdown(AAEData, params, output);
			break;
		case	PF_Cmd_GET_FLATTENED_SEQUENCE_DATA:
			break;
		// Called each Frame
		case	PF_Cmd_AUDIO_SETUP:
		case	PF_Cmd_AUDIO_RENDER:
		case	PF_Cmd_AUDIO_SETDOWN:
			break;
		case	PF_Cmd_FRAME_SETUP:
			//Allow resizing of drawing area
			break;
		case	PF_Cmd_SMART_PRE_RENDER:
			result = AEPlugin.PreRender(AAEData);
			//Can be called several times for one render
			break;
		case	PF_Cmd_SMART_RENDER:
			result = AEPlugin.SmartRender(AAEData);
			break;
		case	PF_Cmd_FRAME_SETDOWN:
			//Allow resizing of drawing area
			break;
		// Messaging
		case	PF_Cmd_EVENT:
			break;
		case	PF_Cmd_USER_CHANGED_PARAM:
			AEPlugin.UpdateParams(AAEData, params);
			//If PF_ParamFlag_SUPERVIZE if set when adding param, PF_Cmd_USER_CHANGED_PARAM is called when value change
			break;
		case	PF_Cmd_UPDATE_PARAMS_UI:
			result = AEPlugin.UpdateParamsUI(AAEData, params);
			break;
		case	PF_Cmd_ARBITRARY_CALLBACK:
		case	PF_Cmd_GET_EXTERNAL_DEPENDENCIES:
			break;
		case	PF_Cmd_COMPLETELY_GENERAL:
			result = AEPlugin.HandleDataFromAEGP(AAEData, params);
			break;
		case	PF_Cmd_DO_DIALOG:
			//Send when user click on Options
			//Received if PF_OutFlag_I_DO_DIALOG is set in PF_Cmd_GLOBAL_SETUP
			break;
		case	PF_Cmd_QUERY_DYNAMIC_FLAGS:
			break;
		default:
			break;
		}
	
#if _DEBUG
	}
	catch (...)
	{
		assert(false);
	}
#endif
	return result;
}

//----------------------------------------------------------------------------

