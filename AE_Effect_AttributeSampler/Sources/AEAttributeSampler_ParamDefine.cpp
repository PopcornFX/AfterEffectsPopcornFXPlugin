//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"

#include "AEAttributeSampler_ParamDefine.h"

#include "PopcornFX_Suite.h"

__AAEPK_BEGIN

//----------------------------------------------------------------------------

struct	STableString
{
	A_u_long	index;
	A_char		str[256];
} ;

//----------------------------------------------------------------------------

STableString		g_strs[StrID_NUMTYPES] =
{
	{ StrID_NONE,												"" },
	{ StrID_Name,												"Attribute Sampler" },
	{ StrID_Description,										"PopcornFX Plugin." },
	{ StrID_Generic_Infernal_Uuid,								"AttributeKey" },
	{ StrID_Generic_Infernal_Name,								"AttributeSamplerName" },
	{ StrID_Parameters_Shapes,									"Geometry" },
	{ StrID_Parameters_Shapes_Combobox,							"Box|Sphere|Ellipsoid|Cylinder|Capsule|Cone|Mesh" },
	{ StrID_Topic_Shape_Start,									"Shapes Properties" },
	{ StrID_Topic_Shape_Box_Start,								"Box" },
	{ StrID_Parameters_Box_Size_X,								"Width" },
	{ StrID_Parameters_Box_Size_Y,								"Height" },
	{ StrID_Parameters_Box_Size_Z,								"Depth" },
	{ StrID_Topic_Shape_Sphere_Start,							"Sphere" },
	{ StrID_Parameters_Sphere_Radius,							"Radius" },
	{ StrID_Parameters_Sphere_InnerRadius,						"Inner Radius" },
	{ StrID_Topic_Shape_Ellipsoid_Start,						"Complex Ellipsoid" },
	{ StrID_Parameters_Ellipsoid_Radius,						"Radius" },
	{ StrID_Parameters_Ellipsoid_InnerRadius,					"Inner Radius" },
	{ StrID_Topic_Shape_Cylinder_Start,							"Cylinder" },
	{ StrID_Parameters_Cylinder_Radius,							"Radius" },
	{ StrID_Parameters_Cylinder_Height,							"Height" },
	{ StrID_Parameters_Cylinder_InnerRadius,					"Inner Radius" },
	{ StrID_Topic_Shape_Capsule_Start,							"Capsule" },
	{ StrID_Parameters_Capsule_Radius,							"Radius" },
	{ StrID_Parameters_Capsule_Height,							"Height" },
	{ StrID_Parameters_Capsule_InnerRadius,						"Inner Radius" },
	{ StrID_Topic_Shape_Cone_Start,								"Cone" },
	{ StrID_Parameters_Cone_Radius,								"Radius" },
	{ StrID_Parameters_Cone_Height,								"Height" },
	{ StrID_Topic_Shape_Mesh_Start,								"Mesh" },
	{ StrID_Parameters_Mesh_Scale,								"Scale" },
	{ StrID_Parameters_Mesh_Path,								"Mesh Path" },
	{ StrID_Parameters_Mesh_Path_Button,						"Browse" },
	{ StrID_Parameters_Mesh_Bind_Backdrop,						"Bind to Backdrop" },
	{ StrID_Parameters_Mesh_Bind_Backdrop_Weight_Enabled,		"Weight Sampling" },
	{ StrID_Parameters_Mesh_Bind_Backdrop_ColorStreamID,		"Color Stream ID" },
	{ StrID_Parameters_Mesh_Bind_Backdrop_WeightStreamID,		"Weight Stream ID" },
	{ StrID_Parameters_Layer_Pick,								"Layer" },
	{ StrID_Parameters_Layer_Sample_Once,						"Sample once" },
	{ StrID_Parameters_Layer_Sample_Seeking,					"Sample while seeking" },
	{ StrID_Parameters_VectorField_Path,						"VectorField Path" },
	{ StrID_Parameters_VectorField_Path_Button,					"Browse" },
	{ StrID_Parameters_VectorField_Strength,					"Strength" },
	{ StrID_Parameters_VectorField_Position,					"Position" },
	{ StrID_Parameters_VectorField_Interpolation,				"Interpolation Type" },
	{ StrID_Parameters_VectorField_Interpolation_Combobox,		"Point|Trilinear|Quadrilinear" },
	{ StrID_Parameters_Layer_Sample_Downsampling_X,				"Downsample X" },
	{ StrID_Parameters_Layer_Sample_Downsampling_Y,				"Downsample Y" },
};

//----------------------------------------------------------------------------

struct	STableVisibility
{
	A_u_long	index;
	bool		visibility[__AttributeSamplerType_Count];
};

//----------------------------------------------------------------------------

STableVisibility		g_attributeSamplerParamVisibility[__AttributeSamplerType_Parameters_Count] =
{
	//																		  None , AT   , Audio, Curve, Event, Geom , Img  , ImgAT, Text , Vector
	{ 0,/*Padding*/															{ false, false, false, false, false, false, false, false, false, false } },/*Padding*/
	{ AttributeSamplerType_Parameters_Infernal_Uuid,						{ false, false, false, false, false, false, false, false, false, false } },
	{ AttributeSamplerType_Parameters_Infernal_Name,						{ false, false, false, false, false, false, false, false, false, false } },
	{ AttributeSamplerType_Parameters_Shapes,								{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Topic_Shape_Start,								{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Topic_Shape_Box_Start,							{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Parameters_Box_Size_X,							{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Parameters_Box_Size_Y,							{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Parameters_Box_Size_Z,							{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Topic_Shape_Box_End,								{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Topic_Shape_Sphere_Start,						{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Parameters_Sphere_Radius,						{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Parameters_Sphere_InnerRadius,					{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Topic_Shape_Sphere_End,							{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Topic_Shape_Ellipsoid_Start,						{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Parameters_Ellipsoid_Radius,						{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Parameters_Ellipsoid_InnerRadius,				{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Topic_Shape_Ellipsoid_End,						{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Topic_Shape_Cylinder_Start,						{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Parameters_Cylinder_Radius,						{ false, false, false, false, false, TRUE,  false, false, false, false } },
	//																		  None , AT   , Audio, Curve, Event, Geom , Img  , ImgAT, Text , Vector
	{ AttributeSamplerType_Parameters_Cylinder_Height,						{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Parameters_Cylinder_InnerRadius,					{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Topic_Shape_Cylinder_End,						{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Topic_Shape_Capsule_Start,						{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Parameters_Capsule_Radius,						{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Parameters_Capsule_Height,						{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Parameters_Capsule_InnerRadius,					{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Topic_Shape_Capsule_End,							{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Topic_Shape_Cone_Start,							{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Parameters_Cone_Radius,							{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Parameters_Cone_Height,							{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Topic_Shape_Cone_End,							{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Topic_Shape_Mesh_Start,							{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Parameters_Mesh_Scale,							{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Parameters_Mesh_Path,							{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Parameters_Mesh_Bind_Backdrop,					{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Parameters_Mesh_Bind_Backdrop_Weighted_Enabled,	{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Parameters_Mesh_Bind_Backdrop_ColorStreamID,		{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Parameters_Mesh_Bind_Backdrop_WeightStreamID,	{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Topic_Shape_Mesh_End,							{ false, false, false, false, false, TRUE,  false, false, false, false } },
	{ AttributeSamplerType_Topic_Shape_End,									{ false, false, false, false, false, TRUE,  false, false, false, false } },
	//																		  None , AT   , Audio, Curve, Event, Geom , Img  , ImgAT, Text , Vector
	{ AttributeSamplerType_Layer_Pick,										{ false, false, TRUE,  false, false, false, TRUE,  false, TRUE,  false } },
	{ AttributeSamplerType_Layer_Sample_Once,								{ false, false, false, false, false, false, TRUE,  false, TRUE,  false } },
	{ AttributeSamplerType_Layer_Sample_Seeking,							{ false, false, TRUE,  false, false, false, TRUE,  false, TRUE,  false } },

	{ AttributeSamplerType_Parameters_VectorField_Path,						{ false, false, false, false, false, false, false, false, false, TRUE  } },
	{ AttributeSamplerType_Parameters_VectorField_Strength,					{ false, false, false, false, false, false, false, false, false, TRUE  } },
	{ AttributeSamplerType_Parameters_VectorField_Position,					{ false, false, false, false, false, false, false, false, false, TRUE  } },
	{ AttributeSamplerType_Parameters_VectorField_Interpolation,			{ false, false, false, false, false, false, false, false, false, TRUE  } },

	{ AttributeSamplerType_Layer_Sample_Downsampling_X,						{ false, false, false, false, false, false, TRUE,  false, false, false } },
	{ AttributeSamplerType_Layer_Sample_Downsampling_Y,						{ false, false, false, false, false, false, TRUE,  false, false, false } },
	//																		  None , AT   , Audio, Curve, Event, Geom , Img  , ImgAT, Text , Vector
};

//----------------------------------------------------------------------------

__AAEPK_END

//----------------------------------------------------------------------------

#ifdef 	__cplusplus
extern "C"
{
#endif

A_char	*GetStringPtr(int strNum)
{
	return AAePk::g_strs[strNum].str;
}

#ifdef 	__cplusplus
}
#endif

//----------------------------------------------------------------------------

bool	GetParamsVisibility(int num, AAePk::EAttributeSamplerType type)
{
	return AAePk::g_attributeSamplerParamVisibility[num].visibility[type];
}
