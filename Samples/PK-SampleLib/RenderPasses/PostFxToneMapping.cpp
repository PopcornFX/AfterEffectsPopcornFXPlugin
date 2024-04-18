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

#include "PostFxToneMapping.h"

#include <PK-SampleLib/ShaderDefinitions/UnitTestsShaderDefinitions.h>
#include <PK-SampleLib/ShaderDefinitions/BasicSceneShaderDefinitions.h>

#include "BlueNoise.h"

#include <pk_rhi/include/AllInterfaces.h>

#define	QUAD_VERTEX_SHADER			"./Shaders/FullScreenQuad.vert"

#define	COPY_FRAGMENT_SHADER		"./Shaders/Copy.frag"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

CPostFxToneMapping::CPostFxToneMapping()
:	m_SamplerRT(null)
,	m_RenderState(null)
,	m_RenderStateSkip(null)
,	m_RenderStateSkipPrecomputeLuma(null)
{
	SetExposure(1.0f);	// identity exposure is not '1.0' in m_Parameters.m_Params.x, initialize to correct value for exposure of 1
	SetSaturation(1.f);
	SetPrecomputeLuma(true);
}

//----------------------------------------------------------------------------

CPostFxToneMapping::~CPostFxToneMapping()
{
}

//----------------------------------------------------------------------------

bool	CPostFxToneMapping::Init(	const RHI::PApiManager &apiManager,
									const RHI::PGpuBuffer &fullScreenQuadVbo,
									const RHI::PConstantSampler &samplerRT,
									const RHI::SConstantSetLayout &samplerRTLayout)
{
	m_FullScreenQuadVbo = fullScreenQuadVbo;
	m_ApiManager = apiManager;
	m_SamplerRT = samplerRT;
	m_SamplerRTLayout = samplerRTLayout;

	RHI::SConstantSetLayout		blueNoise;
	blueNoise.m_ShaderStagesMask = RHI::FragmentShaderMask;
	blueNoise.AddConstantsLayout(RHI::SConstantSamplerDesc("BlueNoise", RHI::SamplerTypeSingle));

	RHI::PConstantSampler sampler = m_ApiManager->CreateConstantSampler(	RHI::SRHIResourceInfos("ToneMapping Sampler"),
																			RHI::SampleLinear,
																			RHI::SampleLinearMipmapLinear,
																			RHI::SampleRepeat,
																			RHI::SampleRepeat,
																			RHI::SampleRepeat,
																			1);
	const CImageMap map = CImageMap(CUint3(32, 32, 1), const_cast<void*>(reinterpret_cast<const void*>(&kBlueNoiseMap)), sizeof(u8) * 4 * 32 * 32);
  	RHI::PTexture	texture = apiManager->CreateTexture(RHI::SRHIResourceInfos("ToneMapping Blue Noise Texture"), TMemoryView<const CImageMap>(map), RHI::EPixelFormat::FormatSrgb8RGBA);
	m_BlueNoise = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("ToneMapping Constant Set"), blueNoise);
	m_BlueNoise->SetConstants(sampler, texture, 0);
	m_BlueNoise->UpdateConstantValues();

	return true;
}

//----------------------------------------------------------------------------

bool	CPostFxToneMapping::UpdateFrameBuffer(	const TMemoryView<RHI::PFrameBuffer> &frameBuffers,
												TArray<RHI::ELoadRTOperation> &loadOP,
												TArray<RHI::SFrameBufferClearValue> &clearValues,
												const SInOutRenderTargets *inOut)
{
	(void)clearValues;
	const CUint2	frameBufferSize = frameBuffers.First()->GetSize();

	if (!inOut->m_OutputRtIdx.Valid())
	{
		if (!PK_VERIFY(m_OutputRenderTarget.CreateRenderTarget(RHI::SRHIResourceInfos("ToneMapping Output Render Target"), m_ApiManager, m_SamplerRT, RHI::FormatFloat16RGBA, frameBufferSize, m_SamplerRTLayout)))
			return false;

		if (!PK_VERIFY(loadOP.PushBack(RHI::LoadDontCare).Valid()))
			return false;

		for (u32 i = 0; i < frameBuffers.Count(); ++i)
		{
			PK_ASSERT(frameBufferSize == frameBuffers[i]->GetSize());

			m_OutputRenderTargetIdx = frameBuffers[i]->GetRenderTargets().Count();
			if (!PK_VERIFY(frameBuffers[i]->AddRenderTarget(m_OutputRenderTarget.m_RenderTarget)))
				return false;
		}
	}
	m_ToTonemapRenderTarget = *inOut->m_ToTonemapRt;
	return true;
}

//----------------------------------------------------------------------------

bool	CPostFxToneMapping::AddSubPasses(const RHI::PRenderPass &renderPass, const SInOutRenderTargets *inOut)
{
	bool	success = true;
	RHI::SSubPassDefinition		toneMappingSubPass;

	m_SubPassIdx = renderPass->GetSubPassDefinitions().Count();
	if (inOut->m_OutputRtIdx.Valid())
		success &= toneMappingSubPass.m_OutputRenderTargets.PushBack(inOut->m_OutputRtIdx).Valid();
	else
		success &= toneMappingSubPass.m_OutputRenderTargets.PushBack(m_OutputRenderTargetIdx).Valid();
	if (inOut->m_ToTonemapRtIdx.Valid())
		success &= toneMappingSubPass.m_InputRenderTargets.PushBack(inOut->m_ToTonemapRtIdx).Valid();
	success &= renderPass->AddRenderSubPass(toneMappingSubPass);
	return success;
}

//----------------------------------------------------------------------------

bool	CPostFxToneMapping::CreateRenderStates(	const TMemoryView<const RHI::SRenderTargetDesc> &frameBufferLayout,
												CShaderLoader &shaderLoader,
												const RHI::PRenderPass &renderPass)
{
	m_RenderState = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("ToneMapping Render State"));
	m_RenderStateSkip = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("ToneMapping Skip Render State"));
	m_RenderStateSkipPrecomputeLuma = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("ToneMapping Skip PrecomputeLuma Render State"));

	if (m_RenderState == null || m_RenderStateSkip == null || m_RenderStateSkipPrecomputeLuma == null)
		return false;

	CShaderLoader::SShadersPaths	shadersPathsCopy;
	shadersPathsCopy.m_Vertex = QUAD_VERTEX_SHADER;
	shadersPathsCopy.m_Fragment = COPY_FRAGMENT_SHADER;

	// Main renderState

	RHI::SRenderState		renderState;
	renderState.m_PipelineState.m_DynamicScissor = true;
	renderState.m_PipelineState.m_DynamicViewport = true;
	if (!renderState.m_InputVertexBuffers.PushBack().Valid())
		return false;
	renderState.m_InputVertexBuffers.Last().m_Stride = sizeof(CFloat2);

	FillCopyShaderBindings(CopyCombination_ToneMapping, renderState.m_ShaderBindings, m_SamplerRTLayout);

	if (shaderLoader.LoadShader(renderState, shadersPathsCopy, m_ApiManager) == false)
		return false;
	m_RenderState->m_RenderState = renderState;

	// Skip renderState

	RHI::SRenderState		renderStateSkip;
	renderStateSkip.m_PipelineState.m_DynamicScissor = true;
	renderStateSkip.m_PipelineState.m_DynamicViewport = true;
	if (!renderStateSkip.m_InputVertexBuffers.PushBack().Valid())
		return false;
	renderStateSkip.m_InputVertexBuffers.Last().m_Stride = sizeof(CFloat2);

	FillCopyShaderBindings(CopyCombination_Basic, renderStateSkip.m_ShaderBindings, m_SamplerRTLayout);

	if (shaderLoader.LoadShader(renderStateSkip, shadersPathsCopy, m_ApiManager) == false)
		return false;
	m_RenderStateSkip->m_RenderState = renderStateSkip;

	// Skip renderState precompute luma

	RHI::SRenderState	renderStateSkipPrecomputeLuma;
	renderStateSkipPrecomputeLuma.m_PipelineState.m_DynamicScissor = true;
	renderStateSkipPrecomputeLuma.m_PipelineState.m_DynamicViewport = true;
	if (!renderStateSkipPrecomputeLuma.m_InputVertexBuffers.PushBack().Valid())
		return false;
	renderStateSkipPrecomputeLuma.m_InputVertexBuffers.Last().m_Stride = sizeof(CFloat2);
	
	FillCopyShaderBindings(CopyCombination_ComputeLuma, renderStateSkipPrecomputeLuma.m_ShaderBindings, m_SamplerRTLayout);
	
	if (shaderLoader.LoadShader(renderStateSkipPrecomputeLuma, shadersPathsCopy, m_ApiManager) == false)
		return false;
	m_RenderStateSkipPrecomputeLuma->m_RenderState = renderStateSkipPrecomputeLuma;

	// Bake
	return	m_ApiManager->BakeRenderState(m_RenderState, frameBufferLayout, renderPass, m_SubPassIdx) &&
			m_ApiManager->BakeRenderState(m_RenderStateSkipPrecomputeLuma, frameBufferLayout, renderPass, m_SubPassIdx) &&
			m_ApiManager->BakeRenderState(m_RenderStateSkip, frameBufferLayout, renderPass, m_SubPassIdx);
}

//----------------------------------------------------------------------------

bool	CPostFxToneMapping::Draw(const RHI::PCommandBuffer &cmdBuff)
{
	PK_NAMEDSCOPEDPROFILE("Tone-Mapping pass");

	cmdBuff->BindRenderState(m_RenderState);
	cmdBuff->BindVertexBuffers(TMemoryView<const RHI::PGpuBuffer>(m_FullScreenQuadVbo));

	cmdBuff->PushConstant(&m_Parameters, 0);

	TStaticCountedArray<RHI::PConstantSet, 2> 	constantSets;
	constantSets.PushBack(m_ToTonemapRenderTarget.m_SamplerConstantSet);
	constantSets.PushBack(m_BlueNoise);

	cmdBuff->BindConstantSets(constantSets);
	cmdBuff->Draw(0, 6);
	return true;
}

//----------------------------------------------------------------------------

bool	CPostFxToneMapping::Draw_JustCopy(const RHI::PCommandBuffer &cmdBuff)
{
	PK_NAMEDSCOPEDPROFILE("Tone-Mapping Skip pass (just a copy)");

	RHI::PRenderState renderState = m_PrecomputeLuma ? m_RenderStateSkipPrecomputeLuma : m_RenderStateSkip;
	cmdBuff->BindRenderState(renderState);
	cmdBuff->BindVertexBuffers(TMemoryView<const RHI::PGpuBuffer>(m_FullScreenQuadVbo));

	cmdBuff->BindConstantSets(TMemoryView<const RHI::PConstantSet>(m_ToTonemapRenderTarget.m_SamplerConstantSet));
	cmdBuff->Draw(0, 6);
	return true;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
