//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"

#include "AEAttributeSampler_PluginInterface.h"
#include "AEAttributeSampler_SequenceData.h"
#include "AEAttributeSampler_ParamDefine.h"

#include "PopcornFX_UID.h"

//AE
#include <AE_Effect.h>
#include <AE_EffectCB.h>
#include <AE_Macros.h>
#include <Param_Utils.h>
#include <Smart_Utils.h>
#include <AE_EffectCBSuites.h>
#include <String_Utils.h>
#include <AE_GeneralPlug.h>
#include <AEFX_ChannelDepthTpl.h>
#include <AEGP_SuiteHandler.h>

//AAE Plugin code
#include <PopcornFX_Suite.h>
#include <PopcornFX_Define.h>

#include <limits>
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
	AAEData.m_OutData->out_flags = PF_OutFlag_DEEP_COLOR_AWARE |
								   PF_OutFlag_I_AM_OBSOLETE |
								   PF_OutFlag_SEND_UPDATE_PARAMS_UI;
	//PF_OutFlag2_SUPPORTS_QUERY_DYNAMIC_FLAGS	-> To be able to change dynamicly some out_flags. see doc.
	//PF_OutFlag2_FLOAT_COLOR_AWARE				-> To support 32bit per chan format. Need PF_OutFlag2_SUPPORTS_SMART_RENDER
	//PF_OutFlag2_SUPPORTS_SMART_RENDER			-> Necessary for new render pipeline.
	//PF_OutFlag2_I_USE_3D_CAMERA				-> Can query 3D camera information, Not sure if necessery in Attribute or just on Effect.
	AAEData.m_OutData->out_flags2 = PF_OutFlag2_SUPPORTS_QUERY_DYNAMIC_FLAGS |
									PF_OutFlag2_FLOAT_COLOR_AWARE |
									PF_OutFlag2_SUPPORTS_SMART_RENDER |
									PF_OutFlag2_I_USE_3D_CAMERA |
									PF_OutFlag2_SUPPORTS_THREADED_RENDERING;

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
	PF_InData		*in_data = AAEData.m_InData; //Used in AE Macros
	PF_ParamDef		def;
	float			minFloat = -100000.0f; //std::numeric_limits<float>::min() crashes AfterFX, smashing his stack to pieces.
	float			maxFloat =  100000.0f; //same as above.

	m_ParametersIndexes = new int[__AttributeSamplerType_Parameters_Count];
	for (unsigned int i = 0; i < __AttributeSamplerType_Parameters_Count; ++i)
		m_ParametersIndexes[i] = -1;
	m_ParametersIndexes[0] = 0; // First Parameter is reserved to AE

	CBasePluginInterface::AddCheckBoxParameter(in_data, GetStringPtr(StrID_Generic_Infernal_Uuid), AttributeSamplerType_Parameters_Infernal_Uuid);
	CBasePluginInterface::AddCheckBoxParameter(in_data, GetStringPtr(StrID_Generic_Infernal_Name), AttributeSamplerType_Parameters_Infernal_Name, false, 0, PF_PUI_INVISIBLE);

	AEFX_CLR_STRUCT(def);
	def.flags = PF_ParamFlag_SUPERVISE | PF_ParamFlag_CANNOT_INTERP;
	PF_ADD_POPUP(	GetStringPtr(StrID_Parameters_Shapes),
					__SamplerShapeType_Count,
					SamplerShapeType_Box,
					GetStringPtr(StrID_Parameters_Shapes_Combobox),
					AttributeSamplerType_Parameters_Shapes);
	m_ParametersIndexes[AttributeSamplerType_Parameters_Shapes] = ++m_CurrentIndex;

	CBasePluginInterface::StartParameterCategory(in_data, GetStringPtr(StrID_Topic_Shape_Start), AttributeSamplerType_Topic_Shape_Start);
	{
		CBasePluginInterface::StartParameterCategory(in_data, GetStringPtr(StrID_Topic_Shape_Box_Start), AttributeSamplerType_Topic_Shape_Box_Start);
		{
			CBasePluginInterface::AddFloatParameterUnbound(in_data, GetStringPtr(StrID_Parameters_Box_Size_X), AttributeSamplerType_Parameters_Box_Size_X, 0.5f);
			CBasePluginInterface::AddFloatParameterUnbound(in_data, GetStringPtr(StrID_Parameters_Box_Size_Y), AttributeSamplerType_Parameters_Box_Size_Y, 0.5f);
			CBasePluginInterface::AddFloatParameterUnbound(in_data, GetStringPtr(StrID_Parameters_Box_Size_Z), AttributeSamplerType_Parameters_Box_Size_Z, 0.5f);
		}
		CBasePluginInterface::EndParameterCategory(in_data, AttributeSamplerType_Topic_Shape_Box_End);

		StartParameterCategory(in_data, GetStringPtr(StrID_Topic_Shape_Sphere_Start), AttributeSamplerType_Topic_Shape_Sphere_Start);
		{
			AddFloatParameterUnbound(in_data, GetStringPtr(StrID_Parameters_Sphere_Radius), AttributeSamplerType_Parameters_Sphere_Radius, 1.0f);
			AddFloatParameterUnbound(in_data, GetStringPtr(StrID_Parameters_Sphere_InnerRadius), AttributeSamplerType_Parameters_Sphere_InnerRadius, 0.0f);
		}
		EndParameterCategory(in_data, AttributeSamplerType_Topic_Shape_Sphere_End);

		StartParameterCategory(in_data, GetStringPtr(StrID_Topic_Shape_Ellipsoid_Start), AttributeSamplerType_Topic_Shape_Ellipsoid_Start);
		{
			AddFloatParameterUnbound(in_data, GetStringPtr(StrID_Parameters_Ellipsoid_Radius), AttributeSamplerType_Parameters_Ellipsoid_Radius, 1.0f);
			AddFloatParameterUnbound(in_data, GetStringPtr(StrID_Parameters_Ellipsoid_InnerRadius), AttributeSamplerType_Parameters_Ellipsoid_InnerRadius, 0.0f);
		}
		EndParameterCategory(in_data, AttributeSamplerType_Topic_Shape_Ellipsoid_End);

		StartParameterCategory(in_data, GetStringPtr(StrID_Topic_Shape_Cylinder_Start), AttributeSamplerType_Topic_Shape_Cylinder_Start);
		{
			AddFloatParameterUnbound(in_data, GetStringPtr(StrID_Parameters_Cylinder_Radius), AttributeSamplerType_Parameters_Cylinder_Radius, 1.0f);
			AddFloatParameterUnbound(in_data, GetStringPtr(StrID_Parameters_Cylinder_Height), AttributeSamplerType_Parameters_Cylinder_Height, 0.5f);
			AddFloatParameterUnbound(in_data, GetStringPtr(StrID_Parameters_Cylinder_InnerRadius), AttributeSamplerType_Parameters_Cylinder_InnerRadius, 0.0f);
		}
		EndParameterCategory(in_data, AttributeSamplerType_Topic_Shape_Cylinder_End);

		StartParameterCategory(in_data, GetStringPtr(StrID_Topic_Shape_Capsule_Start), AttributeSamplerType_Topic_Shape_Capsule_Start);
		{
			AddFloatParameterUnbound(in_data, GetStringPtr(StrID_Parameters_Capsule_Radius), AttributeSamplerType_Parameters_Capsule_Radius, 1.0f);
			AddFloatParameterUnbound(in_data, GetStringPtr(StrID_Parameters_Capsule_Height), AttributeSamplerType_Parameters_Capsule_Height, 0.5f);
			AddFloatParameterUnbound(in_data, GetStringPtr(StrID_Parameters_Capsule_InnerRadius), AttributeSamplerType_Parameters_Capsule_InnerRadius, 0.0f);
		}
		EndParameterCategory(in_data, AttributeSamplerType_Topic_Shape_Capsule_End);

		StartParameterCategory(in_data, GetStringPtr(StrID_Topic_Shape_Cone_Start), AttributeSamplerType_Topic_Shape_Cone_Start);
		{
			AddFloatParameterUnbound(in_data, GetStringPtr(StrID_Parameters_Cone_Radius), AttributeSamplerType_Parameters_Cone_Radius, 1.0f);
			AddFloatParameterUnbound(in_data, GetStringPtr(StrID_Parameters_Cone_Height), AttributeSamplerType_Parameters_Cone_Height, 0.5f);
		}
		EndParameterCategory(in_data, AttributeSamplerType_Topic_Shape_Cone_End);

		StartParameterCategory(in_data, GetStringPtr(StrID_Topic_Shape_Mesh_Start), AttributeSamplerType_Topic_Shape_Mesh_Start);
		{
			AEFX_CLR_STRUCT(def);
			PF_ADD_FLOAT_SLIDERX(GetStringPtr(StrID_Parameters_Mesh_Scale), minFloat, maxFloat, minFloat, maxFloat, 1.0f/*default*/, 6, 0, 0, AttributeSamplerType_Parameters_Mesh_Scale);
			m_ParametersIndexes[AttributeSamplerType_Parameters_Mesh_Scale] = ++m_CurrentIndex;

			AEFX_CLR_STRUCT(def);
			PF_ADD_BUTTON(GetStringPtr(StrID_Parameters_Mesh_Path), GetStringPtr(StrID_Parameters_Mesh_Path_Button), 0, PF_ParamFlag_SUPERVISE, AttributeSamplerType_Parameters_Mesh_Path);
			m_ParametersIndexes[AttributeSamplerType_Parameters_Mesh_Path] = ++m_CurrentIndex;

			AddCheckBoxParameter(in_data, GetStringPtr(StrID_Parameters_Mesh_Bind_Backdrop), AttributeSamplerType_Parameters_Mesh_Bind_Backdrop);

			AddCheckBoxParameter(in_data, GetStringPtr(StrID_Parameters_Mesh_Bind_Backdrop_Weight_Enabled), AttributeSamplerType_Parameters_Mesh_Bind_Backdrop_Weighted_Enabled);

			AddIntParameter(in_data, GetStringPtr(StrID_Parameters_Mesh_Bind_Backdrop_ColorStreamID), AttributeSamplerType_Parameters_Mesh_Bind_Backdrop_ColorStreamID, 0, 0, 100);
			AddIntParameter(in_data, GetStringPtr(StrID_Parameters_Mesh_Bind_Backdrop_WeightStreamID), AttributeSamplerType_Parameters_Mesh_Bind_Backdrop_WeightStreamID, 0, 0, 100);

		}
		EndParameterCategory(in_data, AttributeSamplerType_Topic_Shape_Mesh_End);
	}
	EndParameterCategory(in_data, AttributeSamplerType_Topic_Shape_End);

	AEFX_CLR_STRUCT(def);
	def.flags = PF_ParamFlag_SUPERVISE;
	PF_ADD_LAYER(GetStringPtr(StrID_Parameters_Layer_Pick), PF_LayerDefault_NONE, AttributeSamplerType_Layer_Pick);
	m_ParametersIndexes[AttributeSamplerType_Layer_Pick] = ++m_CurrentIndex;

	AddCheckBoxParameter(in_data, GetStringPtr(StrID_Parameters_Layer_Sample_Once), AttributeSamplerType_Layer_Sample_Once);

	AddCheckBoxParameter(in_data, GetStringPtr(StrID_Parameters_Layer_Sample_Seeking), AttributeSamplerType_Layer_Sample_Seeking);

	AEFX_CLR_STRUCT(def);
	PF_ADD_BUTTON(GetStringPtr(StrID_Parameters_VectorField_Path), GetStringPtr(StrID_Parameters_Mesh_Path_Button), 0, PF_ParamFlag_SUPERVISE, AttributeSamplerType_Parameters_VectorField_Path);
	m_ParametersIndexes[AttributeSamplerType_Parameters_VectorField_Path] = ++m_CurrentIndex;

	AddFloatParameter(in_data, GetStringPtr(StrID_Parameters_VectorField_Strength), AttributeSamplerType_Parameters_VectorField_Strength, 0.1f, 0.0f, 10.0f);

	AEFX_CLR_STRUCT(def);
	def.ui_flags = PF_PUI_INVISIBLE;
	def.param_type = PF_Param_POINT_3D;
	PF_STRCPY(def.name, (GetStringPtr(StrID_Parameters_VectorField_Position)));
	def.u.point3d_d.x_value = def.u.point3d_d.x_dephault = 0;
	def.u.point3d_d.y_value = def.u.point3d_d.y_dephault = 0;
	def.u.point3d_d.z_value = def.u.point3d_d.z_dephault = 0;
	def.uu.id = (AttributeSamplerType_Parameters_VectorField_Position);
	PF_ADD_PARAM(in_data, -1, &def);
	m_ParametersIndexes[AttributeSamplerType_Parameters_VectorField_Position] = ++m_CurrentIndex;

	AEFX_CLR_STRUCT(def);
	def.flags = PF_ParamFlag_SUPERVISE |
		PF_ParamFlag_CANNOT_TIME_VARY |
		PF_ParamFlag_CANNOT_INTERP;
	//def.ui_flags = PF_PUI_STD_CONTROL_ONLY;
	PF_ADD_POPUP(GetStringPtr(StrID_Parameters_VectorField_Interpolation),
				 __EInterpolationType_Count,
				 EInterpolationType_Point,
				 GetStringPtr(StrID_Parameters_VectorField_Interpolation_Combobox),
				 AttributeSamplerType_Parameters_VectorField_Interpolation);
	m_ParametersIndexes[AttributeSamplerType_Parameters_VectorField_Interpolation] = ++m_CurrentIndex;

	AddFloatParameter(in_data, GetStringPtr(StrID_Parameters_Layer_Sample_Downsampling_X), AttributeSamplerType_Layer_Sample_Downsampling_X, 1.0f, 1.0f, 100.0f);
	AddFloatParameter(in_data, GetStringPtr(StrID_Parameters_Layer_Sample_Downsampling_Y), AttributeSamplerType_Layer_Sample_Downsampling_Y, 1.0f, 1.0f, 100.0f);

	AAEData.m_OutData->num_params = __AttributeSamplerType_Parameters_Count;

	popcornFXSuite->SetParametersIndexes(m_ParametersIndexes, EPKChildPlugins::SAMPLER);
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

	A_Err								result = A_Err_NONE;
	SAttributeSamplerSequenceDataFlat	*sequenceData = nullptr;
	AEGP_SuiteHandler					suites(AAEData.m_InData->pica_basicP);
	PF_Handle							sequenceDataHandle = suites.HandleSuite1()->host_new_handle(sizeof(SAttributeSamplerSequenceDataFlat));

	if (!sequenceDataHandle)
		return PF_Err_OUT_OF_MEMORY;
	sequenceData = static_cast<SAttributeSamplerSequenceDataFlat*>(suites.HandleSuite1()->host_lock_handle(sequenceDataHandle));
	if (sequenceData != nullptr)
	{
		AEFX_CLR_STRUCT(*sequenceData);

		sequenceData->m_IsFlat = true;

		sequenceData->SetUUID(CUUIDGenerator::Get16().data());
		sequenceData->SetName("AttributeSamplerName");
		sequenceData->SetResourcePath("");

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

	AEGP_SuiteHandler				suites(AAEData.m_InData->pica_basicP);
	PF_Err							result = PF_Err_NONE;

	if (AAEData.m_InData->sequence_data)
	{
		PF_Handle							sequenceDataFlatHandle = AAEData.m_InData->sequence_data;
		SAttributeSamplerSequenceDataFlat	*sequenceDataFlat = static_cast<SAttributeSamplerSequenceDataFlat*>(suites.HandleSuite1()->host_lock_handle(sequenceDataFlatHandle));

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

	PF_Handle							sequenceDataHandle = AAEData.m_InData->sequence_data;
	SAttributeSamplerSequenceDataFlat	*sequenceData = static_cast<SAttributeSamplerSequenceDataFlat*>(suites.HandleSuite1()->host_lock_handle(sequenceDataHandle));

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

		if (GetParamsSequenceUID(AAEData, uuid, m_ParametersIndexes[AttributeSamplerType_Parameters_Infernal_Uuid]) != PF_Err_NONE)
			return result;

		if (m_AttributeData.count(uuid) == 0)
			return result;

		SAttributeSamplerDesc	*descriptor = m_AttributeData[uuid]->m_DescAttribute;

		if (descriptor == nullptr)
			return result;

		sequenceData->m_IsFlat = true;
		sequenceData->SetName(descriptor->GetAttributePKKey().c_str());
		sequenceData->SetResourcePath(descriptor->m_ResourcePath.c_str());

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
															ATTRIBUTESAMPLER_INPUT,
															ATTRIBUTESAMPLER_INPUT,
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

PF_Err	CPluginInterface::SmartRender(SAAEIOData &AAEData)
{
	AEGP_SuiteHandler	suites(AAEData.m_InData->pica_basicP);
	PF_EffectWorld		*inputWorld = nullptr;
	PF_EffectWorld		*outputWorld = nullptr;
	PF_Err				err = PF_Err_NONE;

	std::string			uuid;

	AAEData.m_ExtraData.m_SmartRenderData->cb->checkout_layer_pixels(AAEData.m_InData->effect_ref, ATTRIBUTESAMPLER_INPUT, &inputWorld);
	AAEData.m_ExtraData.m_SmartRenderData->cb->checkout_output(AAEData.m_InData->effect_ref, &outputWorld);

	if (inputWorld == nullptr || outputWorld == nullptr)
	{
		return PF_Err_BAD_CALLBACK_PARAM;
	}
	outputWorld->data = inputWorld->data;

	AAEData.m_ExtraData.m_SmartRenderData->cb->checkin_layer_pixels(AAEData.m_InData->effect_ref, ATTRIBUTESAMPLER_INPUT);

	return err;
}

//----------------------------------------------------------------------------

PF_Err CPluginInterface::UpdateParams(SAAEIOData &AAEData, PF_ParamDef *params[])
{
	(void)AAEData;
	(void)params;

	PF_Err				result = PF_Err_NONE;
	std::string			uuid;

	if (AAEData.m_InData->appl_id == 'PrMr')
		return result;

	if (_GetAttributeSequenceUID(AAEData, uuid) == false)
		return PF_Err_BAD_CALLBACK_PARAM;
	if (m_AttributeData.count(uuid) == 0)
		return result;

	AEGP_SuiteHandler			suites(AAEData.m_InData->pica_basicP);
	SAttributeSamplerDesc		*descriptor = m_AttributeData[uuid]->m_DescAttribute;

	if (descriptor == nullptr)
		return result;

	if (descriptor != nullptr)
	{
		if (m_ParametersIndexes[AttributeSamplerType_Parameters_Mesh_Path] == (AAEData.m_ExtraData.m_ChangeParamData->param_index) ||
			m_ParametersIndexes[AttributeSamplerType_Parameters_VectorField_Path] == (AAEData.m_ExtraData.m_ChangeParamData->param_index))
		{
			AEFX_SuiteScoper<PopcornFXSuite1>	PopcornFXSuite = AEFX_SuiteScoper<PopcornFXSuite1>(AAEData.m_InData, kPopcornFXSuite1, kPopcornFXSuiteVersion1, AAEData.m_OutData, "PopcornFX suite was not found.");
			result |= PopcornFXSuite->Display_AttributeSampler_BrowseMeshDialog(AAEData, descriptor);
			AAEData.m_OutData->out_flags |= PF_OutFlag_FORCE_RERENDER;
		}
		else if (m_ParametersIndexes[AttributeSamplerType_Layer_Pick] == (AAEData.m_ExtraData.m_ChangeParamData->param_index))
		{
			AAEData.m_OutData->out_flags |= PF_OutFlag_FORCE_RERENDER;
		}
	}
	return result;
}

//----------------------------------------------------------------------------

bool	CPluginInterface::_GetAttributeSequenceUID(SAAEIOData &AAEData, std::string &out)
{
	AEGP_SuiteHandler					suites(AAEData.m_InData->pica_basicP);
	PF_Handle							sequenceDataHandle = AAEData.m_InData->sequence_data;
	SAttributeSamplerSequenceDataFlat	*sequenceDataFlat = static_cast<SAttributeSamplerSequenceDataFlat*>(suites.HandleSuite1()->host_lock_handle(sequenceDataHandle));

	out.clear();
	if (sequenceDataFlat && sequenceDataFlat->m_IsFlat == true)
	{
		out.append(sequenceDataFlat->m_AttributeUUID, strlen(sequenceDataFlat->m_AttributeUUID));
		suites.HandleSuite1()->host_unlock_handle(sequenceDataHandle);
		return true;
	}
	return false;
}

//----------------------------------------------------------------------------

PF_Err CPluginInterface::_UpdateParamsVisibility(SAAEIOData &AAEData, SAttributeSamplerData *AttrData)
{
	AEGP_SuiteHandler	suites(AAEData.m_InData->pica_basicP);
	PF_Err				err = PF_Err_NONE;
	AEGP_EffectRefH		effectRef = nullptr;
	AEGP_StreamRefH		streamRef = nullptr;

	AAEData.m_OutData->out_flags |= PF_OutFlag_REFRESH_UI;

	err = suites.PFInterfaceSuite1()->AEGP_GetNewEffectForEffect(m_AAEID, AAEData.m_InData->effect_ref, &effectRef);
	if (!AE_VERIFY(err == A_Err_NONE))
		return err;

	for (int i = 1; i < __AttributeSamplerType_Parameters_Count; ++i) // Start at 1, first params is reserved.
	{
		err |= suites.StreamSuite2()->AEGP_GetNewEffectStreamByIndex(m_AAEID, effectRef, m_ParametersIndexes[i], &streamRef);
		if (!AE_VERIFY(err == A_Err_NONE))
		{
			err |= suites.EffectSuite4()->AEGP_DisposeEffect(effectRef);
			return err;

		}
		bool visibility = GetParamsVisibility(i, AttrData->m_DescAttribute->m_Type);

		// Toggle visibility of parameter
		err |= suites.DynamicStreamSuite2()->AEGP_SetDynamicStreamFlag(streamRef, AEGP_DynStreamFlag_HIDDEN, FALSE, !visibility);
		
		err |= suites.StreamSuite2()->AEGP_DisposeStream(streamRef);
		streamRef = nullptr;
		if (!AE_VERIFY(err == A_Err_NONE))
		{
			err |= suites.EffectSuite4()->AEGP_DisposeEffect(effectRef);
			return err;
		}
	}
	err |= suites.EffectSuite4()->AEGP_DisposeEffect(effectRef);
	return err;
}

//----------------------------------------------------------------------------

PF_Err	CPluginInterface::HandleDataFromAEGP(SAAEIOData &AAEData,
											PF_ParamDef	*params[])
{
	(void)params;

	PF_Err					err = PF_Err_NONE;
	void					*extraData = AAEData.m_ExtraData.m_UndefinedData;
	SAttributeSamplerDesc	*desc = reinterpret_cast<SAttributeSamplerDesc*>(extraData);
	AEGP_SuiteHandler		suites(AAEData.m_InData->pica_basicP);

	if (desc != nullptr)
	{
		if (desc->m_IsDeleted)
		{
			std::string			uuid;

			if (_GetAttributeSequenceUID(AAEData, uuid) == false)
			{
				AE_VERIFY(false);
				return PF_Err_BAD_CALLBACK_PARAM;
			}

			if (m_AttributeData.count(uuid) == 0)
				return PF_Err_BAD_CALLBACK_PARAM;

			SAttributeSamplerData	*AttrData = m_AttributeData[uuid];

			if (AttrData)
			{
				m_AttributeData.erase(uuid);

				AttrData->m_DescAttribute = nullptr;
				if (desc->m_Descriptor != nullptr)
					delete desc->m_Descriptor;
				delete desc;
				delete AttrData;
				return PF_Err_NONE;
			}
		}
		else
		{
			m_AttrUID += 1;

			std::string					pkKey;
			std::string					uuid;
			SAttributeSamplerData		*AttrData = nullptr;

			if (_GetAttributeSequenceUID(AAEData, uuid) == false)
			{
				AE_VERIFY(false);
				return PF_Err_BAD_CALLBACK_PARAM;
			}
			if (m_AttributeData.count(uuid) == 0)
			{
				AttrData = new SAttributeSamplerData{};
			}
			AttrData = m_AttributeData[uuid];
			AttrData->m_DescAttribute = desc;

			pkKey = AttrData->m_DescAttribute->GetAttributePKKey();

			AttrData->m_DescAttribute->m_IsDefaultValue = AttrData->m_IsDefault;
			AttrData->m_DescAttribute->m_ResourcePath = AttrData->m_ResourcePath;

			AEGP_EffectRefH		effectRef = nullptr;

			err = suites.PFInterfaceSuite1()->AEGP_GetNewEffectForEffect(m_AAEID, AAEData.m_InData->effect_ref, &effectRef);
			if (!AE_VERIFY(err == A_Err_NONE))
				return err;

			err |= SetEffectName(AAEData, pkKey, effectRef);
			err |= SetParameterStreamName(AAEData, uuid, m_ParametersIndexes[AttributeSamplerType_Parameters_Infernal_Uuid], effectRef);
			err |= SetParameterStreamName(AAEData, pkKey, m_ParametersIndexes[AttributeSamplerType_Parameters_Infernal_Name], effectRef);
			err |= suites.EffectSuite4()->AEGP_DisposeEffect(effectRef);
			effectRef = nullptr;


			_UpdateParamsVisibility(AAEData, AttrData);
		}

	}
	AAEData.m_OutData->out_flags |= PF_OutFlag_FORCE_RERENDER;
	AE_VERIFY(err == A_Err_NONE);
	return err;
}

//----------------------------------------------------------------------------

PF_Err		CPluginInterface::UpdateParamsUI(SAAEIOData &AAEData, PF_ParamDef *params[])
{
	(void)params;
	std::string			uuid;

	if (GetParamsSequenceUID(AAEData, uuid, m_ParametersIndexes[AttributeSamplerType_Parameters_Infernal_Uuid]) != PF_Err_NONE)
		return PF_Err_BAD_CALLBACK_PARAM;
	if (m_AttributeData.count(uuid) == 0)
		return A_Err_NONE;

	PF_Err					err = PF_Err_NONE;
	AEGP_SuiteHandler		suites(AAEData.m_InData->pica_basicP);
	SAttributeSamplerDesc	*descriptor = m_AttributeData[uuid]->m_DescAttribute;

	if (descriptor == nullptr)
		return PF_Err_NONE;

	if (descriptor != nullptr && descriptor->m_IsDeleted)
	{
		m_AttributeData.erase(uuid);
		delete descriptor;
		return PF_Err_NONE;
	}
	return err;
}

//----------------------------------------------------------------------------

A_Err	CPluginInterface::_RegisterAttributeInstancePlugin(SAAEIOData &AAEData, PF_ParamDef *params[], SAttributeSamplerSequenceDataFlat *sequenceData, bool setup)
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
		SAttributeSamplerData		*AttrData = new SAttributeSamplerData{};

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

				err |= SetParameterStreamName(AAEData, uuid, m_ParametersIndexes[AttributeSamplerType_Parameters_Infernal_Uuid], effectRef);
				err |= SetParameterStreamName(AAEData, name, m_ParametersIndexes[AttributeSamplerType_Parameters_Infernal_Name], effectRef);
				err |= suites.EffectSuite4()->AEGP_DisposeEffect(effectRef);
			}
		}
		m_AttributeData[id] = AttrData;
	}
	if (sequenceData != nullptr)
	{
		m_AttributeData[id]->m_ResourcePath = sequenceData->m_ResourcePath;
	}
	AE_VERIFY(err == A_Err_NONE);
	return A_Err_NONE;;
}

//----------------------------------------------------------------------------

__AAEPK_END
