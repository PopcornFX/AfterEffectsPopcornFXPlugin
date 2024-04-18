//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef	__FX_AEATTRIBUTESAMPLER_PARAM_DEFINE_H__
#define	__FX_AEATTRIBUTESAMPLER_PARAM_DEFINE_H__

#include <ae_precompiled.h>
#include <A.h>

#include <PopcornFX_Suite.h>

__AAEPK_BEGIN


//----------------------------------------------------------------------------

enum EPKParams
{
	ATTRIBUTESAMPLER_INPUT = 0,
	ATTRIBUTESAMPLER_PARAM,
	ATTRIBUTESAMPLER_NUM_PARAMS
};

//----------------------------------------------------------------------------

enum	EStrIDType
{
	StrID_NONE = 0,
	StrID_Name,
	StrID_Description,
	StrID_Generic_Infernal_Uuid,
	StrID_Generic_Infernal_Name,
	StrID_Parameters_Shapes,
	StrID_Parameters_Shapes_Combobox,

	StrID_Topic_Shape_Start,

	StrID_Topic_Shape_Box_Start,
	StrID_Parameters_Box_Size_X,
	StrID_Parameters_Box_Size_Y,
	StrID_Parameters_Box_Size_Z,

	StrID_Topic_Shape_Sphere_Start,
	StrID_Parameters_Sphere_Radius,
	StrID_Parameters_Sphere_InnerRadius,

	StrID_Topic_Shape_Ellipsoid_Start,
	StrID_Parameters_Ellipsoid_Radius,
	StrID_Parameters_Ellipsoid_InnerRadius,

	StrID_Topic_Shape_Cylinder_Start,
	StrID_Parameters_Cylinder_Radius,
	StrID_Parameters_Cylinder_Height,
	StrID_Parameters_Cylinder_InnerRadius,

	StrID_Topic_Shape_Capsule_Start,
	StrID_Parameters_Capsule_Radius,
	StrID_Parameters_Capsule_Height,
	StrID_Parameters_Capsule_InnerRadius,

	StrID_Topic_Shape_Cone_Start,
	StrID_Parameters_Cone_Radius,
	StrID_Parameters_Cone_Height,

	StrID_Topic_Shape_Mesh_Start,
	StrID_Parameters_Mesh_Scale,

	StrID_Parameters_Mesh_Path,
	StrID_Parameters_Mesh_Path_Button,

	StrID_Parameters_Mesh_Bind_Backdrop,
	StrID_Parameters_Mesh_Bind_Backdrop_Weight_Enabled,
	StrID_Parameters_Mesh_Bind_Backdrop_ColorStreamID,
	StrID_Parameters_Mesh_Bind_Backdrop_WeightStreamID,

	StrID_Parameters_Layer_Pick,

	StrID_Parameters_Layer_Sample_Once,
	StrID_Parameters_Layer_Sample_Seeking,

	StrID_Parameters_VectorField_Path,
	StrID_Parameters_VectorField_Path_Button,
	StrID_Parameters_VectorField_Strength,
	StrID_Parameters_VectorField_Position,
	StrID_Parameters_VectorField_Interpolation,
	StrID_Parameters_VectorField_Interpolation_Combobox,

	StrID_Parameters_Layer_Sample_Downsampling_X,
	StrID_Parameters_Layer_Sample_Downsampling_Y,

	StrID_NUMTYPES
};

//----------------------------------------------------------------------------

__AAEPK_END

//----------------------------------------------------------------------------

bool	GetParamsVisibility(int num, AAePk::EAttributeSamplerType type);

#endif

