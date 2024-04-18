//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"

#include "AEEffect_PluginInterface.h"
#include "AEEffect_ParamDefine.h"
#include "AEEffect_SequenceData.h"

//AE
#include <AE_Effect.h>
#include <AE_EffectCB.h>
#include <AE_Macros.h>
#include <Param_Utils.h>
#include <Smart_Utils.h>
#include <AE_EffectCBSuites.h>
#include <AE_GeneralPlug.h>
#include <AEFX_ChannelDepthTpl.h>
#include <AEGP_SuiteHandler.h>

//AAE Plugin code
#include "PopcornFX_Suite.h"
#include "PopcornFX_UID.h"

#include <set>
#include <map>

__AAEPK_BEGIN

//----------------------------------------------------------------------------

int									SEmitterDesc::s_ID = 0;

CPluginInterface					*CPluginInterface::m_Instance = nullptr;
uint32_t							CPluginInterface::m_AttrUID = 1;

//----------------------------------------------------------------------------

CPluginInterface::CPluginInterface()
{
}

//----------------------------------------------------------------------------

CPluginInterface::~CPluginInterface()
{
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
		GetParamsStringPtr(StrID_Name_Param_Name),
		AEPOPCORNFX_MAJOR_VERSION,
		AEPOPCORNFX_MINOR_VERSION,
		AEPOPCORNFX_BUG_VERSION,
		GetParamsStringPtr(StrID_Description_Param_Name));
	return PF_Err_NONE;
}

//----------------------------------------------------------------------------

PF_Err	CPluginInterface::GlobalSetup(	SAAEIOData		&AAEData,
										PF_ParamDef		*params[],
										PF_LayerDef		*output)
{
	(void)output;
	(void)params;

	AEFX_SuiteScoper<PopcornFXSuite1> PopcornFXSuite = AEFX_SuiteScoper<PopcornFXSuite1>(	AAEData.m_InData,
																							kPopcornFXSuite1,
																							kPopcornFXSuiteVersion1,
																							AAEData.m_OutData,
																							"PopcornFX suite was not found.");

	AAEData.m_OutData->my_version =	PF_VERSION(AEPOPCORNFX_MAJOR_VERSION, AEPOPCORNFX_MINOR_VERSION, AEPOPCORNFX_BUG_VERSION, AEPOPCORNFX_STAGE_VERSION, AEPOPCORNFX_BUILD_VERSION);

	//PF_OutFlag_DEEP_COLOR_AWARE				-> To support 16bit per chan format. 
	//PF_OutFlag_SEND_UPDATE_PARAMS_UI			-> To be notified when PF_ParamFlag_SUPERVISE is set on parameters
	AAEData.m_OutData->out_flags = PF_OutFlag_DEEP_COLOR_AWARE |
								   PF_OutFlag_SEND_UPDATE_PARAMS_UI |
								   PF_OutFlag_I_USE_SHUTTER_ANGLE;
	//PF_OutFlag2_SUPPORTS_QUERY_DYNAMIC_FLAGS	-> To be able to change dynamicly some out_flags. see doc.
	//PF_OutFlag2_FLOAT_COLOR_AWARE				-> To support 32bit per chan format. Need PF_OutFlag2_SUPPORTS_SMART_RENDER
	//PF_OutFlag2_SUPPORTS_SMART_RENDER			-> Necessary for new render pipeline.
	//PF_OutFlag2_I_USE_3D_CAMERA				-> Can query 3D camera information, Not sure if necessery in Attribute or just on Effect.
	//PF_OutFlag2_I_USE_3D_LIGHTS				-> Can query 3D LIGHT information, Not sure if necessery in Attribute or just on Effect.
	AAEData.m_OutData->out_flags2 = PF_OutFlag2_SUPPORTS_QUERY_DYNAMIC_FLAGS |
									PF_OutFlag2_FLOAT_COLOR_AWARE |
									PF_OutFlag2_SUPPORTS_SMART_RENDER |
									PF_OutFlag2_I_USE_3D_CAMERA |
									PF_OutFlag2_I_USE_3D_LIGHTS |
									PF_OutFlag2_SUPPORTS_THREADED_RENDERING;

	AEGP_SuiteHandler		suites(AAEData.m_InData->pica_basicP);
	suites.UtilitySuite3()->AEGP_RegisterWithAEGP(nullptr, GetParamsStringPtr(StrID_Name_Param_Name), &m_AAEID);

	return PopcornFXSuite->InitializePopcornFXIFN(AAEData);
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
	PF_InData		*in_data = AAEData.m_InData;
	PF_ParamDef		def;

	const float		minFloat = -1000000.0f;//std::numeric_limits<float>::min() crashes AfterFX, smashing his stack to pieces.
	const float		maxFloat = 1000000.0f;//same as above.

	m_ParametersIndexes = new int[__Effect_Parameters_Count];
	for (unsigned int i = 0; i < __Effect_Parameters_Count; ++i)
		m_ParametersIndexes[i] = -1;
	m_ParametersIndexes[0] = 0; // First Parameter is reserved to AE

	AddFloatParameterUnbound(in_data, GetParamsStringPtr(StrID_Generic_Infernal_Autorender_Param_Name), Effect_Parameters_Infernal_Autorender, 0.0f, PF_ValueDisplayFlag_NONE, PF_PUI_INVISIBLE);

	AEFX_CLR_STRUCT(def);
	PF_ADD_BUTTON(GetParamsStringPtr(StrID_Parameters_Path_Marketplace), GetParamsStringPtr(StrID_Parameters_Path_Marketplace_Button), 0, PF_ParamFlag_SUPERVISE, Effect_Parameters_Path_Marketplace);
	m_ParametersIndexes[Effect_Parameters_Path_Marketplace] = ++m_CurrentIndex;

	AEFX_CLR_STRUCT(def);
	PF_ADD_BUTTON(GetParamsStringPtr(StrID_Parameters_Path), GetParamsStringPtr(StrID_Parameters_Path_Button), 0, PF_ParamFlag_SUPERVISE, Effect_Parameters_Path);
	m_ParametersIndexes[Effect_Parameters_Path] = ++m_CurrentIndex;

	AEFX_CLR_STRUCT(def);
	PF_ADD_BUTTON(GetParamsStringPtr(StrID_Parameters_Path_Reimport), GetParamsStringPtr(StrID_Parameters_Path_Reimport_Button), 0, PF_ParamFlag_SUPERVISE, Effect_Parameters_Path_Reimport);
	m_ParametersIndexes[Effect_Parameters_Path_Reimport] = ++m_CurrentIndex;

	AddIntParameterUnbound(in_data, GetParamsStringPtr(StrID_Generic_Infernal_Effect_Path_Hash), Effect_Parameters_Infernal_Effect_Path_Hash, 0, PF_ValueDisplayFlag_NONE | PF_ParamFlag_CANNOT_TIME_VARY | PF_ParamFlag_CANNOT_INTERP, PF_PUI_INVISIBLE);

	StartParameterCategory(in_data, GetParamsStringPtr(StrID_Topic_Transform_Start), Effect_Topic_Transform_Start);
	{
		AEFX_CLR_STRUCT(def);
		def.flags = PF_ParamFlag_SUPERVISE |
			PF_ParamFlag_CANNOT_TIME_VARY |
			PF_ParamFlag_CANNOT_INTERP;
		//def.ui_flags = PF_PUI_STD_CONTROL_ONLY;
		PF_ADD_POPUP(GetParamsStringPtr(StrID_Parameters_TransformType),
			__ETransformType_Count,
			ETransformType_3D,
			GetParamsStringPtr(StrID_Parameters_TransformType_Combobox),
			Effect_Parameters_TransformType);
		m_ParametersIndexes[Effect_Parameters_TransformType] = ++m_CurrentIndex;

		AEFX_CLR_STRUCT(def);
		def.param_type = PF_Param_POINT_3D;
		PF_STRCPY(def.name, (GetParamsStringPtr(StrID_Parameters_Position)));
		def.u.point3d_d.x_value = def.u.point3d_d.x_dephault = 50;
		def.u.point3d_d.y_value = def.u.point3d_d.y_dephault = 50;
		def.u.point3d_d.z_value = def.u.point3d_d.z_dephault = 0;
		def.uu.id = (Effect_Parameters_Position);
		PF_ADD_PARAM(in_data, -1, &def);
		m_ParametersIndexes[Effect_Parameters_Position] = ++m_CurrentIndex;

		AEFX_CLR_STRUCT(def);
		def.param_type = PF_Param_POINT;
		PF_STRCPY(def.name, (GetParamsStringPtr(StrID_Parameters_Position_2D)));
		def.u.td.x_value = def.u.td.x_dephault = 50;
		def.u.td.y_value = def.u.td.y_dephault = 50;
		def.uu.id = (Effect_Parameters_Position_2D);
		PF_ADD_PARAM(in_data, -1, &def);
		m_ParametersIndexes[Effect_Parameters_Position_2D] = ++m_CurrentIndex;

		AddFloatParameter(in_data, GetParamsStringPtr(StrID_Parameters_Position_2D_Distance), Effect_Parameters_Position_2D_Distance, 1.0f, -10.f, 500.0f, NULL);

		AddAngleParameter(in_data, GetParamsStringPtr(StrID_Parameters_Rotation_X), Effect_Parameters_Rotation_X, 0.0f);
		AddAngleParameter(in_data, GetParamsStringPtr(StrID_Parameters_Rotation_Y), Effect_Parameters_Rotation_Y, 0.0f);
		AddAngleParameter(in_data, GetParamsStringPtr(StrID_Parameters_Rotation_Z), Effect_Parameters_Rotation_Z, 0.0f);

		AddFloatParameter(in_data, GetParamsStringPtr(StrID_Parameters_Scale_Factor), Effect_Parameters_Scale_Factor, 1.0f, 0.001f, 10000.0f, PF_ParamFlag_CANNOT_TIME_VARY | PF_ParamFlag_CANNOT_INTERP);

		AEFX_CLR_STRUCT(def);
		PF_ADD_BUTTON(GetParamsStringPtr(StrID_Parameters_BringEffectIntoView), GetParamsStringPtr(StrID_Parameters_BringEffectIntoView_Button), 0, PF_ParamFlag_SUPERVISE, Effect_Parameters_BringEffectIntoView);
		m_ParametersIndexes[Effect_Parameters_BringEffectIntoView] = ++m_CurrentIndex;

	}
	EndParameterCategory(in_data, Effect_Topic_Transform_End);

	AddIntParameterUnbound(in_data, GetParamsStringPtr(StrID_Parameters_Seed), Effect_Parameters_Seed, 0, PF_ParamFlag_CANNOT_TIME_VARY | PF_ParamFlag_CANNOT_INTERP);

	AddCheckBoxParameter(in_data, GetParamsStringPtr(StrID_Parameters_Simulation_State), Effect_Parameters_Simulation_State, true, PF_ParamFlag_SUPERVISE | PF_ParamFlag_CANNOT_INTERP);

	AddCheckBoxParameter(in_data, GetParamsStringPtr(StrID_Parameters_Render_Seeking_Toggle), Effect_Parameters_Seeking_Toggle, true, PF_ParamFlag_SUPERVISE | PF_ParamFlag_CANNOT_TIME_VARY | PF_ParamFlag_CANNOT_INTERP, PF_PUI_INVISIBLE);

	StartParameterCategory(in_data, GetParamsStringPtr(StrID_Topic_Rendering_Start), Effect_Topic_Rendering_Start);
	{
		AddCheckBoxParameter(in_data, GetParamsStringPtr(StrID_Parameters_Render_Background_Toggle), Effect_Parameters_Background_Toggle);

		AddPercentParameter(in_data, GetParamsStringPtr(StrID_Parameters_Render_Background_Opacity), Effect_Parameters_Background_Opacity, 0);

		AEFX_CLR_STRUCT(def);
		def.flags = PF_ParamFlag_SUPERVISE |
					PF_ParamFlag_CANNOT_TIME_VARY |
					PF_ParamFlag_CANNOT_INTERP;
		//def.ui_flags = PF_PUI_STD_CONTROL_ONLY;
		PF_ADD_POPUP(GetParamsStringPtr(StrID_Parameters_Render_Type),
			__RenderType_Count,
			RenderType_FinalCompositing,
			GetParamsStringPtr(StrID_Parameters_Render_Type_Combobox),
			Effect_Parameters_Render_Type);
		m_ParametersIndexes[Effect_Parameters_Render_Type] = ++m_CurrentIndex;

		StartParameterCategory(in_data, GetParamsStringPtr(StrID_Topic_Camera_Start), Effect_Topic_Camera_Start);
		{
			AEFX_CLR_STRUCT(def);
			def.flags = PF_ParamFlag_SUPERVISE |
				PF_ParamFlag_CANNOT_TIME_VARY |
				PF_ParamFlag_CANNOT_INTERP;
			//def.ui_flags = PF_PUI_STD_CONTROL_ONLY;
			PF_ADD_POPUP(GetParamsStringPtr(StrID_Parameters_Camera),
				__ECameraType_Count,
				ECameraType_Compo_Default,
				GetParamsStringPtr(StrID_Parameters_Camera_Combobox),
				Effect_Parameters_Camera);
			m_ParametersIndexes[Effect_Parameters_Camera] = ++m_CurrentIndex;

			AEFX_CLR_STRUCT(def);
			def.ui_flags = PF_PUI_INVISIBLE;
			def.param_type = PF_Param_POINT_3D;
			PF_STRCPY(def.name, (GetParamsStringPtr(StrID_Parameters_Camera_Position)));
			def.u.point3d_d.x_value = def.u.point3d_d.x_dephault = 50;
			def.u.point3d_d.y_value = def.u.point3d_d.y_dephault = 50;
			def.u.point3d_d.z_value = def.u.point3d_d.z_dephault = 1;
			def.uu.id = (Effect_Parameters_Camera_Position);
			PF_ADD_PARAM(in_data, -1, &def);
			m_ParametersIndexes[Effect_Parameters_Camera_Position] = ++m_CurrentIndex;

			AddAngleParameter(in_data, GetParamsStringPtr(StrID_Parameters_Camera_Rotation_X), Effect_Parameters_Camera_Rotation_X, 0, 0, PF_PUI_INVISIBLE);
			AddAngleParameter(in_data, GetParamsStringPtr(StrID_Parameters_Camera_Rotation_Y), Effect_Parameters_Camera_Rotation_Y, 0, 0, PF_PUI_INVISIBLE);
			AddAngleParameter(in_data, GetParamsStringPtr(StrID_Parameters_Camera_Rotation_Z), Effect_Parameters_Camera_Rotation_Z, 0, 0, PF_PUI_INVISIBLE);

			AddFloatParameter(in_data, GetParamsStringPtr(StrID_Parameters_Camera_FOV), Effect_Parameters_Camera_FOV, 90.0f, 1.0f, 179.0f, 0, PF_PUI_INVISIBLE);

			AddFloatParameter(in_data, GetParamsStringPtr(StrID_Parameters_Camera_Near), Effect_Parameters_Camera_Near, 0.01f, 0.0001f, 1.0f);
			AddFloatParameter(in_data, GetParamsStringPtr(StrID_Parameters_Camera_Far), Effect_Parameters_Camera_Far, 1000.0f, 1.0f, 100000.0f);
		}
		EndParameterCategory(in_data, Effect_Topic_Camera_End);

		AddCheckBoxParameter(in_data, GetParamsStringPtr(StrID_Parameters_Receive_Light), Effect_Parameters_Receive_Light, false, PF_ParamFlag_SUPERVISE | PF_ParamFlag_CANNOT_TIME_VARY | PF_ParamFlag_CANNOT_INTERP, PF_PUI_DISABLED | PF_PUI_INVISIBLE);

		StartParameterCategory(in_data, GetParamsStringPtr(StrID_Topic_PostFX_Start), Effect_Topic_PostFX_Start);
		{

			StartParameterCategory(in_data, GetParamsStringPtr(StrID_Topic_Distortion_Start), Effect_Topic_Distortion_Start);
			{
				AddCheckBoxParameter(in_data, GetParamsStringPtr(StrID_Parameters_Distortion_Enable), Effect_Parameters_Distortion_Enable, true, PF_ParamFlag_SUPERVISE | PF_ParamFlag_CANNOT_TIME_VARY | PF_ParamFlag_CANNOT_INTERP);
			}
			EndParameterCategory(in_data, Effect_Topic_Distortion_End);


			StartParameterCategory(in_data, GetParamsStringPtr(StrID_Topic_Bloom_Start), Effect_Topic_Bloom_Start);
			{
				AddCheckBoxParameter(in_data, GetParamsStringPtr(StrID_Parameters_Bloom_Enable), Effect_Parameters_Bloom_Enable, true, PF_ParamFlag_SUPERVISE | PF_ParamFlag_CANNOT_TIME_VARY | PF_ParamFlag_CANNOT_INTERP);

				AddFloatParameter(in_data, GetParamsStringPtr(StrID_Parameters_Bloom_BrightPassValue), Effect_Parameters_Bloom_BrightPassValue, 1.0f, 0.0f, 20.0f);
				AddFloatParameter(in_data, GetParamsStringPtr(StrID_Parameters_Bloom_Intensity), Effect_Parameters_Bloom_Intensity, 0.75f, 0.0f, 20.0f);
				AddFloatParameter(in_data, GetParamsStringPtr(StrID_Parameters_Bloom_Attenuation), Effect_Parameters_Bloom_Attenuation, 0.5f, 0.0f, 1.0f);

				AEFX_CLR_STRUCT(def);
				def.flags = PF_ParamFlag_SUPERVISE |
					PF_ParamFlag_CANNOT_TIME_VARY |
					PF_ParamFlag_CANNOT_INTERP;
				PF_ADD_POPUP(GetParamsStringPtr(StrID_Parameters_Bloom_GaussianBlur),
					__GaussianBlurPixelRadius_Count,
					GaussianBlurPixelRadius_5,
					GetParamsStringPtr(StrID_Parameters_Bloom_GaussianBlur_Combobox),
					Effect_Parameters_Bloom_GaussianBlur);
				m_ParametersIndexes[Effect_Parameters_Bloom_GaussianBlur] = ++m_CurrentIndex;

				AddIntParameter(in_data, GetParamsStringPtr(StrID_Parameters_Bloom_RenderPassCount), Effect_Parameters_Bloom_RenderPassCount, 6, 0, 13, PF_ParamFlag_CANNOT_TIME_VARY | PF_ParamFlag_CANNOT_INTERP);
			}
			EndParameterCategory(in_data, Effect_Topic_Bloom_End);

			StartParameterCategory(in_data, GetParamsStringPtr(StrID_Topic_ToneMapping_Start), Effect_Topic_ToneMapping_Start);
			{
				AddCheckBoxParameter(in_data, GetParamsStringPtr(StrID_Parameters_ToneMapping_Enable), Effect_Parameters_ToneMapping_Enable, true, PF_ParamFlag_SUPERVISE | PF_ParamFlag_CANNOT_TIME_VARY | PF_ParamFlag_CANNOT_INTERP);

				AddFloatParameter(in_data, GetParamsStringPtr(StrID_Parameters_ToneMapping_Saturation), Effect_Parameters_ToneMapping_Saturation, 1.0f, 0.0f, 2.0f);
				AddFloatParameter(in_data, GetParamsStringPtr(StrID_Parameters_ToneMapping_Exposure), Effect_Parameters_ToneMapping_Exposure, 0.0f, -5.0f, 5.0f);
			}
			EndParameterCategory(in_data, Effect_Topic_ToneMapping_End);

			StartParameterCategory(in_data, GetParamsStringPtr(StrID_Topic_FXAA_Start), Effect_Topic_FXAA_Start);
			{
				AddCheckBoxParameter(in_data, GetParamsStringPtr(StrID_Parameters_FXAA_Enable), Effect_Parameters_FXAA_Enable, true, PF_ParamFlag_SUPERVISE | PF_ParamFlag_CANNOT_TIME_VARY | PF_ParamFlag_CANNOT_INTERP);
			}
			EndParameterCategory(in_data, Effect_Topic_FXAA_End);
		}
		EndParameterCategory(in_data, Effect_Topic_PostFX_End);
	}
	EndParameterCategory(in_data, Effect_Topic_Rendering_End);

	StartParameterCategory(in_data, GetParamsStringPtr(StrID_Topic_BackdropMesh_Start), Effect_Topic_BackdropMesh_Start);
	{
		AddCheckBoxParameter(in_data, GetParamsStringPtr(StrID_Parameters_BackdropMesh_Enable_Rendering), Effect_Parameters_BackdropMesh_Enable_Rendering, true, PF_ParamFlag_SUPERVISE | PF_ParamFlag_CANNOT_TIME_VARY | PF_ParamFlag_CANNOT_INTERP);
		AddCheckBoxParameter(in_data, GetParamsStringPtr(StrID_Parameters_BackdropMesh_Enable_Collisions), Effect_Parameters_BackdropMesh_Enable_Collisions, true, PF_ParamFlag_SUPERVISE | PF_ParamFlag_CANNOT_TIME_VARY | PF_ParamFlag_CANNOT_INTERP);
		AddCheckBoxParameter(in_data, GetParamsStringPtr(StrID_Parameters_BackdropMesh_Enable_Animation), Effect_Parameters_BackdropMesh_Enable_Animation, true, PF_ParamFlag_SUPERVISE | PF_ParamFlag_CANNOT_TIME_VARY | PF_ParamFlag_CANNOT_INTERP);

		AEFX_CLR_STRUCT(def);
		PF_ADD_BUTTON(GetParamsStringPtr(StrID_Parameters_BackdropMesh_Path), GetParamsStringPtr(StrID_Parameters_BackdropMesh_Path_Button), 0, PF_ParamFlag_SUPERVISE, Effect_Parameters_BackdropMesh_Path);
		m_ParametersIndexes[Effect_Parameters_BackdropMesh_Path] = ++m_CurrentIndex;

		AEFX_CLR_STRUCT(def);
		PF_ADD_BUTTON(GetParamsStringPtr(StrID_Parameters_BackdropMesh_Reset), GetParamsStringPtr(StrID_Parameters_BackdropMesh_Reset_Button), 0, PF_ParamFlag_SUPERVISE, Effect_Parameters_BackdropMesh_Reset);
		m_ParametersIndexes[Effect_Parameters_BackdropMesh_Reset] = ++m_CurrentIndex;

		StartParameterCategory(in_data, GetParamsStringPtr(StrID_Topic_BackdropMesh_Transform_Start), Effect_Topic_BackdropMesh_Transform_Start);
		{
			AEFX_CLR_STRUCT(def);
			PF_ADD_POINT_3D(GetParamsStringPtr(StrID_Parameters_BackdropMesh_Position), 0, 0, 0, Effect_Parameters_BackdropMesh_Position);
			m_ParametersIndexes[Effect_Parameters_BackdropMesh_Position] = ++m_CurrentIndex;

			AddAngleParameter(in_data, GetParamsStringPtr(StrID_Parameters_BackdropMesh_Rotation_X), Effect_Parameters_BackdropMesh_Rotation_X, 0);
			AddAngleParameter(in_data, GetParamsStringPtr(StrID_Parameters_BackdropMesh_Rotation_Y), Effect_Parameters_BackdropMesh_Rotation_Y, 0);
			AddAngleParameter(in_data, GetParamsStringPtr(StrID_Parameters_BackdropMesh_Rotation_Z), Effect_Parameters_BackdropMesh_Rotation_Z, 0);

			AddFloatParameterUnbound(in_data, GetParamsStringPtr(StrID_Parameters_BackdropMesh_Scale_X), Effect_Parameters_BackdropMesh_Scale_X, 1.0f);
			AddFloatParameterUnbound(in_data, GetParamsStringPtr(StrID_Parameters_BackdropMesh_Scale_Y), Effect_Parameters_BackdropMesh_Scale_Y, 1.0f);
			AddFloatParameterUnbound(in_data, GetParamsStringPtr(StrID_Parameters_BackdropMesh_Scale_Z), Effect_Parameters_BackdropMesh_Scale_Z, 1.0f);
		}
		EndParameterCategory(in_data, Effect_Topic_BackdropMesh_Transform_End);

		AddFloatParameter(in_data, GetParamsStringPtr(StrID_Parameters_BackdropMesh_Roughness), Effect_Parameters_BackdropMesh_Roughness, 1.0f, 0.0f, 1.0f);
		AddFloatParameter(in_data, GetParamsStringPtr(StrID_Parameters_BackdropMesh_Metalness), Effect_Parameters_BackdropMesh_Metalness, 0.0f, 0.0f, 1.0f);
	}
	EndParameterCategory(in_data, Effect_Topic_BackdropMesh_End);

	StartParameterCategory(in_data, GetParamsStringPtr(StrID_Topic_BackdropAudio_Start), Effect_Topic_BackdropAudio_Start);
	{
		AEFX_CLR_STRUCT(def);
		def.flags = PF_ParamFlag_SUPERVISE;
		PF_ADD_LAYER(GetParamsStringPtr(StrID_Parameters_Audio), PF_LayerDefault_NONE, Effect_Parameters_Audio);
		m_ParametersIndexes[Effect_Parameters_Audio] = ++m_CurrentIndex;
	}
	EndParameterCategory(in_data, Effect_Topic_BackdropAudio_End);

	StartParameterCategory(in_data, GetParamsStringPtr(StrID_Topic_BackdropEnvMap_Start), Effect_Topic_BackdropEnvMap_Start);
	{
		AddCheckBoxParameter(in_data, GetParamsStringPtr(StrID_Parameters_BackdropEnvMap_Enable_Rendering), Effect_Parameters_BackdropEnvMap_Enable_Rendering);

		PF_ADD_BUTTON(GetParamsStringPtr(StrID_Parameters_BackdropEnvMap_Path), GetParamsStringPtr(StrID_Parameters_BackdropEnvMap_Path_Button), 0, PF_ParamFlag_SUPERVISE, Effect_Parameters_BackdropEnvMap_Path);
		m_ParametersIndexes[Effect_Parameters_BackdropEnvMap_Path] = ++m_CurrentIndex;

		AEFX_CLR_STRUCT(def);
		PF_ADD_BUTTON(GetParamsStringPtr(StrID_Parameters_BackdropEnvMap_Reset), GetParamsStringPtr(StrID_Parameters_BackdropEnvMap_Reset_Button), 0, PF_ParamFlag_SUPERVISE, Effect_Parameters_BackdropEnvMap_Reset);
		m_ParametersIndexes[Effect_Parameters_BackdropEnvMap_Reset] = ++m_CurrentIndex;
		
		AddFloatParameter(in_data, GetParamsStringPtr(StrID_Parameters_BackdropEnvMap_Intensity), Effect_Parameters_BackdropEnvMap_Intensity, 1.0f, 0.0f, 10.0f);

		AEFX_CLR_STRUCT(def);
		PF_ADD_COLOR(GetParamsStringPtr(StrID_Parameters_BackdropEnvMap_Color), 51, 51, 51, Effect_Parameters_BackdropEnvMap_Color);
		m_ParametersIndexes[Effect_Parameters_BackdropEnvMap_Color] = ++m_CurrentIndex;
	}
	EndParameterCategory(in_data, Effect_Topic_BackdropEnvMap_End);

	StartParameterCategory(in_data, GetParamsStringPtr(StrID_Topic_Light_Start), Effect_Topic_Light_Start);
	{
		AEFX_CLR_STRUCT(def);
		def.flags = PF_ParamFlag_SUPERVISE |
			PF_ParamFlag_CANNOT_TIME_VARY |
			PF_ParamFlag_CANNOT_INTERP;
		PF_ADD_POPUP(GetParamsStringPtr(StrID_Parameters_Light_Category),
			__ELightCategory_Count,
			ELightCategory_Debug_Default,
			GetParamsStringPtr(StrID_Parameters_Light_Combobox),
			Effect_Parameters_Light_Category);
		m_ParametersIndexes[Effect_Parameters_Light_Category] = ++m_CurrentIndex;

		AEFX_CLR_STRUCT(def);
		//def.ui_flags = PF_PUI_INVISIBLE;
		def.param_type = PF_Param_POINT_3D;
		PF_STRCPY(def.name, (GetParamsStringPtr(StrID_Parameters_Light_Direction)));
		def.u.point3d_d.x_value = def.u.point3d_d.x_dephault = 0;
		def.u.point3d_d.y_value = def.u.point3d_d.y_dephault = 1;
		def.u.point3d_d.z_value = def.u.point3d_d.z_dephault = 0;
		def.uu.id = (Effect_Parameters_Light_Direction);
		PF_ADD_PARAM(in_data, -1, &def);
		m_ParametersIndexes[Effect_Parameters_Light_Direction] = ++m_CurrentIndex;

		AddFloatParameter(in_data, GetParamsStringPtr(StrID_Parameters_Light_Intensity), Effect_Parameters_Light_Intensity, 1.0f, -100.0f, 100.0f);

		AEFX_CLR_STRUCT(def);
		PF_ADD_COLOR(GetParamsStringPtr(StrID_Parameters_Light_Color), 51, 51, 51, Effect_Parameters_Light_Color);
		m_ParametersIndexes[Effect_Parameters_Light_Color] = ++m_CurrentIndex;

		AEFX_CLR_STRUCT(def);
		PF_ADD_COLOR(GetParamsStringPtr(StrID_Parameters_Light_Ambient), 51, 51, 51, Effect_Parameters_Light_Ambient);
		m_ParametersIndexes[Effect_Parameters_Light_Ambient] = ++m_CurrentIndex;
	}
	EndParameterCategory(in_data, Effect_Topic_Light_End);

	AddFloatParameter(in_data, GetParamsStringPtr(StrID_Parameters_Refresh_Render), Effect_Parameters_Refresh_Render, 0.0f, 0.0f, 10000.0f, 0, PF_PUI_INVISIBLE | PF_PUI_DISABLED);

	AAEData.m_OutData->num_params = __Effect_Parameters_Count;

	popcornFXSuite->SetParametersIndexes(m_ParametersIndexes, EPKChildPlugins::EMITTER);
	return result;
}

//----------------------------------------------------------------------------

PF_Err CPluginInterface::GlobalSetdown(	SAAEIOData	&AAEData,
										PF_ParamDef	*params[],
										PF_LayerDef	*output)
{
	(void)AAEData;
	(void)output;
	(void)params;

	if (m_ParametersIndexes != nullptr)
		delete[] m_ParametersIndexes;
	m_ParametersIndexes = nullptr;
	for (auto it = m_QueuedEffectData.begin(); it != m_QueuedEffectData.end(); ++it)
	{
		if (*it != nullptr)
		{
			if ((*it)->m_Desc != nullptr)
			{
				delete ((*it)->m_Desc);
				(*it)->m_Desc = nullptr;
			}
			delete (*it);
		}
	}

	for (auto it = m_EffectData.begin(); it != m_EffectData.end(); ++it)
	{
		if (it->second != nullptr)
		{
			if (it->second->m_Desc != nullptr)
			{
				delete (it->second->m_Desc);
				it->second->m_Desc = nullptr;
			}
			delete (it->second);
			it->second = nullptr;
		}
	}
	m_EffectData.clear();

	return PF_Err_NONE;
}

//----------------------------------------------------------------------------

bool	CPluginInterface::GetEffectSequenceUID(SAAEIOData &AAEData, std::string &out)
{
	AEGP_SuiteHandler		suites(AAEData.m_InData->pica_basicP);
	AEFX_SuiteScoper<PF_EffectSequenceDataSuite1> seqdata_suite = AEFX_SuiteScoper<PF_EffectSequenceDataSuite1>(AAEData.m_InData, kPFEffectSequenceDataSuite, kPFEffectSequenceDataSuiteVersion1, AAEData.m_OutData);

	PF_ConstHandle constSeq;
	seqdata_suite->PF_GetConstSequenceData(AAEData.m_InData->effect_ref, &constSeq);

	const SEffectSequenceDataFlat	*sequenceDataFlat	= static_cast<const SEffectSequenceDataFlat*>(suites.HandleSuite1()->host_lock_handle((PF_Handle)constSeq));
	
	out.clear();
	if (sequenceDataFlat && sequenceDataFlat->m_IsFlat == true)
	{
		out.append(sequenceDataFlat->m_EffectUUID, strlen(sequenceDataFlat->m_EffectUUID));
		suites.HandleSuite1()->host_unlock_handle((PF_Handle)constSeq);
		return true;
	}
	if (sequenceDataFlat)
		suites.HandleSuite1()->host_unlock_handle((PF_Handle)constSeq);
	return false;
}

//----------------------------------------------------------------------------

//Called when creating an effect only.
PF_Err CPluginInterface::SequenceSetup(	SAAEIOData &AAEData,
										PF_ParamDef *params[],
										PF_LayerDef *output)
{
	(void)output;
	(void)params;

	A_Err					result = A_Err_NONE;
	SEffectSequenceDataFlat	*sequenceData = nullptr;
	AEGP_SuiteHandler		suites(AAEData.m_InData->pica_basicP);
	PF_Handle				sequenceDataHandle = suites.HandleSuite1()->host_new_handle(sizeof(SEffectSequenceDataFlat));

	if (!sequenceDataHandle)
		return PF_Err_OUT_OF_MEMORY;
	sequenceData = static_cast<SEffectSequenceDataFlat*>(suites.HandleSuite1()->host_lock_handle(sequenceDataHandle));
	if (sequenceData != nullptr)
	{
		AEFX_CLR_STRUCT(*sequenceData);

		sequenceData->m_IsFlat = true;

		sequenceData->SetEffectPathSource("");
		sequenceData->SetEffectBackdropMeshPath("");
		sequenceData->SetEffectName("");

		AEGP_LayerH			layerH;
		A_long				dstID = 0;
		
		result |= suites.PFInterfaceSuite1()->AEGP_GetEffectLayer(AAEData.m_InData->effect_ref, &layerH);
		result |= suites.LayerSuite8()->AEGP_GetLayerID(layerH, &dstID);

		if (result == A_Err_NONE)
			sequenceData->SetLayerID(dstID);

		sequenceData->SetUUID(std::to_string(dstID));

		AAEData.m_OutData->sequence_data = sequenceDataHandle;
		suites.HandleSuite1()->host_unlock_handle(sequenceDataHandle);
	}
	return PF_Err_NONE;
}

//----------------------------------------------------------------------------

PF_Err CPluginInterface::SequenceReSetup(SAAEIOData &AAEData, PF_ParamDef *params[], PF_LayerDef *output)
{
	(void)output;
	(void)params;

	AEGP_SuiteHandler	suites(AAEData.m_InData->pica_basicP);
	PF_Err				result = PF_Err_NONE;

	if (AAEData.m_InData->sequence_data)
	{
		PF_Handle				sequenceDataFlatHandle = AAEData.m_InData->sequence_data;
		SEffectSequenceDataFlat	*sequenceDataFlat = static_cast<SEffectSequenceDataFlat*>(suites.HandleSuite1()->host_lock_handle(sequenceDataFlatHandle));
	
		if (sequenceDataFlat)
		{
			//Check Layer ID to determine if its a duplicate. if so, update LayerID and regenerate UUID
			AEGP_LayerH			layerH;
			A_long				dstID = 0;
	
			result |= suites.PFInterfaceSuite1()->AEGP_GetEffectLayer(AAEData.m_InData->effect_ref, &layerH);
			result |= suites.LayerSuite8()->AEGP_GetLayerID(layerH, &dstID);

			if (result == A_Err_NONE)
			{
				if (sequenceDataFlat->m_LayerID != dstID)
				{
					sequenceDataFlat->m_LayerID = dstID;
					sequenceDataFlat->SetUUID(std::to_string(dstID).c_str());
				}
			}
			_RegisterEffectInstancePlugin(AAEData, params, sequenceDataFlat);

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
	
	PF_Handle				sequenceDataHandle = AAEData.m_InData->sequence_data;
	SEffectSequenceDataFlat	*sequenceData = static_cast<SEffectSequenceDataFlat*>(suites.HandleSuite1()->host_lock_handle(sequenceDataHandle));
	
	if (sequenceData)
	{
		//First Update sequence Data to reflect RT information
		std::string	uuid;
		
		if (GetEffectSequenceUID(AAEData, uuid) == false)
			return false;
		if (m_EffectData.count(uuid) == 0)
			return false;
		SEffectData	*data = m_EffectData[uuid];
		if (data->m_Desc != nullptr)
		{
			AEGP_LayerH			layerH;
			A_long				dstID = 0;

			result |= suites.PFInterfaceSuite1()->AEGP_GetEffectLayer(AAEData.m_InData->effect_ref, &layerH);
			result |= suites.LayerSuite8()->AEGP_GetLayerID(layerH, &dstID);

			size_t extensionIdx = data->m_Desc->m_Name.find_last_of('.');
			if (extensionIdx != std::string::npos)
				data->m_Desc->m_Name = data->m_Desc->m_Name.substr(0, extensionIdx) + ".pkfx";

			sequenceData->SetEffectName(data->m_Desc->m_Name.data());
			sequenceData->SetEffectPathSource(data->m_Desc->m_PathSource.data());
			sequenceData->SetEffectBackdropMeshPath(data->m_Desc->m_BackdropMesh.m_Path.data());
			sequenceData->SetEffectEnvironmentMapPath(data->m_Desc->m_BackdropEnvironmentMap.m_Path.data());
			sequenceData->SetLayerID(dstID);
			sequenceData->SetUUID(std::to_string(dstID));
		}
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
		PF_Handle			sequenceDataHandle = AAEData.m_InData->sequence_data;

		_UnRegisterEffectInstancePlugin(AAEData, params, nullptr);

		suites.HandleSuite1()->host_dispose_handle(sequenceDataHandle);
	}
	AAEData.m_InData->sequence_data = nullptr;
	AAEData.m_OutData->sequence_data = nullptr;

	return PF_Err_NONE;
}

//----------------------------------------------------------------------------

PF_Err CPluginInterface::QueryDynamicFlags(SAAEIOData &AAEData, PF_ParamDef *params[])
{
	(void)params;
	(void)AAEData;

	PF_Err	result = PF_Err_NONE;
	
	return result;
}

//----------------------------------------------------------------------------

PF_Err	CPluginInterface::PreRender(SAAEIOData &AAEData)
{
	PF_Err				result = PF_Err_NONE;
	PF_RenderRequest	req = AAEData.m_ExtraData.m_PreRenderData->input->output_request;
	PF_CheckoutResult	in_result;

	req.preserve_rgb_of_zero_alpha = TRUE;
	AAEData.m_ExtraData.m_PreRenderData->output->solid = false;

	result = AAEData.m_ExtraData.m_PreRenderData->cb->checkout_layer(	AAEData.m_InData->effect_ref,
																		Effect_Parameters_InputReserved,
																		Effect_Parameters_InputReserved,
																		&req,
																		AAEData.m_InData->current_time,
																		AAEData.m_InData->local_time_step,
																		AAEData.m_InData->time_scale,
																		&in_result);

	UnionLRect(&in_result.result_rect, &AAEData.m_ExtraData.m_PreRenderData->output->result_rect);
	UnionLRect(&in_result.max_result_rect, &AAEData.m_ExtraData.m_PreRenderData->output->max_result_rect);
	return result;
}

//----------------------------------------------------------------------------

PF_Err	CPluginInterface::SmartRender(SAAEIOData &AAEData)
{
	PF_Err			result = PF_Err_NONE;
	std::string		uuid;

	if (GetEffectSequenceUID(AAEData, uuid) == false)
		return PF_Err_NONE;
	if (m_EffectData.count(uuid) == 0)
		return PF_Err_NONE;

	SEffectData							*data = m_EffectData[uuid];
	SEmitterDesc						*emitterDesc = data->m_Desc;
	AEFX_SuiteScoper<PopcornFXSuite1>	PopcornFXSuite = AEFX_SuiteScoper<PopcornFXSuite1>(AAEData.m_InData, kPopcornFXSuite1, kPopcornFXSuiteVersion1, AAEData.m_OutData, "PopcornFX suite was not found.");

	if (emitterDesc == nullptr || !PopcornFXSuite->CheckEmitterValidity(AAEData, data->m_Desc))
	{
		return PF_Interrupt_CANCEL;
	}
	// With multi-frame rendering, multiple threads can execute this at the same time even with just one emitter per layer:
	data->m_Lock.lock();
	{
		SAAEScopedParams		emitterTransformType	{ AAEData, Effect_Parameters_TransformType };

		SAAEScopedParams		emitterPosition			{ AAEData, Effect_Parameters_Position };

		SAAEScopedParams		emitterPosition2D		{ AAEData, Effect_Parameters_Position_2D };
		SAAEScopedParams		emitterPosition2DDist	{ AAEData, Effect_Parameters_Position_2D_Distance };

		SAAEScopedParams		emitterRotationX	{ AAEData, Effect_Parameters_Rotation_X };
		SAAEScopedParams		emitterRotationY	{ AAEData, Effect_Parameters_Rotation_Y };
		SAAEScopedParams		emitterRotationZ	{ AAEData, Effect_Parameters_Rotation_Z };
		SAAEScopedParams		emitterSeed			{ AAEData, Effect_Parameters_Seed };

		SAAEScopedParams		emitterSimulationState { AAEData, Effect_Parameters_Simulation_State };

		emitterDesc->m_TransformType = (ETransformType)emitterTransformType.GetComboBoxValue();
		if (emitterDesc->m_TransformType == ETransformType_3D)
			emitterDesc->m_Position = emitterPosition.GetPoint3D();
		else
		{
			A_FloatPoint	xy = emitterPosition2D.GetPoint2D();
			emitterDesc->m_Position.x = xy.x;
			emitterDesc->m_Position.y = xy.y;
			emitterDesc->m_Position.z = emitterPosition2DDist.GetFloat();
		}
		emitterDesc->m_Rotation.x = emitterRotationX.GetAngle();
		emitterDesc->m_Rotation.y = emitterRotationY.GetAngle();
		emitterDesc->m_Rotation.z = emitterRotationZ.GetAngle();
		emitterDesc->m_Seed = emitterSeed.GetInt();

		{//Camera
			SAAEScopedParams		emitterCameraType{ AAEData, Effect_Parameters_Camera };
			emitterDesc->m_Camera.m_Internal = false;

			SAAEScopedParams		cameraPosition{ AAEData, Effect_Parameters_Camera_Position };
			SAAEScopedParams		cameraRotationX{ AAEData, Effect_Parameters_Camera_Rotation_X };
			SAAEScopedParams		cameraRotationY{ AAEData, Effect_Parameters_Camera_Rotation_Y };
			SAAEScopedParams		cameraRotationZ{ AAEData, Effect_Parameters_Camera_Rotation_Z };
			SAAEScopedParams		cameraFOV{ AAEData, Effect_Parameters_Camera_FOV };

			emitterDesc->m_Camera.m_Position = cameraPosition.GetPoint3D();
			emitterDesc->m_Camera.m_Rotation.x = cameraRotationX.GetAngle();
			emitterDesc->m_Camera.m_Rotation.y = cameraRotationY.GetAngle();
			emitterDesc->m_Camera.m_Rotation.z = cameraRotationZ.GetAngle();
			emitterDesc->m_Camera.m_FOV = cameraFOV.GetFloat();

			SAAEScopedParams		cameraNear	{ AAEData, Effect_Parameters_Camera_Near };
			SAAEScopedParams		cameraFar	{ AAEData, Effect_Parameters_Camera_Far };

			emitterDesc->m_Camera.m_Near = cameraNear.GetFloat();
			emitterDesc->m_Camera.m_Far = cameraFar.GetFloat();
		}
		{//Rendering
			SAAEScopedParams		renderingType					{ AAEData, Effect_Parameters_Render_Type };
			SAAEScopedParams		renderingReceiveLight			{ AAEData, Effect_Parameters_Receive_Light };
			SAAEScopedParams		renderingDistortionEnable		{ AAEData, Effect_Parameters_Distortion_Enable };
			SAAEScopedParams		renderingBloomEnable			{ AAEData, Effect_Parameters_Bloom_Enable };
			SAAEScopedParams		renderingBloomBrightPass		{ AAEData, Effect_Parameters_Bloom_BrightPassValue };
			SAAEScopedParams		renderingBloomIntensity			{ AAEData, Effect_Parameters_Bloom_Intensity };
			SAAEScopedParams		renderingBloomAttenuation		{ AAEData, Effect_Parameters_Bloom_Attenuation };
			SAAEScopedParams		renderingBloomGaussianBlur		{ AAEData, Effect_Parameters_Bloom_GaussianBlur };
			SAAEScopedParams		renderingBloomRenderPassCount	{ AAEData, Effect_Parameters_Bloom_RenderPassCount };
			SAAEScopedParams		renderingToneMappingEnable		{ AAEData, Effect_Parameters_ToneMapping_Enable };
			SAAEScopedParams		renderingToneMappingSaturation	{ AAEData, Effect_Parameters_ToneMapping_Saturation };
			SAAEScopedParams		renderingToneMappingExposure	{ AAEData, Effect_Parameters_ToneMapping_Exposure };
			SAAEScopedParams		renderingFXAAEnable				{ AAEData, Effect_Parameters_FXAA_Enable };

			SAAEScopedParams		renderingAlphaOverride			{ AAEData, Effect_Parameters_Background_Toggle };
			SAAEScopedParams		renderingAlphaOverrideValue		{ AAEData, Effect_Parameters_Background_Opacity };


			emitterDesc->m_Rendering.m_Type = (ERenderType)renderingType.GetComboBoxValue();
			emitterDesc->m_Rendering.m_ReceiveLight = renderingReceiveLight.GetCheckBoxValue();

			emitterDesc->m_Rendering.m_Distortion.m_Enable = renderingDistortionEnable.GetCheckBoxValue();

			emitterDesc->m_Rendering.m_Bloom.m_Enable = renderingBloomEnable.GetCheckBoxValue();
			emitterDesc->m_Rendering.m_Bloom.m_BrightPassValue = renderingBloomBrightPass.GetFloat();
			emitterDesc->m_Rendering.m_Bloom.m_Intensity = renderingBloomIntensity.GetFloat();
			emitterDesc->m_Rendering.m_Bloom.m_Attenuation = renderingBloomAttenuation.GetFloat();
			emitterDesc->m_Rendering.m_Bloom.m_GaussianBlur = (EGaussianBlurPixelRadius)renderingBloomGaussianBlur.GetComboBoxValue();
			emitterDesc->m_Rendering.m_Bloom.m_RenderPassCount = renderingBloomRenderPassCount.GetInt();

			emitterDesc->m_Rendering.m_ToneMapping.m_Enable = renderingToneMappingEnable.GetCheckBoxValue();
			emitterDesc->m_Rendering.m_ToneMapping.m_Saturation = renderingToneMappingSaturation.GetFloat();
			emitterDesc->m_Rendering.m_ToneMapping.m_Exposure = renderingToneMappingExposure.GetFloat();

			emitterDesc->m_Rendering.m_FXAA.m_Enable = renderingFXAAEnable.GetCheckBoxValue();

			emitterDesc->m_IsAlphaBGOverride = renderingAlphaOverride.GetCheckBoxValue();
			emitterDesc->m_AlphaBGOverride = renderingAlphaOverrideValue.GetPercent();
		}
		{//BackdropMesh
			SAAEScopedParams		backdropMeshEnableRendering		{ AAEData, Effect_Parameters_BackdropMesh_Enable_Rendering };
			SAAEScopedParams		backdropMeshEnableCollisions	{ AAEData, Effect_Parameters_BackdropMesh_Enable_Collisions };
			SAAEScopedParams		backdropMeshEnableAnimation		{ AAEData, Effect_Parameters_BackdropMesh_Enable_Animation };
			SAAEScopedParams		backdropMeshPosition			{ AAEData, Effect_Parameters_BackdropMesh_Position };
			SAAEScopedParams		backdropMeshRotationX			{ AAEData, Effect_Parameters_BackdropMesh_Rotation_X };
			SAAEScopedParams		backdropMeshRotationY			{ AAEData, Effect_Parameters_BackdropMesh_Rotation_Y };
			SAAEScopedParams		backdropMeshRotationZ			{ AAEData, Effect_Parameters_BackdropMesh_Rotation_Z };
			SAAEScopedParams		backdropMeshScaleX				{ AAEData, Effect_Parameters_BackdropMesh_Scale_X };
			SAAEScopedParams		backdropMeshScaleY				{ AAEData, Effect_Parameters_BackdropMesh_Scale_Y };
			SAAEScopedParams		backdropMeshScaleZ				{ AAEData, Effect_Parameters_BackdropMesh_Scale_Z };
			SAAEScopedParams		backdropMeshRoughness			{ AAEData, Effect_Parameters_BackdropMesh_Roughness };
			SAAEScopedParams		backdropMeshMetalness			{ AAEData, Effect_Parameters_BackdropMesh_Metalness };

			emitterDesc->m_BackdropMesh.m_EnableRendering = backdropMeshEnableRendering.GetCheckBoxValue();
			emitterDesc->m_BackdropMesh.m_EnableCollisions = backdropMeshEnableCollisions.GetCheckBoxValue();
			emitterDesc->m_BackdropMesh.m_EnableAnimations = backdropMeshEnableAnimation.GetCheckBoxValue();

			emitterDesc->m_BackdropMesh.m_Position = backdropMeshPosition.GetPoint3D();
			emitterDesc->m_BackdropMesh.m_Rotation.x = DegToRad(backdropMeshRotationX.GetAngle());
			emitterDesc->m_BackdropMesh.m_Rotation.y = DegToRad(backdropMeshRotationY.GetAngle());
			emitterDesc->m_BackdropMesh.m_Rotation.z = DegToRad(backdropMeshRotationZ.GetAngle());
			emitterDesc->m_BackdropMesh.m_Scale = A_FloatPoint3{ backdropMeshScaleX.GetFloat(), backdropMeshScaleY.GetFloat(), backdropMeshScaleZ.GetFloat() };

			emitterDesc->m_BackdropMesh.m_Roughness = backdropMeshRoughness.GetFloat();
			emitterDesc->m_BackdropMesh.m_Metalness = backdropMeshMetalness.GetFloat();

		}
		{//Backdrop Environment
			SAAEScopedParams		backdropEnvMapEnableRendering	{ AAEData, Effect_Parameters_BackdropEnvMap_Enable_Rendering };
			SAAEScopedParams		backdropEnvMapIntensity			{ AAEData, Effect_Parameters_BackdropEnvMap_Intensity };
			SAAEScopedParams		backdropEnvMapColor				{ AAEData, Effect_Parameters_BackdropEnvMap_Color };

			emitterDesc->m_BackdropEnvironmentMap.m_EnableRendering = backdropEnvMapEnableRendering.GetCheckBoxValue();
			emitterDesc->m_BackdropEnvironmentMap.m_Intensity = backdropEnvMapIntensity.GetFloat();
			emitterDesc->m_BackdropEnvironmentMap.m_Color = backdropEnvMapColor.GetColor();
		}

		{//Light
			SAAEScopedParams		LightCategory{ AAEData, Effect_Parameters_Light_Category };
			SAAEScopedParams		LightDirection{ AAEData, Effect_Parameters_Light_Direction };
			SAAEScopedParams		LightIntensity{ AAEData, Effect_Parameters_Light_Intensity };
			SAAEScopedParams		LightColor{ AAEData, Effect_Parameters_Light_Color };
			SAAEScopedParams		LightAmbient{ AAEData, Effect_Parameters_Light_Ambient };

			if (LightCategory.GetComboBoxValue() == ELightCategory_Debug_Default)
			{
				emitterDesc->m_Light.m_Internal = true;
			}
			else
			{
				emitterDesc->m_Light.m_Internal = false;
			}
			emitterDesc->m_Light.m_Category = (ELightCategory)LightCategory.GetComboBoxValue();
			emitterDesc->m_Light.m_Direction = LightDirection.GetPoint3D();
			emitterDesc->m_Light.m_Intensity = LightIntensity.GetFloat();
			emitterDesc->m_Light.m_Color = LightColor.GetColor();
			emitterDesc->m_Light.m_Ambient = LightAmbient.GetColor();
		}
		{
			SAAEScopedParams		scaleFactor{ AAEData, Effect_Parameters_Scale_Factor };

			emitterDesc->m_ScaleFactor = scaleFactor.GetFloat();
		}
	}

#if defined(PK_SCALE_DOWN)
	emitterDesc->m_Position.x = emitterDesc->m_Position.x / emitterDesc->m_ScaleFactor;
	emitterDesc->m_Position.y = emitterDesc->m_Position.y / emitterDesc->m_ScaleFactor;
	emitterDesc->m_Position.z = emitterDesc->m_Position.z / emitterDesc->m_ScaleFactor;

	emitterDesc->m_BackdropMesh.m_Position.x = emitterDesc->m_BackdropMesh.m_Position.x / emitterDesc->m_ScaleFactor;
	emitterDesc->m_BackdropMesh.m_Position.y = emitterDesc->m_BackdropMesh.m_Position.y / emitterDesc->m_ScaleFactor;
	emitterDesc->m_BackdropMesh.m_Position.z = emitterDesc->m_BackdropMesh.m_Position.z / emitterDesc->m_ScaleFactor;

	emitterDesc->m_Camera.m_Position.x = emitterDesc->m_Camera.m_Position.x / emitterDesc->m_ScaleFactor;
	emitterDesc->m_Camera.m_Position.y = emitterDesc->m_Camera.m_Position.y / emitterDesc->m_ScaleFactor;
	emitterDesc->m_Camera.m_Position.z = emitterDesc->m_Camera.m_Position.z / emitterDesc->m_ScaleFactor;
#endif

	PF_EffectWorld		*input_worldP = nullptr;
	result |= AAEData.m_ExtraData.m_SmartRenderData->cb->checkout_layer_pixels(AAEData.m_InData->effect_ref, 0/*POPCORN_INPUT*/, &input_worldP);
	if (input_worldP == nullptr)
	{
		data->m_Lock.unlock();
		return result;
	}

	AEGP_SuiteHandler		suites(AAEData.m_InData->pica_basicP);
	AEGP_LayerH				layerH;
	A_long					dstID = 0;

	result |= suites.PFInterfaceSuite1()->AEGP_GetEffectLayer(AAEData.m_InData->effect_ref, &layerH);
	result |= suites.LayerSuite8()->AEGP_GetLayerID(layerH, &dstID);

	emitterDesc->m_UUID = std::to_string(dstID);
	emitterDesc->m_LayerID = dstID;

	PopcornFXSuite->UpdateScene(AAEData, emitterDesc);

	result = PF_ABORT(AAEData.m_InData);
	if (result)
	{
		AAEData.m_ExtraData.m_SmartRenderData->cb->checkin_layer_pixels(AAEData.m_InData->effect_ref, 0);
		data->m_Lock.unlock();
		return result;
	}

	AAEData.m_ExtraData.m_SmartRenderData->cb->checkin_layer_pixels(AAEData.m_InData->effect_ref, 0);
	data->m_LastRenderTime = AAEData.m_InData->current_time;

	data->m_Lock.unlock();
	return result;
}

//----------------------------------------------------------------------------

PF_Err		CPluginInterface::ParamValueChanged(SAAEIOData &AAEData, PF_ParamDef *params[])
{
	(void)params;

	std::string			id;
	PF_Err				result = PF_Err_NONE;

	if (GetEffectSequenceUID(AAEData, id) == false)
		return false;
	if (m_EffectData.count(id) == 0)
		return false;

	// this block must be in this function, if not, it will fail.
	AEGP_SuiteHandler		suites(AAEData.m_InData->pica_basicP);
	A_Time					AETime;
	AEGP_LayerH				cameraLayer = nullptr;
	AETime.scale = AAEData.m_InData->time_scale;
	AETime.value = AAEData.m_InData->current_time;
	result |= suites.PFInterfaceSuite1()->AEGP_GetEffectCamera(AAEData.m_InData->effect_ref, &AETime, &cameraLayer);
	if (cameraLayer == nullptr)
	{
		A_FloatPoint	center;
		AEGP_CompH		effectComp = nullptr;

		center.x = AAEData.m_InData->width / 2.0f;
		center.y = AAEData.m_InData->height / 2.0f;

		A_UTF16Char			name[64];
		CopyCharToUTF16("PopcornFX Camera", name);

		AEGP_LayerH	layerH = nullptr;
		result |= suites.PFInterfaceSuite1()->AEGP_GetEffectLayer(AAEData.m_InData->effect_ref, &layerH);
		result |= suites.LayerSuite5()->AEGP_GetLayerParentComp(layerH, &effectComp);
		result |= suites.CompSuite11()->AEGP_CreateCameraInComp(name, center, effectComp, &cameraLayer);
		if (result != A_Err_NONE)
			return false;

		AEFX_SuiteScoper<PopcornFXSuite1> PopcornFXSuite = AEFX_SuiteScoper<PopcornFXSuite1>(AAEData.m_InData, kPopcornFXSuite1, kPopcornFXSuiteVersion1, AAEData.m_OutData, "PopcornFX suite was not found.");
		PopcornFXSuite->SetDefaultLayerPosition(AAEData, cameraLayer);

	}
	SEffectData							*data = m_EffectData[id];
	AEFX_SuiteScoper<PopcornFXSuite1>	PopcornFXSuite = AEFX_SuiteScoper<PopcornFXSuite1>(AAEData.m_InData, kPopcornFXSuite1, kPopcornFXSuiteVersion1, AAEData.m_OutData, "PopcornFX suite was not found.");

	if (data->m_Desc != nullptr)
	{
		if (data->m_Desc->m_IsDeleted)
		{
			m_EffectData.erase(id);
			delete data->m_Desc;
			delete data;
			return PF_Err_NONE;
		}
		if (m_ParametersIndexes[Effect_Parameters_Path] == AAEData.m_ExtraData.m_ChangeParamData->param_index)
		{
			if (PopcornFXSuite->DisplayBrowseEffectPanel(AAEData, (data->m_Desc)) != A_Err_NONE)
			{
				strcpy(AAEData.m_OutData->return_msg, "PopcornFX Error: Unable to load effect");
				AAEData.m_OutData->out_flags |= PF_OutFlag_DISPLAY_ERROR_MESSAGE;
			}
			else
			{
				SetParameterStreamName(AAEData, data->m_Desc->m_Name, m_ParametersIndexes[Effect_Parameters_Path]); //_UpdateEmitterName(AAEData, desc);
				AAEData.m_OutData->out_flags |= PF_OutFlag_FORCE_RERENDER | PF_OutFlag_REFRESH_UI;
			}
		}
		else if (m_ParametersIndexes[Effect_Parameters_Path_Marketplace] == AAEData.m_ExtraData.m_ChangeParamData->param_index)
		{
			if (PopcornFXSuite->DisplayMarketplacePanel(AAEData, (data->m_Desc)) != A_Err_NONE)
			{
				strcpy(AAEData.m_OutData->return_msg, "PopcornFX Error: Unable to load effect");
				AAEData.m_OutData->out_flags |= PF_OutFlag_DISPLAY_ERROR_MESSAGE;
			}
		}
		else if (m_ParametersIndexes[Effect_Parameters_Path_Reimport] == AAEData.m_ExtraData.m_ChangeParamData->param_index)
		{
			PopcornFXSuite->ReimportEffect(AAEData, (data->m_Desc));

			AAEData.m_OutData->out_flags |= PF_OutFlag_FORCE_RERENDER | PF_OutFlag_REFRESH_UI;
		}
		else if (m_ParametersIndexes[Effect_Parameters_BackdropMesh_Path] == AAEData.m_ExtraData.m_ChangeParamData->param_index)
		{
			if (PopcornFXSuite->DisplayBrowseMeshDialog(AAEData, (data->m_Desc)) != A_Err_NONE)
			{
				strcpy(AAEData.m_OutData->return_msg, "PopcornFX Error: Unable to load mesh");
				AAEData.m_OutData->out_flags |= PF_OutFlag_DISPLAY_ERROR_MESSAGE;
			}
			else
				AAEData.m_OutData->out_flags |= PF_OutFlag_FORCE_RERENDER | PF_OutFlag_REFRESH_UI;
		}
		else if (m_ParametersIndexes[Effect_Parameters_BackdropEnvMap_Path] == AAEData.m_ExtraData.m_ChangeParamData->param_index)
		{
			if (PopcornFXSuite->DisplayBrowseEnvironmentMapDialog(AAEData, (data->m_Desc)) != A_Err_NONE)
			{
				strcpy(AAEData.m_OutData->return_msg, "PopcornFX Error: Unable to load Env Map");
				AAEData.m_OutData->out_flags |= PF_OutFlag_DISPLAY_ERROR_MESSAGE;
			}
			else
				AAEData.m_OutData->out_flags |= PF_OutFlag_FORCE_RERENDER | PF_OutFlag_REFRESH_UI;
		}
		else if (m_ParametersIndexes[Effect_Parameters_BackdropEnvMap_Reset] == AAEData.m_ExtraData.m_ChangeParamData->param_index)
		{
			data->m_Desc->m_BackdropEnvironmentMap.m_Path = "";
			data->m_Desc->m_Update = true;
			AAEData.m_OutData->out_flags |= PF_OutFlag_FORCE_RERENDER | PF_OutFlag_REFRESH_UI;
		}
		else if (m_ParametersIndexes[Effect_Parameters_BackdropMesh_Reset] == AAEData.m_ExtraData.m_ChangeParamData->param_index)
		{
			data->m_Desc->m_BackdropMesh.m_Path = "";
			data->m_Desc->m_Update = true;
			AAEData.m_OutData->out_flags |= PF_OutFlag_FORCE_RERENDER | PF_OutFlag_REFRESH_UI;
		}
		else if (m_ParametersIndexes[Effect_Parameters_Audio] == AAEData.m_ExtraData.m_ChangeParamData->param_index)
		{
			AAEData.m_OutData->out_flags |= PF_OutFlag_FORCE_RERENDER | PF_OutFlag_REFRESH_UI;
		}
		else if (m_ParametersIndexes[Effect_Parameters_BringEffectIntoView] == AAEData.m_ExtraData.m_ChangeParamData->param_index)
		{
			PopcornFXSuite->MoveEffectIntoCurrentView(AAEData, (data->m_Desc));

			AAEData.m_OutData->out_flags |= PF_OutFlag_FORCE_RERENDER | PF_OutFlag_REFRESH_UI;
		}
	}
	return result;
}

//----------------------------------------------------------------------------

PF_Err		CPluginInterface::UpdateParamsUI(SAAEIOData &AAEData, PF_ParamDef *params[])
{
	(void)AAEData;
	(void)params;

	PF_Err					err = PF_Err_NONE;
	AEGP_SuiteHandler		suites(AAEData.m_InData->pica_basicP);
	std::string				id;

	if (GetEffectSequenceUID(AAEData, id) == false)
		return false;

	// PK to AE
	if (m_EffectData.count(id) != 0)
	{
		SEffectData			*effectData = m_EffectData[id];
		SEmitterDesc		*desc = effectData->m_Desc;

		if (desc)
		{
			if (desc->m_IsDeleted)
			{
				m_EffectData.erase(id);
				delete desc;
				delete effectData;
				return PF_Err_NONE;
			}
			
			if (desc->m_Update)
			{
				desc->m_Update = false;
				err |= SetParameterStreamName(AAEData, desc->m_Name, m_ParametersIndexes[Effect_Parameters_Path]); //_UpdateEmitterName(AAEData, desc);
				err |= SetParameterStreamName(AAEData, desc->m_BackdropMesh.m_Path, m_ParametersIndexes[Effect_Parameters_BackdropMesh_Path]); //_UpdateBackdropMeshPath(AAEData, desc);
			}
		}
	}
	PF_ParamDef				paramCopy[__Effect_Parameters_Count];
	std::map<int, int>		indexChanged;

	MakeParamCopy(params, paramCopy, __Effect_Parameters_Count);

	if (params[m_ParametersIndexes[Effect_Parameters_TransformType]]->u.pd.value == ETransformType_3D)
	{
		indexChanged.insert({ { m_ParametersIndexes[Effect_Parameters_Position],		paramCopy[m_ParametersIndexes[Effect_Parameters_Position]].ui_flags &= ~(PF_PUI_DISABLED | PF_PUI_INVISIBLE) },
							  { m_ParametersIndexes[Effect_Parameters_Position_2D],		paramCopy[m_ParametersIndexes[Effect_Parameters_Position_2D]].ui_flags |= PF_PUI_DISABLED | PF_PUI_INVISIBLE },
							  { m_ParametersIndexes[Effect_Parameters_Position_2D_Distance],		paramCopy[m_ParametersIndexes[Effect_Parameters_Position_2D_Distance]].ui_flags |= PF_PUI_DISABLED | PF_PUI_INVISIBLE },
							});

	}
	else if (params[m_ParametersIndexes[Effect_Parameters_TransformType]]->u.pd.value == ETransformType_2D)
	{
		indexChanged.insert({ { m_ParametersIndexes[Effect_Parameters_Position],		paramCopy[m_ParametersIndexes[Effect_Parameters_Position]].ui_flags |= PF_PUI_DISABLED | PF_PUI_INVISIBLE },
							  { m_ParametersIndexes[Effect_Parameters_Position_2D],		paramCopy[m_ParametersIndexes[Effect_Parameters_Position_2D]].ui_flags &= ~(PF_PUI_DISABLED | PF_PUI_INVISIBLE)  },
							  { m_ParametersIndexes[Effect_Parameters_Position_2D_Distance],		paramCopy[m_ParametersIndexes[Effect_Parameters_Position_2D_Distance]].ui_flags &= ~(PF_PUI_DISABLED | PF_PUI_INVISIBLE)  },
							});
	}
	//Apply Change on param
	AEGP_EffectRefH			effectHandle = nullptr;
	err |= suites.PFInterfaceSuite1()->AEGP_GetNewEffectForEffect(m_AAEID, AAEData.m_InData->effect_ref, &effectHandle);
	if (err != A_Err_NONE)
		return err;
	for (auto it = indexChanged.begin(); it != indexChanged.end(); ++it)
	{
		AEGP_StreamRefH		streamHandle = nullptr;
		// Toggle visibility of parameter
		err |= suites.StreamSuite2()->AEGP_GetNewEffectStreamByIndex(m_AAEID, effectHandle, it->first, &streamHandle);

		bool	visibility = false;
		if ((it->second & PF_PUI_INVISIBLE) != 0)
			visibility = true;
		err |= suites.DynamicStreamSuite2()->AEGP_SetDynamicStreamFlag(streamHandle, AEGP_DynStreamFlag_HIDDEN, FALSE, visibility);
		err |= suites.StreamSuite2()->AEGP_DisposeStream(streamHandle);

		paramCopy[it->first].ui_flags = it->second;
		err |= suites.ParamUtilsSuite3()->PF_UpdateParamUI(AAEData.m_InData->effect_ref, it->first, &paramCopy[it->first]);
	}

	if (effectHandle)
		err |= suites.EffectSuite2()->AEGP_DisposeEffect(effectHandle);
	return err;
}

//----------------------------------------------------------------------------

PF_Err	CPluginInterface::HandleDataFromAEGP(SAAEIOData &AAEData, PF_ParamDef *params[])
{
	(void)params;

	PF_Err			result = PF_Err_NONE;
	std::string		id;
	void			*extraData = AAEData.m_ExtraData.m_UndefinedData;

	const EEmitterEffectGenericCall *extraDataType = (EEmitterEffectGenericCall*)(extraData);

	if (*extraDataType == EEmitterEffectGenericCall::EmitterDesc)
	{
		SEmitterDesc	*desc = reinterpret_cast<SEmitterDesc*>(extraData);

		if (desc)
		{
			if (desc->m_IsDeleted)
			{
				if (GetEffectSequenceUID(AAEData, id) == false)
					return false;
				if (m_EffectData.count(id) != 0)
				{
					SEffectData	*effectData = m_EffectData[id];
					delete effectData;
					m_EffectData.erase(id);
				}
				delete desc;
				return PF_Err_NONE;
			}
			else
			{
				desc->m_Update = false;


				size_t extensionIdx = desc->m_Name.find_last_of('.');
				if (extensionIdx != std::string::npos)
					desc->m_Name = desc->m_Name.substr(0, extensionIdx) + ".pkfx";
				result |= SetParameterStreamName(AAEData, desc->m_Name, m_ParametersIndexes[Effect_Parameters_Path]); //_UpdateEmitterName(AAEData, desc);
				result |= SetParameterStreamName(AAEData, desc->m_BackdropMesh.m_Path, m_ParametersIndexes[Effect_Parameters_BackdropMesh_Path]); //_UpdateBackdropMeshPath(AAEData, desc);
			}
			AAEData.m_OutData->out_flags |= PF_OutFlag_FORCE_RERENDER;
			AAEData.m_OutData->out_flags |= PF_OutFlag_REFRESH_UI;
		}
	}
	else if (*extraDataType == EEmitterEffectGenericCall::GetEmitterInfos)
	{
		SGetEmitterInfos	*infoToFill = reinterpret_cast<SGetEmitterInfos*>(extraData);

		if (GetEffectSequenceUID(AAEData, id) == false)
			return false;
		if (m_EffectData.count(id) != 0)
		{
			strncpy(infoToFill->m_Name, m_EffectData[id]->m_Desc->m_Name.c_str(), 1023);
			infoToFill->m_Name[1023] = '\0';
			strncpy(infoToFill->m_PathSource, m_EffectData[id]->m_Desc->m_PathSource.c_str(), 1023);
			infoToFill->m_PathSource[1023] = '\0';
		}
	}
	return result;
}

//----------------------------------------------------------------------------

bool	CPluginInterface::_RegisterEffectInstancePlugin(SAAEIOData &AAEData, PF_ParamDef *params[], SEffectSequenceDataFlat *sequenceData)
{
	(void)params;

	std::string		id;

	if (sequenceData != nullptr)
		id = sequenceData->m_EffectUUID;
	else if (GetEffectSequenceUID(AAEData, id) == false)
		return false;

	if (m_EffectData.count(id) == 0)
	{
		//Check if effect is valid
		{
			PF_Err					result = PF_Err_NONE;
			AEGP_SuiteHandler		suites(AAEData.m_InData->pica_basicP);
			AEGP_EffectRefH			effectHandle = nullptr;
			AEGP_LayerH				layerH;
			bool					discard = false;

			result |= suites.PFInterfaceSuite1()->AEGP_GetNewEffectForEffect(m_AAEID, AAEData.m_InData->effect_ref, &effectHandle);
			result |= suites.PFInterfaceSuite1()->AEGP_GetEffectLayer(AAEData.m_InData->effect_ref, &layerH);

			if (result != PF_Err_NONE || effectHandle == nullptr || layerH == nullptr)
				discard = true;
			if (effectHandle)
				result |= suites.EffectSuite2()->AEGP_DisposeEffect(effectHandle);
			if (discard == true)
				return false;
		}
		SEffectData	*effectData = new SEffectData{};
		effectData->m_Desc = new SEmitterDesc{};
		m_EffectData[id] = effectData;

		if (sequenceData != nullptr)
		{
			PF_Err					result = PF_Err_NONE;
			AEGP_SuiteHandler		suites(AAEData.m_InData->pica_basicP);
			AEGP_LayerH				layerH;
			A_long					dstID = 0;

			result |= suites.PFInterfaceSuite1()->AEGP_GetEffectLayer(AAEData.m_InData->effect_ref, &layerH);
			result |= suites.LayerSuite8()->AEGP_GetLayerID(layerH, &dstID);

			effectData->m_Desc->m_UUID = std::to_string(dstID);
			effectData->m_Desc->m_LayerID = dstID;
			if (sequenceData->m_EffectName)
				effectData->m_Desc->m_Name = sequenceData->m_EffectName;
			if (sequenceData->m_EffectPathSource)
				effectData->m_Desc->m_PathSource = sequenceData->m_EffectPathSource;
			if (sequenceData->m_EffectBackdropMeshPath)
				effectData->m_Desc->m_BackdropMesh.m_Path = sequenceData->m_EffectBackdropMeshPath;
			if (sequenceData->m_EffectEnvironmentMapPath)
				effectData->m_Desc->m_BackdropEnvironmentMap.m_Path = sequenceData->m_EffectEnvironmentMapPath;
			AEFX_SuiteScoper<PopcornFXSuite1> PopcornFXSuite = AEFX_SuiteScoper<PopcornFXSuite1>(	AAEData.m_InData, kPopcornFXSuite1, kPopcornFXSuiteVersion1, AAEData.m_OutData, "PopcornFX suite was not found.");
			PopcornFXSuite->HandleNewEmitterEvent(AAEData, effectData->m_Desc);

			AAEData.m_OutData->out_flags |= PF_OutFlag_REFRESH_UI;
		}
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CPluginInterface::_UnRegisterEffectInstancePlugin(SAAEIOData &AAEData, PF_ParamDef *params[], SEffectSequenceDataFlat *sequenceData)
{
	(void)params;

	std::string		id;

	if (sequenceData != nullptr)
		id = sequenceData->m_EffectUUID;
	else if (GetEffectSequenceUID(AAEData, id) == false)
		return false;

	if (m_EffectData.count(id) != 0)
	{
		SEffectData	*effectData = m_EffectData[id];

		AEFX_SuiteScoper<PopcornFXSuite1> PopcornFXSuite = AEFX_SuiteScoper<PopcornFXSuite1>(AAEData.m_InData, kPopcornFXSuite1, kPopcornFXSuiteVersion1, AAEData.m_OutData, "PopcornFX suite was not found.");
		PopcornFXSuite->HandleDeleteEmitterEvent(AAEData, effectData->m_Desc);

		delete effectData->m_Desc;
		delete effectData;

		m_EffectData.erase(id);
	}
	return true;
}

//----------------------------------------------------------------------------

__AAEPK_END
