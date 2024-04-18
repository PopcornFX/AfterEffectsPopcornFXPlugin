//----------------------------------------------------------------------------
// This program is the property of Persistant Studios SARL.
//
// You may not redistribute it and/or modify it under any conditions
// without written permission from Persistant Studios SARL, unless
// otherwise stated in the latest Persistant Studios Code License.
//
// See the Persistant Studios Code License for further details.
//----------------------------------------------------------------------------

#include "precompiled.h"

#include "ShaderDefinitions.h"
#include "SampleLibShaderDefinitions.h"
#include "BasicSceneShaderDefinitions.h"
#include "UnitTestsShaderDefinitions.h"
#include "RenderIntegrationRHI/RHIGraphicResources.h"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

void	CreateGBufferConstSetLayouts(EGBufferCombination combination, RHI::SConstantSetLayout &layout)
{
	layout.Reset();
	layout.m_ShaderStagesMask = RHI::FragmentShaderMask;

	RHI::SConstantBufferDesc	meshFragmentConstant("MeshFragmentConstant");
	meshFragmentConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat, "Roughness"));
	meshFragmentConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat, "Metalness"));

	layout.AddConstantsLayout(meshFragmentConstant);

	// The 3 combinations:
	RHI::SConstantSamplerDesc	diffuseSamplerLayout("DiffuseTex", RHI::SamplerTypeSingle);
	RHI::SConstantSamplerDesc	roughMetalSamplerLayout("RoughMetalTex", RHI::SamplerTypeSingle);
	RHI::SConstantSamplerDesc	normalSamplerLayout("NormalTex", RHI::SamplerTypeSingle);

	if (combination == GBufferCombination_Diffuse)
	{
		layout.AddConstantsLayout(diffuseSamplerLayout);
	}
	else if (combination == GBufferCombination_Diffuse_RoughMetal)
	{
		layout.AddConstantsLayout(diffuseSamplerLayout);
		layout.AddConstantsLayout(roughMetalSamplerLayout);
	}
	else if (combination == GBufferCombination_Diffuse_RoughMetal_Normal)
	{
		layout.AddConstantsLayout(diffuseSamplerLayout);
		layout.AddConstantsLayout(roughMetalSamplerLayout);
		layout.AddConstantsLayout(normalSamplerLayout);
	}
}

//----------------------------------------------------------------------------

void	FillGBufferShaderBindings(	RHI::SShaderBindings &bindings,
									const RHI::SConstantSetLayout &sceneInfo,
									const RHI::SConstantSetLayout &meshInfo,
									bool hasVertexColor,
									bool hasTangents)
{
	bindings.Reset();

	// Mesh
	RHI::SPushConstantBuffer	meshVertexConstant("MeshVertexConstant", RHI::VertexShaderMask);
	meshVertexConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4x4, "ModelMatrix"));
	meshVertexConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4x4, "NormalMatrix"));
	bindings.m_PushConstants.PushBack(meshVertexConstant);

	// Vertex inputs
	u32	location = 0;
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = location++;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat3;
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Normal";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = location++;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat3;
	if (hasTangents)
	{
		bindings.m_InputAttributes.PushBack();
		bindings.m_InputAttributes.Last().m_Name = "Tangent";
		bindings.m_InputAttributes.Last().m_ShaderLocationBinding = location++;
		bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat4;
		bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::VertexShaderStage, "HAS_TANGENTS"));
	}
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "TexCoord";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = location++;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat2;
	if (hasVertexColor)
	{
		bindings.m_InputAttributes.PushBack();
		bindings.m_InputAttributes.Last().m_Name = "Color";
		bindings.m_InputAttributes.Last().m_ShaderLocationBinding = location++;
		bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat4;
		// Mandatory define to avoid fragment shader hash collision
		bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::FragmentShaderStage, "HAS_VERTEX_COLOR"));
	}

	bindings.m_ConstantSets.PushBack(sceneInfo);
	bindings.m_ConstantSets.PushBack(meshInfo);
}

//----------------------------------------------------------------------------

void	AddGBufferDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;

	description.m_Pipeline = RHI::VsPs;

	// Vertex output
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragNormal";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat3;
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragTangent";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat4;
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragTexCoord";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat2;
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragWorldPosition";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat4;

	// Fragment output
	description.m_FragmentOutput.PushBack();
	description.m_FragmentOutput.Last().m_Name = "outDiffuse";
	description.m_FragmentOutput.Last().m_Type = RHI::FlagRGBA;
	description.m_FragmentOutput.PushBack();
	description.m_FragmentOutput.Last().m_Name = "outDepth";
	description.m_FragmentOutput.Last().m_Type = RHI::FlagR;
	description.m_FragmentOutput.PushBack();
	description.m_FragmentOutput.Last().m_Name = "outEmissive";
	description.m_FragmentOutput.Last().m_Type = RHI::FlagRGBA;
	// note(Alex): NormalSpec always after
	description.m_FragmentOutput.PushBack();
	description.m_FragmentOutput.Last().m_Name = "outNormalSpec";
	description.m_FragmentOutput.Last().m_Type = RHI::FlagRGBA;

	shaders.PushBack();
	shaders.Last().m_VertexShader = "SolidMesh.vert";
	shaders.Last().m_FragmentShader = "GBuffer.frag";

	RHI::SConstantSetLayout		sceneInfo;
	CreateSceneInfoConstantLayout(sceneInfo);

	RHI::SConstantSetLayout		meshInfo[GBufferCombination_Count];
	for (u32 i = 0; i < static_cast<u32>(GBufferCombination_Count); ++i)
	{
		CreateGBufferConstSetLayouts(static_cast<EGBufferCombination>(i), meshInfo[i]);
	}

	FillGBufferShaderBindings(description.m_Bindings, sceneInfo, meshInfo[GBufferCombination_SolidColor], false);
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

	FillGBufferShaderBindings(description.m_Bindings, sceneInfo, meshInfo[GBufferCombination_Diffuse], false);
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

	FillGBufferShaderBindings(description.m_Bindings, sceneInfo, meshInfo[GBufferCombination_Diffuse_RoughMetal], false);
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

	FillGBufferShaderBindings(description.m_Bindings, sceneInfo, meshInfo[GBufferCombination_Diffuse_RoughMetal_Normal], false, true);
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragColor";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat4;
	FillGBufferShaderBindings(description.m_Bindings, sceneInfo, meshInfo[GBufferCombination_SolidColor], true);
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

	FillGBufferShaderBindings(description.m_Bindings, sceneInfo, meshInfo[GBufferCombination_Diffuse], true);
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

	FillGBufferShaderBindings(description.m_Bindings, sceneInfo, meshInfo[GBufferCombination_Diffuse_RoughMetal_Normal], true, true);
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
}

//----------------------------------------------------------------------------

void	FillGBufferShadowShaderBindings(RHI::SShaderBindings &bindings, const RHI::SConstantSetLayout &sceneInfo)
{
	bindings.Reset();

	// Mesh
	RHI::SPushConstantBuffer	meshVertexConstant("MeshVertexConstant", RHI::VertexShaderMask);
	meshVertexConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4x4, "ModelMatrix"));
	bindings.m_PushConstants.PushBack(meshVertexConstant);

	// Vertex inputs
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat3;

	bindings.m_ConstantSets.PushBack(sceneInfo);
}

//----------------------------------------------------------------------------

void	AddGBufferShadowDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;

	description.m_Pipeline = RHI::VsPs;

	// Vertex output
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragWorldPosition";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat4;

	// Fragment output
	description.m_FragmentOutput.PushBack();
	description.m_FragmentOutput.Last().m_Name = "outDepth";
	description.m_FragmentOutput.Last().m_Type = RHI::FlagRG;

	shaders.PushBack();
	shaders.Last().m_VertexShader = "SolidMeshShadow.vert";
	shaders.Last().m_FragmentShader = "SolidMeshShadow.frag";

	RHI::SConstantSetLayout		sceneInfo;
	CreateSceneInfoConstantLayout(sceneInfo);

	FillGBufferShadowShaderBindings(description.m_Bindings, sceneInfo);

	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
}

//----------------------------------------------------------------------------

void	CreateDeferredSamplerLightingConstSetLayout(RHI::SConstantSetLayout &deferredSamplersLayout)
{
	deferredSamplersLayout.Reset();
	deferredSamplersLayout.m_ShaderStagesMask = RHI::FragmentShaderMask;

	RHI::SConstantSamplerDesc	depthSampler("DepthSampler", RHI::SamplerTypeSingle);
	RHI::SConstantSamplerDesc	normalSampler("NormalRoughMetalSampler", RHI::SamplerTypeSingle);
	RHI::SConstantSamplerDesc	diffuseSampler("DiffuseSampler", RHI::SamplerTypeSingle);

	deferredSamplersLayout.AddConstantsLayout(depthSampler);
	deferredSamplersLayout.AddConstantsLayout(normalSampler);
	deferredSamplersLayout.AddConstantsLayout(diffuseSampler);
}

//----------------------------------------------------------------------------

void	FillDeferredLightShaderBindings(RHI::SShaderBindings &bindings,
										RHI::SConstantSetLayout &sceneInfoLayout,
										RHI::SConstantSetLayout &lightingLayout,
										RHI::SConstantSetLayout &shadowLayout,
										RHI::SConstantSetLayout &brdfLUTLayout,
										RHI::SConstantSetLayout &environmentMapLayout,
										RHI::SConstantSetLayout &deferredSamplersLayout)
{
	bindings.Reset();

	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat2;

	bindings.m_ConstantSets.PushBack(sceneInfoLayout);
	bindings.m_ConstantSets.PushBack(lightingLayout);
	bindings.m_ConstantSets.PushBack(shadowLayout);
	bindings.m_ConstantSets.PushBack(brdfLUTLayout);
	bindings.m_ConstantSets.PushBack(environmentMapLayout);
	bindings.m_ConstantSets.PushBack(deferredSamplersLayout);
}

//----------------------------------------------------------------------------

void	AddDeferredLightDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;

	description.m_Pipeline = RHI::VsPs;

	// Vertex output
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragTexCoord";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat2;

	// Fragment output
	description.m_FragmentOutput.PushBack();
	description.m_FragmentOutput.Last().m_Name = "outColor";
	description.m_FragmentOutput.Last().m_Type = RHI::FlagRGBA;

	RHI::SConstantSetLayout		sceneInfoLayout;
	RHI::SConstantSetLayout		lightingLayout;
	RHI::SConstantSetLayout		shadowLayout;
	RHI::SConstantSetLayout		brdfLUTLayout;
	RHI::SConstantSetLayout		environmentMapLayout;
	RHI::SConstantSetLayout		samplerLayout;

	CreateSceneInfoConstantLayout(sceneInfoLayout);
	CreateLightingSceneInfoConstantLayout(lightingLayout, shadowLayout, brdfLUTLayout, environmentMapLayout);
	CreateDeferredSamplerLightingConstSetLayout(samplerLayout);
	FillDeferredLightShaderBindings(description.m_Bindings, sceneInfoLayout, lightingLayout, shadowLayout, brdfLUTLayout, environmentMapLayout, samplerLayout);

	shaders.PushBack();
	shaders.Last().m_VertexShader = "FullScreenQuad.vert";
	shaders.Last().m_FragmentShader = "DeferredLights.frag";
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
}

//----------------------------------------------------------------------------

void	CreateDeferredMergingConstSetLayouts(RHI::SConstantSetLayout &layout)
{
	layout.Reset();
	layout.m_ShaderStagesMask = RHI::FragmentShaderMask;
	RHI::SConstantSamplerDesc	lightAccuSampler("LightAccuTex", RHI::SamplerTypeSingle);

	layout.AddConstantsLayout(lightAccuSampler);

	for (u32 i = 0, gbufferCount = SPassDescription::s_GBufferDefaultFormats.Count(); i < gbufferCount; ++i)
	{
		const CString					name = CString::Format("GBuffer%uTex", i);
		const RHI::SConstantSamplerDesc	gbufferSampler(name, RHI::SamplerTypeSingle);

		layout.AddConstantsLayout(gbufferSampler);
	}
}

//----------------------------------------------------------------------------

void	FillDeferredMergingShaderBindings(RHI::SShaderBindings &bindings, const RHI::SConstantSetLayout &layout)
{
	bindings.Reset();

	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat2;

	RHI::SPushConstantBuffer	pushConstants("DeferredMergingPushConstants", RHI::FragmentShaderMask);
	pushConstants.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat, "MinAlpha"));
	bindings.m_PushConstants.PushBack(pushConstants);

	bindings.m_ConstantSets.PushBack(layout);
}

//----------------------------------------------------------------------------

void	AddDeferredMergingDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;

	description.m_Pipeline = RHI::VsPs;

	// Vertex output
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragTexCoord";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat2;

	// Fragment output
	description.m_FragmentOutput.PushBack();
	description.m_FragmentOutput.Last().m_Name = "outColor";
	description.m_FragmentOutput.Last().m_Type = RHI::FlagRGBA;

	RHI::SConstantSetLayout	layout;

	CreateDeferredMergingConstSetLayouts(layout);

	FillDeferredMergingShaderBindings(description.m_Bindings, layout);

	shaders.PushBack();
	shaders.Last().m_VertexShader = "FullScreenQuad.vert";
	shaders.Last().m_FragmentShader = "DeferredMerging.frag";
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
