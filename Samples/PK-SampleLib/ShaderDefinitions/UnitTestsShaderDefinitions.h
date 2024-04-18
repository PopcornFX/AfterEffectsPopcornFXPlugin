#pragma once

//----------------------------------------------------------------------------
// This program is the property of Persistant Studios SARL.
//
// You may not redistribute it and/or modify it under any conditions
// without written permission from Persistant Studios SARL, unless
// otherwise stated in the latest Persistant Studios Code License.
//
// See the Persistant Studios Code License for further details.
//----------------------------------------------------------------------------

#include <PK-SampleLib/PKSample.h>
#include <pk_rhi/include/interfaces/SShaderBindings.h>
#include <pk_rhi/include/interfaces/IApiManager.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

struct	SShaderCombination;

//----------------------------------------------------------------------------
// Create the constant set layouts:
//----------------------------------------------------------------------------

// - SimpleSampler
//		- sampler2d 	Texture
void	CreateSimpleSamplerConstSetLayouts(RHI::SConstantSetLayout &layout, bool multiSampled);

// - MultiSampler
//		- sampler2d 	Texture0
//		- sampler2d 	Texture1
//		- ...
//		- sampler2d 	TextureN
void	CreateMultiSamplersConstSetLayouts(RHI::SConstantSetLayout &layout, u32 textureCount = 3*4);

// - PerView
//		- ViewData
//			- float4x4 	Model
//			- float4x4 	View
//			- float4x4	Proj
//			- float4x4 	Normal
void	CreatePerViewConstSetLayouts(RHI::SConstantSetLayout &layout);

// - PerFrame
//		- LightData
//			- float 	LightCount
//			- float3 	LightDirections[256]
//			- float3 	LightColors[256]
void	CreatePerFrameConstSetLayouts(RHI::SConstantSetLayout &layout);

// - TextureProcess
//		- sampler2d		ToProcess
//		- sampler2d		LookUp
void	CreateProcessTextureConstSetLayouts(RHI::SConstantSetLayout &layout);

//----------------------------------------------------------------------------
// Unit tests shader programs:
//----------------------------------------------------------------------------

void	AddTriangleNoVboDefinition(TArray<SShaderCombination> &shaders);

//----------------------------------------------------------------------------

void	FillPosColVBOShaderBindings(RHI::SShaderBindings &bindings);
void	AddPosColVBODefinition(TArray<SShaderCombination> &shaders);

//----------------------------------------------------------------------------

void	FillUniformTestShaderBindings(RHI::SShaderBindings &bindings, const RHI::SConstantSetLayout &perView, const RHI::SConstantSetLayout &perFrame);
void	AddUniformTestDefinition(TArray<SShaderCombination> &shaders);


void	CreateUnitTestStructLayoutConstantSetLayout(RHI::SConstantSetLayout &layout);
void	AddUnitTestStructLayoutDefinition(TArray<SShaderCombination> &shaders);

//----------------------------------------------------------------------------

void	FillConstMat4ShaderBindings(RHI::SShaderBindings &bindings,
									const RHI::SConstantSetLayout &perView,
									const RHI::SConstantSetLayout *perFrame = null,
									const RHI::SConstantSetLayout *simpleSampler = null);
void	AddConstMat4Definition(TArray<SShaderCombination> &shaders);

//----------------------------------------------------------------------------

void	CreateCubemapRenderConstantSetLayout(RHI::SConstantSetLayout &layout);
void	FillCubemapRenderShaderBindings(RHI::SShaderBindings &bindings, const RHI::SConstantSetLayout &layout, const RHI::SConstantSetLayout &simpleSampler);
void	AddCubemapRenderDefinition(TArray<SShaderCombination> &shaders);
void	CreateCubemapComputeConstantSetLayout(RHI::SConstantSetLayout &layout, bool inputTxtIsLatLong);
void	FillCubemapComputeShaderBindings(RHI::SShaderBindings &bindings, bool inputTxtIsLatLong);
void	AddCubemapComputeDefinition(TArray<SShaderCombination> &shaders);

//----------------------------------------------------------------------------

void	AddComputeUnitTestBufferDefinition(TArray<SShaderCombination> &shaders);
void	AddComputeUnitTestTextureDefinition(TArray<SShaderCombination> &shaders);

//----------------------------------------------------------------------------

// Blur push constants:
struct	SBlurInfo
{
	CFloat2		m_ContextSize;
	CFloat2		m_Direction;
	CFloat4		m_SubUV;

	SBlurInfo(const CFloat2 &contextSize, const CFloat2 &direction)
	:	m_ContextSize(contextSize)
	,	m_Direction(direction)
	,	m_SubUV(0.0f, 0.0f, 1.0f, 1.0f)
	{
	}

	SBlurInfo(const CFloat2 &contextSize, const CFloat2 &direction, const CFloat4 &subUV)
	:	m_ContextSize(contextSize)
	,	m_Direction(direction)
	,	m_SubUV(subUV)
	{
	}
};

enum	EGaussianBlurCombination
{
	GaussianBlurCombination_5_Tap = 0,
	GaussianBlurCombination_9_Tap,
	GaussianBlurCombination_13_Tap,
	GaussianBlurCombination_Count
};
void	FillGaussianBlurShaderBindings(	EGaussianBlurCombination combination,
										RHI::SShaderBindings &bindings,
										const RHI::SConstantSetLayout &layout);
void	AddGaussianBlurDefinition(TArray<SShaderCombination> &shaders);

//----------------------------------------------------------------------------

// UTBasicDrawMultiPass, UTBasicDrawRTMSAA
// FullScreenQuad.vert
// Copy.frag
enum	ECopyCombination
{
	CopyCombination_Basic = 0,
	CopyCombination_FlippedBasic,
	CopyCombination_Alpha,
	CopyCombination_MulAdd,
	CopyCombination_ToneMapping,
	CopyCombination_Depth,
	CopyCombination_Normal,
	CopyCombination_UnpackedNormal,
	CopyCombination_Specular,
	CopyCombination_ComputeLuma,
	CopyCombination_Count
};
void	FillCopyShaderBindings(ECopyCombination combination, RHI::SShaderBindings &bindings, const RHI::SConstantSetLayout &layout);
void	AddCopyDefinition(TArray<SShaderCombination> &shaders);

//----------------------------------------------------------------------------

void	FillDistortionShaderBindings(RHI::SShaderBindings &bindings, RHI::SConstantSetLayout &layout);
void	AddDistortionDefinition(TArray<SShaderCombination> &shaders);

//----------------------------------------------------------------------------

void	FillColorRemapShaderBindings(RHI::SShaderBindings &bindings, RHI::SConstantSetLayout &layout);
void	AddColorRemapDefinition(TArray<SShaderCombination> &shaders);

//----------------------------------------------------------------------------

void	FillTextureFormatShaderBindings(RHI::SShaderBindings &bindings, const RHI::SConstantSetLayout &layout);
void	AddTextureFormatDefinition(TArray<SShaderCombination> &shaders);

//----------------------------------------------------------------------------

void	FillMipmappingShaderBindings(	RHI::SShaderBindings &bindings,
										const RHI::SConstantSetLayout &perView,
										const RHI::SConstantSetLayout &simpleSampler);
void	AddMipmappingDefinition(TArray<SShaderCombination> &shaders);

//----------------------------------------------------------------------------

void	FillMipmappingModeShaderBindings(RHI::SShaderBindings &bindings, const RHI::SConstantSetLayout &layout);
void	AddMipmappingModeDefinition(TArray<SShaderCombination> &shaders);

//----------------------------------------------------------------------------

void	FillHDRTextureShaderBindings(bool gammaCorrect, RHI::SShaderBindings &bindings, const RHI::SConstantSetLayout &layout);
void	AddHDRTextureDefinition(TArray<SShaderCombination> &shaders);

//----------------------------------------------------------------------------

void	FillRTMSAAShaderBindings(RHI::SShaderBindings &bindings, const RHI::SConstantSetLayout &layout);
void	AddRTMSAADefinition(TArray<SShaderCombination> &shaders);

//----------------------------------------------------------------------------

void	FillDrawInstancedShaderBindings(RHI::SShaderBindings &bindings);
void	AddDrawInstancedDefinition(TArray<SShaderCombination> &shaders);

//----------------------------------------------------------------------------

void	FillGeometryShaderBindings(RHI::SShaderBindings &bindings);
void	AddGeometryDefinition(TArray<SShaderCombination> &shaders);

//----------------------------------------------------------------------------

void	FillGeometry2ShaderBindings(RHI::SShaderBindings &bindings);
void	AddGeometry2Definition(TArray<SShaderCombination> &shaders);

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
