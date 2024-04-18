//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"

#include "AEEffect_ParamDefine.h"

__AAEPK_BEGIN

//----------------------------------------------------------------------------

struct	STableString
{
	A_u_long	index;
	A_char		str[256];
};

//----------------------------------------------------------------------------

STableString		g_strs_params[StrID_num_types] =
{
	{ StrID_None_Param_Name,							"" },
	{ StrID_Name_Param_Name,							"Emitter" },
	{ StrID_Description_Param_Name,						"PopcornFX Emitter plugin" },

	{ StrID_Generic_Infernal_Autorender_Param_Name,		"Internal Autorender" },

	{ StrID_Parameters_Path,							"" },
	{ StrID_Parameters_Path_Button,						"Browse local files" },

	{ StrID_Parameters_Path_Reimport,					"" },
	{ StrID_Parameters_Path_Reimport_Button,			"Reimport" },

	{ StrID_Generic_Infernal_Effect_Path_Hash,			"Internal - Param Path Hash" },

	{ StrID_Topic_Transform_Start,						"Transform" },
	{ StrID_Parameters_Position,						"Position" },
	{ StrID_Parameters_Rotation_X,						"Rotation X" },
	{ StrID_Parameters_Rotation_Y,						"Rotation Y" },
	{ StrID_Parameters_Rotation_Z,						"Rotation Z" },

	{ StrID_Parameters_Seed,							"Seed" },

	{ StrID_Topic_Rendering_Start,						"Rendering" },

	{ StrID_Parameters_Render_Background_Toggle,		"Override background opacity" },
	{ StrID_Parameters_Render_Background_Opacity,		"Background opacity" },
	{ StrID_Parameters_Render_Type,						"Render Output" },
	{ StrID_Parameters_Render_Type_Combobox,			"Final|Emissive(Not impld)|Albedo(Not impld)|Normal|Depth" },

	{ StrID_Topic_Camera_Start,							"Camera Options" },
	{ StrID_Parameters_Camera,							"Camera" },
	{ StrID_Parameters_Camera_Combobox,					"Default Composition|Custom Composition(Not impld)" },
	{ StrID_Parameters_Camera_Position,					"Position" },
	{ StrID_Parameters_Camera_Rotation_X,				"Rotation X" },
	{ StrID_Parameters_Camera_Rotation_Y,				"Rotation Y" },
	{ StrID_Parameters_Camera_Rotation_Z,				"Rotation Z" },
	{ StrID_Parameters_Camera_FOV,						"FOV" },
	{ StrID_Parameters_Camera_Near,						"Near" },
	{ StrID_Parameters_Camera_Far,						"Far" },

	{ StrID_Parameters_Receive_Light,					"Receive Light(Not impld)" },

	{ StrID_Topic_PostFX_Start,							"Post Effects" },

	{ StrID_Topic_Distortion_Start,						"Distortion" },
	{ StrID_Parameters_Distortion_Enable,				"Enable" },

	{ StrID_Topic_Bloom_Start,							"Bloom" },
	{ StrID_Parameters_Bloom_Enable,					"Enable" },
	{ StrID_Parameters_Bloom_BrightPassValue,			"Bright Pass Value" },
	{ StrID_Parameters_Bloom_Intensity,					"Intensity" },
	{ StrID_Parameters_Bloom_Attenuation,				"Attenuation" },
	{ StrID_Parameters_Bloom_GaussianBlur,				"Blur Pixel Radius" },
	{ StrID_Parameters_Bloom_GaussianBlur_Combobox,		"5|9|13" },
	{ StrID_Parameters_Bloom_RenderPassCount,			"Render Pass Count" },

	{ StrID_Topic_ToneMapping_Start,					"Tone Mapping" },
	{ StrID_Parameters_ToneMapping_Enable,				"Enable" },
	{ StrID_Parameters_ToneMapping_Saturation,			"Saturation" },
	{ StrID_Parameters_ToneMapping_Exposure,			"Exposure" },

	{ StrID_Topic_FXAA_Start,							"FXAA" },
	{ StrID_Parameters_FXAA_Enable,						"Enable" },

	{ StrID_Topic_BackdropMesh_Start,					"Backdrop Mesh" },

	{ StrID_Parameters_BackdropMesh_Enable_Rendering,	"Enable Rendering" },
	{ StrID_Parameters_BackdropMesh_Enable_Collisions,	"Enable Collisions" },
	{ StrID_Parameters_BackdropMesh_Path,				"Mesh Path" },
	{ StrID_Parameters_BackdropMesh_Path_Button,		"Browse" },
	{ StrID_Parameters_BackdropMesh_Reset,				"" },
	{ StrID_Parameters_BackdropMesh_Reset_Button,		"Reset" },

	{ StrID_Topic_BackdropMesh_Transform_Start,			"Transform" },
	{ StrID_Parameters_BackdropMesh_Position,			"Position" },
	{ StrID_Parameters_BackdropMesh_Rotation_X,			"Rotation X" },
	{ StrID_Parameters_BackdropMesh_Rotation_Y,			"Rotation Y" },
	{ StrID_Parameters_BackdropMesh_Rotation_Z,			"Rotation Z" },
	{ StrID_Parameters_BackdropMesh_Scale_X,			"Scale X" },
	{ StrID_Parameters_BackdropMesh_Scale_Y,			"Scale Y" },
	{ StrID_Parameters_BackdropMesh_Scale_Z,			"Scale Z" },
	{ StrID_Parameters_BackdropMesh_Roughness,			"Roughness" },
	{ StrID_Parameters_BackdropMesh_Metalness,			"Metalness" },

	{ StrID_Topic_BackdropAudio_Start,					"Backdrop Audio" },
	{ StrID_Parameters_Audio,							"Audio Layer" },

	{ StrID_Topic_BackdropEnvMap_Start,					"Environment Map" },
	{ StrID_Parameters_BackdropEnvMap_Enable_Rendering,	"Enable Rendering" },
	{ StrID_Parameters_BackdropEnvMap_Path,				"Map Path" },
	{ StrID_Parameters_BackdropEnvMap_Path_Button,		"Browse" },
	{ StrID_Parameters_BackdropEnvMap_Reset,			"" },
	{ StrID_Parameters_BackdropEnvMap_Reset_Button,		"Reset" },
	{ StrID_Parameters_BackdropEnvMap_Intensity,		"Intensity" },
	{ StrID_Parameters_BackdropEnvMap_Color,			"Color" },

	{ StrID_Topic_Light_Start,							"Light" },
	{ StrID_Parameters_Light_Category,					"Category" },
	{ StrID_Parameters_Light_Combobox,					"Internal|All in Composition" },
	{ StrID_Parameters_Light_Direction,					"Direction" },
	{ StrID_Parameters_Light_Intensity,					"Intensity" },
	{ StrID_Parameters_Light_Color,						"Color" },
	{ StrID_Parameters_Light_Ambient,					"Ambient" },

	{ StrID_Parameters_Scale_Factor,					"Effect Scale Up Factor" },
	{ StrID_Parameters_Refresh_Render,					"Internal" },
	{ StrID_Parameters_Render_Seeking_Toggle,			"Seeking Simulation" },
	{ StrID_Parameters_Simulation_State,				"Simulation state" },

	{ StrID_Parameters_BackdropMesh_Enable_Animation,	"Enable Animations" },

	{ StrID_Parameters_Path_Marketplace,				"Emitter Effect" },
	{ StrID_Parameters_Path_Marketplace_Button,			"Browse Marketplace" },

	{ StrID_Parameters_TransformType,					"Transform Type" },
	{ StrID_Parameters_TransformType_Combobox,			"3D|2D" },

	{ StrID_Parameters_Position_2D,						"Position" },
	{ StrID_Parameters_Position_2D_Distance,			"Distance To camera" },

	{ StrID_Parameters_BringEffectIntoView,				"" }, 
	{ StrID_Parameters_BringEffectIntoView_Button,		"Bring Effect to view" }, 
};

//----------------------------------------------------------------------------

A_char	*GetParamsStringPtr(int strNum)
{
	return AAePk::g_strs_params[strNum].str;
}

//----------------------------------------------------------------------------

__AAEPK_END
