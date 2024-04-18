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
#include "GBuffer.h"

#include <PK-SampleLib/ShaderLoader.h>
#include <PK-SampleLib/ShaderDefinitions/BasicSceneShaderDefinitions.h>
#include <PK-SampleLib/ShaderDefinitions/UnitTestsShaderDefinitions.h>
#include <PK-SampleLib/ShaderDefinitions/EditorShaderDefinitions.h>
#include <PK-SampleLib/ShaderDefinitions/SampleLibShaderDefinitions.h>

#include <pk_rhi/include/PixelFormatFallbacks.h>
#include <pk_rhi/include/interfaces/SApiContext.h>

//----------------------------------------------------------------------------

#define	GBUFFER_VERTEX_SHADER_PATH					"./Shaders/SolidMesh.vert"
#define	GBUFFER_FRAGMENT_SHADER_PATH				"./Shaders/GBuffer.frag"
#define	FULL_SCREEN_QUAD_VERTEX_SHADER_PATH			"./Shaders/FullScreenQuad.vert"
#define	DEFERRED_LIGHTS_FRAGMENT_SHADER_PATH		"./Shaders/DeferredLights.frag"
#define	MERGING_FRAGMENT_SHADER_PATH				"./Shaders/DeferredMerging.frag"

// light particle shaders:
#define	LIGHT_INSTANCED_VERTEX_SHADER_PATH			"./Shaders/LightInstanced.vert"
#define	LIGHT_INSTANCED_FRAGMENT_SHADER_PATH		"./Shaders/PointLightInstanced.frag"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

const RHI::EPixelFormat	CGBuffer::s_MergeBufferFormat				= RHI::FormatFloat16RGBA;
const RHI::EPixelFormat	CGBuffer::s_LightAccumBufferFormat			= RHI::FormatFloat16RGBA;
const RHI::EPixelFormat	CGBuffer::s_DepthBufferFormat				= RHI::FormatUnorm24DepthUint8Stencil;
const RHI::EPixelFormat	CGBuffer::s_NormalRoughMetalBufferFormat	= RHI::FormatFloat16RGBA;

CGBuffer::CGBuffer()
{
	// Set all RT indices to invalid:
	m_MergeBufferIndex = CGuid::INVALID;
	m_DepthBufferIndex = CGuid::INVALID;
	m_LightAccumBufferIndex = CGuid::INVALID;
	m_NormalRoughMetalBufferIndex = CGuid::INVALID;
	m_MergeClearValue = RHI::SFrameBufferClearValue(0, 0, 0, 0);
}

//----------------------------------------------------------------------------

CGBuffer::~CGBuffer()
{
}

//----------------------------------------------------------------------------

bool	CGBuffer::Init(	const RHI::PApiManager &apiManager,
						const RHI::PConstantSampler &samplerRT,
						const RHI::SConstantSetLayout &samplerRTLayout,
						const RHI::SConstantSetLayout &sceneInfoLayout)
{
	m_ApiManager = apiManager;
	m_SamplerRT = samplerRT;
	m_SamplerRTLayout = samplerRTLayout;
	m_SceneInfoLayout = sceneInfoLayout;

	// ----------------------------
	// Create the vertex buffers:
	// ----------------------------
	if (!Utils::CreateGpuBuffers(m_ApiManager, Utils::ScreenQuadHelper, m_QuadBuffers))
		return false;
//	if (!CreateLightSphere(30))
//		return false;
//	if (!CreateDecalCube())
//		return false;

	// ----------------------------
	// Create the constant set layouts:
	// ----------------------------
	// Init the lighting constant layout
	CreateLightingSceneInfoConstantLayout(m_LightInfoConstLayout, m_ShadowsInfoConstLayout, m_BRDFLUTLayout, m_EnvironmentMapLayout);
	CreateDeferredSamplerLightingConstSetLayout(m_LightingSamplersConstLayout);

	// Init the GBuffer mesh rendering constant sets
	for (u32 i = 0; i < static_cast<u32>(GBufferCombination_Count); ++i)
	{
		CreateGBufferConstSetLayouts(static_cast<EGBufferCombination>(i), m_GBufferMeshInfoConstLayout[i]);
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CGBuffer::UpdateFrameBuffer(const TMemoryView<const RHI::PFrameBuffer> &frameBuffers,
									TArray<RHI::ELoadRTOperation> &loadOP,
									TArray<RHI::SFrameBufferClearValue> &clearValues,
									bool clearMergingBuffer,
									const SInOutRenderTargets *inOut)
{
	// Create all render targets
	const RHI::SGPUCaps		&caps = m_ApiManager->GetApiContext()->m_GPUCaps;
	CUint2					frameBufferSize = frameBuffers.First()->GetSize();
	RHI::EPixelFormat		idealDepthFormat = RHI::PixelFormatFallbacks::FindClosestSupportedDepthStencilFormat(caps, s_DepthBufferFormat);

	// Set up framebuffer layouts
	// N	- Merge
	// N+1	- Depth
	// N+2	- distortion
	// N+3	- post-distortion
	// N+4	- light accu
	// N+5	- normal rough metal

	// Create the render targets:
	// Merge render target
	if (inOut->m_OutputRtIdx.Valid())
	{
		m_MergeBufferIndex = inOut->m_OutputRtIdx;
	}
	else
	{
		if (!PK_VERIFY(m_Merge.CreateRenderTarget(RHI::SRHIResourceInfos("Merge Render Target"), m_ApiManager, m_SamplerRT, s_MergeBufferFormat, frameBufferSize, m_SamplerRTLayout)))
			return false;
	}
	// Depth
	if (!PK_VERIFY(m_Depth.CreateRenderTarget(RHI::SRHIResourceInfos("Depth Render Target"), m_ApiManager, m_SamplerRT, idealDepthFormat, frameBufferSize, m_SamplerRTLayout)))
		return false;
	// Light accu
	if (!PK_VERIFY(m_LightAccu.CreateRenderTarget(RHI::SRHIResourceInfos("LightAccu Render Target"), m_ApiManager, m_SamplerRT, s_LightAccumBufferFormat, frameBufferSize, m_SamplerRTLayout)))
		return false;
	// Normal rough metal
	if (!PK_VERIFY(m_NormalRoughMetal.CreateRenderTarget(RHI::SRHIResourceInfos("NormalRoughMetal Render Target"), m_ApiManager, m_SamplerRT, s_NormalRoughMetalBufferFormat, frameBufferSize, m_SamplerRTLayout)))
		return false;

	// Add the render targets to the frame buffer:
	for (u32 i = 0; i < frameBuffers.Count(); ++i)
	{
		PK_ASSERT(frameBufferSize == frameBuffers[i]->GetSize());

		if (!inOut->m_OutputRtIdx.Valid())
		{
			// Merge render target
			m_MergeBufferIndex = frameBuffers[i]->GetRenderTargets().Count();
			if (!PK_VERIFY(frameBuffers[i]->AddRenderTarget(m_Merge.m_RenderTarget)))
				return false;
		}
		// Depth
		m_DepthBufferIndex = frameBuffers[i]->GetRenderTargets().Count();
		if (!PK_VERIFY(frameBuffers[i]->AddRenderTarget(m_Depth.m_RenderTarget)))
			return false;
		// Light accu
		m_LightAccumBufferIndex = frameBuffers[i]->GetRenderTargets().Count();
		if (!PK_VERIFY(frameBuffers[i]->AddRenderTarget(m_LightAccu.m_RenderTarget)))
			return false;
		// Normal rough metal
		m_NormalRoughMetalBufferIndex = frameBuffers[i]->GetRenderTargets().Count();
		if (!PK_VERIFY(frameBuffers[i]->AddRenderTarget(m_NormalRoughMetal.m_RenderTarget)))
			return false;
	}

	// Add the load op and the clear values:
	// Merge render target
	if (!inOut->m_OutputRtIdx.Valid())
	{
		if (clearMergingBuffer)
		{
			if (!PK_VERIFY(loadOP.PushBack(RHI::LoadClear).Valid() &&
				clearValues.PushBack(m_MergeClearValue).Valid()))
				return false;
		}
		else
		{
			if (!PK_VERIFY(loadOP.PushBack(RHI::LoadKeepValue).Valid()))
				return false;
		}
	}
	// Depth
	if (!PK_VERIFY(	loadOP.PushBack(RHI::LoadClear).Valid() &&
					clearValues.PushBack(RHI::SFrameBufferClearValue(1.f, 0U)).Valid()))
		return false;
	// Light accu
	if (!PK_VERIFY(	loadOP.PushBack(RHI::LoadClear).Valid() &&
					clearValues.PushBack(RHI::SFrameBufferClearValue(0.f, 0.f, 0.f, 0.f)).Valid()))
		return false;
	// Normal rough metal
	if (!PK_VERIFY(	loadOP.PushBack(RHI::LoadClear).Valid() &&
					clearValues.PushBack(RHI::SFrameBufferClearValue(0.f, 0.f, 0.f, 0.f)).Valid()))
		return false;

	bool	result;

	PK_ASSERT(inOut->m_SamplableDepthRtIdx.Valid() && inOut->m_SamplableDepthRtIdx < frameBuffers.First()->GetRenderTargets().Count());
	PK_ASSERT(inOut->m_SamplableDiffuseRtIdx.Valid() && inOut->m_SamplableDiffuseRtIdx < frameBuffers.First()->GetRenderTargets().Count());
	PK_ASSERT(inOut->m_SamplableDiffuseRt != null && inOut->m_SamplableDepthRt != null);
	PK_ASSERT(inOut->m_SamplableDiffuseRt->m_RenderTarget != null && inOut->m_SamplableDepthRt->m_RenderTarget != null);
	PK_ASSERT(inOut->m_SamplableDiffuseRt->m_RenderTarget->GetTexture() != null && inOut->m_SamplableDepthRt->m_RenderTarget->GetTexture() != null);

	// ----------------------------
	// Lighting:
	// ----------------------------
	// Create constants set IFN
	if (m_LightingSamplersSet == null)
	{
		m_LightingSamplersSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Lighting Constant Set"), m_LightingSamplersConstLayout);
		if (m_LightingSamplersSet == null)
			return false;
	}

	// Bind the new render targets:
	result = m_LightingSamplersSet->SetConstants(m_SamplerRT, inOut->m_SamplableDepthRt->m_RenderTarget->GetTexture(), 0);
	result &= m_LightingSamplersSet->SetConstants(m_SamplerRT, m_NormalRoughMetal.m_RenderTarget->GetTexture(), 1);
	result &= m_LightingSamplersSet->SetConstants(m_SamplerRT, inOut->m_SamplableDiffuseRt->m_RenderTarget->GetTexture(), 2);
	result &= m_LightingSamplersSet->UpdateConstantValues();

	if (result == false)
		return false;

	// ----------------------------
	// Merging:
	// ----------------------------

	// Init the Merging constant layout
	CreateDeferredMergingConstSetLayouts(m_MergingSamplersConstLayout);

	// Create constants set IFN
	if (m_MergingSamplersSet == null)
	{
		m_MergingSamplersSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Merging Constant Set"), m_MergingSamplersConstLayout);
		if (m_MergingSamplersSet == null)
			return false;
	}

	// Bind the new render targets:
	result = m_MergingSamplersSet->SetConstants(m_SamplerRT, m_LightAccu.m_RenderTarget->GetTexture(), 0);
	for (u32 i = 0; i < inOut->m_OpaqueRTs.Count(); ++i)
	{
		PK_ASSERT(inOut->m_OpaqueRTs[i].m_RenderTarget != null);
		PK_ASSERT(inOut->m_OpaqueRTs[i].m_RenderTarget->GetTexture() != null);
		result &= m_MergingSamplersSet->SetConstants(m_SamplerRT, inOut->m_OpaqueRTs[i].m_RenderTarget->GetTexture(), i + 1);
	}
	result &= m_MergingSamplersSet->UpdateConstantValues();
	return true;
}

//----------------------------------------------------------------------------

bool	CGBuffer::AddSubPasses(	const RHI::PRenderPass &renderPass,
								const SInOutRenderTargets *inOut)
{
	bool	result = true;

	result &= AddGBufferSubPass(renderPass, inOut->m_OpaqueRTsIdx);
	result &= AddDecalSubPass(renderPass, inOut->m_OpaqueRTsIdx, inOut->m_SamplableDepthRtIdx);
	result &= AddLightingSubPass(renderPass, inOut->m_SamplableDepthRtIdx, inOut->m_SamplableDiffuseRtIdx);
	result &= AddMergingSubPass(renderPass, inOut->m_OpaqueRTsIdx);
	return result;
}

//----------------------------------------------------------------------------

bool	CGBuffer::CreateRenderStates(	const TMemoryView<const RHI::SRenderTargetDesc> &frameBufferLayout,
										CShaderLoader &shaderLoader,
										const RHI::PRenderPass &renderPass)
{
	bool	result = true;

	// ----------------------------
	// Create the render states:
	// ----------------------------
	result &= CreateGBufferRenderStates(frameBufferLayout, shaderLoader, renderPass);
	result &= CreateLightingRenderStates(frameBufferLayout, shaderLoader, renderPass);
	result &= CreateParticleLightingRenderStates(frameBufferLayout, shaderLoader, renderPass);
	result &= CreateMergingRenderStates(frameBufferLayout, shaderLoader, renderPass);
	return result;
}

//----------------------------------------------------------------------------

void	CGBuffer::SetClearMergeBufferColor(const CFloat4 &clearColor)
{
	*((CFloat4*)m_MergeClearValue.m_ColorFloat) = clearColor;
}

//----------------------------------------------------------------------------

bool	CGBuffer::CreateGBufferRenderStates(const TMemoryView<const RHI::SRenderTargetDesc> &frameBufferLayout, CShaderLoader &shaderLoader, const RHI::PRenderPass &renderPass)
{
	// We create the render states in this order:
	// -----------------------------------------
	//	Material_Diffuse
	//	Material_Diffuse_Normal
	//	Material_Diffuse_Specular
	//	Material_Diffuse_Normal_Specular

	for (u32 i = 0; i < static_cast<u32>(GBufferCombination_Count); ++i)
	{
		RHI::PRenderState	material = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("GBuffer Render State"));
		if (material == null)
			return false;
		RHI::SRenderState	&GBufferRenderState = material->m_RenderState;
		// -------------------------------
		// GBUFFER - Bindings
		// -------------------------------
		const bool	hasVertexColor = false;
		const bool	hasTangent = i == GBufferCombination_Diffuse_RoughMetal_Normal;
		FillGBufferShaderBindings(GBufferRenderState.m_ShaderBindings, m_SceneInfoLayout, m_GBufferMeshInfoConstLayout[i], hasVertexColor, hasTangent);

		GBufferRenderState.m_ShaderBindings.m_InputAttributes[0].m_BufferIdx = 0;
		GBufferRenderState.m_ShaderBindings.m_InputAttributes[1].m_BufferIdx = 1;
		GBufferRenderState.m_ShaderBindings.m_InputAttributes[2].m_BufferIdx = 2;
		if (hasTangent)
		{
			GBufferRenderState.m_ShaderBindings.m_InputAttributes[3].m_BufferIdx = 3;
			if (!GBufferRenderState.m_InputVertexBuffers.Resize(4))
				return false;
			GBufferRenderState.m_InputVertexBuffers[0].m_Stride = sizeof(CFloat3); // pos
			GBufferRenderState.m_InputVertexBuffers[1].m_Stride = sizeof(CFloat3); // normal
			GBufferRenderState.m_InputVertexBuffers[2].m_Stride = sizeof(CFloat4); // tangent
			GBufferRenderState.m_InputVertexBuffers[3].m_Stride = sizeof(CFloat2); // texcoords
		}
		else
		{
			if (!GBufferRenderState.m_InputVertexBuffers.Resize(3))
				return false;
			GBufferRenderState.m_InputVertexBuffers[0].m_Stride = sizeof(CFloat3); // pos
			GBufferRenderState.m_InputVertexBuffers[1].m_Stride = sizeof(CFloat3); // normal
			GBufferRenderState.m_InputVertexBuffers[2].m_Stride = sizeof(CFloat2); // texcoords
		}

		// -------------------------------
		// GBUFFER - Pipeline State
		// -------------------------------
		GBufferRenderState.m_PipelineState.m_DynamicScissor = true;
		GBufferRenderState.m_PipelineState.m_DynamicViewport = true;
		GBufferRenderState.m_PipelineState.m_DepthWrite = true;
		GBufferRenderState.m_PipelineState.m_DepthTest = RHI::Less;
		GBufferRenderState.m_PipelineState.m_CullMode = RHI::CullBackFaces;

		CShaderLoader::SShadersPaths	shadersPaths;
		shadersPaths.m_Vertex	= GBUFFER_VERTEX_SHADER_PATH;
		shadersPaths.m_Fragment	= GBUFFER_FRAGMENT_SHADER_PATH;

		if (!shaderLoader.LoadShader(GBufferRenderState, shadersPaths, m_ApiManager))
			return false;
		if (!m_ApiManager->BakeRenderState(material, frameBufferLayout, renderPass, m_GBufferSubPassIdx))
			return false;

		m_GBufferMaterials[i] = material;
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CGBuffer::CreateLightingRenderStates(const TMemoryView<const RHI::SRenderTargetDesc> &frameBufferLayout, CShaderLoader &shaderLoader, const RHI::PRenderPass &renderPass)
{
	// -------------------------------
	// LIGHTING
	// -------------------------------
	m_LightsRenderState = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("Lighting Render State"));

	if (m_LightsRenderState == null)
		return false;

	RHI::SRenderState		&lightingRenderState = m_LightsRenderState->m_RenderState;

	lightingRenderState.m_PipelineState.m_DynamicScissor = true;
	lightingRenderState.m_PipelineState.m_DynamicViewport = true;

	// Additive blending - one render pass for each light
	lightingRenderState.m_PipelineState.m_Blending = true;
	lightingRenderState.m_PipelineState.m_ColorBlendingEquation = RHI::BlendAdd;
	lightingRenderState.m_PipelineState.m_ColorBlendingDst = RHI::BlendOne;
	lightingRenderState.m_PipelineState.m_ColorBlendingSrc = RHI::BlendOne;
	lightingRenderState.m_PipelineState.m_AlphaBlendingDst = RHI::BlendOne;
	lightingRenderState.m_PipelineState.m_AlphaBlendingSrc = RHI::BlendOne;
	lightingRenderState.m_PipelineState.m_CullMode = RHI::CullBackFaces;

	if (!lightingRenderState.m_InputVertexBuffers.PushBack().Valid())
		return false;
	lightingRenderState.m_InputVertexBuffers.Last().m_Stride = sizeof(CFloat2);

	FillDeferredLightShaderBindings(lightingRenderState.m_ShaderBindings,
									m_SceneInfoLayout,
									m_LightInfoConstLayout,
									m_ShadowsInfoConstLayout,
									m_BRDFLUTLayout,
									m_EnvironmentMapLayout,
									m_LightingSamplersConstLayout);

	CShaderLoader::SShadersPaths	lightsShaderPath;
	lightsShaderPath.m_Vertex	= FULL_SCREEN_QUAD_VERTEX_SHADER_PATH;
	lightsShaderPath.m_Fragment = DEFERRED_LIGHTS_FRAGMENT_SHADER_PATH;

	if (shaderLoader.LoadShader(m_LightsRenderState->m_RenderState, lightsShaderPath, m_ApiManager) == false)
		return false;
	if (!m_ApiManager->BakeRenderState(m_LightsRenderState, frameBufferLayout, renderPass, m_LightingSubPassIdx))
		return false;
	return true;
}

//----------------------------------------------------------------------------

bool	CGBuffer::CreateParticleLightingRenderStates(const TMemoryView<const RHI::SRenderTargetDesc> &frameBufferLayout, CShaderLoader &shaderLoader, const RHI::PRenderPass &renderPass)
{
	// -------------------------------
	// LIGHTING
	// -------------------------------
	m_ParticlePointLightRenderState = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("Particle Point Light Render State"));

	if (m_ParticlePointLightRenderState == null)
		return false;

	RHI::SRenderState		&lightingRenderState = m_ParticlePointLightRenderState->m_RenderState;

	lightingRenderState.m_PipelineState.m_DynamicScissor = true;
	lightingRenderState.m_PipelineState.m_DynamicViewport = true;

	// Additive blending - one render pass for each light
	lightingRenderState.m_PipelineState.m_Blending = true;
	lightingRenderState.m_PipelineState.m_ColorBlendingEquation = RHI::BlendAdd;
	lightingRenderState.m_PipelineState.m_ColorBlendingDst = RHI::BlendOne;
	lightingRenderState.m_PipelineState.m_ColorBlendingSrc = RHI::BlendOne;
	lightingRenderState.m_PipelineState.m_AlphaBlendingDst = RHI::BlendOne;
	lightingRenderState.m_PipelineState.m_AlphaBlendingSrc = RHI::BlendOne;

	// full screen quad:
	if (!lightingRenderState.m_InputVertexBuffers.PushBack().Valid())
		return false;
	lightingRenderState.m_InputVertexBuffers.Last().m_Stride = sizeof(CFloat3);

	// light position:
	if (!lightingRenderState.m_InputVertexBuffers.PushBack().Valid())
		return false;
	lightingRenderState.m_InputVertexBuffers.Last().m_Stride = sizeof(CFloat3);
	lightingRenderState.m_InputVertexBuffers.Last().m_InputRate = RHI::PerInstanceInput;
	// light radius:
	if (!lightingRenderState.m_InputVertexBuffers.PushBack().Valid())
		return false;
	lightingRenderState.m_InputVertexBuffers.Last().m_Stride = sizeof(float);
	lightingRenderState.m_InputVertexBuffers.Last().m_InputRate = RHI::PerInstanceInput;
	// light color:
	if (!lightingRenderState.m_InputVertexBuffers.PushBack().Valid())
		return false;
	lightingRenderState.m_InputVertexBuffers.Last().m_Stride = sizeof(CFloat3);
	lightingRenderState.m_InputVertexBuffers.Last().m_InputRate = RHI::PerInstanceInput;

	FillParticleLightShaderBindings(lightingRenderState.m_ShaderBindings);

	CShaderLoader::SShadersPaths	pointLightShadersPaths;
	pointLightShadersPaths.m_Vertex = LIGHT_INSTANCED_VERTEX_SHADER_PATH;
	pointLightShadersPaths.m_Fragment = LIGHT_INSTANCED_FRAGMENT_SHADER_PATH;

	if (shaderLoader.LoadShader(m_ParticlePointLightRenderState->m_RenderState, pointLightShadersPaths, m_ApiManager) == false)
		return false;
	if (!m_ApiManager->BakeRenderState(m_ParticlePointLightRenderState, frameBufferLayout, renderPass, m_LightingSubPassIdx))
		return false;
	return true;
}

//----------------------------------------------------------------------------

bool	CGBuffer::CreateMergingRenderStates(const TMemoryView<const RHI::SRenderTargetDesc> &frameBufferLayout, CShaderLoader &shaderLoader, const RHI::PRenderPass &renderPass)
{
	// -------------------------------
	// MERGING
	// -------------------------------
	m_MergingRenderState = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("Merging Render State"));
	if (m_MergingRenderState == null)
		return false;
	RHI::SRenderState		&mergingRenderState = m_MergingRenderState->m_RenderState;

	mergingRenderState.m_PipelineState.m_DynamicScissor = true;
	mergingRenderState.m_PipelineState.m_DynamicViewport = true;

	mergingRenderState.m_PipelineState.m_DepthTest = RHI::Greater;

	if (!mergingRenderState.m_InputVertexBuffers.PushBack().Valid())
		return false;
	mergingRenderState.m_InputVertexBuffers.Last().m_Stride = sizeof(CFloat2);

	FillDeferredMergingShaderBindings(mergingRenderState.m_ShaderBindings, m_MergingSamplersConstLayout);

	CShaderLoader::SShadersPaths shadersPaths;
	shadersPaths.m_Vertex	= FULL_SCREEN_QUAD_VERTEX_SHADER_PATH;
	shadersPaths.m_Fragment	= MERGING_FRAGMENT_SHADER_PATH;

	if (shaderLoader.LoadShader(mergingRenderState, shadersPaths, m_ApiManager) == false)
		return false;
	return m_ApiManager->BakeRenderState(m_MergingRenderState, frameBufferLayout, renderPass, m_MergingSubPassIdx);
}

//----------------------------------------------------------------------------

bool	CGBuffer::AddGBufferSubPass(const RHI::PRenderPass &beforePostFX, const TMemoryView<const CGuid> &outputRTs)
{
	RHI::SSubPassDefinition		subPass;

	subPass.m_DepthStencilRenderTarget = m_DepthBufferIndex;
	for (u32 i = 0; i < outputRTs.Count(); ++i)
	{
		subPass.m_OutputRenderTargets.PushBack(outputRTs[i]);
	}
	subPass.m_OutputRenderTargets.PushBack(m_NormalRoughMetalBufferIndex);
	m_GBufferSubPassIdx = beforePostFX->GetSubPassDefinitions().Count();
	return beforePostFX->AddRenderSubPass(subPass);
}

//----------------------------------------------------------------------------

bool	CGBuffer::AddDecalSubPass(const RHI::PRenderPass &beforePostFX, const TMemoryView<const CGuid> &outputRTs, CGuid samplableDepthBufferIdx)
{
	RHI::SSubPassDefinition		subPass;

	subPass.m_InputRenderTargets.PushBack(samplableDepthBufferIdx);
	subPass.m_DepthStencilRenderTarget = m_DepthBufferIndex;
	for (u32 i = 0; i < outputRTs.Count(); ++i)
	{
		if (outputRTs[i] != samplableDepthBufferIdx)
			subPass.m_OutputRenderTargets.PushBack(outputRTs[i]);
	}
	subPass.m_OutputRenderTargets.PushBack(m_NormalRoughMetalBufferIndex);
	m_DecalSubPassIdx = beforePostFX->GetSubPassDefinitions().Count();
	return beforePostFX->AddRenderSubPass(subPass);
}

//----------------------------------------------------------------------------

bool	CGBuffer::AddLightingSubPass(const RHI::PRenderPass &beforePostFX, CGuid samplableDepthBufferIdx, CGuid samplableDiffuseBufferIdx)
{
	RHI::SSubPassDefinition		subPass;

	subPass.m_OutputRenderTargets.PushBack(m_LightAccumBufferIndex);

	subPass.m_InputRenderTargets.PushBack(m_NormalRoughMetalBufferIndex);
	subPass.m_InputRenderTargets.PushBack(samplableDepthBufferIdx);
	subPass.m_InputRenderTargets.PushBack(samplableDiffuseBufferIdx);

	subPass.m_DepthStencilRenderTarget = m_DepthBufferIndex;

	m_LightingSubPassIdx = beforePostFX->GetSubPassDefinitions().Count();
	return beforePostFX->AddRenderSubPass(subPass);
}

//----------------------------------------------------------------------------

bool	CGBuffer::AddMergingSubPass(const RHI::PRenderPass &beforePostFX, const TMemoryView<const CGuid> &mergingToSampleRT)
{
	RHI::SSubPassDefinition	subPass;

	subPass.m_DepthStencilRenderTarget = m_DepthBufferIndex;
	subPass.m_OutputRenderTargets.PushBack(m_MergeBufferIndex);
	subPass.m_InputRenderTargets.PushBack(m_LightAccumBufferIndex);
	for (u32 i = 0; i < mergingToSampleRT.Count(); ++i)
	{
		subPass.m_InputRenderTargets.PushBack(mergingToSampleRT[i]);
	}
	m_MergingSubPassIdx = beforePostFX->GetSubPassDefinitions().Count();
	return beforePostFX->AddRenderSubPass(subPass);
}

//----------------------------------------------------------------------------

__PK_SAMPLE_API_END
