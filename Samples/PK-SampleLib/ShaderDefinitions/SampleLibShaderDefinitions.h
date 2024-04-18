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

void	FillGizmoShaderBindings(RHI::SShaderBindings &bindings);
void	AddGizmoDefinition(TArray<SShaderCombination> &shaders);

void	FillImGuiShaderBindings(RHI::SShaderBindings &bindings, const RHI::SConstantSetLayout &apiManager);
void	AddImGuiDefinition(TArray<SShaderCombination> &shaders);

void	FillProfilerShaderBindings(RHI::SShaderBindings &bindings);
void	AddProfilerDefinition(TArray<SShaderCombination> &shaders);

void	FillProfilerDrawNodeShaderBindings(RHI::SShaderBindings &bindings, bool isDashed);
void	AddProfilerDrawNodeDefinition(TArray<SShaderCombination> &shaders);

void	CreateComputeParticleCountPerMeshConstantSetLayout(RHI::SConstantSetLayout &layout, bool hasMeshIDs, bool hasLODs);
void	FillComputeParticleCountPerMeshShaderBindings(RHI::SShaderBindings &bindings, bool hasMeshIDs, bool hasLODs);
void	AddComputeParticleCountPerMeshDefinition(TArray<SShaderCombination> &shaders);

void	CreateComputeMeshMatricesConstantSetLayout(RHI::SConstantSetLayout &layout);
void	FillComputeMeshMatricesShaderBindings(RHI::SShaderBindings &bindings);
void	AddComputeMeshMatricesDefinition(TArray<SShaderCombination> &shaders);

void	CreateInitIndirectionOffsetsBufferConstantSetLayout(RHI::SConstantSetLayout &layout, bool lodNoAtlas);
void	FillInitIndirectionOffsetsBufferShaderBindings(RHI::SShaderBindings &bindings, bool lodNoAtlas);
void	AddInitIndirectionOffsetsBufferDefinition(TArray<SShaderCombination> &shaders);

void	CreateComputeMeshIndirectionBufferConstantSetLayout(RHI::SConstantSetLayout &layout, bool hasMeshIDs, bool hasLODs);
void	FillComputeMeshIndirectionBufferShaderBindings(RHI::SShaderBindings &bindings, bool hasMeshIDs, bool hasLODs);
void	AddComputeMeshIndirectionBufferDefinition(TArray<SShaderCombination> &shaders);

void	CreateComputeSortKeysConstantSetLayout(RHI::SConstantSetLayout &layout, bool sortByCameraDistance, bool hasRibbonIndirection);
void	FillComputeSortKeysShaderBindings(RHI::SShaderBindings &bindings, bool sortByCameraDistance, bool hasRibbonIndirection);
void	AddComputeSortKeysDefinition(TArray<SShaderCombination> &shaders);

void	CreateSortUpSweepConstantSetLayout(RHI::SConstantSetLayout &layout);
void	FillSortUpSweepShaderBindings(RHI::SShaderBindings &bindings, bool keyStride);
void	AddSortUpSweepDefinition(TArray<SShaderCombination> &shaders);

void	CreateSortPrefixSumConstantSetLayout(RHI::SConstantSetLayout &layout);
void	FillSortPrefixSumShaderBindings(RHI::SShaderBindings &bindings);
void	AddSortPrefixSumDefinition(TArray<SShaderCombination> &shaders);

void	CreateSortDownSweepConstantSetLayout(RHI::SConstantSetLayout &layout);
void	FillSortDownSweepShaderBindings(RHI::SShaderBindings &bindings, bool keyStride);
void	AddSortDownSweepDefinition(TArray<SShaderCombination> &shaders);

void	FillFXAAShaderBindings(RHI::SShaderBindings &bindings, bool lumaInAlpha);
void	AddFXAADefinition(TArray<SShaderCombination> &shaders);

void	CreateComputeRibbonSortKeysConstantSetLayout(RHI::SConstantSetLayout &layout);
void	FillComputeRibbonSortKeysShaderBindings(RHI::SShaderBindings &bindings);
void	AddComputeRibbonSortKeysDefinition(TArray<SShaderCombination> &shaders);

void	CreateComputeCubemapConstantSetLayout(RHI::SConstantSetLayout &layout, bool inputTxtIsLatLong);
void	FillComputeCubemapShaderBindings(RHI::SShaderBindings &bindings, bool inputTxtIsLatLong);
void	AddComputeCubemapDefinition(TArray<SShaderCombination> &shaders);

void	CreateFilterCubemapConstantSetLayout(RHI::SConstantSetLayout &layout);
void	FillFilterCubemapShaderBindings(RHI::SShaderBindings &bindings);
void	AddFilterCubemapDefinition(TArray<SShaderCombination> &shaders);

void	CreateComputeMipMapConstantSetLayout(RHI::SConstantSetLayout &layout);
void	FillComputeMipMapShaderBindings(RHI::SShaderBindings &bindings);
void	AddComputeMipMapDefinition(TArray<SShaderCombination> &shaders);

void	CreateBlurCubemapRenderFaceConstantSetLayout(RHI::SConstantSetLayout &layout);
void	FillBlurCubemapRenderFaceShaderBindings(RHI::SShaderBindings &bindings);
void	AddBlurCubemapRenderFaceDefinition(TArray<SShaderCombination> &shaders);

void	CreateBlurCubemapProcessConstantSetLayout(RHI::SConstantSetLayout &layout);
void	FillBlurCubemapProcessShaderBindings(RHI::SShaderBindings &bindings);
void	AddBlurCubemapProcessDefinition(TArray<SShaderCombination> &shaders);

void	CreateCubemapSamplerConstantSetLayout(RHI::SConstantSetLayout &layout);

//----------------------------------------------------------------------------
// Common utils:
//----------------------------------------------------------------------------

bool	CreateSceneInfoConstantLayout(RHI::SConstantSetLayout &sceneInfoLayout);
bool	CreateLightingSceneInfoConstantLayout(	RHI::SConstantSetLayout &lightLayout,
												RHI::SConstantSetLayout &shadowLayout,
												RHI::SConstantSetLayout &brdfLUTLayout,
												RHI::SConstantSetLayout &envMapLayout);

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
