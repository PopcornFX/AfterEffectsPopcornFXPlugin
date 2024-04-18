//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"

#include "AEAttribute_PluginInterface.h"
#include "AEAttribute_SequenceData.h"
#include "AEAttribute_ParamDefine.h"

#include "PopcornFX_UID.h"

//AE
#include <AE_Effect.h>
#include <AE_EffectCB.h>
#include <AE_Macros.h>
#include <Param_Utils.h>
#include <Smart_Utils.h>
#include <String_Utils.h>
#include <AE_EffectCBSuites.h>
#include <AE_GeneralPlug.h>
#include <AEFX_ChannelDepthTpl.h>
#include <AEGP_SuiteHandler.h>

//AAE Plugin code
#include <PopcornFX_Suite.h>
#include <PopcornFX_Define.h>

#include <string>
#include <assert.h>

__AAEPK_BEGIN

//----------------------------------------------------------------------------

CPluginInterface					*CPluginInterface::m_Instance = nullptr;
uint32_t							 CPluginInterface::m_AttrUID = 1;

//----------------------------------------------------------------------------

CPluginInterface::CPluginInterface()
{
}

//----------------------------------------------------------------------------

CPluginInterface::~CPluginInterface()
{
	for (auto& it : m_AttributeData)
	{
		delete(it.second);
	}
	m_AttributeData.clear();
}

//----------------------------------------------------------------------------

CPluginInterface	&CPluginInterface::Instance()
{
	if (CPluginInterface::m_Instance == nullptr)
	{
		m_Instance = new CPluginInterface();
	}
	return *m_Instance;
}

//----------------------------------------------------------------------------

PF_Err	CPluginInterface::About(	SAAEIOData	&AAEData,
									PF_ParamDef	*params[],
									PF_LayerDef	*output)
{
	(void)output;
	(void)params;

	AEGP_SuiteHandler	suites(AAEData.m_InData->pica_basicP);
	suites.ANSICallbacksSuite1()->sprintf(AAEData.m_OutData->return_msg,
		"%s v%d.%d.%d\r%s",
		STR(StrID_Name),
		AEPOPCORNFX_MAJOR_VERSION,
		AEPOPCORNFX_MINOR_VERSION,
		AEPOPCORNFX_BUG_VERSION,
		STR(StrID_Description));
	return PF_Err_NONE;
}

//----------------------------------------------------------------------------

PF_Err	CPluginInterface::GlobalSetup(	SAAEIOData		&AAEData,
										PF_ParamDef		*params[],
										PF_LayerDef		*output)
{
	(void)output;
	(void)params;
	m_MainThreadID = std::this_thread::get_id();

	AEFX_SuiteScoper<PopcornFXSuite1>	popcornFXSuite = AEFX_SuiteScoper<PopcornFXSuite1>(	AAEData.m_InData,
																							kPopcornFXSuite1,
																							kPopcornFXSuiteVersion1,
																							AAEData.m_OutData,
																							"PopcornFX suite was not found.");
	AEGP_SuiteHandler					suites(AAEData.m_InData->pica_basicP);

	AAEData.m_OutData->my_version = PF_VERSION(	AEPOPCORNFX_MAJOR_VERSION,
												AEPOPCORNFX_MINOR_VERSION,
												AEPOPCORNFX_BUG_VERSION,
												AEPOPCORNFX_STAGE_VERSION,
												AEPOPCORNFX_BUILD_VERSION);

	//PF_OutFlag_DEEP_COLOR_AWARE				-> To support 16bit per chan format. 
	//PF_OutFlag_I_AM_OBSOLETE					-> Do not show in menu. We do not want user to create this effect manually.
	//PF_OutFlag_SEND_UPDATE_PARAMS_UI			-> To be notified when PF_ParamFlag_SUPERVISE is set on parameters
	AAEData.m_OutData->out_flags = PF_OutFlag_DEEP_COLOR_AWARE | PF_OutFlag_I_AM_OBSOLETE | PF_OutFlag_SEND_UPDATE_PARAMS_UI;
	//PF_OutFlag2_SUPPORTS_QUERY_DYNAMIC_FLAGS	-> To be able to change dynamicly some out_flags. see doc.
	//PF_OutFlag2_FLOAT_COLOR_AWARE				-> To support 32bit per chan format. Need PF_OutFlag2_SUPPORTS_SMART_RENDER
	//PF_OutFlag2_SUPPORTS_SMART_RENDER			-> Necessary for new render pipeline.
	//PF_OutFlag2_I_USE_3D_CAMERA				-> Can query 3D camera information, Not sure if necessery in Attribute or just on Effect.
	AAEData.m_OutData->out_flags2 = PF_OutFlag2_SUPPORTS_QUERY_DYNAMIC_FLAGS | PF_OutFlag2_FLOAT_COLOR_AWARE | PF_OutFlag2_SUPPORTS_SMART_RENDER | PF_OutFlag2_I_USE_3D_CAMERA |
		PF_OutFlag2_SUPPORTS_THREADED_RENDERING;// | PF_OutFlag2_MUTABLE_RENDER_SEQUENCE_DATA_SLOWER;;

	suites.UtilitySuite3()->AEGP_RegisterWithAEGP(nullptr, STR(StrID_Name), &m_AAEID);

	return popcornFXSuite->InitializePopcornFXIFN(AAEData);
}

//----------------------------------------------------------------------------

PF_Err	CPluginInterface::ParamsSetup(	SAAEIOData		&AAEData,
										PF_ParamDef		*params[],
										PF_LayerDef		*output)
{
	(void)output;
	(void)params;

	AEFX_SuiteScoper<PopcornFXSuite1>	popcornFXSuite = AEFX_SuiteScoper<PopcornFXSuite1>(	AAEData.m_InData,
																							kPopcornFXSuite1,
																							kPopcornFXSuiteVersion1,
																							AAEData.m_OutData,
																							"PopcornFX suite was not found.");
	PF_Err			result = PF_Err_NONE;
	//TMP Used in Macros
	PF_InData		*in_data = AAEData.m_InData;
	PF_ParamDef		def;

	m_ParametersIndexes = new int[__Attribute_Parameters_Count];
	for (unsigned int i = 0; i < __Attribute_Parameters_Count; ++i)
		m_ParametersIndexes[i] = -1;
	m_ParametersIndexes[0] = 0; // First Parameter is reserved to AE

	AddCheckBoxParameter(in_data, STR(StrID_Generic_Bool1), Attribute_Parameters_Bool1);
	AddCheckBoxParameter(in_data, STR(StrID_Generic_Bool2), Attribute_Parameters_Bool2);
	AddCheckBoxParameter(in_data, STR(StrID_Generic_Bool3), Attribute_Parameters_Bool3);
	AddCheckBoxParameter(in_data, STR(StrID_Generic_Bool4), Attribute_Parameters_Bool4);

	AddFloatParameterUnbound(in_data, STR(StrID_Generic_Int1), Attribute_Parameters_Int1, 0.0f, PF_ValueDisplayFlag_NONE | PF_ParamFlag_SUPERVISE | PF_ParamFlag_COLLAPSE_TWIRLY);
	AddFloatParameterUnbound(in_data, STR(StrID_Generic_Int2), Attribute_Parameters_Int2, 0.0f, PF_ValueDisplayFlag_NONE | PF_ParamFlag_SUPERVISE | PF_ParamFlag_COLLAPSE_TWIRLY);
	AddFloatParameterUnbound(in_data, STR(StrID_Generic_Int3), Attribute_Parameters_Int3, 0.0f, PF_ValueDisplayFlag_NONE | PF_ParamFlag_SUPERVISE | PF_ParamFlag_COLLAPSE_TWIRLY);
	AddFloatParameterUnbound(in_data, STR(StrID_Generic_Int4), Attribute_Parameters_Int4, 0.0f, PF_ValueDisplayFlag_NONE | PF_ParamFlag_SUPERVISE | PF_ParamFlag_COLLAPSE_TWIRLY);

	AddFloatParameterUnbound(in_data, STR(StrID_Generic_Float1), Attribute_Parameters_Float1, 0.0f, PF_ValueDisplayFlag_NONE | PF_ParamFlag_SUPERVISE | PF_ParamFlag_COLLAPSE_TWIRLY);
	AddFloatParameterUnbound(in_data, STR(StrID_Generic_Float2), Attribute_Parameters_Float2, 0.0f, PF_ValueDisplayFlag_NONE | PF_ParamFlag_SUPERVISE | PF_ParamFlag_COLLAPSE_TWIRLY);
	AddFloatParameterUnbound(in_data, STR(StrID_Generic_Float3), Attribute_Parameters_Float3, 0.0f, PF_ValueDisplayFlag_NONE | PF_ParamFlag_SUPERVISE | PF_ParamFlag_COLLAPSE_TWIRLY);
	AddFloatParameterUnbound(in_data, STR(StrID_Generic_Float4), Attribute_Parameters_Float4, 0.0f, PF_ValueDisplayFlag_NONE | PF_ParamFlag_SUPERVISE | PF_ParamFlag_COLLAPSE_TWIRLY);

	//To do add Quaternion
	//PF_ADD_POINT_3D(STR(StrID_Generic_Quaternion), 50, 50, 0, AttributeType_Quaternion)

	AddCheckBoxParameter(in_data, STR(StrID_Generic_Infernal_Uuid), Attribute_Parameters_Infernal_Uuid, false, 0, PF_PUI_INVISIBLE);

	AddCheckBoxParameter(in_data, STR(StrID_Scale_Checkbox), Attribute_Parameters_AffectedByScale, false, PF_ParamFlag_CANNOT_TIME_VARY | PF_ParamFlag_CANNOT_INTERP);

	AddCheckBoxParameter(in_data, STR(StrID_Generic_Infernal_Name), Attribute_Parameters_Infernal_Name, false, 0, PF_PUI_INVISIBLE);


	AEFX_CLR_STRUCT(def);
	PF_ADD_COLOR(STR(StrID_Generic_Color_RGB),
		(char)(0.0f),
		(char)(0.0f),
		(char)(0.0f),
		Attribute_Parameters_Color_RGB);
	m_ParametersIndexes[Attribute_Parameters_Color_RGB] = ++m_CurrentIndex;

	AddPercentParameter(in_data, STR(StrID_Generic_Color_A), Attribute_Parameters_Color_A, 100);

	AEFX_CLR_STRUCT(def);
	PF_ADD_BUTTON(STR(StrID_Parameters_Reset), STR(StrID_Parameters_Reset_Button), 0, PF_ParamFlag_SUPERVISE, Attribute_Parameters_Reset);
	m_ParametersIndexes[Attribute_Parameters_Reset] = ++m_CurrentIndex;

	AAEData.m_OutData->num_params = __Attribute_Parameters_Count;

	popcornFXSuite->SetParametersIndexes(m_ParametersIndexes, EPKChildPlugins::ATTRIBUTE);

	return result;
}

//----------------------------------------------------------------------------

PF_Err	CPluginInterface::GlobalSetdown(	SAAEIOData	&AAEData,
											PF_ParamDef	*params[],
											PF_LayerDef	*output)
{
	(void)output;
	(void)params;

	AEFX_SuiteScoper<PopcornFXSuite1> PopcornFXSuite = AEFX_SuiteScoper<PopcornFXSuite1>(	AAEData.m_InData,
																							kPopcornFXSuite1,
																							kPopcornFXSuiteVersion1,
																							AAEData.m_OutData,
																							"PopcornFX suite was not found.");


	if (m_ParametersIndexes != nullptr)
		delete[] m_ParametersIndexes;
	m_ParametersIndexes = nullptr;
	for (auto it = m_AttributeData.begin(); it != m_AttributeData.end(); ++it)
	{
		if (it->second != nullptr)
		{
			delete (it->second);
			it->second = nullptr;
		}
	}
	return PF_Err_NONE;
}

//----------------------------------------------------------------------------

PF_Err	CPluginInterface::SequenceSetup(SAAEIOData &AAEData, PF_ParamDef *params[], PF_LayerDef *output)
{
	(void)output;

	A_Err						result = A_Err_NONE;
	SAttributeSequenceDataFlat	*sequenceData = nullptr;
	AEGP_SuiteHandler			suites(AAEData.m_InData->pica_basicP);
	PF_Handle					sequenceDataHandle = suites.HandleSuite1()->host_new_handle(sizeof(SAttributeSequenceDataFlat));

	if (!sequenceDataHandle)
		return PF_Err_OUT_OF_MEMORY;
	sequenceData = static_cast<SAttributeSequenceDataFlat*>(suites.HandleSuite1()->host_lock_handle(sequenceDataHandle));
	if (sequenceData != nullptr)
	{
		AEFX_CLR_STRUCT(*sequenceData);

		sequenceData->m_IsDefault = true;
		sequenceData->m_IsFlat = true;
		sequenceData->SetUUID(CUUIDGenerator::Get16().data());
		sequenceData->SetName("AttributeName");

		AEGP_LayerH			layerH;
		A_long				dstID = 0;

		result |= suites.PFInterfaceSuite1()->AEGP_GetEffectLayer(AAEData.m_InData->effect_ref, &layerH);
		result |= suites.LayerSuite8()->AEGP_GetLayerID(layerH, &dstID);

		if (result == A_Err_NONE)
			sequenceData->SetLayerID(dstID);

		_RegisterAttributeInstancePlugin(AAEData, params, sequenceData, true);

		AAEData.m_OutData->sequence_data = sequenceDataHandle;
		suites.HandleSuite1()->host_unlock_handle(sequenceDataHandle);
	}
	return PF_Err_NONE;
}

//----------------------------------------------------------------------------

PF_Err	CPluginInterface::SequenceReSetup(SAAEIOData &AAEData, PF_ParamDef *params[], PF_LayerDef *output)
{
	(void)output;
	(void)params;

	AEGP_SuiteHandler			suites(AAEData.m_InData->pica_basicP);
	PF_Err						result = PF_Err_NONE;

	if (AAEData.m_InData->sequence_data)
	{
		PF_Handle					sequenceDataFlatHandle = AAEData.m_InData->sequence_data;
		SAttributeSequenceDataFlat	*sequenceDataFlat = static_cast<SAttributeSequenceDataFlat*>(suites.HandleSuite1()->host_lock_handle(sequenceDataFlatHandle));

		if (sequenceDataFlat)
		{
			_RegisterAttributeInstancePlugin(AAEData, params, sequenceDataFlat, false);
			AAEData.m_OutData->sequence_data = sequenceDataFlatHandle;

			suites.HandleSuite1()->host_unlock_handle(sequenceDataFlatHandle);
		}
	}
	else
	{
		result = SequenceSetup(AAEData, params, output);
	}
	return result;
}

//----------------------------------------------------------------------------

PF_Err	CPluginInterface::SequenceFlatten(SAAEIOData &AAEData, PF_ParamDef *params[], PF_LayerDef *output)
{
	(void)output;
	(void)params;

	AEGP_SuiteHandler	suites(AAEData.m_InData->pica_basicP);
	PF_Err				result = PF_Err_NONE;

	if (!AAEData.m_InData->sequence_data)
		return result;

	PF_Handle					sequenceDataHandle = AAEData.m_InData->sequence_data;
	SAttributeSequenceDataFlat	*sequenceData = static_cast<SAttributeSequenceDataFlat*>(suites.HandleSuite1()->host_lock_handle(sequenceDataHandle));

	if (sequenceData)
	{
		std::string			uuid;
		//Check Layer ID to determine if its a duplicate. if so, update LayerID and regenerate UUID
		AEGP_LayerH			layerH;
		A_long				dstID = 0;

		result |= suites.PFInterfaceSuite1()->AEGP_GetEffectLayer(AAEData.m_InData->effect_ref, &layerH);
		result |= suites.LayerSuite8()->AEGP_GetLayerID(layerH, &dstID);

		if (result == A_Err_NONE)
		{
			if (sequenceData->m_LayerID != dstID)
			{
				sequenceData->m_LayerID = dstID;
				sequenceData->SetUUID(CUUIDGenerator::Get16().data());
			}
		}
		_RegisterAttributeInstancePlugin(AAEData, params, sequenceData, false);

		if (GetParamsSequenceUID(AAEData, uuid, m_ParametersIndexes[Attribute_Parameters_Infernal_Uuid]) != A_Err_NONE)
			return result;

		if (m_AttributeData.count(uuid) == 0)
			return result;

		SAttributeDesc	*descriptor = m_AttributeData[uuid]->m_DescAttribute;

		if (descriptor == nullptr)
			return result;

		sequenceData->m_IsFlat = true;
		sequenceData->SetIsDefaultValue(descriptor->m_IsDefaultValue);
		sequenceData->SetName(descriptor->GetAttributePKKey().c_str());

		AAEData.m_OutData->sequence_data = sequenceDataHandle;
		suites.HandleSuite1()->host_unlock_handle(sequenceDataHandle);
	}
	else
		result = PF_Err_INTERNAL_STRUCT_DAMAGED;
	return result;
}

//----------------------------------------------------------------------------

PF_Err	CPluginInterface::SequenceShutdown(SAAEIOData &AAEData, PF_ParamDef *params[], PF_LayerDef *output)
{
	(void)output;
	(void)params;

	AEGP_SuiteHandler	suites(AAEData.m_InData->pica_basicP);

	if (AAEData.m_InData->sequence_data != nullptr)
	{
		PF_Handle				sequenceDataHandle = AAEData.m_InData->sequence_data;
		suites.HandleSuite1()->host_dispose_handle(sequenceDataHandle);
	}
	AAEData.m_InData->sequence_data = nullptr;
	AAEData.m_OutData->sequence_data = nullptr;

	return PF_Err_NONE;

}

//----------------------------------------------------------------------------

PF_Err	CPluginInterface::PreRender(SAAEIOData &AAEData)
{
	PF_RenderRequest	req = AAEData.m_ExtraData.m_PreRenderData->input->output_request;
	PF_CheckoutResult	in_result;

	AE_VERIFY(AAEData.m_ExtraData.m_PreRenderData != nullptr);
	AE_VERIFY(AAEData.m_ExtraData.m_PreRenderData->cb != nullptr);

	AAEData.m_ExtraData.m_PreRenderData->cb->checkout_layer(AAEData.m_InData->effect_ref,
															ATTRIBUTE_INPUT,
															ATTRIBUTE_INPUT,
															&req,
															AAEData.m_InData->current_time,
															AAEData.m_InData->local_time_step,
															AAEData.m_InData->time_scale,
															&in_result);

	UnionLRect(&in_result.result_rect, &AAEData.m_ExtraData.m_PreRenderData->output->result_rect);
	UnionLRect(&in_result.max_result_rect, &AAEData.m_ExtraData.m_PreRenderData->output->max_result_rect);
	return PF_Err_NONE;
}

//----------------------------------------------------------------------------

void	CPluginInterface::UpdateBoolAttribute(SAAEIOData &AAEData, SAttributeDesc *descriptor, bool *uiVisibility)
{
	bool	value[4];

	for (uint32_t i = 0; i < 4; ++i)
	{
		if (uiVisibility[i + Attribute_Parameters_Bool1])
		{
			SAAEScopedParams	params{ AAEData, i + Attribute_Parameters_Bool1 };

			value[i] = params.GetCheckBoxValue();
		}
	}
	descriptor->SetValue(&value);
}

//----------------------------------------------------------------------------

void	CPluginInterface::UpdateIntAttribute(SAAEIOData &AAEData, SAttributeDesc *descriptor, bool *uiVisibility)
{
	int		value[4];

	if (descriptor->m_AttributeSemantic == AttributeSemantic_Color)
	{
		SAAEScopedParams	color{ AAEData, Attribute_Parameters_Color_RGB };
		SAAEScopedParams	alpha{ AAEData, Attribute_Parameters_Color_A };

		A_FloatPoint3 AEColor = color.GetColor();

		value[0] = (int)AEColor.x * 255;
		value[1] = (int)AEColor.y * 255;
		value[2] = (int)AEColor.z * 255;
		value[3] = (int)alpha.GetPercent() *255;
	}
	else
	{
		for (uint32_t i = 0; i < 4; ++i)
		{
			if (uiVisibility[i + Attribute_Parameters_Int1])
			{
				SAAEScopedParams	params{ AAEData, i + Attribute_Parameters_Int1 };

				value[i] = params.GetInt();
			}
		}
	}
	descriptor->SetValue(&value);
}

//----------------------------------------------------------------------------

void	CPluginInterface::UpdateFloatAttribute(SAAEIOData &AAEData, SAttributeDesc *descriptor, bool *uiVisibility)
{
	float	value[4];

	if (descriptor->m_AttributeSemantic == AttributeSemantic_Color)
	{
		SAAEScopedParams	color{ AAEData, Attribute_Parameters_Color_RGB };
		SAAEScopedParams	alpha{ AAEData, Attribute_Parameters_Color_A };

		A_FloatPoint3 AEColor = color.GetColor();

		value[0] = (float)AEColor.x;
		value[1] = (float)AEColor.y;
		value[2] = (float)AEColor.z;
		value[3] = alpha.GetPercent();
	}
	else
	{
		for (uint32_t i = 0; i < 4; ++i)
		{
			if (uiVisibility[i + Attribute_Parameters_Float1])
			{
				SAAEScopedParams	params{ AAEData, i + Attribute_Parameters_Float1 };
				value[i] = params.GetFloat();
			}
		}
	}
	descriptor->SetValue(&value);
}

//----------------------------------------------------------------------------

PF_Err	CPluginInterface::SmartRender(SAAEIOData &AAEData)
{
	AEGP_SuiteHandler	suites(AAEData.m_InData->pica_basicP);
	PF_EffectWorld		*inputWorld = nullptr;
	PF_EffectWorld		*outputWorld = nullptr;
	PF_Err				err = PF_Err_NONE;

	std::string			uuid;

	AAEData.m_ExtraData.m_SmartRenderData->cb->checkout_layer_pixels(AAEData.m_InData->effect_ref, ATTRIBUTE_INPUT, &inputWorld);
	AAEData.m_ExtraData.m_SmartRenderData->cb->checkout_output(AAEData.m_InData->effect_ref, &outputWorld);

	if (inputWorld == nullptr || outputWorld == nullptr)
		return PF_Err_BAD_CALLBACK_PARAM;
	outputWorld->data = inputWorld->data;

	if (GetParamsSequenceUID(AAEData, uuid, m_ParametersIndexes[Attribute_Parameters_Infernal_Uuid]) != PF_Err_NONE)
		return PF_Err_BAD_CALLBACK_PARAM;
	if ( m_AttributeData.count(uuid) == 0)
		return err;
	
	SAttributeDesc	*descriptor = m_AttributeData[uuid]->m_DescAttribute;
	bool			*uiVisibility = m_AttributeData[uuid]->m_UIVisibility;

	if (descriptor != nullptr)
	{
		switch (descriptor->m_Type)
		{
		case Attribute_Parameters_Bool1:
		case Attribute_Parameters_Bool2:
		case Attribute_Parameters_Bool3:
		case Attribute_Parameters_Bool4:
			UpdateBoolAttribute(AAEData, descriptor, uiVisibility);
			break;
		case Attribute_Parameters_Int1:
		case Attribute_Parameters_Int2:
		case Attribute_Parameters_Int3:
		case Attribute_Parameters_Int4:
			UpdateIntAttribute(AAEData, descriptor, uiVisibility);
			break;
		case Attribute_Parameters_Float1:
		case Attribute_Parameters_Float2:
		case Attribute_Parameters_Float3:
		case Attribute_Parameters_Float4:
			UpdateFloatAttribute(AAEData, descriptor, uiVisibility);
			break;
		}
		{
			SAAEScopedParams		affectedByScale{ AAEData, Attribute_Parameters_AffectedByScale };

			descriptor->m_IsAffectedByScale = affectedByScale.GetCheckBoxValue();
		}
	}

	AAEData.m_ExtraData.m_SmartRenderData->cb->checkin_layer_pixels(AAEData.m_InData->effect_ref, ATTRIBUTE_INPUT);

	return err;
}

//----------------------------------------------------------------------------

PF_Err CPluginInterface::UpdateParams(SAAEIOData &AAEData, PF_ParamDef *params[])
{
	(void)AAEData;
	(void)params;

	PF_Err				result = PF_Err_NONE;
#if 1
	std::string			uuid;

	if (GetParamsSequenceUID(AAEData, uuid, m_ParametersIndexes[Attribute_Parameters_Infernal_Uuid]) != A_Err_NONE)
		return PF_Err_BAD_CALLBACK_PARAM;
	if (m_AttributeData.count(uuid) == 0)
		return result;

	AEGP_SuiteHandler	suites(AAEData.m_InData->pica_basicP);
	SAttributeDesc		*descriptor = m_AttributeData[uuid]->m_DescAttribute;

	if (descriptor == nullptr)
		return result;

	if (descriptor != nullptr)
	{
		if (AAEData.m_ExtraData.m_ChangeParamData->param_index == m_ParametersIndexes[Attribute_Parameters_Reset])
		{
			descriptor->ResetValues();
			m_AttributeData[uuid]->m_IsDefault = true;
			SetDefaultValueIFN(AAEData, params, m_AttributeData[uuid]);
		}
	}
#endif
	return result;
}

//----------------------------------------------------------------------------

bool	CPluginInterface::_GetAttributeSequenceUID(SAAEIOData &AAEData, std::string &out)
{
	AEGP_SuiteHandler		suites(AAEData.m_InData->pica_basicP);
	AEFX_SuiteScoper<PF_EffectSequenceDataSuite1> seqdata_suite = AEFX_SuiteScoper<PF_EffectSequenceDataSuite1>(AAEData.m_InData, kPFEffectSequenceDataSuite, kPFEffectSequenceDataSuiteVersion1, AAEData.m_OutData);
	PF_ConstHandle			constSeq;
	seqdata_suite->PF_GetConstSequenceData(AAEData.m_InData->effect_ref, &constSeq);

	const SAttributeSequenceDataFlat	*sequenceDataFlat = static_cast<const SAttributeSequenceDataFlat*>(suites.HandleSuite1()->host_lock_handle((PF_Handle)constSeq));

	out.clear();
	if (sequenceDataFlat && sequenceDataFlat->m_IsFlat == true)
	{
		out.append(sequenceDataFlat->m_AttributeUUID, strlen(sequenceDataFlat->m_AttributeUUID));
		suites.HandleSuite1()->host_unlock_handle((PF_Handle)constSeq);
		return true;
	}
	if (sequenceDataFlat)
		suites.HandleSuite1()->host_unlock_handle((PF_Handle)constSeq);
	return false;
}

//----------------------------------------------------------------------------

bool	Debug_IsInterfaceRelevant(EAttributeParameterType current, EAttributeType attribute, EAttributeSemantic semantic)
{
	if (semantic == AttributeSemantic_Color)
	{
		if (current == Attribute_Parameters_Color_RGB)
			return true;
		else if (current == Attribute_Parameters_Color_A &&
				 (attribute == AttributeType_Int4 || attribute == AttributeType_Float4))
			return true;
		else
			return false;
	}
	if (attribute >= AttributeType_Bool1 && attribute <= AttributeType_Bool4)
	{
		return (current >= AttributeType_Bool1 && current <= attribute);
	}
	if (attribute >= AttributeType_Int1 && attribute <= AttributeType_Int4)
	{
		return (current >= AttributeType_Int1 && current <= attribute);
	}
	if (attribute >= AttributeType_Float1 && attribute <= AttributeType_Float4)
	{
		return (current >= AttributeType_Float1 && current <= attribute);
	}
	return false;
}

//----------------------------------------------------------------------------

PF_Err	CPluginInterface::SetDefaultValueIFN(SAAEIOData &AAEData, PF_ParamDef *params[], SAttributeData *AttrData)
{
	(void)params; 

	PF_Err				err = PF_Err_NONE;
	AEGP_SuiteHandler	suites(AAEData.m_InData->pica_basicP);
	AEGP_StreamRefH		streamRef = nullptr;
	AEGP_EffectRefH		effectRef = nullptr;
	bool				*uiVisibility = AttrData->m_UIVisibility;
	SAttributeDesc		*desc = AttrData->m_DescAttribute;

	err = suites.PFInterfaceSuite1()->AEGP_GetNewEffectForEffect(m_AAEID, AAEData.m_InData->effect_ref, &effectRef);
	if (!AE_VERIFY(err == A_Err_NONE))
		return err;

	for (int i = 1; i < __Attribute_Parameters_Count; ++i) // Start at 1, first params is reserved.
	{
		if (i == Attribute_Parameters_Infernal_Uuid ||
			i == Attribute_Parameters_AffectedByScale ||
			i == Attribute_Parameters_Infernal_Name ||
			i == Attribute_Parameters_Reset)
			continue;
		err |= suites.StreamSuite2()->AEGP_GetNewEffectStreamByIndex(m_AAEID, effectRef, m_ParametersIndexes[i], &streamRef);
		if (!AE_VERIFY(err == A_Err_NONE))
			return err;

		if (Debug_IsInterfaceRelevant((EAttributeParameterType)i, desc->m_Type, desc->m_AttributeSemantic) == false)	// Disable
		{
			uiVisibility[i] = false;
		}
		else	// Enable and Update name
		{
			uiVisibility[i] = true;
		}
		// Toggle visibility of parameter
		err |= suites.DynamicStreamSuite2()->AEGP_SetDynamicStreamFlag(streamRef, AEGP_DynStreamFlag_HIDDEN, FALSE, !uiVisibility[i]);

		// Set Default Value
		if (uiVisibility[i] && AttrData->m_IsDefault)
		{
			AEGP_StreamValue	streamValue;
			if (i < __AttributeType_Count)
			{
				AttrData->m_DescAttribute->GetValueAsStreamValue((EAttributeType)i, &streamValue);
				err |= suites.StreamSuite2()->AEGP_SetStreamValue(m_AAEID, streamRef, &streamValue);
			}	
			else if (desc->m_AttributeSemantic == AttributeSemantic_Color)
			{
				float	color[4];
				float	mult = 1.0f;

				if (desc->m_Type >= AttributeType_Int1 && desc->m_Type <= AttributeType_Int4)
					mult = 255.0f;

				AttrData->m_DescAttribute->GetValue<float>(color);
				if (i == Attribute_Parameters_Color_RGB)
				{
					streamValue.val.color.redF = color[0] / mult;
					streamValue.val.color.greenF = color[1] / mult;
					streamValue.val.color.blueF = color[2] / mult;
					err |= suites.StreamSuite2()->AEGP_SetStreamValue(m_AAEID, streamRef, &streamValue);
				}
				else if (i == Attribute_Parameters_Color_A)
				{
					streamValue.val.one_d = color[3] * 100.0f / mult;
					err |= suites.StreamSuite2()->AEGP_SetStreamValue(m_AAEID, streamRef, &streamValue);
				}
			}
		}

		err |= suites.StreamSuite2()->AEGP_DisposeStream(streamRef);
		streamRef = nullptr;
	}
	AAEData.m_OutData->out_flags |= PF_OutFlag_REFRESH_UI;
	err |= suites.EffectSuite4()->AEGP_DisposeEffect(effectRef);

	if (!AE_VERIFY(err == A_Err_NONE))
		return err;
	return err;
}

//----------------------------------------------------------------------------

PF_Err	CPluginInterface::HandleDataFromAEGP(SAAEIOData		&AAEData,
											 PF_ParamDef	*params[])
{
	PF_Err				err = PF_Err_NONE;
	void				*extraData = AAEData.m_ExtraData.m_UndefinedData;
	SAttributeDesc		*desc = reinterpret_cast<SAttributeDesc*>(extraData);
	AEGP_SuiteHandler	suites(AAEData.m_InData->pica_basicP);

	if (desc != nullptr)
	{
		if (desc->m_IsDeleted)
		{
			std::string	uuid;

			if (_GetAttributeSequenceUID(AAEData, uuid) == false)
			{
				AE_VERIFY(false);
				return PF_Err_BAD_CALLBACK_PARAM;
			}
			if (m_AttributeData.count(uuid) == 0)
				return PF_Err_BAD_CALLBACK_PARAM;

			SAttributeData	*AttrData = m_AttributeData[uuid];

			if (AttrData)
			{
				m_AttributeData.erase(uuid);

				AttrData->m_DescAttribute = nullptr;
				delete desc;
				delete AttrData;
				return PF_Err_NONE;
			}
		}
		else
		{
			m_AttrUID += 1;

			std::string			pkKey;
			std::string			uuid;
			SAttributeData		*AttrData = nullptr;

			if (_GetAttributeSequenceUID(AAEData, uuid) == false)
			{
				AE_VERIFY(false);
				return PF_Err_BAD_CALLBACK_PARAM;
			}
			if (m_AttributeData.count(uuid) == 0)
			{
				AttrData = new SAttributeData{};
			}
			AttrData = m_AttributeData[uuid];
			AttrData->m_DescAttribute = desc;

			pkKey = AttrData->m_DescAttribute->GetAttributePKKey();

			AttrData->m_DescAttribute->m_IsDefaultValue = AttrData->m_IsDefault;

			err |= SetDefaultValueIFN(AAEData, params, AttrData);

			AEGP_EffectRefH		effectRef = nullptr;

			err = suites.PFInterfaceSuite1()->AEGP_GetNewEffectForEffect(m_AAEID, AAEData.m_InData->effect_ref, &effectRef);
			if (!AE_VERIFY(err == A_Err_NONE))
				return err;

			err |= SetEffectName(AAEData, pkKey, effectRef);
			err |= SetParameterStreamName(AAEData, uuid, m_ParametersIndexes[Attribute_Parameters_Infernal_Uuid] , effectRef);
			err |= SetParameterStreamName(AAEData, pkKey, m_ParametersIndexes[Attribute_Parameters_Infernal_Name], effectRef);
						
			params[m_ParametersIndexes[Attribute_Parameters_Infernal_Name]]->ui_flags |= PF_PUI_INVISIBLE;
			params[m_ParametersIndexes[Attribute_Parameters_Infernal_Name]]->ui_flags |= PF_PUI_DISABLED;
			
			err |= suites.EffectSuite4()->AEGP_DisposeEffect(effectRef);

			AAEData.m_OutData->out_flags |= PF_OutFlag_REFRESH_UI;
			effectRef = nullptr;
		}
	}
	AAEData.m_OutData->out_flags |= PF_OutFlag_FORCE_RERENDER;
	AE_VERIFY(err == A_Err_NONE);
	return err;
}

//----------------------------------------------------------------------------

PF_Err		CPluginInterface::UpdateParamsUI(SAAEIOData &AAEData, PF_ParamDef *params[])
{
	std::string			uuid;

	if (GetParamsSequenceUID(AAEData, uuid, m_ParametersIndexes[Attribute_Parameters_Infernal_Uuid]) != A_Err_NONE)
		return PF_Err_BAD_CALLBACK_PARAM;
	if (m_AttributeData.count(uuid) == 0)
		return A_Err_NONE;

	PF_Err				err = PF_Err_NONE;
	AEGP_SuiteHandler	suites(AAEData.m_InData->pica_basicP);
	SAttributeDesc		*descriptor = m_AttributeData[uuid]->m_DescAttribute;
	bool				*uiVisibility = m_AttributeData[uuid]->m_UIVisibility;

	if (descriptor == nullptr)
		return PF_Err_NONE;

	if (descriptor != nullptr && descriptor->m_IsDeleted)
	{
		m_AttributeData.erase(uuid);
		delete descriptor;
		return PF_Err_NONE;
	}
	PF_ParamDef			paramCopy[__Attribute_Parameters_Count];

	MakeParamCopy(params, paramCopy, __Attribute_Parameters_Count);
	for (int i = 1; i < __Attribute_Parameters_Count; ++i) // Start at 1, first params is reserved.
	{
		if (i == Attribute_Parameters_Infernal_Uuid ||
			i == Attribute_Parameters_AffectedByScale ||
			i == Attribute_Parameters_Infernal_Name)
			continue;
		if (uiVisibility[i] == false)	// Disable
		{
			paramCopy[m_ParametersIndexes[i]].ui_flags |= PF_PUI_DISABLED;
		}
		else							// Enable
		{
			if ((paramCopy[m_ParametersIndexes[i]].ui_flags & PF_PUI_DISABLED) != 0)
			{
				paramCopy[m_ParametersIndexes[i]].ui_flags &= ~PF_PUI_DISABLED;
			}
			//Should set min and max value.
			if (paramCopy[m_ParametersIndexes[i]].param_type == PF_Param_FLOAT_SLIDER)
			{
				float	value;
				descriptor->GetDefaultValueByType((EAttributeType)i, &value);
				paramCopy[m_ParametersIndexes[i]].u.fs_d.dephault = static_cast<PF_FpShort>(value);

				if (descriptor->m_HasMax || descriptor->m_HasMin)
				{
					if (descriptor->m_HasMin)
					{
						descriptor->GetMinValueByType((EAttributeType)i, &value);
						paramCopy[m_ParametersIndexes[i]].u.fs_d.slider_min = static_cast<PF_FpShort>(value);
					}
					if (descriptor->m_HasMax)
					{
						descriptor->GetMaxValueByType((EAttributeType)i, &value);
						paramCopy[m_ParametersIndexes[i]].u.fs_d.slider_max = static_cast<PF_FpShort>(value);
					}
					if ((paramCopy[m_ParametersIndexes[i]].flags & PF_ParamFlag_COLLAPSE_TWIRLY) != 0)
						paramCopy[m_ParametersIndexes[i]].flags &= ~PF_ParamFlag_COLLAPSE_TWIRLY;
				}
				else
				{
					paramCopy[m_ParametersIndexes[i]].flags |= PF_ParamFlag_COLLAPSE_TWIRLY;
				}
				
				paramCopy[m_ParametersIndexes[i]].uu.change_flags = PF_ChangeFlag_CHANGED_VALUE;
			}
		}
		//Apply Change on param
		err |= suites.ParamUtilsSuite3()->PF_UpdateParamUI(AAEData.m_InData->effect_ref, m_ParametersIndexes[i], &paramCopy[m_ParametersIndexes[i]]);
	}
	AE_VERIFY(err == A_Err_NONE);
	return err;
}

//----------------------------------------------------------------------------

A_Err	CPluginInterface::_RegisterAttributeInstancePlugin(SAAEIOData &AAEData, PF_ParamDef *params[], SAttributeSequenceDataFlat *sequenceData, bool setup)
{
	(void)params;

	PF_Err				err = PF_Err_NONE;
	std::string			id;

	if (sequenceData != nullptr)
		id = sequenceData->m_AttributeUUID;
	else if (_GetAttributeSequenceUID(AAEData, id) == false)
		return A_Err_NONE;

	if (m_AttributeData.count(id) == 0)
	{
		SAttributeData		*AttrData = new SAttributeData{};

		if (!AE_VERIFY(AttrData != nullptr))
			return A_Err_ALLOC;

		if (!setup)
		{
			if (m_MainThreadID == std::this_thread::get_id())
			{
				AEGP_SuiteHandler	suites(AAEData.m_InData->pica_basicP);
				AEGP_EffectRefH		effectRef = nullptr;

				err |= suites.PFInterfaceSuite1()->AEGP_GetNewEffectForEffect(m_AAEID, AAEData.m_InData->effect_ref, &effectRef);

				std::string		uuid(sequenceData->m_AttributeUUID);
				std::string		name(sequenceData->m_AttributeName);
				err |= SetParameterStreamName(AAEData, uuid, m_ParametersIndexes[Attribute_Parameters_Infernal_Uuid], effectRef);
				err |= SetParameterStreamName(AAEData, name, m_ParametersIndexes[Attribute_Parameters_Infernal_Name], effectRef);
				err |= suites.EffectSuite4()->AEGP_DisposeEffect(effectRef);
			}
		}
		m_AttributeData[id] = AttrData;
	}
	if (sequenceData != nullptr)
	{
		m_AttributeData[id]->m_IsDefault = sequenceData->m_IsDefault;
	}
	return A_Err_NONE;;
}

//----------------------------------------------------------------------------

__AAEPK_END
