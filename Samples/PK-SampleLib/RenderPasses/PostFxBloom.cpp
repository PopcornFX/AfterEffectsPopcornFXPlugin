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

#include "PostFxBloom.h"

#include <PK-SampleLib/ShaderDefinitions/UnitTestsShaderDefinitions.h>
#include <PK-SampleLib/ShaderDefinitions/BasicSceneShaderDefinitions.h>

#include <pk_rhi/include/AllInterfaces.h>

#define	QUAD_VERTEX_SHADER			"./Shaders/FullScreenQuad.vert"

#define	BLUR_FRAGMENT_SHADER		"./Shaders/GaussianBlur.frag"
#define	COPY_FRAGMENT_SHADER		"./Shaders/Copy.frag"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

CPostFxBloom::CPostFxBloom()
:	m_RenderPassBloom(null)
,	m_SubtractValue(-1)
,	m_Intensity(1)
,	m_Attenuation(0.5)
{
}

//----------------------------------------------------------------------------

CPostFxBloom::~CPostFxBloom()
{
}

//----------------------------------------------------------------------------

bool	CPostFxBloom::Init(	const RHI::PApiManager &apiManager,
							const RHI::PGpuBuffer &fullScreenQuadVbo)
{
	m_FullScreenQuadVbo = fullScreenQuadVbo;
	m_ApiManager = apiManager;
	CreateSimpleSamplerConstSetLayouts(m_CopyBlurConstSetLayout, false);
	return m_DownSampler.Init(apiManager, fullScreenQuadVbo);
}

//----------------------------------------------------------------------------

bool	CPostFxBloom::UpdateFrameBuffer(	const RHI::PConstantSampler &sampler,
											u32 maxDownSampleBloom,
											const SInOutRenderTargets *inOut)
{
	PK_ASSERT(!inOut->m_OutputRenderTargets.Empty());
	CUint2		rtSize = inOut->m_OutputRenderTargets.First()->GetSize();

	if (!m_DownSampler.CreateRenderTargets(sampler, maxDownSampleBloom, rtSize, RHI::FormatFloat16RGBA, true))
		return false;
	m_DownSampledBrightPassRTs = m_DownSampler.GetSamplableRenderTargets();
	const u32	bloomRenderPassCount = m_DownSampledBrightPassRTs.Count();

	// Create the samplable render targets
	if (m_TmpBlurRTs.Resize(bloomRenderPassCount) == false)
		return false;
	for (u32 i = 0; i < bloomRenderPassCount; ++i)
	{
		if (m_TmpBlurRTs[i].CreateRenderTarget(RHI::SRHIResourceInfos("Bloom Pass Render Target"), m_ApiManager, sampler, RHI::FormatFloat16RGBA, m_DownSampledBrightPassRTs[i].m_Size, m_CopyBlurConstSetLayout) == false)
			return false;
	}

	// Fed by user:
	m_SwapChainRTs = inOut->m_OutputRenderTargets;
	m_InputRenderTarget = *inOut->m_InputRenderTarget;

	// Layout:
	m_FrameBufferBloomLayout[0] = m_TmpBlurRTs.First().m_RenderTarget->GetRenderTargetLayout();
	m_FrameBufferBloomLayout[1] = m_DownSampledBrightPassRTs.First().m_RenderTarget->GetRenderTargetLayout();
	m_FrameBufferMergeLayout[0] = m_SwapChainRTs.First()->GetRenderTargetLayout();
	return true;
}

//----------------------------------------------------------------------------

bool	CPostFxBloom::CreateRenderPass()
{
	// Allocations
	m_RenderPassBloom = m_ApiManager->CreateRenderPass(RHI::SRHIResourceInfos("Bloom Render Pass"));
	m_RenderPassMerge = m_ApiManager->CreateRenderPass(RHI::SRHIResourceInfos("Merge Render Pass"));
	if (m_RenderPassBloom == null || m_RenderPassMerge == null)
		return false;

	bool						result = true;

	// --------------------------------------------------------------------------------------------
	// Render Targets Definitions:
	// --------------------------------------------------------------------------------------------
	// Definition
	// Original RT:		[RT]
	// Downscaled RTs:					[RT / 2]			[RT / 4]			[RT / 8]
	// --------------------------------------------------------------------------------------------
	// Bloom RTs:						[m_TmpBlurRT / 2]	[m_TmpBlurRT / 4]	[m_TmpBlurRT / 8]
	// --------------------------------------------------------------------------------------------

	RHI::SSubPassDefinition		blitPreviousPass;
	RHI::SSubPassDefinition		bloomVerticalBlurSubPass;
	RHI::SSubPassDefinition		bloomHorizontalBlurSubPass;

	result &= blitPreviousPass.m_OutputRenderTargets.PushBack(0).Valid();

	result &= bloomHorizontalBlurSubPass.m_InputRenderTargets.PushBack(0).Valid();
	result &= bloomHorizontalBlurSubPass.m_OutputRenderTargets.PushBack(1).Valid();

	result &= bloomVerticalBlurSubPass.m_InputRenderTargets.PushBack(1).Valid();
	result &= bloomVerticalBlurSubPass.m_OutputRenderTargets.PushBack(0).Valid();

	result &= m_RenderPassBloom->AddRenderSubPass(blitPreviousPass);
	result &= m_RenderPassBloom->AddRenderSubPass(bloomHorizontalBlurSubPass);
	result &= m_RenderPassBloom->AddRenderSubPass(bloomVerticalBlurSubPass);

	// --------------------------------------------------------------------------------------------
	// Final Merge Render Pass:
	// --------------------------------------------------------------------------------------------
	// Frame buffer =	[RT]
	// --------------------------------------------------------------------------------------------
	// 1 - [RT / 2]				-> Copy ->					[RT]
	// --------------------------------------------------------------------------------------------

	RHI::SSubPassDefinition		finalBlit;

	result &= finalBlit.m_OutputRenderTargets.PushBack(0).Valid();
	result &= m_RenderPassMerge->AddRenderSubPass(finalBlit);

	// Baking
	const RHI::ELoadRTOperation		loadRt[] = { RHI::LoadDontCare, RHI::LoadDontCare };
	const RHI::ELoadRTOperation		keepRt[] = { RHI::LoadKeepValue };

	result &= m_RenderPassBloom->BakeRenderPass(m_FrameBufferBloomLayout, loadRt);
	result &= m_RenderPassMerge->BakeRenderPass(m_FrameBufferMergeLayout, keepRt);

	return result && m_DownSampler.CreateRenderPass();
}

//----------------------------------------------------------------------------

bool	CPostFxBloom::BakeRenderStates()
{
	bool	success = true;

	// Bloom:
	success &= m_ApiManager->BakeRenderState(m_AdditiveBlitRS, m_FrameBufferBloomLayout, m_RenderPassBloom, 0);
	success &= m_ApiManager->BakeRenderState(m_HorizontalBlurRS, m_FrameBufferBloomLayout, m_RenderPassBloom, 1);
	success &= m_ApiManager->BakeRenderState(m_VerticalBlurRS, m_FrameBufferBloomLayout, m_RenderPassBloom, 2);
	// Final merge:
	success &= m_ApiManager->BakeRenderState(m_FinalAdditiveBlitRS, m_FrameBufferMergeLayout, m_RenderPassMerge, 0);
	success &= m_ApiManager->BakeRenderState(m_FinalBlitRS, m_FrameBufferMergeLayout, m_RenderPassMerge, 0);
	return success;
}

//----------------------------------------------------------------------------

bool	CPostFxBloom::CreateFrameBuffers()
{
	// Create the frame buffers for the blur
	if (m_FrameBuffersBloom.Resize(m_TmpBlurRTs.Count()) == false)
		return false;
	for (u32 i = 0; i < m_TmpBlurRTs.Count(); ++i)
	{
		RHI::PRenderTarget	renderTargets[] =
		{
			m_DownSampledBrightPassRTs[i].m_RenderTarget,
			m_TmpBlurRTs[i].m_RenderTarget,
		};
		if (!Utils::CreateFrameBuffer(RHI::SRHIResourceInfos("Bloom Frame Buffer"), m_ApiManager, TMemoryView<RHI::PRenderTarget>(renderTargets), m_FrameBuffersBloom[i], m_RenderPassBloom))
			return false;
	}

	// Create the frame buffers for the merging
	if (m_FrameBuffersMerge.Resize(m_SwapChainRTs.Count()) == false)
		return false;
	for (u32 i = 0; i < m_SwapChainRTs.Count(); ++i)
	{
		if (!Utils::CreateFrameBuffer(RHI::SRHIResourceInfos("SwapChain Frame Buffer"), m_ApiManager, TMemoryView<const RHI::PRenderTarget>(m_SwapChainRTs[i]), m_FrameBuffersMerge[i], m_RenderPassMerge))
			return false;
	}
	return m_DownSampler.CreateFrameBuffers();
}

//----------------------------------------------------------------------------

void	CPostFxBloom::SetSubtractValue(float subtractValue)
{
	m_SubtractValue = CFloat4(-subtractValue);
}

//----------------------------------------------------------------------------

void	CPostFxBloom::SetIntensity(float intensity)
{
	m_Intensity = CFloat4(intensity);
}

//----------------------------------------------------------------------------

void	CPostFxBloom::SetAttenuation(float attenuation)
{
	m_Attenuation = CFloat4(attenuation);
}

//----------------------------------------------------------------------------

bool	CPostFxBloom::Draw(const RHI::PCommandBuffer &cmdBuff, u32 swapChainIdx)
{
	PK_NAMEDSCOPEDPROFILE("Bloom pass");
	SMulAddInfo		firstPass(m_Intensity, m_SubtractValue);
	SMulAddInfo		otherPass(m_Attenuation, CFloat4::ZERO);

	if (!m_DownSampler.Draw(cmdBuff, m_InputRenderTarget.m_SamplerConstantSet, firstPass, otherPass))
		return false;
	for (u32 i = 0; i < m_FrameBuffersBloom.Count(); ++i)
	{
		const u32		currentRT = m_FrameBuffersBloom.Count() - i - 1;
		const CFloat2	currentRTSize = m_DownSampledBrightPassRTs[currentRT].m_Size;
		const CInt2		currentRTSize_ScreenRatio = m_DownSampler.GetDownscaledSizes()[currentRT];

		cmdBuff->BeginRenderPass(m_RenderPassBloom, m_FrameBuffersBloom[currentRT], TMemoryView<RHI::SFrameBufferClearValue>());

		cmdBuff->SetViewport(CInt2(0, 0), currentRTSize, CFloat2(0, 1));
		cmdBuff->SetScissor(CInt2(0), currentRTSize);

		if (i != 0)
		{
			// -------------- Additive copy of the prev pass --------------
			PK_NAMEDSCOPEDPROFILE("Additive copy of the prev pass");
			cmdBuff->BindRenderState(m_AdditiveBlitRS);
			cmdBuff->BindVertexBuffers(TMemoryView<const RHI::PGpuBuffer>(m_FullScreenQuadVbo));

			cmdBuff->BindConstantSets(TMemoryView<const RHI::PConstantSet>(m_DownSampledBrightPassRTs[currentRT + 1].m_SamplerConstantSet));

			cmdBuff->Draw(0, 6);
		}

		cmdBuff->NextRenderSubPass();

		SBlurInfo		blurInfo(currentRTSize_ScreenRatio, CFloat2(1.0f, 0.0f));

		{
			// -------------- Horizontal blur sub pass --------------
			PK_NAMEDSCOPEDPROFILE("Horizontal blur sub pass");
			cmdBuff->BindRenderState(m_HorizontalBlurRS);
			cmdBuff->BindVertexBuffers(TMemoryView<const RHI::PGpuBuffer>(m_FullScreenQuadVbo));
			cmdBuff->BindConstantSets(TMemoryView<const RHI::PConstantSet>(m_DownSampledBrightPassRTs[currentRT].m_SamplerConstantSet));

			cmdBuff->PushConstant(&blurInfo, 0);

			cmdBuff->Draw(0, 6);

			cmdBuff->NextRenderSubPass();
		}

		{
			// -------------- Vertical blur sub pass --------------
			PK_NAMEDSCOPEDPROFILE("Vertical blur sub pass");
			cmdBuff->BindRenderState(m_VerticalBlurRS);
			cmdBuff->BindConstantSets(TMemoryView<const RHI::PConstantSet>(m_TmpBlurRTs[currentRT].m_SamplerConstantSet));

			blurInfo.m_Direction = CFloat2(0.0f, 1.0f);
			cmdBuff->PushConstant(&blurInfo, 0);

			cmdBuff->Draw(0, 6);

			cmdBuff->EndRenderPass();
			cmdBuff->SyncPreviousRenderPass(RHI::OutputColorPipelineStage, RHI::FragmentPipelineStage);
		}
	}

	// Just in case the index of swap chain overflows
	swapChainIdx = swapChainIdx % m_FrameBuffersMerge.Count();

	// When the input render target is not the same as the output render target,
	// we need an additional copy to make the bloom work correctly:
	const bool		inputAndOutputDiffer =	m_SwapChainRTs.Count() > 1 || m_SwapChainRTs.First() != m_InputRenderTarget.m_RenderTarget;

	// -------------- Additive blit to final buffer --------------
	{
		PK_NAMEDSCOPEDPROFILE("Additive blit to final buffer");
		cmdBuff->BeginRenderPass(m_RenderPassMerge, m_FrameBuffersMerge[swapChainIdx], TMemoryView<RHI::SFrameBufferClearValue>());

		const CUint2	contextSize = m_SwapChainRTs.First()->GetSize();

		if (inputAndOutputDiffer)
		{
			cmdBuff->BindRenderState(m_FinalBlitRS);
			cmdBuff->BindVertexBuffers(TMemoryView<const RHI::PGpuBuffer>(m_FullScreenQuadVbo));
			cmdBuff->SetViewport(CInt2(0, 0), contextSize, CFloat2(0, 1));
			cmdBuff->SetScissor(CInt2(0), contextSize);
			cmdBuff->BindConstantSets(TMemoryView<const RHI::PConstantSet>(m_InputRenderTarget.m_SamplerConstantSet));
			cmdBuff->Draw(0, 6);
		}

		cmdBuff->BindRenderState(m_FinalAdditiveBlitRS);
		cmdBuff->BindVertexBuffers(TMemoryView<const RHI::PGpuBuffer>(m_FullScreenQuadVbo));
		cmdBuff->SetViewport(CInt2(0, 0), contextSize, CFloat2(0, 1));
		cmdBuff->SetScissor(CInt2(0), contextSize);
		cmdBuff->BindConstantSets(TMemoryView<const RHI::PConstantSet>(m_DownSampledBrightPassRTs[0].m_SamplerConstantSet));
		cmdBuff->Draw(0, 6);

		cmdBuff->EndRenderPass();
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CPostFxBloom::CreateRenderStates(CShaderLoader &loader, EGaussianBlurCombination blurTap)
{
	m_HorizontalBlurRS = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("Bloom HBlur Render State"));
	m_VerticalBlurRS = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("Bloom VBlur Render State"));
	m_AdditiveBlitRS = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("Bloom AdditiveBlit Render State"));
	m_FinalAdditiveBlitRS = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("Bloom FinalAdditiveBlit Render State"));
	m_FinalBlitRS = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("Bloom FinalBlit Render State"));

	if (m_HorizontalBlurRS == null || m_VerticalBlurRS == null ||
		m_AdditiveBlitRS == null || m_FinalAdditiveBlitRS == null || m_FinalBlitRS == null)
		return false;

	// Render pass bloom
	RHI::SRenderState		opaqueRenderState;
	opaqueRenderState.m_PipelineState.m_DynamicScissor = true;
	opaqueRenderState.m_PipelineState.m_DynamicViewport = true;
	if (!opaqueRenderState.m_InputVertexBuffers.PushBack().Valid())
		return false;
	opaqueRenderState.m_InputVertexBuffers.Last().m_Stride = sizeof(CFloat2);

	CShaderLoader::SShadersPaths	shadersPathsBloom;
	shadersPathsBloom.m_Vertex = QUAD_VERTEX_SHADER;
	shadersPathsBloom.m_Fragment = BLUR_FRAGMENT_SHADER;

	// Horizontal & VerticalBlur (two passes)
	FillGaussianBlurShaderBindings(	blurTap,
									opaqueRenderState.m_ShaderBindings,
									m_CopyBlurConstSetLayout);

	if (loader.LoadShader(opaqueRenderState, shadersPathsBloom, m_ApiManager) == false)
		return false;
	m_HorizontalBlurRS->m_RenderState = opaqueRenderState;
	m_VerticalBlurRS->m_RenderState = opaqueRenderState;

	// Additive blit
	RHI::SRenderState		additiveRenderState;
	additiveRenderState.m_PipelineState.m_DynamicScissor = true;
	additiveRenderState.m_PipelineState.m_DynamicViewport = true;
	if (!additiveRenderState.m_InputVertexBuffers.PushBack().Valid())
		return false;
	additiveRenderState.m_InputVertexBuffers.Last().m_Stride = sizeof(CFloat2);
	additiveRenderState.m_PipelineState.m_Blending = true;
	additiveRenderState.m_PipelineState.m_ColorBlendingEquation = RHI::BlendAdd;
	additiveRenderState.m_PipelineState.m_ColorBlendingDst = RHI::BlendOne;
	additiveRenderState.m_PipelineState.m_ColorBlendingSrc = RHI::BlendOne;

	CShaderLoader::SShadersPaths	shadersPaths;
	shadersPaths.m_Vertex = QUAD_VERTEX_SHADER;
	shadersPaths.m_Fragment = COPY_FRAGMENT_SHADER;

	FillCopyShaderBindings(CopyCombination_Basic, additiveRenderState.m_ShaderBindings, m_CopyBlurConstSetLayout);
	FillCopyShaderBindings(CopyCombination_Basic, opaqueRenderState.m_ShaderBindings, m_CopyBlurConstSetLayout);
	if (loader.LoadShader(additiveRenderState, shadersPaths, m_ApiManager) == false)
		return false;
	if (loader.LoadShader(opaqueRenderState, shadersPaths, m_ApiManager) == false)
		return false;
	// Those two render states are exactly the same but not used int the same render pass:
	m_AdditiveBlitRS->m_RenderState = additiveRenderState;
	m_FinalBlitRS->m_RenderState = opaqueRenderState;
	m_FinalAdditiveBlitRS->m_RenderState = additiveRenderState;
	return BakeRenderStates() && m_DownSampler.CreateRenderStates(loader, CopyCombination_MulAdd, CopyCombination_MulAdd);
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
