//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"

#include "AEGP_AEPKConversion.h"
#include "AEGP_World.h"

#include <PopcornFX_Suite.h>
#include <pk_particles/include/ps_samplers_vectorfield.h>


__AEGP_PK_BEGIN

//----------------------------------------------------------------------------

RHI::EPixelFormat	AAEToPK(PF_PixelFormat format)
{
	RHI::EPixelFormat	result = RHI::EPixelFormat::FormatUnknown;
	switch (format)
	{
	case PF_PixelFormat_ARGB32:
		result = RHI::EPixelFormat::FormatUnorm8RGBA;
		break;
	case PF_PixelFormat_ARGB64:
		result = RHI::EPixelFormat::FormatUnorm16RGBA;
		break;
	case PF_PixelFormat_ARGB128:
		result = RHI::EPixelFormat::FormatFloat32RGBA;
		break;
	default:
		//PF_PixelFormat_INVALID 1717854562
		PK_ASSERT_MESSAGE(false, "Unknown conversion of pixel format between PopcornFX and AfterEffects.");
		break;
	}
	return result;
}

//----------------------------------------------------------------------------

PF_PixelFormat	PKToAAE(RHI::EPixelFormat format)
{
	PF_PixelFormat		result = PF_PixelFormat_INVALID;
	switch (format)
	{
	case RHI::EPixelFormat::FormatUnorm8RGBA:
		result = PF_PixelFormat_ARGB32;
		break;
	case  RHI::EPixelFormat::FormatUnorm16RGBA:
		result = PF_PixelFormat_ARGB64;
		break;
	case RHI::EPixelFormat::FormatFloat32RGBA:
		result = PF_PixelFormat_ARGB128;
		break;
	default:
		PK_ASSERT_MESSAGE(false, "Unknown conversion of pixel format between PopcornFX and AfterEffects.");
		break;
	}
	return result;
}

//----------------------------------------------------------------------------

u32	GetPixelSizeFromPixelFormat(RHI::EPixelFormat format)
{
	u32	result = 0;

	switch (format)
	{
	case RHI::EPixelFormat::FormatUnorm8RGBA:
		return sizeof(PF_Pixel8);
		break;
	case  RHI::EPixelFormat::FormatUnorm16RGBA:
		return sizeof(PF_Pixel16);
		break;
	case RHI::EPixelFormat::FormatFloat32RGBA:
		return sizeof(PF_PixelFloat);
		break;
	default:
		PK_ASSERT_MESSAGE(false, "Unknown conversion of pixel format between PopcornFX and AfterEffects.");
		break;
	}
	return result;
}

//----------------------------------------------------------------------------

void	AAEToPK(A_Matrix4 &in, CFloat4x4 &out)
{
	out.XAxis() = CFloat4((float)in.mat[0][0], (float)in.mat[0][1], (float)in.mat[0][2], (float)in.mat[0][3]);
	out.YAxis() = CFloat4((float)in.mat[1][0], (float)in.mat[1][1], (float)in.mat[1][2], (float)in.mat[1][3]);
	out.ZAxis() = CFloat4((float)in.mat[2][0], (float)in.mat[2][1], (float)in.mat[2][2], (float)in.mat[2][3]);
	out.WAxis() = CFloat4((float)in.mat[3][0], (float)in.mat[3][1], (float)in.mat[3][2], (float)in.mat[3][3]);
}

//----------------------------------------------------------------------------

void	AAEToPK(A_Matrix4 &in, CFloat4x4 *out)
{
	AAEToPK(in, *out);
}

//----------------------------------------------------------------------------

CFloat3	AAEToPK(A_FloatPoint3 &in)
{
	CFloat3	res((float)in.x, (float)in.y, (float)in.z);
	return res;
}

//----------------------------------------------------------------------------

A_FloatPoint3	PKToAAE(CFloat3 &in)
{
	A_FloatPoint3	res;
	res.x = in.x();
	res.y = in.y();
	res.z = in.z();
	return res;
}

//----------------------------------------------------------------------------

CFloat3	AngleAAEToPK(A_FloatPoint3 &in)
{
	return CFloat3(DegToRad((float)in.x), DegToRad((float)in.y), DegToRad((float)in.z));
}

//----------------------------------------------------------------------------

A_FloatPoint3	AnglePKToAAE(CFloat3 &in)
{
	CFloat3	degree(RadToDeg(in.x()), RadToDeg(in.y()), RadToDeg(in.z()));
	return PKToAAE(degree);
}

//----------------------------------------------------------------------------

EAttributeSemantic	AttributePKToAAE(EDataSemantic value)
{
	PK_STATIC_ASSERT((int)EDataSemantic::DataSemantic_None == (int)EAttributeSemantic::AttributeSemantic_None);
	PK_STATIC_ASSERT((int)EDataSemantic::DataSemantic_3DCoordinate == (int)EAttributeSemantic::AttributeSemantic_Coordinate);
	PK_STATIC_ASSERT((int)EDataSemantic::DataSemantic_3DScale == (int)EAttributeSemantic::AttributeSemantic_Scale);
	PK_STATIC_ASSERT((int)EDataSemantic::DataSemantic_Color == (int)EAttributeSemantic::AttributeSemantic_Color);

	return (EAttributeSemantic)value;
}

//----------------------------------------------------------------------------

EAttributeType	AttributePKToAAE(EBaseTypeID value)
{
	EAttributeType res = AttributeType_None;
	switch(value)
	{
	case (BaseType_Bool):
		res = AttributeType_Bool1;
		break;
	case (BaseType_Bool2):
		res = AttributeType_Bool2;
		break;
	case (BaseType_Bool3):
		res = AttributeType_Bool3;
		break;
	case (BaseType_Bool4):
		res = AttributeType_Bool4;
		break;
	case (BaseType_I32):
		res = AttributeType_Int1;
		break;
	case (BaseType_Int2):
		res = AttributeType_Int2;
		break;
	case (BaseType_Int3):
		res = AttributeType_Int3;
		break;
	case (BaseType_Int4):
		res = AttributeType_Int4;
		break;
	case (BaseType_Float):
		res = AttributeType_Float1;
		break;
	case (BaseType_Float2):
		res = AttributeType_Float2;
		break;
	case (BaseType_Float3):
		res = AttributeType_Float3;
		break;
	case (BaseType_Float4):
		res = AttributeType_Float4;
		break;
	default:
		PK_ASSERT_NOT_REACHED_MESSAGE("Attribute Type missmatch !");
	}
	return res;
}

//----------------------------------------------------------------------------

EAttributeSamplerType	AttributeSamplerPKToAAE(SParticleDeclaration::SSampler::ESamplerType type)
{
	EAttributeSamplerType res = AttributeSamplerType_None;
	switch (type)
	{
	case (SParticleDeclaration::SSampler::ESamplerType::Sampler_Animtrack):
		res = AttributeSamplerType_Animtrack;
		break;
	case (SParticleDeclaration::SSampler::ESamplerType::Sampler_Audio):
		res = AttributeSamplerType_Audio;
		break;
	case (SParticleDeclaration::SSampler::ESamplerType::Sampler_Curve):
		res = AttributeSamplerType_Curve;
		break;
	case (SParticleDeclaration::SSampler::ESamplerType::Sampler_EventStream):
		res = AttributeSamplerType_EventStream;
		break;
	case (SParticleDeclaration::SSampler::ESamplerType::Sampler_Geometry):
		res = AttributeSamplerType_Geometry;
		break;
	case (SParticleDeclaration::SSampler::ESamplerType::Sampler_Image):
		res = AttributeSamplerType_Image;
		break;
	case (SParticleDeclaration::SSampler::ESamplerType::Sampler_Grid):
		res = AttributeSamplerType_Grid;
		break;
	case (SParticleDeclaration::SSampler::ESamplerType::Sampler_ImageAtlas):
		res = AttributeSamplerType_ImageAtlas;
		break;
	case (SParticleDeclaration::SSampler::ESamplerType::Sampler_Text):
		res = AttributeSamplerType_Text;
		break;
	case (SParticleDeclaration::SSampler::ESamplerType::Sampler_VectorField):
		res = AttributeSamplerType_VectorField;
		break;
	default:
		PK_ASSERT_NOT_REACHED_MESSAGE("Attribute Sampler Type missmatch !");
		break;
	}
	return res;
}

//----------------------------------------------------------------------------

EBaseTypeID	AttributeAAEToPK(EAttributeType value)
{
	EBaseTypeID res = BaseType_Evolved;
	switch (value)
	{
	case (AttributeType_Bool1):
		res = BaseType_Bool;
		break;
	case (AttributeType_Bool2):
		res = BaseType_Bool2;
		break;
	case (AttributeType_Bool3):
		res = BaseType_Bool3;
		break;
	case (AttributeType_Bool4):
		res = BaseType_Bool4;
		break;
	case (AttributeType_Int1):
		res = BaseType_I32;
		break;
	case (AttributeType_Int2):
		res = BaseType_Int2;
		break;
	case (AttributeType_Int3):
		res = BaseType_Int3;
		break;
	case (AttributeType_Int4):
		res = BaseType_Int4;
		break;
	case (AttributeType_Float1):
		res = BaseType_Float;
		break;
	case (AttributeType_Float2):
		res = BaseType_Float2;
		break;
	case (AttributeType_Float3):
		res = BaseType_Float3;
		break;
	case (AttributeType_Float4):
		res = BaseType_Float4;
		break;
	default:
		PK_ASSERT_NOT_REACHED_MESSAGE("Attribute Type missmatch !");
	}
	return res;
}

//----------------------------------------------------------------------------

PKSample::CRHIParticleSceneRenderHelper::ERenderTargetDebug AAEToPK(ERenderType value)
{
	PKSample::CRHIParticleSceneRenderHelper::ERenderTargetDebug res = PKSample::CRHIParticleSceneRenderHelper::RenderTargetDebug_NoDebug;
	switch (value)
	{
	case (RenderType_FinalCompositing):
		res = PKSample::CRHIParticleSceneRenderHelper::RenderTargetDebug_NoDebug;
		break;
	case (RenderType_Emissive):
		res = PKSample::CRHIParticleSceneRenderHelper::RenderTargetDebug_NoDebug;
		break;
	case (RenderType_Albedo):
		res = PKSample::CRHIParticleSceneRenderHelper::RenderTargetDebug_Diffuse;
		break;
	case (RenderType_Normal):
		res = PKSample::CRHIParticleSceneRenderHelper::RenderTargetDebug_NormalUnpacked;
		break;
	case (RenderType_Depth):
		res = PKSample::CRHIParticleSceneRenderHelper::RenderTargetDebug_Depth;
		break;
	default:
		PK_ASSERT_NOT_REACHED_MESSAGE("Render Type missmatch !");

	}
	return res;
}

//----------------------------------------------------------------------------

void	AAEToPK(SPostFXBloomDesc &in, PKSample::SParticleSceneOptions::SBloom &out)
{
	out.m_Enable = in.m_Enable;
	out.m_BrightPassValue = in.m_BrightPassValue;
	out.m_Intensity = in.m_Intensity;
	out.m_Attenuation = in.m_Attenuation;

	switch (in.m_GaussianBlur)
	{
	case (GaussianBlurPixelRadius_5):
		out.m_BlurTap = PKSample::GaussianBlurCombination_5_Tap;
		break;
	case (GaussianBlurPixelRadius_9):
		out.m_BlurTap = PKSample::GaussianBlurCombination_9_Tap;
		break;
	case (GaussianBlurPixelRadius_13):
		out.m_BlurTap = PKSample::GaussianBlurCombination_13_Tap;
		break;
	}
	out.m_RenderPassCount = in.m_RenderPassCount;
}

//----------------------------------------------------------------------------

void	AAEToPK(SPostFXDistortionDesc &in, PKSample::SParticleSceneOptions::SDistortion &out)
{
	out.m_Enable = in.m_Enable;
}

//----------------------------------------------------------------------------

void	AAEToPK(SPostFXToneMappingDesc &in, PKSample::SParticleSceneOptions::SToneMapping &out)
{
	out.m_Saturation = in.m_Saturation;
	out.m_Enable = in.m_Enable;
	out.m_Exposure = in.m_Exposure;
}

//----------------------------------------------------------------------------

void	AAEToPK(SPostFXAADesc &in, PKSample::SParticleSceneOptions::SFXAA &out)
{
	out.m_Enable = in.m_Enable;
}

//----------------------------------------------------------------------------

void	AAEToPK(SRenderingDesc &in, PKSample::SParticleSceneOptions &out)
{
	AAEToPK(in.m_Bloom, out.m_Bloom);
	AAEToPK(in.m_Distortion, out.m_Distortion);
	AAEToPK(in.m_ToneMapping, out.m_ToneMapping);
	AAEToPK(in.m_FXAA, out.m_FXAA);
}

//----------------------------------------------------------------------------

CParticleSamplerDescriptor_VectorField_Grid::EInterpolation AAEToPK(EInterpolationType &value)
{
	CParticleSamplerDescriptor_VectorField_Grid::EInterpolation res = CParticleSamplerDescriptor_VectorField_Grid::EInterpolation::__MaxInterpolations;
	switch (value)
	{
	case EInterpolationType::EInterpolationType_Point:
		res = CParticleSamplerDescriptor_VectorField_Grid::EInterpolation::Interpolation_Point;
		break;
	case EInterpolationType::EInterpolationType_Trilinear:
		res = CParticleSamplerDescriptor_VectorField_Grid::EInterpolation::Interpolation_Trilinear;
		break;
	case EInterpolationType::EInterpolationType_Quadrilinear:
		res = CParticleSamplerDescriptor_VectorField_Grid::EInterpolation::Interpolation_Quadrilinear;
		break;
	default:
		PK_ASSERT_NOT_REACHED_MESSAGE("Render Type missmatch !");
		break;
	}
	return res;
}

//----------------------------------------------------------------------------

CUbyte3	ConvertSRGBToLinear(CUbyte3 v)
{
	CFloat3	vf(v.x() / 255.0f, v.y() / 255.0f, v.z() / 255.0f);

	CFloat3	vfl = PKSample::ConvertSRGBToLinear(vf);
	return CUbyte3(static_cast<u8>(vfl.x() * 255.0f), static_cast<u8>(vfl.y() * 255.0f), static_cast<u8>(vfl.z() * 255.0f));
}

//----------------------------------------------------------------------------

CUbyte3	ConvertLinearToSRGB(CUbyte3 v)
{
	CFloat3	vf(v.x() / 255.0f, v.y() / 255.0f, v.z() / 255.0f);

	CFloat3	vfl = PKSample::ConvertLinearToSRGB(vf);
	return CUbyte3(static_cast<u8>(vfl.x() * 255.0f), static_cast<u8>(vfl.y() * 255.0f), static_cast<u8>(vfl.z() * 255.0f));
}

//----------------------------------------------------------------------------

EApiValue	RHIApiToAEApi(RHI::EGraphicalApi value)
{
	EApiValue ret;
	switch (value)
	{
#if PK_BUILD_WITH_D3D12_SUPPORT != 0
	case RHI::EGraphicalApi::GApi_D3D12:
		ret = EApiValue::D3D12;
		break;
#endif
#if PK_BUILD_WITH_D3D11_SUPPORT != 0
	case RHI::EGraphicalApi::GApi_D3D11:
		ret = EApiValue::D3D11;
		break;
#endif
#if	PK_BUILD_WITH_METAL_SUPPORT != 0
	case RHI::EGraphicalApi::GApi_Metal:
		ret = EApiValue::Metal;
		break;
#endif
	default:
#if defined(PK_WINDOWS) && PK_BUILD_WITH_D3D11_SUPPORT != 0
		ret = EApiValue::D3D11;
#elif defined(PK_MACOSX) && PK_BUILD_WITH_METAL_SUPPORT != 0
		ret = EApiValue::Metal;
#else
		PK_ASSERT_NOT_REACHED_MESSAGE("Cannot choose a compatible default API for the current platform");
#endif
		break;
	}
	return ret;
}

//----------------------------------------------------------------------------

RHI::EGraphicalApi	AEApiToRHIApi(EApiValue value)
{
	RHI::EGraphicalApi ret;
	switch (value)
	{
#if PK_BUILD_WITH_D3D12_SUPPORT != 0
	case EApiValue::D3D12:
		ret = RHI::EGraphicalApi::GApi_D3D12;
		break;
#endif
#if PK_BUILD_WITH_D3D11_SUPPORT != 0
	case  EApiValue::D3D11:
		ret = RHI::EGraphicalApi::GApi_D3D11;
		break;
#endif
#if PK_BUILD_WITH_METAL_SUPPORT != 0
	case EApiValue::Metal:
		ret = RHI::EGraphicalApi::GApi_Metal;
		break;
#endif
	default:
#if defined(PK_WINDOWS) && PK_BUILD_WITH_D3D11_SUPPORT != 0
		ret = RHI::EGraphicalApi::GApi_D3D11;
#elif defined(PK_MACOSX) && PK_BUILD_WITH_METAL_SUPPORT != 0
		ret = RHI::EGraphicalApi::GApi_Metal;
#else
		PK_ASSERT_NOT_REACHED_MESSAGE("Cannot choose a compatible default API for the current platform");
#endif
		break;
	}
	return ret;

}

//----------------------------------------------------------------------------
__AEGP_PK_END
