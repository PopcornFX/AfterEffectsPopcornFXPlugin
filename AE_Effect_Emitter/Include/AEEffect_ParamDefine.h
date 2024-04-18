//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#include <ae_precompiled.h>
#include <A.h>

__AAEPK_BEGIN

//----------------------------------------------------------------------------

enum	EStrIDType
{
	StrID_None_Param_Name,
	StrID_Name_Param_Name,
	StrID_Description_Param_Name,

	StrID_Generic_Infernal_Autorender_Param_Name,

	StrID_Parameters_Path,
	StrID_Parameters_Path_Button,

	StrID_Parameters_Path_Reimport,
	StrID_Parameters_Path_Reimport_Button,

	StrID_Generic_Infernal_Effect_Path_Hash,

	StrID_Topic_Transform_Start,
	StrID_Parameters_Position,
	StrID_Parameters_Rotation_X,
	StrID_Parameters_Rotation_Y,
	StrID_Parameters_Rotation_Z,

	StrID_Parameters_Seed,

	StrID_Topic_Rendering_Start,

	StrID_Parameters_Render_Background_Toggle,
	StrID_Parameters_Render_Background_Opacity,

	StrID_Parameters_Render_Type,
	StrID_Parameters_Render_Type_Combobox,

	StrID_Topic_Camera_Start,
	StrID_Parameters_Camera,
	StrID_Parameters_Camera_Combobox,
	StrID_Parameters_Camera_Position,
	StrID_Parameters_Camera_Rotation_X,
	StrID_Parameters_Camera_Rotation_Y,
	StrID_Parameters_Camera_Rotation_Z,
	StrID_Parameters_Camera_FOV,
	StrID_Parameters_Camera_Near,
	StrID_Parameters_Camera_Far,

	StrID_Parameters_Receive_Light,

	StrID_Topic_PostFX_Start,

	StrID_Topic_Distortion_Start,
	StrID_Parameters_Distortion_Enable,

	StrID_Topic_Bloom_Start,
	StrID_Parameters_Bloom_Enable,
	StrID_Parameters_Bloom_BrightPassValue,
	StrID_Parameters_Bloom_Intensity,
	StrID_Parameters_Bloom_Attenuation,
	StrID_Parameters_Bloom_GaussianBlur,
	StrID_Parameters_Bloom_GaussianBlur_Combobox,
	StrID_Parameters_Bloom_RenderPassCount,

	StrID_Topic_ToneMapping_Start,
	StrID_Parameters_ToneMapping_Enable,
	StrID_Parameters_ToneMapping_Saturation,
	StrID_Parameters_ToneMapping_Exposure,

	StrID_Topic_FXAA_Start,
	StrID_Parameters_FXAA_Enable,

	StrID_Topic_BackdropMesh_Start,

	StrID_Parameters_BackdropMesh_Enable_Rendering,
	StrID_Parameters_BackdropMesh_Enable_Collisions,
	StrID_Parameters_BackdropMesh_Path,
	StrID_Parameters_BackdropMesh_Path_Button,

	StrID_Parameters_BackdropMesh_Reset,
	StrID_Parameters_BackdropMesh_Reset_Button,

	StrID_Topic_BackdropMesh_Transform_Start,
	StrID_Parameters_BackdropMesh_Position,
	StrID_Parameters_BackdropMesh_Rotation_X,
	StrID_Parameters_BackdropMesh_Rotation_Y,
	StrID_Parameters_BackdropMesh_Rotation_Z,
	StrID_Parameters_BackdropMesh_Scale_X,
	StrID_Parameters_BackdropMesh_Scale_Y,
	StrID_Parameters_BackdropMesh_Scale_Z,
	StrID_Parameters_BackdropMesh_Roughness,
	StrID_Parameters_BackdropMesh_Metalness,

	StrID_Topic_BackdropAudio_Start,
	StrID_Parameters_Audio,

	StrID_Topic_BackdropEnvMap_Start,
	StrID_Parameters_BackdropEnvMap_Enable_Rendering,
	StrID_Parameters_BackdropEnvMap_Path,
	StrID_Parameters_BackdropEnvMap_Path_Button,
	StrID_Parameters_BackdropEnvMap_Reset,
	StrID_Parameters_BackdropEnvMap_Reset_Button,
	StrID_Parameters_BackdropEnvMap_Intensity,
	StrID_Parameters_BackdropEnvMap_Color,

	StrID_Topic_Light_Start,
	StrID_Parameters_Light_Category,
	StrID_Parameters_Light_Combobox,
	StrID_Parameters_Light_Direction,
	StrID_Parameters_Light_Intensity,
	StrID_Parameters_Light_Color,
	StrID_Parameters_Light_Ambient,

	StrID_Parameters_Scale_Factor,
	StrID_Parameters_Refresh_Render,
	StrID_Parameters_Render_Seeking_Toggle,
	StrID_Parameters_Simulation_State,

	StrID_Parameters_BackdropMesh_Enable_Animation,

	StrID_Parameters_Path_Marketplace,
	StrID_Parameters_Path_Marketplace_Button,

	StrID_Parameters_TransformType,
	StrID_Parameters_TransformType_Combobox,

	StrID_Parameters_Position_2D,
	StrID_Parameters_Position_2D_Distance,

	StrID_Parameters_BringEffectIntoView,
	StrID_Parameters_BringEffectIntoView_Button,

	StrID_num_types
};

//----------------------------------------------------------------------------

char	*GetParamsStringPtr(int strNum);

//----------------------------------------------------------------------------

__AAEPK_END
