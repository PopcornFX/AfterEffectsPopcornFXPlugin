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

struct SShaderCombination;

enum	EGBufferCombination
{
	GBufferCombination_SolidColor,
	GBufferCombination_Diffuse,
	GBufferCombination_Diffuse_RoughMetal,
	GBufferCombination_Diffuse_RoughMetal_Normal,
	GBufferCombination_Count
};

void	CreateGBufferConstSetLayouts(EGBufferCombination combination, RHI::SConstantSetLayout &layout);
void	FillGBufferShaderBindings(	RHI::SShaderBindings &bindings,
									const RHI::SConstantSetLayout &sceneInfo,
									const RHI::SConstantSetLayout &meshInfo,
									bool hasVertexColor = false,
									bool hasTangents = false);
void	AddGBufferDefinition(TArray<SShaderCombination> &shaders);
void	FillGBufferShadowShaderBindings(RHI::SShaderBindings &bindings,
										const RHI::SConstantSetLayout &sceneInfo);
void	AddGBufferShadowDefinition(TArray<SShaderCombination> &shaders);
void	CreateDeferredSamplerLightingConstSetLayout(RHI::SConstantSetLayout &deferredSamplersLayout);
void	FillDeferredLightShaderBindings(RHI::SShaderBindings &bindings,
										RHI::SConstantSetLayout &sceneInfoLayout,
										RHI::SConstantSetLayout &lightingLayout,
										RHI::SConstantSetLayout &shadowLayout,
										RHI::SConstantSetLayout &brdfLUTLayout,
										RHI::SConstantSetLayout &environmentMapLayout,
										RHI::SConstantSetLayout &deferredSamplersLayout);
void	AddDeferredLightDefinition(TArray<SShaderCombination> &shaders);

void	CreateDeferredMergingConstSetLayouts(RHI::SConstantSetLayout &layout);
void	FillDeferredMergingShaderBindings(RHI::SShaderBindings &bindings, const RHI::SConstantSetLayout &layout);
void	AddDeferredMergingDefinition(TArray<SShaderCombination> &shaders);

__PK_SAMPLE_API_END
