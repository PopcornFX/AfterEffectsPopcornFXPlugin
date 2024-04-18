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
#include "UnitTestsShaderDefinitions.h"

#include <pk_rhi/include/interfaces/IApiManager.h>
#include "PK-SampleLib/RenderIntegrationRHI/RHIRenderIntegrationConfig.h"
#include "PK-SampleLib/RenderIntegrationRHI/RHIGPUSorter.h"

__PK_SAMPLE_API_BEGIN

//----------------------------------------------------------------------------

void	FillGizmoShaderBindings(RHI::SShaderBindings &bindings)
{
	// Vertex inputs
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat3;
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Color";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 1;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat3;

	RHI::SPushConstantBuffer	gizmoConstants("GizmoConstant", RHI::VertexShaderMask);
	gizmoConstants.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4x4, "ModelViewProj"));
	gizmoConstants.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4, "HoveredColor"));
	gizmoConstants.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4, "GrabbedColor"));

	bindings.m_PushConstants.PushBack(gizmoConstants);
}

//----------------------------------------------------------------------------

void	AddGizmoDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;

	description.m_Pipeline = RHI::VsPs;

	// Vertex output
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragColor";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat3;

	// Fragment output
	description.m_FragmentOutput.PushBack();
	description.m_FragmentOutput.Last().m_Name = "outColor";
	description.m_FragmentOutput.Last().m_Type = RHI::FlagRGBA;

	FillGizmoShaderBindings(description.m_Bindings);

	shaders.PushBack();
	shaders.Last().m_VertexShader = "Gizmo.vert";
	shaders.Last().m_FragmentShader = "Gizmo.frag";
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
}

//----------------------------------------------------------------------------

void	FillImGuiShaderBindings(RHI::SShaderBindings &bindings, const RHI::SConstantSetLayout &layout)
{
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat2;
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "TexCoord";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 1;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat2;
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Color";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 2;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeRGBA8;

	RHI::SPushConstantBuffer	gizmoConstants("ImGui", RHI::VertexShaderMask);
	gizmoConstants.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat2, "Scale"));
	gizmoConstants.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat2, "Translate"));

	bindings.m_PushConstants.PushBack(gizmoConstants);

	bindings.m_ConstantSets.PushBack(layout);
}

//----------------------------------------------------------------------------

void	AddImGuiDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;

	description.m_Pipeline = RHI::VsPs;

	// Vertex output
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragTexCoord";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat2;
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragColor";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat4;

	// Fragment output
	description.m_FragmentOutput.PushBack();
	description.m_FragmentOutput.Last().m_Name = "outColor";
	description.m_FragmentOutput.Last().m_Type = RHI::FlagRGBA;

	RHI::SConstantSetLayout		layout;

	CreateSimpleSamplerConstSetLayouts(layout, false);

	FillImGuiShaderBindings(description.m_Bindings, layout);

	shaders.PushBack();
	shaders.Last().m_VertexShader = "ImGui.vert";
	shaders.Last().m_FragmentShader = "ImGui.frag";
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
}

//----------------------------------------------------------------------------

void	FillProfilerShaderBindings(RHI::SShaderBindings &bindings)
{
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat2;
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Color";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 1;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat4;

	RHI::SPushConstantBuffer	profilerConstant("ProfilerConstant", RHI::VertexShaderMask);
	profilerConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4x4, "Transform"));

	bindings.m_PushConstants.PushBack(profilerConstant);
}

//----------------------------------------------------------------------------

void	AddProfilerDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;

	description.m_Pipeline = RHI::VsPs;

	// Vertex output
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragColor";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat4;

	// Fragment output
	description.m_FragmentOutput.PushBack();
	description.m_FragmentOutput.Last().m_Name = "outColor";
	description.m_FragmentOutput.Last().m_Type = RHI::FlagRGBA;

	FillProfilerShaderBindings(description.m_Bindings);

	shaders.PushBack();
	shaders.Last().m_VertexShader = "Profiler.vert";
	shaders.Last().m_FragmentShader = "Profiler.frag";
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
}

//----------------------------------------------------------------------------

void	FillProfilerDrawNodeShaderBindings(RHI::SShaderBindings &bindings, bool isDashed)
{
	bindings.Reset();
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat4;
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Color0";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 1;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat4;
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Color1";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 2;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat4;

	if (isDashed)
	{
		bindings.m_InputAttributes.PushBack();
		bindings.m_InputAttributes.Last().m_Name = "Color2";
		bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 3;
		bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat4;
		bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::AllShaderMask, "HAS_DASHED_LINES"));
	}

	RHI::SPushConstantBuffer	profilerConstant("ProfilerConstant", RHI::VertexShaderMask);
	profilerConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4x4, "Transform"));

	bindings.m_PushConstants.PushBack(profilerConstant);
}

//----------------------------------------------------------------------------

void	AddProfilerDrawNodeDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;

	description.m_Pipeline = RHI::VsPs;

	// Vertex output
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragColor0";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat4;

	// Fragment output
	description.m_FragmentOutput.PushBack();
	description.m_FragmentOutput.Last().m_Name = "outColor";
	description.m_FragmentOutput.Last().m_Type = RHI::FlagRGBA;

	FillProfilerDrawNodeShaderBindings(description.m_Bindings, false);

	shaders.PushBack();
	shaders.Last().m_VertexShader = "ProfilerDrawNode.vert";
	shaders.Last().m_FragmentShader = "ProfilerDrawNode.frag";
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

	FillProfilerDrawNodeShaderBindings(description.m_Bindings, true);

	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragColor1";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat4;

	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragColor2";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat4;

	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragNodeExtendedPixelCoord";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat4;

	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragNodePixelCoord";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat4;

	shaders.PushBack();
	shaders.Last().m_VertexShader = "ProfilerDrawNode.vert";
	shaders.Last().m_FragmentShader = "ProfilerDrawNode.frag";
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
}

//----------------------------------------------------------------------------

bool	CreateSceneInfoConstantLayout(RHI::SConstantSetLayout &sceneInfoLayout)
{
	bool	success = true;

	sceneInfoLayout.Reset();

	RHI::SConstantBufferDesc	bufferLayout("SceneInfo");

	success &= bufferLayout.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4x4, "ViewProj"));
	success &= bufferLayout.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4x4, "View"));
	success &= bufferLayout.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4x4, "Proj"));
	success &= bufferLayout.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4x4, "InvView"));
	success &= bufferLayout.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4x4, "InvViewProj"));
	success &= bufferLayout.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4x4, "UserToLHZ"));
	success &= bufferLayout.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4x4, "LHZToUser"));
	success &= bufferLayout.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4x4, "BillboardingView"));
	success &= bufferLayout.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4x4, "PackNormalView"));
	success &= bufferLayout.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4x4, "UnpackNormalView"));
	success &= bufferLayout.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4, "SideVector"));
	success &= bufferLayout.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4, "DepthVector"));
	success &= bufferLayout.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat2, "ZBufferLimits"));
	success &= bufferLayout.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat2, "ViewportSize"));
	success &= bufferLayout.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat, "Handedness"));

	success &= sceneInfoLayout.AddConstantsLayout(bufferLayout);

	return success;
}

//----------------------------------------------------------------------------

bool	CreateLightingSceneInfoConstantLayout(	RHI::SConstantSetLayout &lightLayout,
												RHI::SConstantSetLayout &shadowLayout,
												RHI::SConstantSetLayout &brdfLUTLayout,
												RHI::SConstantSetLayout &envMapLayout)
{
	bool						success = true;

	lightLayout.Reset();
	lightLayout.m_ShaderStagesMask = RHI::FragmentShaderMask;
	RHI::SConstantBufferDesc	lightConstant("LightInfo");

	success &= lightConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeInt, "DirectionalLightsCount"));
	success &= lightConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeInt, "SpotLightsCount"));
	success &= lightConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeInt, "PointLightsCount"));
	success &= lightConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat3, "AmbientColor"));
	success &= lightLayout.AddConstantsLayout(lightConstant);

	success &= lightLayout.AddConstantsLayout(RHI::SRawBufferDesc("DirectionalLightsInfo", true));
	success &= lightLayout.AddConstantsLayout(RHI::SRawBufferDesc("SpotLightsInfo", true));
	success &= lightLayout.AddConstantsLayout(RHI::SRawBufferDesc("PointLightsInfo", true));

	shadowLayout.Reset();

	RHI::SConstantBufferDesc	shadowConstant("ShadowsInfo");

	success &= shadowConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4x4, "Cascade0_WorldToShadow"));
	success &= shadowConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4x4, "Cascade1_WorldToShadow"));
	success &= shadowConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4x4, "Cascade2_WorldToShadow"));
	success &= shadowConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4x4, "Cascade3_WorldToShadow"));
	success &= shadowConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4, "ShadowsRanges"));
	success &= shadowConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat2, "m_ShadowsAspectRatio0"));
	success &= shadowConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat2, "m_ShadowsAspectRatio1"));
	success &= shadowConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat2, "m_ShadowsAspectRatio2"));
	success &= shadowConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat2, "m_ShadowsAspectRatio3"));
	success &= shadowConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4, "ShadowsConstants"));
	success &= shadowConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeInt, "ShadowsFlags"));
	success &= shadowLayout.AddConstantsLayout(shadowConstant);

	RHI::SConstantSamplerDesc	shadowMapSampler0("Cascade0_ShadowMap", RHI::SamplerTypeSingle);
	RHI::SConstantSamplerDesc	shadowMapSampler1("Cascade1_ShadowMap", RHI::SamplerTypeSingle);
	RHI::SConstantSamplerDesc	shadowMapSampler2("Cascade2_ShadowMap", RHI::SamplerTypeSingle);
	RHI::SConstantSamplerDesc	shadowMapSampler3("Cascade3_ShadowMap", RHI::SamplerTypeSingle);

	success &= shadowLayout.AddConstantsLayout(shadowMapSampler0);
	success &= shadowLayout.AddConstantsLayout(shadowMapSampler1);
	success &= shadowLayout.AddConstantsLayout(shadowMapSampler2);
	success &= shadowLayout.AddConstantsLayout(shadowMapSampler3);

	brdfLUTLayout.Reset();
	brdfLUTLayout.m_ShaderStagesMask = RHI::FragmentShaderMask;
	RHI::SConstantSamplerDesc	BRDFLUTSampler("BRDFLUTSampler", RHI::SamplerTypeSingle);
	success &= brdfLUTLayout.AddConstantsLayout(BRDFLUTSampler);

	envMapLayout.Reset();
	CreateCubemapSamplerConstantSetLayout(envMapLayout);
	return success;
}

//----------------------------------------------------------------------------

void	CreateComputeParticleCountPerMeshConstantSetLayout(RHI::SConstantSetLayout &layout, bool hasMeshIDs, bool hasLODs)
{
	layout.Reset();
	layout.m_ShaderStagesMask = RHI::ComputeShaderMask;
	layout.AddConstantsLayout(RHI::SRawBufferDesc("StreamInfo", true));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("GPUSimData", true));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("EnabledsOffsets", true));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("OutIndirect", false));
	if (hasMeshIDs)
		layout.AddConstantsLayout(RHI::SRawBufferDesc("MeshIDsOffsets", true));
	RHI::SConstantBufferDesc	LODConstants("LODConstants");
	LODConstants.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "PerLODMeshCount", 0x10)); // We limit the max lod count to 16.
	if (hasLODs)
	{
		layout.AddConstantsLayout(RHI::SRawBufferDesc("LODsOffsets", true));
		LODConstants.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "LODMeshOffsets", 0x10 + 1)); // + 1 as we add a last element to store the total mesh count to simplify logic in the shader.
	}
	layout.AddConstantsLayout(LODConstants);
}

//----------------------------------------------------------------------------

void	FillComputeParticleCountPerMeshShaderBindings(RHI::SShaderBindings &bindings, bool hasMeshIDs, bool hasLODs)
{
	bindings.Reset();

	RHI::SConstantSetLayout	layout;
	CreateComputeParticleCountPerMeshConstantSetLayout(layout, hasMeshIDs, hasLODs);
	bindings.m_ConstantSets.PushBack(layout);

	RHI::SPushConstantBuffer	pushConstant("Info");
	pushConstant.m_ShaderStagesMask = RHI::ComputeShaderMask;
	pushConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "DrawRequest"));
	pushConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "MeshCount"));
	if (hasLODs)
	{
		pushConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "LODCount"));
		bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::ComputeShaderStage, "HAS_LODS"));
	}
	bindings.m_PushConstants.PushBack(pushConstant);
	if (hasMeshIDs)
		bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::ComputeShaderStage, "HAS_MESHIDS"));
}

//----------------------------------------------------------------------------

void	AddComputeParticleCountPerMeshDefinition(TArray<SShaderCombination> &shaders)
{
	// Mesh atlas, no LOD
	{
		RHI::SShaderDescription		description;

		description.m_Pipeline = RHI::Cs;
		description.m_DrawMode = RHI::DrawModeInvalid;
		description.m_DispatchThreadSize = CUint3(PK_RH_GPU_THREADGROUP_SIZE, 1, 1);

		FillComputeParticleCountPerMeshShaderBindings(description.m_Bindings, true, false);

		shaders.PushBack();
		shaders.Last().m_ComputeShader = "ComputeParticleCountPerMesh.comp";
		shaders.Last().m_ShaderDescriptions.PushBack(description);
	}
	// No mesh atlas, no LOD
	{
		RHI::SShaderDescription		description;

		description.m_Pipeline = RHI::Cs;
		description.m_DrawMode = RHI::DrawModeInvalid;
		description.m_DispatchThreadSize = CUint3(PK_RH_GPU_THREADGROUP_SIZE, 1, 1);

		FillComputeParticleCountPerMeshShaderBindings(description.m_Bindings, false, false);

		shaders.PushBack();
		shaders.Last().m_ComputeShader = "ComputeParticleCountPerMesh.comp";
		shaders.Last().m_ShaderDescriptions.PushBack(description);
	}
	// Mesh atlas, LOD
	{
		RHI::SShaderDescription		description;

		description.m_Pipeline = RHI::Cs;
		description.m_DrawMode = RHI::DrawModeInvalid;
		description.m_DispatchThreadSize = CUint3(PK_RH_GPU_THREADGROUP_SIZE, 1, 1);

		FillComputeParticleCountPerMeshShaderBindings(description.m_Bindings, true, true);

		shaders.PushBack();
		shaders.Last().m_ComputeShader = "ComputeParticleCountPerMesh.comp";
		shaders.Last().m_ShaderDescriptions.PushBack(description);
	}
	// No mesh atlas, LOD
	{
		RHI::SShaderDescription		description;

		description.m_Pipeline = RHI::Cs;
		description.m_DrawMode = RHI::DrawModeInvalid;
		description.m_DispatchThreadSize = CUint3(PK_RH_GPU_THREADGROUP_SIZE, 1, 1);

		FillComputeParticleCountPerMeshShaderBindings(description.m_Bindings, false, true);

		shaders.PushBack();
		shaders.Last().m_ComputeShader = "ComputeParticleCountPerMesh.comp";
		shaders.Last().m_ShaderDescriptions.PushBack(description);
	}
}


//----------------------------------------------------------------------------

void	CreateComputeMeshMatricesConstantSetLayout(RHI::SConstantSetLayout &layout)
{
	layout.Reset();
	layout.m_ShaderStagesMask = RHI::ComputeShaderMask;
	layout.AddConstantsLayout(RHI::SRawBufferDesc("StreamInfo", true));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("GPUSimData", true));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("PositionsOffsets", true));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("ScalesOffsets", true));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("OrientationsOffsets", true));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("MatricesOffsets", true));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("OutMatrices", false));
}

//----------------------------------------------------------------------------

void	FillComputeMeshMatricesShaderBindings(RHI::SShaderBindings &bindings)
{
	bindings.Reset();
	RHI::SConstantSetLayout	layout;
	CreateComputeMeshMatricesConstantSetLayout(layout);
	bindings.m_ConstantSets.PushBack(layout);
	RHI::SPushConstantBuffer	pushConstant("Info");
	pushConstant.m_ShaderStagesMask = RHI::ComputeShaderMask;
	pushConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "DrawRequest"));
	bindings.m_PushConstants.PushBack(pushConstant);
}

//----------------------------------------------------------------------------

void	AddComputeMeshMatricesDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;

	description.m_Pipeline = RHI::Cs;
	description.m_DrawMode = RHI::DrawModeInvalid;
	description.m_DispatchThreadSize = CUint3(PK_RH_GPU_THREADGROUP_SIZE, 1, 1);

	FillComputeMeshMatricesShaderBindings(description.m_Bindings);

	shaders.PushBack();
	shaders.Last().m_ComputeShader = "ComputeMeshMatrices.comp";
	shaders.Last().m_ShaderDescriptions.PushBack(description);
}

//----------------------------------------------------------------------------

void	CreateInitIndirectionOffsetsBufferConstantSetLayout(RHI::SConstantSetLayout &layout, bool lodNoAtlas)
{
	layout.Reset();
	layout.m_ShaderStagesMask = RHI::ComputeShaderMask;
	layout.AddConstantsLayout(RHI::SRawBufferDesc("IndirectBuffer", true));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("OutIndirectionOffsets", false));

	if (lodNoAtlas)
	{
		RHI::SConstantBufferDesc	LODConstants("LODConstants");
		LODConstants.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "PerLODMeshCount", 0x10)); // We limit the max lod count to 16.
		LODConstants.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "LODMeshOffsets", 0x10 + 1)); // + 1 as we add a last element to store the total mesh count to simplify logic in the shader.
		layout.AddConstantsLayout(LODConstants);
	}
}

//----------------------------------------------------------------------------

void	FillInitIndirectionOffsetsBufferShaderBindings(RHI::SShaderBindings &bindings, bool lodNoAtlas)
{
	bindings.Reset();
	RHI::SConstantSetLayout	layout;
	CreateInitIndirectionOffsetsBufferConstantSetLayout(layout, lodNoAtlas);
	bindings.m_ConstantSets.PushBack(layout);

	RHI::SPushConstantBuffer	pushConstant("Info");
	pushConstant.m_ShaderStagesMask = RHI::ComputeShaderMask;
	pushConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "DrawCallCount"));
	bindings.m_PushConstants.PushBack(pushConstant);
	if (lodNoAtlas)
		bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::ComputeShaderStage, "HAS_LODSNOATLAS"));
}

//----------------------------------------------------------------------------

void	AddInitIndirectionOffsetsBufferDefinition(TArray<SShaderCombination> &shaders)
{
	// Mesh atlas enabled (with or without LOD) or mesh atlas disabled (no LOD)
	{
		RHI::SShaderDescription		description;

		description.m_Pipeline = RHI::Cs;
		description.m_DrawMode = RHI::DrawModeInvalid;
		description.m_DispatchThreadSize = CUint3(PK_RH_GPU_THREADGROUP_SIZE_2D, PK_RH_GPU_THREADGROUP_SIZE_2D, 1);

		FillInitIndirectionOffsetsBufferShaderBindings(description.m_Bindings, false);

		shaders.PushBack();
		shaders.Last().m_ComputeShader = "InitIndirectionOffsetsBuffer.comp";
		shaders.Last().m_ShaderDescriptions.PushBack(description);
	}
	// Mesh atlas disabled, LOD enabled (indirection offsets buffer is indexed differently)
	{
		RHI::SShaderDescription		description;

		description.m_Pipeline = RHI::Cs;
		description.m_DrawMode = RHI::DrawModeInvalid;
		description.m_DispatchThreadSize = CUint3(PK_RH_GPU_THREADGROUP_SIZE_2D, PK_RH_GPU_THREADGROUP_SIZE_2D, 1);

		FillInitIndirectionOffsetsBufferShaderBindings(description.m_Bindings, true);

		shaders.PushBack();
		shaders.Last().m_ComputeShader = "InitIndirectionOffsetsBuffer.comp";
		shaders.Last().m_ShaderDescriptions.PushBack(description);
	}
}

//----------------------------------------------------------------------------

void	CreateComputeMeshIndirectionBufferConstantSetLayout(RHI::SConstantSetLayout &layout, bool hasMeshIDs, bool hasLODs)
{
	layout.Reset();
	layout.m_ShaderStagesMask = RHI::ComputeShaderMask;
	layout.AddConstantsLayout(RHI::SRawBufferDesc("StreamInfo", true));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("GPUSimData", true));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("EnabledOffsets", true));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("OutIndirection", false));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("IndirectionOffsets", false));
	if (hasMeshIDs)
		layout.AddConstantsLayout(RHI::SRawBufferDesc("MeshIDsOffsets", true));

	RHI::SConstantBufferDesc	LODConstants("LODConstants");
	LODConstants.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "PerLODMeshCount", 0x10)); // We limit the max lod count to 16.
	if (hasLODs)
	{
		layout.AddConstantsLayout(RHI::SRawBufferDesc("LODsOffsets", true));
		LODConstants.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "LODMeshOffsets", 0x10 + 1)); // + 1 as we add a last element to store the total mesh count to simplify logic in the shader.
	}
	layout.AddConstantsLayout(LODConstants);
}

//----------------------------------------------------------------------------

void	FillComputeMeshIndirectionBufferShaderBindings(RHI::SShaderBindings &bindings, bool hasMeshIDs, bool hasLODs)
{
	bindings.Reset();
	RHI::SConstantSetLayout	layout;
	CreateComputeMeshIndirectionBufferConstantSetLayout(layout, hasMeshIDs, hasLODs);
	bindings.m_ConstantSets.PushBack(layout);

	RHI::SPushConstantBuffer	pushConstant("Info");
	pushConstant.m_ShaderStagesMask = RHI::ComputeShaderMask;
	pushConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "DrawRequest"));
	pushConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "MeshCount"));
	if (hasLODs)
	{
		pushConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "LODCount"));
		bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::ComputeShaderStage, "HAS_LODS"));
	}
	bindings.m_PushConstants.PushBack(pushConstant);

	if (hasMeshIDs)
		bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::ComputeShaderStage, "HAS_MESHIDS"));
}

//----------------------------------------------------------------------------

void	AddComputeMeshIndirectionBufferDefinition(TArray<SShaderCombination> &shaders)
{
	// Mesh atlas, no LOD
	{
		RHI::SShaderDescription		description;

		description.m_Pipeline = RHI::Cs;
		description.m_DrawMode = RHI::DrawModeInvalid;
		description.m_DispatchThreadSize = CUint3(PK_RH_GPU_THREADGROUP_SIZE, 1, 1);

		FillComputeMeshIndirectionBufferShaderBindings(description.m_Bindings, true, false);

		shaders.PushBack();
		shaders.Last().m_ComputeShader = "ComputeMeshIndirectionBuffer.comp";
		shaders.Last().m_ShaderDescriptions.PushBack(description);
	}
	// No mesh atlas, no LOD
	{
		RHI::SShaderDescription		description;

		description.m_Pipeline = RHI::Cs;
		description.m_DrawMode = RHI::DrawModeInvalid;
		description.m_DispatchThreadSize = CUint3(PK_RH_GPU_THREADGROUP_SIZE, 1, 1);

		FillComputeMeshIndirectionBufferShaderBindings(description.m_Bindings, false, false);

		shaders.PushBack();
		shaders.Last().m_ComputeShader = "ComputeMeshIndirectionBuffer.comp";
		shaders.Last().m_ShaderDescriptions.PushBack(description);
	}
	// Mesh atlas, LOD
	{
		RHI::SShaderDescription		description;

		description.m_Pipeline = RHI::Cs;
		description.m_DrawMode = RHI::DrawModeInvalid;
		description.m_DispatchThreadSize = CUint3(PK_RH_GPU_THREADGROUP_SIZE, 1, 1);

		FillComputeMeshIndirectionBufferShaderBindings(description.m_Bindings, true, true);

		shaders.PushBack();
		shaders.Last().m_ComputeShader = "ComputeMeshIndirectionBuffer.comp";
		shaders.Last().m_ShaderDescriptions.PushBack(description);
	}
	// No Mesh atlas, LOD
	{
		RHI::SShaderDescription		description;

		description.m_Pipeline = RHI::Cs;
		description.m_DrawMode = RHI::DrawModeInvalid;
		description.m_DispatchThreadSize = CUint3(PK_RH_GPU_THREADGROUP_SIZE, 1, 1);

		FillComputeMeshIndirectionBufferShaderBindings(description.m_Bindings, false, true);

		shaders.PushBack();
		shaders.Last().m_ComputeShader = "ComputeMeshIndirectionBuffer.comp";
		shaders.Last().m_ShaderDescriptions.PushBack(description);
	}
}

//----------------------------------------------------------------------------

void	CreateComputeSortKeysConstantSetLayout(RHI::SConstantSetLayout &layout, bool sortByCameraDistance, bool hasRibbonIndirection )
{
	layout.Reset();
	layout.m_ShaderStagesMask = RHI::ComputeShaderMask;
	layout.AddConstantsLayout(RHI::SRawBufferDesc("StreamInfo", true));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("GPUSimData", true));
	if (sortByCameraDistance)
		layout.AddConstantsLayout(RHI::SRawBufferDesc("PositionsOffsets", true));
	else
		layout.AddConstantsLayout(RHI::SRawBufferDesc("CustomSortKeysOffsets", true));
	if (hasRibbonIndirection)
	{
		layout.AddConstantsLayout(RHI::SRawBufferDesc("RibbonIndirection", true));
		layout.AddConstantsLayout(RHI::SRawBufferDesc("IndirectDraw", true));
	}
	layout.AddConstantsLayout(RHI::SRawBufferDesc("OutSortKeys", false));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("OutIndirection", false));

}

//----------------------------------------------------------------------------

void	FillComputeSortKeysShaderBindings(RHI::SShaderBindings &bindings, bool sortByCameraDistance, bool hasRibbonIndirection)
{
	bindings.Reset();
	RHI::SConstantSetLayout	layout;
	if (sortByCameraDistance)
	{
		CreateSceneInfoConstantLayout(layout);
		bindings.m_ConstantSets.PushBack(layout);
		bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::ComputeShaderStage, "SORT_BY_CAMERA_DISTANCE"));
	}
	if (hasRibbonIndirection)
		bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::ComputeShaderStage, "HAS_RIBBON_INDIRECTION"));
	CreateComputeSortKeysConstantSetLayout(layout, sortByCameraDistance, hasRibbonIndirection);
	bindings.m_ConstantSets.PushBack(layout);

	RHI::SPushConstantBuffer	pushConstant("Info");
	pushConstant.m_ShaderStagesMask = RHI::ComputeShaderMask;
	pushConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "DrawRequest"));
	bindings.m_PushConstants.PushBack(pushConstant);
}

//----------------------------------------------------------------------------

void	AddComputeSortKeysDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;

	description.m_Pipeline = RHI::Cs;
	description.m_DrawMode = RHI::DrawModeInvalid;
	description.m_DispatchThreadSize = CUint3(PK_RH_GPU_THREADGROUP_SIZE, 1, 1);

	FillComputeSortKeysShaderBindings(description.m_Bindings, true, false);

	shaders.PushBack();
	shaders.Last().m_ComputeShader = "ComputeSortKeys.comp";
	shaders.Last().m_ShaderDescriptions.PushBack(description);

	FillComputeSortKeysShaderBindings(description.m_Bindings, false, false);

	shaders.PushBack();
	shaders.Last().m_ComputeShader = "ComputeSortKeys.comp";
	shaders.Last().m_ShaderDescriptions.PushBack(description);

	FillComputeSortKeysShaderBindings(description.m_Bindings, true, true);

	shaders.PushBack();
	shaders.Last().m_ComputeShader = "ComputeSortKeys.comp";
	shaders.Last().m_ShaderDescriptions.PushBack(description);

	FillComputeSortKeysShaderBindings(description.m_Bindings, false, true);

	shaders.PushBack();
	shaders.Last().m_ComputeShader = "ComputeSortKeys.comp";
	shaders.Last().m_ShaderDescriptions.PushBack(description);
}

//----------------------------------------------------------------------------

void	CreateSortUpSweepConstantSetLayout(RHI::SConstantSetLayout &layout)
{
	layout.Reset();
	layout.m_ShaderStagesMask = RHI::ComputeShaderMask;
	layout.AddConstantsLayout(RHI::SRawBufferDesc("InIndirection", true));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("InKeys", true));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("OutCounts", false));
}

//----------------------------------------------------------------------------

void	FillSortUpSweepShaderBindings(RHI::SShaderBindings &bindings, bool keyStride64)
{
	bindings.Reset();

	RHI::SConstantSetLayout	layout;
	CreateSortUpSweepConstantSetLayout(layout);
	bindings.m_ConstantSets.PushBack(layout);

	RHI::SPushConstantBuffer	pushConstant("Info");
	pushConstant.m_ShaderStagesMask = RHI::ComputeShaderMask;
	pushConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "CurrentBit"));
	bindings.m_PushConstants.PushBack(pushConstant);

	RHI::SGroupsharedVariable	groupsharedVariable;
	groupsharedVariable.m_Name = "localCounts";
	groupsharedVariable.m_ArraySize = PK_RH_GPU_THREADGROUP_SIZE * 4 + ((PK_RH_GPU_THREADGROUP_SIZE * 4) >> 3);
	groupsharedVariable.m_Type = RHI::TypeUint4;
	bindings.m_GroupsharedVariables.PushBack(groupsharedVariable);

	if (keyStride64)
		bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::ComputeShaderStage, "KEYSTRIDE64"));

	bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::ComputeShaderStage, "NUM_KEY_PER_THREAD", CString::Format("%d", PK_GPU_SORT_NUM_KEY_PER_THREAD)));
}

//----------------------------------------------------------------------------

void	AddSortUpSweepDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;

	description.m_Pipeline = RHI::Cs;
	description.m_DrawMode = RHI::DrawModeInvalid;
	description.m_DispatchThreadSize = CUint3(PK_RH_GPU_THREADGROUP_SIZE, 1, 1); // Parallel up sweep : Here 1 thread handles 2 keys

	FillSortUpSweepShaderBindings(description.m_Bindings, false);

	shaders.PushBack();
	shaders.Last().m_ComputeShader = "SortUpSweep.comp";
	shaders.Last().m_ShaderDescriptions.PushBack(description);

	FillSortUpSweepShaderBindings(description.m_Bindings, true);

	shaders.PushBack();
	shaders.Last().m_ComputeShader = "SortUpSweep.comp";
	shaders.Last().m_ShaderDescriptions.PushBack(description);
}

//----------------------------------------------------------------------------

void	CreateSortPrefixSumConstantSetLayout(RHI::SConstantSetLayout &layout)
{
	layout.Reset();
	layout.m_ShaderStagesMask = RHI::ComputeShaderMask;
	layout.AddConstantsLayout(RHI::SRawBufferDesc("InOutCounts", false));
}

//----------------------------------------------------------------------------

void	FillSortPrefixSumShaderBindings(RHI::SShaderBindings &bindings)
{
	bindings.Reset();

	RHI::SConstantSetLayout	layout;
	CreateSortPrefixSumConstantSetLayout(layout);
	bindings.m_ConstantSets.PushBack(layout);

	RHI::SPushConstantBuffer	pushConstant("Info");
	pushConstant.m_ShaderStagesMask = RHI::ComputeShaderMask;
	pushConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "CurrentBit"));
	pushConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "GroupCount"));
	bindings.m_PushConstants.PushBack(pushConstant);

	RHI::SGroupsharedVariable	groupsharedVariable;
	groupsharedVariable.m_Name = "groupCounts";
	groupsharedVariable.m_ArraySize = PK_RH_GPU_THREADGROUP_SIZE * 4 + ((PK_RH_GPU_THREADGROUP_SIZE * 4) >> 3);
	groupsharedVariable.m_Type = RHI::TypeUint4;
	bindings.m_GroupsharedVariables.PushBack(groupsharedVariable);
}

//----------------------------------------------------------------------------

void	AddSortPrefixSumDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;

	description.m_Pipeline = RHI::Cs;
	description.m_DrawMode = RHI::DrawModeInvalid;
	description.m_DispatchThreadSize = CUint3(PK_RH_GPU_THREADGROUP_SIZE, 1, 1);

	FillSortPrefixSumShaderBindings(description.m_Bindings);

	shaders.PushBack();
	shaders.Last().m_ComputeShader = "SortPrefixSum.comp";
	shaders.Last().m_ShaderDescriptions.PushBack(description);
}

//----------------------------------------------------------------------------

void	CreateSortDownSweepConstantSetLayout(RHI::SConstantSetLayout &layout)
{
	layout.Reset();
	layout.m_ShaderStagesMask = RHI::ComputeShaderMask;
	layout.AddConstantsLayout(RHI::SRawBufferDesc("InCounts", true));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("InKeys", true));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("InIndirection", true));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("OutKeys", false));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("OutIndirection", false));
}

//----------------------------------------------------------------------------

void	FillSortDownSweepShaderBindings(RHI::SShaderBindings &bindings, bool keyStride64)
{
	bindings.Reset();

	RHI::SConstantSetLayout	layout;
	CreateSortDownSweepConstantSetLayout(layout);
	bindings.m_ConstantSets.PushBack(layout);

	// Necessary size for the thread group digit bins, used to compute the keys offset. the memory shifts are only necessary to reduce bank conflicts.
	// The bank conflicts could be avoided with a better access pattern for the prefix sum phase, but this is an easy fix without a huge memory cost. TODO enhance shared memory access.
	const u32 histCountsSize = PK_RH_GPU_THREADGROUP_SIZE * 16 + ((PK_RH_GPU_THREADGROUP_SIZE * 16) >> 4);
	// Necessary size to hold all of the thread group keys to reorder them after reading them from global memory and before writing them to global memory.
	const u32 keysReorderSize = PK_RH_GPU_THREADGROUP_SIZE * PK_GPU_SORT_NUM_KEY_PER_THREAD * ((keyStride64) ? 2 : 1);
	// The total multi-purpose group shared memory size
	const u32 tempMemorySize = PKMax(histCountsSize, keysReorderSize);

	RHI::SGroupsharedVariable	tempSharedMemoryVariable;
	tempSharedMemoryVariable.m_Name = "tempSharedMem";
	tempSharedMemoryVariable.m_ArraySize = tempMemorySize;
	tempSharedMemoryVariable.m_Type = RHI::TypeUint;
	bindings.m_GroupsharedVariables.PushBack(tempSharedMemoryVariable);

	// shared memory used to hold the thread group offset computed in the prefix sum kernel. Only used for memory access improvements
	RHI::SGroupsharedVariable	groupOffsetsVariable;
	groupOffsetsVariable.m_Name = "groupOffsets";
	groupOffsetsVariable.m_ArraySize = 16;
	groupOffsetsVariable.m_Type = RHI::TypeUint;
	bindings.m_GroupsharedVariables.PushBack(groupOffsetsVariable);

	// shared memory used to compute the keys relative offset from one digit to another in the thread group.
	// This is used to reorder the keys in a ascending order locally to allow coalesced write in global memory for keys and values.
	RHI::SGroupsharedVariable	localDigitOffsetsVariable;
	localDigitOffsetsVariable.m_Name = "localDigitOffsets";
	localDigitOffsetsVariable.m_ArraySize = 16;
	localDigitOffsetsVariable.m_Type = RHI::TypeUint;
	bindings.m_GroupsharedVariables.PushBack(localDigitOffsetsVariable);

	RHI::SPushConstantBuffer	pushConstant("Info");
	pushConstant.m_ShaderStagesMask = RHI::ComputeShaderMask;
	pushConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "CurrentBit"));
	pushConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "GroupCount"));
	bindings.m_PushConstants.PushBack(pushConstant);

	if (keyStride64)
		bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::ComputeShaderStage, "KEYSTRIDE64"));

	bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::ComputeShaderStage, "NUM_KEY_PER_THREAD", CString::Format("%d", PK_GPU_SORT_NUM_KEY_PER_THREAD)));
}

//----------------------------------------------------------------------------

void	AddSortDownSweepDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;

	description.m_Pipeline = RHI::Cs;
	description.m_DrawMode = RHI::DrawModeInvalid;
	description.m_DispatchThreadSize = CUint3(PK_RH_GPU_THREADGROUP_SIZE, 1, 1);

	FillSortDownSweepShaderBindings(description.m_Bindings, false);

	shaders.PushBack();
	shaders.Last().m_ComputeShader = "SortDownSweep.comp";
	shaders.Last().m_ShaderDescriptions.PushBack(description);

	FillSortDownSweepShaderBindings(description.m_Bindings, true);

	shaders.PushBack();
	shaders.Last().m_ComputeShader = "SortDownSweep.comp";
	shaders.Last().m_ShaderDescriptions.PushBack(description);
}

//----------------------------------------------------------------------------

void	FillFXAAShaderBindings(RHI::SShaderBindings &bindings, bool lumaInAlpha)
{
	bindings.Reset();
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat2;

	RHI::SConstantSetLayout		layout;
	layout.m_ShaderStagesMask = RHI::FragmentShaderMask;
	layout.AddConstantsLayout(RHI::SConstantSamplerDesc("Texture", RHI::SamplerTypeSingle));
	bindings.m_ConstantSets.PushBack(layout);

	RHI::SPushConstantBuffer	pushConstantBuffer("PushConstantBuffer", RHI::FragmentShaderMask);
	pushConstantBuffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeInt2, "Resolution"));
	bindings.m_PushConstants.PushBack(pushConstantBuffer);

	if (lumaInAlpha)
		bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::FragmentShaderMask, "LUMAINALPHA"));
}

//----------------------------------------------------------------------------

void	AddFXAADefinition(TArray<SShaderCombination> &shaders)
{
	{
		RHI::SShaderDescription		description;

		description.m_Pipeline = RHI::VsPs;
		description.m_DrawMode = RHI::DrawModeTriangle;

		// Vertex output
		description.m_VertexOutput.PushBack();
		description.m_VertexOutput.Last().m_Name = "fragTexCoord";
		description.m_VertexOutput.Last().m_Type = RHI::TypeFloat2;

		// Fragment output
		description.m_FragmentOutput.PushBack();
		description.m_FragmentOutput.Last().m_Name = "outColor";
		description.m_FragmentOutput.Last().m_Type = RHI::FlagRGBA;

		FillFXAAShaderBindings(description.m_Bindings, true);

		shaders.PushBack();
		shaders.Last().m_VertexShader = "FullScreenQuad.vert";
		shaders.Last().m_FragmentShader = "FXAA.frag";
		shaders.Last().m_ShaderDescriptions.PushBack(description);

		FillFXAAShaderBindings(description.m_Bindings, false);

		shaders.PushBack();
		shaders.Last().m_VertexShader = "FullScreenQuad.vert";
		shaders.Last().m_FragmentShader = "FXAA.frag";
		shaders.Last().m_ShaderDescriptions.PushBack(description);
	}
}

//----------------------------------------------------------------------------

void	CreateComputeRibbonSortKeysConstantSetLayout(RHI::SConstantSetLayout &layout)
{
	layout.Reset();
	layout.m_ShaderStagesMask = RHI::ComputeShaderMask;
	layout.AddConstantsLayout(RHI::SRawBufferDesc("StreamInfo", true));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("GPUSimData", true));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("SelfIDsOffsets", true));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("ParentIDsOffsets", true));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("EnabledsOffsets", true));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("OutSortKeys", false));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("OutIndirection", false));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("OutIndirectDraw", false));
}

//----------------------------------------------------------------------------

void	FillComputeRibbonSortKeysShaderBindings(RHI::SShaderBindings &bindings)
{
	bindings.Reset();
	RHI::SConstantSetLayout	layout;
	CreateComputeRibbonSortKeysConstantSetLayout(layout);
	bindings.m_ConstantSets.PushBack(layout);

	RHI::SPushConstantBuffer	pushConstant("Info");
	pushConstant.m_ShaderStagesMask = RHI::ComputeShaderMask;
	pushConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "DrawRequest"));
	bindings.m_PushConstants.PushBack(pushConstant);
}

//----------------------------------------------------------------------------

void	AddComputeRibbonSortKeysDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;

	description.m_Pipeline = RHI::Cs;
	description.m_DrawMode = RHI::DrawModeInvalid;
	description.m_DispatchThreadSize = CUint3(PK_RH_GPU_THREADGROUP_SIZE, 1, 1);

	FillComputeRibbonSortKeysShaderBindings(description.m_Bindings);

	shaders.PushBack();
	shaders.Last().m_ComputeShader = "ComputeRibbonSortKeys.comp";
	shaders.Last().m_ShaderDescriptions.PushBack(description);
}

//----------------------------------------------------------------------------

void	CreateComputeCubemapConstantSetLayout(RHI::SConstantSetLayout &layout, bool inputTxtIsLatLong)
{
	layout.Reset();
	layout.m_ShaderStagesMask = RHI::ComputeShaderMask;

	RHI::SConstantBufferDesc	buffer("CubemapData");
	buffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "Face"));
	buffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "Size"));

	layout.AddConstantsLayout(buffer);
	if (!inputTxtIsLatLong)
		layout.AddConstantsLayout(RHI::SConstantSamplerDesc("InputTextureCube", RHI::SamplerTypeCube));
	else
		layout.AddConstantsLayout(RHI::SConstantSamplerDesc("InputTextureLatlong", RHI::SamplerTypeSingle));

	layout.AddConstantsLayout(RHI::STextureStorageDesc("OutData", false));
}

//----------------------------------------------------------------------------

void	FillComputeCubemapShaderBindings(RHI::SShaderBindings &bindings, bool inputTxtIsLatLong)
{
	bindings.Reset();
	RHI::SConstantSetLayout	layout;
	CreateComputeCubemapConstantSetLayout(layout, inputTxtIsLatLong);
	bindings.m_ConstantSets.PushBack(layout);
	bindings.m_Defines.PushBack();
	bindings.m_Defines.Last().m_ShaderStages = RHI::ComputeShaderMask;
	if (inputTxtIsLatLong)
		bindings.m_Defines.Last().m_Define = "INPUTLATLONG";
	else
		bindings.m_Defines.Last().m_Define = "INPUTCUBE";
}

//----------------------------------------------------------------------------

void	AddComputeCubemapDefinition(TArray<SShaderCombination> &shaders)
{
	{
		RHI::SShaderDescription		description;

		description.m_Pipeline = RHI::Cs;
		description.m_DrawMode = RHI::DrawModeInvalid;
		description.m_DispatchThreadSize = CUint3(PK_RH_GPU_THREADGROUP_SIZE_2D, PK_RH_GPU_THREADGROUP_SIZE_2D, 1);

		FillComputeCubemapShaderBindings(description.m_Bindings, true);

		shaders.PushBack();
		shaders.Last().m_ComputeShader = "ComputeCubemap.comp";
		shaders.Last().m_ShaderDescriptions.PushBack(description);
	}
	{
		RHI::SShaderDescription		description;

		description.m_Pipeline = RHI::Cs;
		description.m_DrawMode = RHI::DrawModeInvalid;
		description.m_DispatchThreadSize = CUint3(PK_RH_GPU_THREADGROUP_SIZE_2D, PK_RH_GPU_THREADGROUP_SIZE_2D, 1);

		FillComputeCubemapShaderBindings(description.m_Bindings, false);

		shaders.PushBack();
		shaders.Last().m_ComputeShader = "ComputeCubemap.comp";
		shaders.Last().m_ShaderDescriptions.PushBack(description);
	}
}

//----------------------------------------------------------------------------

void	CreateFilterCubemapConstantSetLayout(RHI::SConstantSetLayout &layout)
{
	layout.Reset();
	layout.m_ShaderStagesMask = RHI::ComputeShaderMask;

	RHI::SConstantBufferDesc	buffer("CubemapData");
	buffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "Face"));
	buffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat, "Roughness"));
	buffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "Size"));
	buffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "SampleCount"));
	buffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "PerformedSampleCount"));
	buffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "TotalSampleCount"));

	layout.AddConstantsLayout(buffer);

	layout.AddConstantsLayout(RHI::SConstantSamplerDesc("InputTextureCube", RHI::SamplerTypeCube));
	
	layout.AddConstantsLayout(RHI::STextureStorageDesc("OutData", false, RHI::FormatFloat16RGBA));
}

//----------------------------------------------------------------------------

void	FillFilterCubemapShaderBindings(RHI::SShaderBindings &bindings)
{
	bindings.Reset();
	RHI::SConstantSetLayout	layout;
	CreateFilterCubemapConstantSetLayout(layout);
	bindings.m_ConstantSets.PushBack(layout);
}

//----------------------------------------------------------------------------

void	AddFilterCubemapDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;

	description.m_Pipeline = RHI::Cs;
	description.m_DrawMode = RHI::DrawModeInvalid;
	description.m_DispatchThreadSize = CUint3(PK_RH_GPU_THREADGROUP_SIZE_2D, PK_RH_GPU_THREADGROUP_SIZE_2D, 1);

	FillFilterCubemapShaderBindings(description.m_Bindings);

	shaders.PushBack();
	shaders.Last().m_ComputeShader = "FilterCubemap.comp";
	shaders.Last().m_ShaderDescriptions.PushBack(description);
}

//----------------------------------------------------------------------------

void	CreateComputeMipMapConstantSetLayout(RHI::SConstantSetLayout &layout)
{
	layout.Reset();
	layout.m_ShaderStagesMask = RHI::ComputeShaderMask;

	RHI::SConstantBufferDesc	buffer("Data");
	buffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat2, "TexelSize"));

	layout.AddConstantsLayout(buffer);

	layout.AddConstantsLayout(RHI::SConstantSamplerDesc("InputTexture", RHI::SamplerTypeSingle));
	
	layout.AddConstantsLayout(RHI::STextureStorageDesc("OutputTexture", false));
}

//----------------------------------------------------------------------------

void	FillComputeMipMapShaderBindings(RHI::SShaderBindings &bindings)
{
	bindings.Reset();
	RHI::SConstantSetLayout	layout;
	CreateComputeMipMapConstantSetLayout(layout);
	bindings.m_ConstantSets.PushBack(layout);
}

//----------------------------------------------------------------------------

void	AddComputeMipMapDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;

	description.m_Pipeline = RHI::Cs;
	description.m_DrawMode = RHI::DrawModeInvalid;
	description.m_DispatchThreadSize = CUint3(PK_RH_GPU_THREADGROUP_SIZE_2D, PK_RH_GPU_THREADGROUP_SIZE_2D, 1);

	FillComputeMipMapShaderBindings(description.m_Bindings);

	shaders.PushBack();
	shaders.Last().m_ComputeShader = "ComputeMipMap.comp";
	shaders.Last().m_ShaderDescriptions.PushBack(description);
}

//----------------------------------------------------------------------------

void	CreateBlurCubemapRenderFaceConstantSetLayout(RHI::SConstantSetLayout &layout)
{
	layout.Reset();
	layout.m_ShaderStagesMask = RHI::ComputeShaderMask;

	RHI::SConstantBufferDesc	buffer("Data");
	buffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "Face"));
	buffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "MipLevel"));
	buffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "Size"));
	buffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "Padding"));

	layout.AddConstantsLayout(buffer);

	layout.AddConstantsLayout(RHI::SConstantSamplerDesc("InputCubeTexture", RHI::SamplerTypeCube));
	
	layout.AddConstantsLayout(RHI::STextureStorageDesc("OutputTexture", false));
}

//----------------------------------------------------------------------------

void	FillBlurCubemapRenderFaceShaderBindings(RHI::SShaderBindings &bindings)
{
	bindings.Reset();
	RHI::SConstantSetLayout	layout;
	CreateBlurCubemapRenderFaceConstantSetLayout(layout);
	bindings.m_ConstantSets.PushBack(layout);
}

//----------------------------------------------------------------------------

void	AddBlurCubemapRenderFaceDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;

	description.m_Pipeline = RHI::Cs;
	description.m_DrawMode = RHI::DrawModeInvalid;
	description.m_DispatchThreadSize = CUint3(PK_RH_GPU_THREADGROUP_SIZE_2D, PK_RH_GPU_THREADGROUP_SIZE_2D, 1);

	FillBlurCubemapRenderFaceShaderBindings(description.m_Bindings);

	shaders.PushBack();
	shaders.Last().m_ComputeShader = "RenderCubemapFace.comp";
	shaders.Last().m_ShaderDescriptions.PushBack(description);
}

//----------------------------------------------------------------------------

void	CreateBlurCubemapProcessConstantSetLayout(RHI::SConstantSetLayout &layout)
{
	layout.Reset();
	layout.m_ShaderStagesMask = RHI::ComputeShaderMask;

	RHI::SConstantBufferDesc	buffer("Data");
	buffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "Size"));
	buffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "KernelSize"));
	buffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat, "HalfAngle"));

	layout.AddConstantsLayout(buffer);

	layout.AddConstantsLayout(RHI::SConstantSamplerDesc("InputTexture", RHI::SamplerTypeSingle));
	
	layout.AddConstantsLayout(RHI::STextureStorageDesc("OutputTexture", false));
}

//----------------------------------------------------------------------------

void	FillBlurCubemapProcessShaderBindings(RHI::SShaderBindings &bindings)
{
	bindings.Reset();
	RHI::SConstantSetLayout	layout;
	CreateBlurCubemapProcessConstantSetLayout(layout);
	bindings.m_ConstantSets.PushBack(layout);
}

//----------------------------------------------------------------------------

void	AddBlurCubemapProcessDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;
	 
	description.m_Pipeline = RHI::Cs;
	description.m_DrawMode = RHI::DrawModeInvalid;
	description.m_DispatchThreadSize = CUint3(PK_RH_GPU_THREADGROUP_SIZE_2D, PK_RH_GPU_THREADGROUP_SIZE_2D, 1);

	FillBlurCubemapProcessShaderBindings(description.m_Bindings);

	shaders.PushBack();
	shaders.Last().m_ComputeShader = "BlurCubemap.comp";
	shaders.Last().m_ShaderDescriptions.PushBack(description);
}

//----------------------------------------------------------------------------

void	CreateCubemapSamplerConstantSetLayout(RHI::SConstantSetLayout &layout)
{
	layout.m_ShaderStagesMask = RHI::FragmentShaderMask;
	RHI::SConstantSamplerDesc environmentMapSamplerDesc("EnvironmentMapSampler", RHI::SamplerTypeCube);
	layout.AddConstantsLayout(environmentMapSamplerDesc);
	RHI::SConstantBufferDesc	envMapInfo = RHI::SConstantBufferDesc("EnvironmentMapInfo");
	envMapInfo.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat2x2, "Rotation"));
	layout.AddConstantsLayout(envMapInfo);
}

//----------------------------------------------------------------------------

__PK_SAMPLE_API_END
