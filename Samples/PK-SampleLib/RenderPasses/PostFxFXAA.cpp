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

#include "PostFxFXAA.h"

#include <PK-SampleLib/ShaderDefinitions/SampleLibShaderDefinitions.h>

#include <pk_rhi/include/AllInterfaces.h>

#define	QUAD_VERTEX_SHADER			"./Shaders/FullScreenQuad.vert"

#define	COPY_FRAGMENT_SHADER		"./Shaders/Copy.frag"

#define	FXAA_FRAGMENT_SHADER		"./Shaders/FXAA.frag"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

CPostFxFXAA::CPostFxFXAA()
:	m_SamplerRT(null)
,	m_RenderState(null)
,	m_RenderStateSkip(null)
{
}

//----------------------------------------------------------------------------

CPostFxFXAA::~CPostFxFXAA()
{
}

//----------------------------------------------------------------------------

bool	CPostFxFXAA::Init(	const RHI::PApiManager &apiManager,
							const RHI::PGpuBuffer &fullScreenQuadVbo,
							const RHI::PConstantSampler &samplerRT,
							const RHI::SConstantSetLayout &samplerRTLayout)
{
	m_FullScreenQuadVbo = fullScreenQuadVbo;
	m_ApiManager = apiManager;
	m_SamplerRT = samplerRT;
	m_SamplerRTLayout = samplerRTLayout;
	m_LumaInAlpha = true;

	return true;
}

//----------------------------------------------------------------------------

bool	CPostFxFXAA::UpdateFrameBuffer(	const TMemoryView<RHI::PFrameBuffer> &frameBuffers,
												TArray<RHI::ELoadRTOperation> &loadOP,
												TArray<RHI::SFrameBufferClearValue> &clearValues,
												const SInOutRenderTargets *inOut)
{
	(void)clearValues;
	m_FrameBufferSize = frameBuffers.First()->GetSize();

	if (!inOut->m_OutputRtIdx.Valid())
	{
		if (!PK_VERIFY(m_OutputRenderTarget.CreateRenderTarget(RHI::SRHIResourceInfos("FXAA Output Render Target"), m_ApiManager, m_SamplerRT, RHI::FormatFloat16RGBA, m_FrameBufferSize, m_SamplerRTLayout)))
			return false;

		if (!PK_VERIFY(loadOP.PushBack(RHI::LoadDontCare).Valid()))
			return false;

		for (u32 i = 0; i < frameBuffers.Count(); ++i)
		{
			PK_ASSERT(m_FrameBufferSize == frameBuffers[i]->GetSize());

			m_OutputRenderTargetIdx = frameBuffers[i]->GetRenderTargets().Count();
			if (!PK_VERIFY(frameBuffers[i]->AddRenderTarget(m_OutputRenderTarget.m_RenderTarget)))
				return false;
		}
	}
	m_InputRenderTarget = *inOut->m_InputRt;

	return true;
}

//----------------------------------------------------------------------------

bool	CPostFxFXAA::AddSubPasses(const RHI::PRenderPass &renderPass, const SInOutRenderTargets *inOut)
{
	bool	success = true;
	RHI::SSubPassDefinition		toneMappingSubPass;

	m_SubPassIdx = renderPass->GetSubPassDefinitions().Count();
	if (inOut->m_OutputRtIdx.Valid())
		success &= toneMappingSubPass.m_OutputRenderTargets.PushBack(inOut->m_OutputRtIdx).Valid();
	else
		success &= toneMappingSubPass.m_OutputRenderTargets.PushBack(m_OutputRenderTargetIdx).Valid();
	if (inOut->m_InputRtIdx.Valid())
		success &= toneMappingSubPass.m_InputRenderTargets.PushBack(inOut->m_InputRtIdx).Valid();
	success &= renderPass->AddRenderSubPass(toneMappingSubPass);
	return success;
}

//----------------------------------------------------------------------------

bool	CPostFxFXAA::CreateRenderStates(const TMemoryView<const RHI::SRenderTargetDesc> &frameBufferLayout,
	CShaderLoader &shaderLoader,
	const RHI::PRenderPass &renderPass)
{
	m_RenderState = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("FXAA Render State"));
	m_RenderStateSkip = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("FXAA Skip Render State"));

	if (m_RenderState == null)
		return false;
	if (m_RenderStateSkip == null)
		return false;

	CShaderLoader::SShadersPaths	shadersPathsCopy;
	shadersPathsCopy.m_Vertex = QUAD_VERTEX_SHADER;
	shadersPathsCopy.m_Fragment = COPY_FRAGMENT_SHADER;

	CShaderLoader::SShadersPaths	shadersPathsFXAA;
	shadersPathsFXAA.m_Vertex = QUAD_VERTEX_SHADER;
	shadersPathsFXAA.m_Fragment = FXAA_FRAGMENT_SHADER;

	// Main renderState
	RHI::SRenderState		renderState;
	renderState.m_PipelineState.m_DynamicScissor = true;
	renderState.m_PipelineState.m_DynamicViewport = true;
	if (!renderState.m_InputVertexBuffers.PushBack().Valid())
		return false;
	renderState.m_InputVertexBuffers.Last().m_Stride = sizeof(CFloat2);

	FillFXAAShaderBindings(renderState.m_ShaderBindings, m_LumaInAlpha);

	if (shaderLoader.LoadShader(renderState, shadersPathsFXAA, m_ApiManager) == false)
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

	// Bake
	return	m_ApiManager->BakeRenderState(m_RenderState, frameBufferLayout, renderPass, m_SubPassIdx) &&
			m_ApiManager->BakeRenderState(m_RenderStateSkip, frameBufferLayout, renderPass, m_SubPassIdx);
}

//----------------------------------------------------------------------------

bool	CPostFxFXAA::Draw(const RHI::PCommandBuffer &cmdBuff)
{
	cmdBuff->BindRenderState(m_RenderState);
	cmdBuff->BindVertexBuffers(TMemoryView<const RHI::PGpuBuffer>(m_FullScreenQuadVbo));
	cmdBuff->BindConstantSet(m_InputRenderTarget.m_SamplerConstantSet, 0);
	cmdBuff->PushConstant(&m_FrameBufferSize, 0);
	cmdBuff->Draw(0, 6);
	return true;
}

//----------------------------------------------------------------------------

bool	CPostFxFXAA::Draw_JustCopy(const RHI::PCommandBuffer &cmdBuff)
{
	PK_NAMEDSCOPEDPROFILE("Tone-Mapping Skip pass (just a copy)");

	cmdBuff->BindRenderState(m_RenderStateSkip);
	cmdBuff->BindVertexBuffers(TMemoryView<const RHI::PGpuBuffer>(m_FullScreenQuadVbo));

	cmdBuff->BindConstantSets(TMemoryView<const RHI::PConstantSet>(m_InputRenderTarget.m_SamplerConstantSet));
	cmdBuff->Draw(0, 6);
	return true;
}

//----------------------------------------------------------------------------

void	CPostFxFXAA::SetLumaInAlpha(const bool lumaInAlpha)
{
	m_LumaInAlpha = lumaInAlpha;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
