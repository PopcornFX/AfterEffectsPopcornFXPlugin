//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef __POPCORNFX_SUITE_H__
#define __POPCORNFX_SUITE_H__

#include "A.h"
#include <SPTypes.h>

#ifdef AE_OS_WIN
#include <windows.h>
#endif

#include <string>

//AE
#include <AE_EffectPixelFormat.h>
#include <AEFX_SuiteHelper.h>
#include <AE_EffectCBSuites.h>
#include <AE_Macros.h>
#include <AE_GeneralPlug.h>
#include <AE_Effect.h>
#include <AE_EffectCB.h>
#include <Param_Utils.h>
#include <Smart_Utils.h>

#include "PopcornFX_Define.h"

#include <math.h>
#include <mutex>
#include <vector>

//----------------------------------------------------------------------------

#define kPopcornFXSuite1				"AEGP PopcornFX Suite"
#define kPopcornFXSuiteVersion1			1

__AAEPK_BEGIN

//----------------------------------------------------------------------------

enum EPKChildPlugins
{
	EMITTER = 0,
	ATTRIBUTE,
	SAMPLER,
	_PLUGIN_COUNT
};

#pragma region Emitter

//----------------------------------------------------------------------------

enum EGaussianBlurPixelRadius
{
	GaussianBlurPixelRadius_5 = 1,
	GaussianBlurPixelRadius_9,
	GaussianBlurPixelRadius_13,
	__GaussianBlurPixelRadius_Count = GaussianBlurPixelRadius_13
};

//----------------------------------------------------------------------------

enum ELightCategory
{
	ELightCategory_Debug_Default = 1,
	ELightCategory_Compo_Default,
	ELightCategory_Compo_Custom,
	__ELightCategory_Count = ELightCategory_Compo_Custom
}; 

//----------------------------------------------------------------------------

enum ETransformType
{
	ETransformType_3D = 1,
	ETransformType_2D,
	__ETransformType_Count = ETransformType_2D
};

//----------------------------------------------------------------------------

enum ECameraType
{
	ECameraType_Compo_Default = 1,
	ECameraType_Compo_Custom,
	__ECameraType_Count = ECameraType_Compo_Custom
};

//----------------------------------------------------------------------------

enum ERenderType
{
	RenderType_FinalCompositing = 1,
	RenderType_Emissive,
	RenderType_Albedo,
	RenderType_Normal,
	RenderType_Depth,
	__RenderType_Count = RenderType_Depth
};

//----------------------------------------------------------------------------

enum EInterpolationType
{
	EInterpolationType_Point = 1,
	EInterpolationType_Trilinear,
	EInterpolationType_Quadrilinear,
	__EInterpolationType_Count = EInterpolationType_Quadrilinear
};

//----------------------------------------------------------------------------

// Add new effect parameters at the end of this enum
enum	EEffectParameterType
{
	Effect_Parameters_InputReserved = 0,

	Effect_Parameters_Infernal_Autorender,

	Effect_Parameters_Path,
	Effect_Parameters_Path_Reimport,

	Effect_Parameters_Infernal_Effect_Path_Hash,

	Effect_Topic_Transform_Start,
		Effect_Parameters_Position,
		Effect_Parameters_Rotation_X,
		Effect_Parameters_Rotation_Y,
		Effect_Parameters_Rotation_Z,
	Effect_Topic_Transform_End,

	Effect_Parameters_Seed,


	Effect_Topic_Rendering_Start,

		Effect_Parameters_Background_Toggle,
		Effect_Parameters_Background_Opacity,

		Effect_Parameters_Render_Type,						// PF_ParamFlag_CANNOT_TIME_VARY | PF_ParamFlag_CANNOT_INTERP

		Effect_Topic_Camera_Start,
			Effect_Parameters_Camera,
			Effect_Parameters_Camera_Position,
			Effect_Parameters_Camera_Rotation_X,
			Effect_Parameters_Camera_Rotation_Y,
			Effect_Parameters_Camera_Rotation_Z,
			Effect_Parameters_Camera_FOV,
			Effect_Parameters_Camera_Near,
			Effect_Parameters_Camera_Far,
		Effect_Topic_Camera_End,

		Effect_Parameters_Receive_Light,					// PF_ParamFlag_CANNOT_TIME_VARY || PF_ParamFlag_CANNOT_INTERP

		Effect_Topic_PostFX_Start,
			Effect_Topic_Distortion_Start,
				Effect_Parameters_Distortion_Enable,		// PF_ParamFlag_CANNOT_TIME_VARY || PF_ParamFlag_CANNOT_INTERP
			Effect_Topic_Distortion_End,

			Effect_Topic_Bloom_Start,
				Effect_Parameters_Bloom_Enable,				// PF_ParamFlag_CANNOT_TIME_VARY || PF_ParamFlag_CANNOT_INTERP
				Effect_Parameters_Bloom_BrightPassValue,
				Effect_Parameters_Bloom_Intensity,
				Effect_Parameters_Bloom_Attenuation,
				Effect_Parameters_Bloom_GaussianBlur,		// PF_ParamFlag_CANNOT_TIME_VARY || PF_ParamFlag_CANNOT_INTERP
				Effect_Parameters_Bloom_RenderPassCount,	// PF_ParamFlag_CANNOT_TIME_VARY || PF_ParamFlag_CANNOT_INTERP
			Effect_Topic_Bloom_End,

			Effect_Topic_ToneMapping_Start,
				Effect_Parameters_ToneMapping_Enable,		// PF_ParamFlag_CANNOT_TIME_VARY || PF_ParamFlag_CANNOT_INTERP
				Effect_Parameters_ToneMapping_Saturation,
				Effect_Parameters_ToneMapping_Exposure,
			Effect_Topic_ToneMapping_End,
		Effect_Topic_PostFX_End,

	Effect_Topic_Rendering_End,

	Effect_Topic_BackdropMesh_Start,
		Effect_Parameters_BackdropMesh_Enable_Rendering,
		Effect_Parameters_BackdropMesh_Enable_Collisions,
		Effect_Parameters_BackdropMesh_Path,
		Effect_Parameters_BackdropMesh_Reset,

		Effect_Topic_BackdropMesh_Transform_Start,
			Effect_Parameters_BackdropMesh_Position,
			Effect_Parameters_BackdropMesh_Rotation_X,
			Effect_Parameters_BackdropMesh_Rotation_Y,
			Effect_Parameters_BackdropMesh_Rotation_Z,
			Effect_Parameters_BackdropMesh_Scale_X,
			Effect_Parameters_BackdropMesh_Scale_Y,
			Effect_Parameters_BackdropMesh_Scale_Z,
		Effect_Topic_BackdropMesh_Transform_End,
		Effect_Parameters_BackdropMesh_Roughness,
		Effect_Parameters_BackdropMesh_Metalness,
	Effect_Topic_BackdropMesh_End,

	Effect_Topic_BackdropEnvMap_Start,
		Effect_Parameters_BackdropEnvMap_Enable_Rendering,
		Effect_Parameters_BackdropEnvMap_Path,
		Effect_Parameters_BackdropEnvMap_Reset,
		Effect_Parameters_BackdropEnvMap_Intensity,
		Effect_Parameters_BackdropEnvMap_Color,
	Effect_Topic_BackdropEnvMap_End,

	Effect_Topic_Light_Start,
		Effect_Parameters_Light_Category,
		Effect_Parameters_Light_Direction,
		Effect_Parameters_Light_Intensity,
		Effect_Parameters_Light_Color,
		Effect_Parameters_Light_Ambient,
	Effect_Topic_Light_End,

	Effect_Parameters_Scale_Factor,
	Effect_Parameters_Refresh_Render,

	Effect_Parameters_Seeking_Toggle,
	Effect_Parameters_Simulation_State,

	Effect_Parameters_BackdropMesh_Enable_Animation,

	Effect_Topic_FXAA_Start,
		Effect_Parameters_FXAA_Enable,		// PF_ParamFlag_CANNOT_TIME_VARY || PF_ParamFlag_CANNOT_INTERP
	Effect_Topic_FXAA_End,

	Effect_Parameters_Path_Marketplace,

	Effect_Parameters_TransformType,
	Effect_Parameters_Position_2D,
	Effect_Parameters_Position_2D_Distance,

	Effect_Topic_BackdropAudio_Start,
		Effect_Parameters_Audio,
	Effect_Topic_BackdropAudio_End,

	Effect_Parameters_BringEffectIntoView,
	__Effect_Parameters_Count
};

//----------------------------------------------------------------------------

struct	SPostFXDistortionDesc
{
	bool		m_Enable;
};

//----------------------------------------------------------------------------

struct	SPostFXBloomDesc
{
	bool						m_Enable;
	float						m_BrightPassValue;
	float						m_Intensity;
	float						m_Attenuation;
	EGaussianBlurPixelRadius	m_GaussianBlur;
	int							m_RenderPassCount;
};

//----------------------------------------------------------------------------

struct	SPostFXToneMappingDesc
{
	bool		m_Enable;

	float		m_Saturation;
	float		m_Exposure;
};

//----------------------------------------------------------------------------

struct	SPostFXAADesc
{
	bool		m_Enable;
};

//----------------------------------------------------------------------------

struct	SRenderingDesc
{
	ERenderType	m_Type;
	bool		m_ReceiveLight;

	SPostFXDistortionDesc	m_Distortion;
	SPostFXBloomDesc		m_Bloom;
	SPostFXToneMappingDesc	m_ToneMapping;
	SPostFXAADesc			m_FXAA;
};

//----------------------------------------------------------------------------

struct	SLightDesc
{
	bool					m_Internal = true;
	ELightCategory			m_Category;

	A_FloatPoint3			m_Direction;
	A_FloatPoint3			m_Position;
	A_FloatPoint3			m_Color;
	A_FloatPoint3			m_Ambient;
	float					m_Intensity;
	float					m_Angle;
	float					m_Feather;

	AEGP_LightType			m_Type;
};

//----------------------------------------------------------------------------

struct	SCameraDesc
{
	bool	m_Internal = false;
	bool	m_AECameraPresent = false;

	A_FloatPoint3		m_Position;
	A_FloatPoint3		m_Rotation;

	float				m_FOV;
	float				m_Near = 0.1f;
	float				m_Far =	1000;

};

//----------------------------------------------------------------------------

struct SBackdropMesh
{
	std::string			m_Path;

	bool				m_EnableRendering;
	bool				m_EnableCollisions;
	bool				m_EnableAnimations;

	A_FloatPoint3		m_Position;
	A_FloatPoint3		m_Rotation;
	A_FloatPoint3		m_Scale;

	float				m_Roughness;
	float				m_Metalness;
};

//----------------------------------------------------------------------------

struct SBackdropEnvironmentMap
{
	std::string			m_Path;

	bool				m_EnableRendering = false;

	float				m_Intensity;
	A_FloatPoint3		m_Color;

};

//----------------------------------------------------------------------------

enum	EEmitterEffectGenericCall : int
{
	EmitterDesc = 0,
	GetEmitterInfos
};


//----------------------------------------------------------------------------

struct	SGetEmitterInfos
{
	const EEmitterEffectGenericCall	m_Type = EEmitterEffectGenericCall::GetEmitterInfos;
	char			m_Name[1024];
	char			m_PathSource[1024];
};

//----------------------------------------------------------------------------

struct	SEmitterDesc
{
	const EEmitterEffectGenericCall	m_Type = EEmitterEffectGenericCall::EmitterDesc;
	static int			s_ID;
public:
	std::string			m_Name;
	std::string			m_PathSource;
	std::string			m_UUID;

	ETransformType		m_TransformType;
	A_FloatPoint3		m_Position;
	A_FloatPoint3		m_Rotation;

	bool				m_IsAlphaBGOverride;
	float				m_AlphaBGOverride;

	int					m_Seed = 0;

	SCameraDesc			m_Camera;
	SRenderingDesc		m_Rendering;

	SBackdropMesh				m_BackdropMesh;
	SBackdropEnvironmentMap		m_BackdropEnvironmentMap;

	SLightDesc			m_Light;

	float				m_ScaleFactor = 1;

	bool				m_Update;
	bool				m_UpdateBackdrop = true;
	bool				m_LoadBackdrop = false;
	int					m_DebugID;

	A_long				m_LayerID;
	bool				m_IsDeleted = false;
	bool				m_ReloadEffect = false;

	bool				m_SimStatePrev = true;
	bool				m_SimState = true;

	SEmitterDesc()
	{
		SEmitterDesc::s_ID++;
		m_DebugID = SEmitterDesc::s_ID;
		m_Position.x = 0;
		m_Position.y = 0;
		m_Position.z = 0;

		m_Rotation.x = 0;
		m_Rotation.y = 0;
		m_Rotation.z = 0;

		m_IsAlphaBGOverride = false;
		m_AlphaBGOverride = 1.0f;
		m_LayerID = 0;
	}
};

//----------------------------------------------------------------------------

#pragma region AttributeSampler

enum ESamplerShapeType : int
{
	SamplerShapeType_Box = 0,
	SamplerShapeType_Sphere,
	SamplerShapeType_Ellipsoid,
	SamplerShapeType_Cylinder,
	SamplerShapeType_Capsule,
	SamplerShapeType_Cone,
	SamplerShapeType_Mesh,

	SamplerShapeType_None,
	__SamplerShapeType_Count = SamplerShapeType_Mesh
};

//----------------------------------------------------------------------------

struct SBaseSamplerDescriptor
{
	uint32_t			m_UsageFlags = 0;
};

//----------------------------------------------------------------------------

struct SShapeSamplerDescriptor : public SBaseSamplerDescriptor
{
	ESamplerShapeType	m_Type;

	float				m_Dimension[3];

	std::string			m_Path;

	//Skinned Backdrop
	bool				m_BindToBackdrop;
	bool				m_WeightedSampling;
	unsigned int		m_ColorStreamID;
	unsigned int		m_WeightStreamID;

	SShapeSamplerDescriptor()
		: m_Type(SamplerShapeType_None)
		, m_BindToBackdrop(false)
		, m_WeightedSampling(false)
		, m_ColorStreamID(0)
		, m_WeightStreamID(0)

	{
		m_Dimension[0] = 1.0f;
		m_Dimension[1] = 1.0f;
		m_Dimension[2] = 1.0f;
	}
};

//----------------------------------------------------------------------------

struct STextSamplerDescriptor : public SBaseSamplerDescriptor
{
	AEGP_LayerIDVal		m_LayerID = AEGP_LayerIDVal_NONE;
	std::string			m_Data;
};

//----------------------------------------------------------------------------

struct SAudioSamplerDescriptor : public SBaseSamplerDescriptor
{
	AEGP_LayerIDVal		m_LayerID = AEGP_LayerIDVal_NONE;

	std::string			m_ChannelGroup = "";
};

//----------------------------------------------------------------------------

struct SImageSamplerDescriptor : public SBaseSamplerDescriptor
{
	AEGP_LayerIDVal		m_LayerID = AEGP_LayerIDVal_NONE;
};

//----------------------------------------------------------------------------

struct SVectorFieldSamplerDescriptor : public SBaseSamplerDescriptor
{
	std::string			m_Path;

	EInterpolationType	m_Interpolation;
	float				m_Strength;
	A_FloatPoint3		m_Position;

	bool				m_ResourceUpdate;
};

//----------------------------------------------------------------------------

enum	EAttributeSamplerType : int
{
	AttributeSamplerType_None = 0,
	AttributeSamplerType_Animtrack,
	AttributeSamplerType_Audio,
	AttributeSamplerType_Curve,
	AttributeSamplerType_EventStream,
	AttributeSamplerType_Geometry,
	AttributeSamplerType_Image,
	AttributeSamplerType_ImageAtlas,
	AttributeSamplerType_Grid,
	AttributeSamplerType_Text,
	AttributeSamplerType_VectorField,
	__AttributeSamplerType_Count,
};

//----------------------------------------------------------------------------

enum	EAttributeSamplerParameterType : int
{
	AttributeSamplerType_Parameters_Infernal_Uuid = 1,
	AttributeSamplerType_Parameters_Infernal_Name,
	AttributeSamplerType_Parameters_Shapes,

	AttributeSamplerType_Topic_Shape_Start,

	AttributeSamplerType_Topic_Shape_Box_Start,
	AttributeSamplerType_Parameters_Box_Size_X,
	AttributeSamplerType_Parameters_Box_Size_Y,
	AttributeSamplerType_Parameters_Box_Size_Z,
	AttributeSamplerType_Topic_Shape_Box_End,

	AttributeSamplerType_Topic_Shape_Sphere_Start,
	AttributeSamplerType_Parameters_Sphere_Radius,
	AttributeSamplerType_Parameters_Sphere_InnerRadius,
	AttributeSamplerType_Topic_Shape_Sphere_End,

	AttributeSamplerType_Topic_Shape_Ellipsoid_Start,
	AttributeSamplerType_Parameters_Ellipsoid_Radius,
	AttributeSamplerType_Parameters_Ellipsoid_InnerRadius,
	AttributeSamplerType_Topic_Shape_Ellipsoid_End,

	AttributeSamplerType_Topic_Shape_Cylinder_Start,
	AttributeSamplerType_Parameters_Cylinder_Radius,
	AttributeSamplerType_Parameters_Cylinder_Height,
	AttributeSamplerType_Parameters_Cylinder_InnerRadius,
	AttributeSamplerType_Topic_Shape_Cylinder_End,

	AttributeSamplerType_Topic_Shape_Capsule_Start,
	AttributeSamplerType_Parameters_Capsule_Radius,
	AttributeSamplerType_Parameters_Capsule_Height,
	AttributeSamplerType_Parameters_Capsule_InnerRadius,
	AttributeSamplerType_Topic_Shape_Capsule_End,

	AttributeSamplerType_Topic_Shape_Cone_Start,
	AttributeSamplerType_Parameters_Cone_Radius,
	AttributeSamplerType_Parameters_Cone_Height,
	AttributeSamplerType_Topic_Shape_Cone_End,

	AttributeSamplerType_Topic_Shape_Mesh_Start,
	AttributeSamplerType_Parameters_Mesh_Scale,
	AttributeSamplerType_Parameters_Mesh_Path,
	AttributeSamplerType_Parameters_Mesh_Bind_Backdrop,
	AttributeSamplerType_Parameters_Mesh_Bind_Backdrop_Weighted_Enabled,
	AttributeSamplerType_Parameters_Mesh_Bind_Backdrop_ColorStreamID,
	AttributeSamplerType_Parameters_Mesh_Bind_Backdrop_WeightStreamID,
	AttributeSamplerType_Topic_Shape_Mesh_End,

	AttributeSamplerType_Topic_Shape_End,

	AttributeSamplerType_Layer_Pick,

	AttributeSamplerType_Layer_Sample_Once,
	AttributeSamplerType_Layer_Sample_Seeking,

	AttributeSamplerType_Parameters_VectorField_Path,
	AttributeSamplerType_Parameters_VectorField_Strength,
	AttributeSamplerType_Parameters_VectorField_Position,
	AttributeSamplerType_Parameters_VectorField_Interpolation,

	AttributeSamplerType_Layer_Sample_Downsampling_X,
	AttributeSamplerType_Layer_Sample_Downsampling_Y,

	__AttributeSamplerType_Parameters_Count
};

//----------------------------------------------------------------------------

struct	SAttributeBaseDesc
{
	unsigned int				m_Order;
	bool						m_IsAttribute;

	std::string					m_Name;
	std::string					m_CategoryName;

	bool						m_IsDeleted = false;

	SAttributeBaseDesc()
		: m_Order(0)
		, m_IsAttribute(false)
		, m_Name("")
		, m_CategoryName("")
	{

	}

	SAttributeBaseDesc(const SAttributeBaseDesc &other)
		: m_Order(other.m_Order)
		, m_Name(other.m_Name)
		, m_CategoryName(other.m_CategoryName)
		, m_IsDeleted(other.m_IsDeleted)
	{
	}

	SAttributeBaseDesc	&operator=(const SAttributeBaseDesc &other)
	{
		m_Name = other.m_Name;
		m_CategoryName = other.m_CategoryName;

		return *this;
	}

	std::string	GetAttributePKKey()
	{
		std::string key = "";
		if (m_CategoryName.length() != 0)
		{
			key += m_CategoryName;
			key += ":";
		}
		key += m_Name;
		return key;
	}
};

//----------------------------------------------------------------------------

struct	SAttributeSamplerDesc : public SAttributeBaseDesc
{
public:
	EAttributeSamplerType		m_Type;
	SBaseSamplerDescriptor		*m_Descriptor = nullptr;
	std::string					m_ResourcePath = "";
	bool						m_IsDefaultValue = true;

	SAttributeSamplerDesc(const char* name = nullptr, const char* category = nullptr, EAttributeSamplerType type = AttributeSamplerType_None)
		: m_Type(type)
	{
		m_Name = name == nullptr ? "" : name;
		m_CategoryName = category == nullptr ? "" : category;
	}

	SAttributeSamplerDesc(const SAttributeSamplerDesc &other)
		: SAttributeBaseDesc(other)
		, m_Type(other.m_Type)
		, m_Descriptor(other.m_Descriptor)
		, m_IsDefaultValue(other.m_IsDefaultValue)
	{
		if(m_ResourcePath.length() == 0)
			m_ResourcePath = (other.m_ResourcePath);
	}

	SAttributeSamplerDesc	&operator=(const SAttributeSamplerDesc &other)
	{
		m_Name = other.m_Name;
		m_CategoryName = other.m_CategoryName;
		m_Type = other.m_Type;
		m_Descriptor = other.m_Descriptor;
		m_IsDeleted = other.m_IsDeleted;
		m_IsDefaultValue = other.m_IsDefaultValue;

		if (m_ResourcePath.length() == 0)
			m_ResourcePath = (other.m_ResourcePath);

		return *this;
	}
};

#pragma endregion

//----------------------------------------------------------------------------
#pragma region Attribute

enum	EAttributeType : int
{
	AttributeType_None = 0,
	AttributeType_Bool1,
	AttributeType_Bool2,
	AttributeType_Bool3,
	AttributeType_Bool4,
	AttributeType_Int1,
	AttributeType_Int2,
	AttributeType_Int3,
	AttributeType_Int4,
	AttributeType_Float1,
	AttributeType_Float2,
	AttributeType_Float3,
	AttributeType_Float4,
	__AttributeType_Count,
};

//----------------------------------------------------------------------------

enum	EAttributeParameterType : int
{
	Attribute_Parameters_Bool1 = 1,
	Attribute_Parameters_Bool2,
	Attribute_Parameters_Bool3,
	Attribute_Parameters_Bool4,
	Attribute_Parameters_Int1,
	Attribute_Parameters_Int2,
	Attribute_Parameters_Int3,
	Attribute_Parameters_Int4,
	Attribute_Parameters_Float1,
	Attribute_Parameters_Float2,
	Attribute_Parameters_Float3,
	Attribute_Parameters_Float4,
	Attribute_Parameters_Infernal_Uuid,
	Attribute_Parameters_AffectedByScale,
	Attribute_Parameters_Infernal_Name,
	Attribute_Parameters_Color_RGB,
	Attribute_Parameters_Color_A,
	Attribute_Parameters_Reset,
	__Attribute_Parameters_Count
};

//----------------------------------------------------------------------------

enum	EAttributeSemantic : int
{
	AttributeSemantic_None = 0,
	AttributeSemantic_Coordinate,
	AttributeSemantic_Scale,
	AttributeSemantic_Color,
	__AttributeSemantic_Count
};

//----------------------------------------------------------------------------

union	UAttributeValue
{
	void			*m_Invalid;
	bool			m_Bool4[4];
	int				m_Int4[4];
	float			m_Float4[4];
	float			m_Angles[3];
};

//----------------------------------------------------------------------------

struct	SAttributeDesc : public SAttributeBaseDesc
{
public:
	EAttributeType		m_Type;
	EAttributeSemantic	m_AttributeSemantic;

	UAttributeValue		m_DefaultValue;

	UAttributeValue		m_MinValue;
	UAttributeValue		m_MaxValue;
	
	bool				m_HasMax = false;
	bool				m_HasMin = false;

	UAttributeValue		m_Value;
	bool				m_IsValid;
	bool				m_IsDirty;

	std::mutex			m_Lock;
public:
	bool				m_IsDefaultValue = true;
	bool				m_IsAffectedByScale = false;

	SAttributeDesc(const char* name = nullptr, const char* category = nullptr, EAttributeType type = AttributeType_None, EAttributeSemantic semantic = EAttributeSemantic::AttributeSemantic_None)
		: m_Type(type)
		, m_AttributeSemantic(semantic)
		, m_DefaultValue()
		, m_MinValue()
		, m_MaxValue()
		, m_Value()
		, m_IsValid(true)
		, m_IsDirty(true)
	{
		m_IsAttribute = true;
		m_Name = name == nullptr ? "" : name;
		m_CategoryName = category == nullptr ? "" : category;
	}

	SAttributeDesc(const SAttributeDesc &other)
		: SAttributeBaseDesc(other)
		, m_Type(other.m_Type)
		, m_AttributeSemantic(other.m_AttributeSemantic)
		, m_DefaultValue(other.m_DefaultValue)
		, m_MinValue(other.m_MinValue)
		, m_MaxValue(other.m_MaxValue)
		, m_HasMax(other.m_HasMax)
		, m_HasMin(other.m_HasMin)
		, m_Value(other.m_Value)
		, m_IsValid(other.m_IsValid)
		, m_IsDirty(true)
		, m_IsDefaultValue(other.m_IsDefaultValue)
	{

	}

	~SAttributeDesc()
	{

	}

	SAttributeDesc& operator=(const SAttributeDesc &other)
	{
		m_Name = other.m_Name;
		m_CategoryName = other.m_CategoryName;
		m_Type = other.m_Type;
		m_AttributeSemantic = other.m_AttributeSemantic;

		m_HasMin = other.m_HasMin;
		m_HasMax = other.m_HasMax;

		m_MinValue = other.m_MinValue;
		m_MaxValue = other.m_MaxValue;

		if (m_IsDefaultValue == true)
		{
			m_Value = other.m_Value;
			m_IsDefaultValue = other.m_IsDefaultValue;
		}
		m_DefaultValue = other.m_DefaultValue;
		m_IsDirty = true;
		m_IsValid = other.m_IsValid;
		m_IsDeleted = other.m_IsDeleted;
		return *this;
	}

	void		Invalidate()
	{
		m_IsValid = false;
	}

	bool		isValid()
	{
		return m_IsValid;
	}

	bool		ResetValues()
	{
		m_Lock.lock();
		memcpy(&m_Value, &m_DefaultValue, sizeof(UAttributeValue));
		m_IsDirty = true;
		m_IsDefaultValue = true;
		m_Lock.unlock();

		return true;
	}

	template<typename T>
	void		SetDefaultValue(T *value)
	{
		return _SetValueImpl(value, m_DefaultValue, true);
	}

	template<typename T>
	void		GetDefaultValue(T *value)
	{
		return _GetValueImpl(value, m_DefaultValue);
	}

	template<typename T>
	void		GetDefaultValueByType(EAttributeType type, T *value)
	{
		_GetValueByTypeImpl(type, value, m_DefaultValue);
	}

	template<typename T>
	void		SetValue(T *value)
	{
		return _SetValueImpl(value, m_Value);
	}

	template<typename T>
	void		GetValue(T *value)
	{
		return _GetValueImpl(value, m_Value);
	}

	template<typename T>
	void		GetValueByType(EAttributeType type, T *value)
	{
		_GetValueByTypeImpl(type, value, m_Value);
	}

	void		*GetRawValue()
	{
		return _GetRawValueImpl(m_Value);
	}

	void		GetValueAsStreamValue(EAttributeType type, AEGP_StreamValue *value)
	{
		_GetValueAsStreamValueImpl(type, value, m_Value);
	}

	template<typename T>
	void		SetMinValue(T *value)
	{
		return _SetValueImpl(value, m_MinValue);
	}

	template<typename T>
	void		GetMinValueByType(EAttributeType type, T *value)
	{
		_GetValueByTypeImpl(type, value, m_MinValue);
	}

	template<typename T>
	void		SetMaxValue(T *value)
	{
		return _SetValueImpl(value, m_MaxValue);
	}

	template<typename T>
	void		GetMaxValueByType(EAttributeType type, T *value)
	{
		_GetValueByTypeImpl(type, value, m_MaxValue);
	}

	void		Reset()
	{
		m_IsDirty = true;
		m_IsValid = false;
		m_Value = {};
		m_Name = "";
		m_CategoryName = "";
		m_Type = AttributeType_None;
		m_AttributeSemantic = EAttributeSemantic::AttributeSemantic_None;
	}
private:
	template<typename T>
	void		_SetValueImpl(T *value, UAttributeValue &field, bool isDefault = false)
	{
		m_Lock.lock();
		switch (m_Type)
		{
		case AttributeType_Bool4:
		case AttributeType_Bool3:
		case AttributeType_Bool2:
		case AttributeType_Bool1:
			memcpy(field.m_Bool4, value, sizeof(bool) * 4);
			break;
		case AttributeType_Int4:
		case AttributeType_Int3:
		case AttributeType_Int2:
		case AttributeType_Int1:
			memcpy(field.m_Int4, value, sizeof(int) * 4);
			break;
		case AttributeType_Float4:
		case AttributeType_Float3:
		case AttributeType_Float2:
		case AttributeType_Float1:
			memcpy(field.m_Float4, value, sizeof(float) * 4);
			break;
		default:
			m_IsValid = false;
			AE_VERIFY(false);
			break;
		}
		m_IsDirty = true;
		m_IsDefaultValue = isDefault;
		m_Lock.unlock();
	}

	template<typename T>
	void		_GetValueByTypeImpl(EAttributeType type, T *value, UAttributeValue &field)
	{
		m_Lock.lock();
		switch (type)
		{
		case AttributeType_Bool4:
			*value = (T)field.m_Bool4[3];
			break;
		case AttributeType_Bool3:
			*value = (T)field.m_Bool4[2];
			break;
		case AttributeType_Bool2:
			*value = (T)field.m_Bool4[1];
			break;
		case AttributeType_Bool1:
			*value = (T)field.m_Bool4[0];
			break;
		case AttributeType_Int4:
			*value = (T)field.m_Int4[3];
			break;
		case AttributeType_Int3:
			*value = (T)field.m_Int4[2];
			break;
		case AttributeType_Int2:
			*value = (T)field.m_Int4[1];
			break;
		case AttributeType_Int1:
			*value = (T)field.m_Int4[0];
			break;
		case AttributeType_Float4:
			*value = (T)field.m_Float4[3];
			break;
		case AttributeType_Float3:
			*value = (T)field.m_Float4[2];
			break;
		case AttributeType_Float2:
			*value = (T)field.m_Float4[1];
			break;
		case AttributeType_Float1:
			*value= (T)field.m_Float4[0];
			break;
		default:
			m_IsValid = false;
			AE_VERIFY(false);
			break;
		}
		m_Lock.unlock();
	}

	void		_GetValueAsStreamValueImpl(EAttributeType type, AEGP_StreamValue *value, UAttributeValue &field)
	{
		m_Lock.lock();
		switch (type)
		{
		case AttributeType_Bool4:
			value->val.one_d = field.m_Bool4[3];
			break;
		case AttributeType_Bool3:
			value->val.one_d = field.m_Bool4[2];
			break;
		case AttributeType_Bool2:
			value->val.one_d = field.m_Bool4[1];
			break;
		case AttributeType_Bool1:
			value->val.one_d = field.m_Bool4[0];
			break;
		case AttributeType_Int4:
			value->val.one_d = field.m_Int4[3];
			break;
		case AttributeType_Int3:
			value->val.one_d = field.m_Int4[2];
			break;
		case AttributeType_Int2:
			value->val.one_d = field.m_Int4[1];
			break;
		case AttributeType_Int1:
			value->val.one_d = field.m_Int4[0];
			break;
		case AttributeType_Float4:
			value->val.one_d = field.m_Float4[3];
			break;
		case AttributeType_Float3:
			value->val.one_d = field.m_Float4[2];
			break;
		case AttributeType_Float2:
			value->val.one_d = field.m_Float4[1];
			break;
		case AttributeType_Float1:
			value->val.one_d = field.m_Float4[0];
			break;
		default:
			m_IsValid = false;
			AE_VERIFY(false);
			break;
		}
		m_Lock.unlock();
	}

	template<typename T>
	void		_GetValueImpl(T *value, UAttributeValue &field)
	{
		m_Lock.lock();
		switch (m_Type)
		{
		case AttributeType_Bool4:
			value[3] = field.m_Bool4[3];
		case AttributeType_Bool3:
			value[2] = field.m_Bool4[2];
		case AttributeType_Bool2:
			value[1] = field.m_Bool4[1];
		case AttributeType_Bool1:
			value[0] = field.m_Bool4[0];
			break;
		case AttributeType_Int4:
			value[3] = (T)field.m_Int4[3];
		case AttributeType_Int3:
			value[2] = (T)field.m_Int4[2];
		case AttributeType_Int2:
			value[1] = (T)field.m_Int4[1];
		case AttributeType_Int1:
			value[0] = (T)field.m_Int4[0];
			break;
		case AttributeType_Float4:
			value[3] = field.m_Float4[3];
		case AttributeType_Float3:
			value[2] = field.m_Float4[2];
		case AttributeType_Float2:
			value[1] = field.m_Float4[1];
		case AttributeType_Float1:
			value[0] = field.m_Float4[0];
			break;
		default:
			m_IsValid = false;
			AE_VERIFY(false);
			break;
		}
		m_Lock.unlock();
	}

	void		*_GetRawValueImpl(UAttributeValue &field)
	{
		m_Lock.lock();

		void*	result = nullptr;
		switch (m_Type)
		{
		case AttributeType_Bool4:
		case AttributeType_Bool3:
		case AttributeType_Bool2:
		case AttributeType_Bool1:
			result = (void*)&field.m_Bool4;
			break;
		case AttributeType_Int4:
		case AttributeType_Int3:
		case AttributeType_Int2:
		case AttributeType_Int1:
			result = (void*)&field.m_Int4;
			break;
		case AttributeType_Float4:
		case AttributeType_Float3:
		case AttributeType_Float2:
		case AttributeType_Float1:
			result = (void*)&field.m_Float4;
			break;
		default:
			AE_VERIFY(false);
			break;
		}
		m_Lock.unlock();
		return result;
	}
};

#pragma endregion

//----------------------------------------------------------------------------

union	SAAEExtraData
{
	SAAEExtraData(PF_Cmd Cmd, void *data)
	{
		switch (Cmd)
		{
		case	PF_Cmd_SMART_PRE_RENDER:
			m_PreRenderData = reinterpret_cast<PF_PreRenderExtra*>(data);
			break;
		case	PF_Cmd_SMART_RENDER:
			m_SmartRenderData = reinterpret_cast<PF_SmartRenderExtra*>(data);
			break;
		case	PF_Cmd_USER_CHANGED_PARAM:
			m_ChangeParamData = reinterpret_cast<PF_UserChangedParamExtra*>(data);
			break;
		default:
			m_UndefinedData = data;
			break;
		}
	}
	PF_SmartRenderExtra			*m_SmartRenderData;
	PF_PreRenderExtra			*m_PreRenderData;
	PF_UserChangedParamExtra	*m_ChangeParamData;
	void						*m_UndefinedData;
};

//----------------------------------------------------------------------------

struct	SAAEIOData
{
	SAAEIOData(PF_Cmd Cmd, PF_InData *InData, PF_OutData *OutData, void *data, const int *paramIndexes)
		: m_Cmd(Cmd)
		, m_InData(InData)
		, m_OutData(OutData)
		, m_ExtraData(Cmd, data)
		, m_WorldSuite(nullptr)
		, m_ReturnCode(PF_Err_NONE)
		, m_ParametersIndexes(paramIndexes)
	{
		if (m_InData && m_OutData)
		{
			AEFX_AcquireSuite(	m_InData,
								m_OutData,
								kPFWorldSuite,
								kPFWorldSuiteVersion2,
								"Couldn't load suite.",
								(void**)&m_WorldSuite);
		}
	};
	~SAAEIOData()
	{
		if (m_InData && m_OutData)
		{
			AEFX_ReleaseSuite(	m_InData,
								m_OutData,
								kPFWorldSuite,
								kPFWorldSuiteVersion2,
								"Couldn't release suite.");
		}
		m_WorldSuite = nullptr;
	};

	PF_Cmd				m_Cmd;

	PF_InData			*m_InData;
	PF_OutData			*m_OutData;

	SAAEExtraData		m_ExtraData;

	PF_WorldSuite2		*m_WorldSuite;

	PF_Err				m_ReturnCode;

	const int			*m_ParametersIndexes;
};

//----------------------------------------------------------------------------

struct	SAAEScopedParams
{
	PF_ParamDef			m_ParamData;
	PF_InData			*m_InData;

	SAAEScopedParams(SAAEIOData& AAEData, A_u_long type) :
		m_InData(AAEData.m_InData)
	{
		AEFX_CLR_STRUCT(m_ParamData);
		int idx = (AAEData.m_ParametersIndexes != nullptr) ? AAEData.m_ParametersIndexes[type] : type;
		PF_CHECKOUT_PARAM(	m_InData,
							idx,
							m_InData->current_time,
							m_InData->time_step,
							m_InData->time_scale,
							&m_ParamData);
	}
	~SAAEScopedParams()
	{
		PF_CHECKIN_PARAM(m_InData, &m_ParamData);
		m_InData = nullptr;
	}

	PF_ParamDef			&GetRawParam()				{ return m_ParamData; }
	float				GetAngle() const			{ return (float)m_ParamData.u.ad.value / 65536.0f; }
	float				GetFloat() const			{ return (float)m_ParamData.u.fs_d.value; }
	float				GetPercent() const			{ return (float)m_ParamData.u.fd.value / (float)m_ParamData.u.fd.valid_max; }
	int					GetInt() const				{ return (int)m_ParamData.u.fs_d.value; }
	int					GetComboBoxValue() const	{ return (int)m_ParamData.u.pd.value; }
	bool				GetCheckBoxValue() const	{ return (bool)m_ParamData.u.bd.value; }
	PF_LayerDef			GetLayer() const			{ return (PF_LayerDef)m_ParamData.u.ld; }
	A_FloatPoint3		GetPoint3D() const
	{
		A_FloatPoint3		position = {(float)m_ParamData.u.point3d_d.x_value * m_InData->downsample_x.den,
										(float)m_ParamData.u.point3d_d.y_value * m_InData->downsample_y.den,
										(float)m_ParamData.u.point3d_d.z_value * ::sqrt(m_InData->downsample_x.den * m_InData->downsample_y.den)};
		return position;
	}
	A_FloatPoint		GetPoint2D() const
	{
		A_FloatPoint		position = { (float)m_ParamData.u.td.x_value * m_InData->downsample_x.den,
										(float)m_ParamData.u.td.y_value * m_InData->downsample_y.den };
		return position;
	}
	A_FloatPoint3	GetColor() const
	{
		const PF_UnionablePixel	&color = m_ParamData.u.cd.value;
		return A_FloatPoint3{ color.red / 255.0f, color.green / 255.0f, color.blue / 255.0f };
	}
};

//----------------------------------------------------------------------------

struct	SAAECamera
{
	float	m_YFov;
};

//----------------------------------------------------------------------------

inline void	CopyCharToUTF16(const char *input,
							A_UTF16Char *destination)
{
	std::string		basicStr(input);
	std::u16string	ws;

	ws.append(basicStr.begin(), basicStr.end());
	memcpy(destination, ws.data(), (ws.length() + 1) * sizeof(char16_t));
}

//----------------------------------------------------------------------------

inline void	WCharToString(aechar_t *input, std::string *destination)
{
	std::u16string ws(input);
	destination->append(ws.begin(), ws.end());
}

//----------------------------------------------------------------------------

inline unsigned int hashString(const char *str)
{
	unsigned int hash = 5381;
	int c;

	while ((c = *str++))
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

	return hash;
}

//----------------------------------------------------------------------------

inline float	RadToDeg(const float angle)
{
	return angle * 180.f / 3.141592f;
}

//----------------------------------------------------------------------------

inline float	DegToRad(const float angle)
{
	return angle * 3.141592f / 180.f;
}

//----------------------------------------------------------------------------

__AAEPK_END

//----------------------------------------------------------------------------

struct	PopcornFXSuite1
{
	SPAPI A_Err		(*InitializePopcornFXIFN)								(AAePk::SAAEIOData& AAEData);

	SPAPI A_Err		(*HandleNewEmitterEvent)								(AAePk::SAAEIOData& AAEData, AAePk::SEmitterDesc *descriptor);

	SPAPI A_Err		(*HandleDeleteEmitterEvent)								(AAePk::SAAEIOData& AAEData, AAePk::SEmitterDesc *descriptor);

	SPAPI bool		(*CheckEmitterValidity)									(AAePk::SAAEIOData& AAEData, AAePk::SEmitterDesc *descriptor);
	
	SPAPI A_Err		(*UpdateScene)											(AAePk::SAAEIOData& AAEData, AAePk::SEmitterDesc* descriptor);

	SPAPI A_Err		(*UpdateEmitter)										(AAePk::SAAEIOData& AAEData, AAePk::SEmitterDesc* descriptor);

	SPAPI A_Err		(*DisplayMarketplacePanel)								(AAePk::SAAEIOData& AAEData, AAePk::SEmitterDesc* descriptor);

	SPAPI A_Err		(*DisplayBrowseEffectPanel)								(AAePk::SAAEIOData& AAEData, AAePk::SEmitterDesc* descriptor);

	SPAPI A_Err		(*DisplayBrowseMeshDialog)								(AAePk::SAAEIOData& AAEData, AAePk::SEmitterDesc* descriptor);

	SPAPI A_Err		(*DisplayBrowseEnvironmentMapDialog)					(AAePk::SAAEIOData& AAEData, AAePk::SEmitterDesc* descriptor);

	SPAPI A_Err		(*ReimportEffect)										(AAePk::SAAEIOData& AAEData, AAePk::SEmitterDesc* descriptor);

	SPAPI A_Err		(*Display_AttributeSampler_BrowseMeshDialog)			(AAePk::SAAEIOData& AAEData, AAePk::SAttributeSamplerDesc* descriptor);

	SPAPI A_Err		(*ShutdownPopcornFXIFN)									(AAePk::SAAEIOData& AAEData);

	SPAPI A_Err		(*SetParametersIndexes)									(const int *indexes, AAePk::EPKChildPlugins plugin);

	SPAPI A_Err		(*SetDefaultLayerPosition)								(AAePk::SAAEIOData& AAEData, AEGP_LayerH layer);

	SPAPI A_Err		(*MoveEffectIntoCurrentView)							(AAePk::SAAEIOData& AAEData, AAePk::SEmitterDesc* descriptor);
	
};

//----------------------------------------------------------------------------

#endif // !__POPCORNFX_SUITE_H__


