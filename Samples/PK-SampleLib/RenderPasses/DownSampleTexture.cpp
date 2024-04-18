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

#include "DownSampleTexture.h"

#include <PK-SampleLib/ShaderDefinitions/UnitTestsShaderDefinitions.h>
#include <PK-SampleLib/ShaderDefinitions/BasicSceneShaderDefinitions.h>

#include <pk_rhi/include/AllInterfaces.h>

#define	QUAD_VERTEX_SHADER			"./Shaders/FullScreenQuad.vert"
#define	COPY_FRAGMENT_SHADER		"./Shaders/Copy.frag"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

CDownSampleTexture::CDownSampleTexture()
{
}

//----------------------------------------------------------------------------

CDownSampleTexture::~CDownSampleTexture()
{
}

//----------------------------------------------------------------------------

bool	CDownSampleTexture::Init(	const RHI::PApiManager &apiManager,
									const RHI::PGpuBuffer &fullScreenQuadVbo)
{
	m_FullScreenQuadVbo = fullScreenQuadVbo;
	m_ApiManager = apiManager;
	CreateSimpleSamplerConstSetLayouts(m_ConstSetLayoutDownScale, false);
	return true;
}

//----------------------------------------------------------------------------

bool	CDownSampleTexture::CreateRenderTargets(const RHI::PConstantSampler &sampler,
												u32 maxDownsampleCount,
												const CUint2 &textureSize,
												RHI::EPixelFormat textureFormat,
												const bool forcePowerOfTwoSizes)
{
	CUint2		rtSize = textureSize; //render-target size
	CUint2		scSize = textureSize; //screen size
	const u32	maxSize = PKMax(rtSize.x(), rtSize.y());
	const u32	pow2 = IntegerTools::Log2(maxSize);
	const u32	downscalePassCount = PKMax(PKMin(pow2, maxDownsampleCount), 1U);

	// Create the samplable render targets
	if (m_DownscaledTextures.Resize(downscalePassCount) == false)
		return false;
	PK_VERIFY(m_DownscaledSizes.Resize(downscalePassCount));

	// Force rtSize to be a power of 2.
	if (forcePowerOfTwoSizes)
	{
		rtSize.x() = IntegerTools::NextOrEqualPowerOfTwo(rtSize.x());
		rtSize.y() = IntegerTools::NextOrEqualPowerOfTwo(rtSize.y());
		// resize screen size (but keep the ratio)
		const float factor = (rtSize.y() * textureSize.x() > rtSize.x() * textureSize.y()) ? rtSize.y() * 1.f / textureSize.y() : rtSize.x() * 1.f / textureSize.x();
		scSize.x() = factor * scSize.x();
		scSize.y() = factor * scSize.y();
	}

	// Compute the reduction size
	for (u32 i = 0; i < downscalePassCount; ++i)
	{
		rtSize = PKMax(rtSize / 2U, CUint2(1));
		scSize = PKMax(scSize / 2U, CUint2(1));
		m_DownscaledSizes[i] = scSize;
		if (!m_DownscaledTextures[i].CreateRenderTarget(RHI::SRHIResourceInfos("Downscaled Render Target"),
														m_ApiManager,
														sampler,
														textureFormat,
														rtSize,
														m_ConstSetLayoutDownScale))
			return false;
	}
	m_FrameBufferLayout = m_DownscaledTextures.First().m_RenderTarget->GetRenderTargetLayout();
	return true;
}

//----------------------------------------------------------------------------

bool	CDownSampleTexture::CreateRenderPass()
{
	// Allocations
	m_RenderPassDownScale = m_ApiManager->CreateRenderPass(RHI::SRHIResourceInfos("Downscale Render Pass"));
	if (m_RenderPassDownScale == null)
		return false;

	bool						result = true;
	// Definition
	RHI::SSubPassDefinition		blitToLowRes;

	// Bloom Pass
	result &= blitToLowRes.m_OutputRenderTargets.PushBack(0).Valid();

	result &= m_RenderPassDownScale->AddRenderSubPass(blitToLowRes);

	// Baking
	const RHI::ELoadRTOperation		loadRt[] = { RHI::LoadDontCare };

	result &= m_RenderPassDownScale->BakeRenderPass(TMemoryView<RHI::SRenderTargetDesc>(m_FrameBufferLayout), loadRt);
	return result;
}

//----------------------------------------------------------------------------

bool	CDownSampleTexture::BakeRenderStates()
{
	bool	success = true;

	success &= m_ApiManager->BakeRenderState(m_FirstCopyDownscale, TMemoryView<RHI::SRenderTargetDesc>(m_FrameBufferLayout), m_RenderPassDownScale, 0);
	success &= m_ApiManager->BakeRenderState(m_CopyDownscale, TMemoryView<RHI::SRenderTargetDesc>(m_FrameBufferLayout), m_RenderPassDownScale, 0);
	return success;
}

//----------------------------------------------------------------------------

bool	CDownSampleTexture::CreateFrameBuffers()
{
	// Create the frame buffers for the blur
	if (m_FrameBuffers.Resize(m_DownscaledTextures.Count()) == false)
		return false;
	for (u32 i = 0; i < m_DownscaledTextures.Count(); ++i)
	{
		if (!Utils::CreateFrameBuffer(	RHI::SRHIResourceInfos("Downscaled Textures Frame Buffer"),
										m_ApiManager,
										TMemoryView<RHI::PRenderTarget>(m_DownscaledTextures[i].m_RenderTarget),
										m_FrameBuffers[i],
										m_RenderPassDownScale))
			return false;
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CDownSampleTexture::Draw(	const RHI::PCommandBuffer &cmdBuff,
									const RHI::PConstantSet &hdrFrameSampler,
									const SMulAddInfo &firstPass,
									const SMulAddInfo &otherPasses)
{
	PK_NAMEDSCOPEDPROFILE("Down sample pass");
	for (u32 i = 0; i < m_DownscaledTextures.Count(); ++i)
	{
		cmdBuff->BeginRenderPass(m_RenderPassDownScale, m_FrameBuffers[i], TMemoryView<RHI::SFrameBufferClearValue>());

		// -------------- Blit render sub pass --------------
		cmdBuff->SetViewport(CInt2(0, 0), m_DownscaledTextures[i].m_Size, CFloat2(0, 1));
		cmdBuff->SetScissor(CInt2(0), m_DownscaledTextures[i].m_Size);

		// -------------- Copy --------------
		if (i == 0)
		{
			cmdBuff->BindRenderState(m_FirstCopyDownscale);
			if (m_FirstCopyCombination == CopyCombination_MulAdd || m_FirstCopyCombination == CopyCombination_ToneMapping)
			{
				cmdBuff->PushConstant(&firstPass, 0);
			}
		}
		else
		{
			cmdBuff->BindRenderState(m_CopyDownscale);
			if (m_CopyCombination == CopyCombination_MulAdd || m_CopyCombination == CopyCombination_ToneMapping)
			{
				cmdBuff->PushConstant(&otherPasses, 0);
			}
		}
		cmdBuff->BindVertexBuffers(TMemoryView<const RHI::PGpuBuffer>(m_FullScreenQuadVbo));

		if (i == 0)
			cmdBuff->BindConstantSets(TMemoryView<const RHI::PConstantSet>(hdrFrameSampler));
		else
			cmdBuff->BindConstantSets(TMemoryView<const RHI::PConstantSet>(m_DownscaledTextures[i - 1].m_SamplerConstantSet));

		cmdBuff->Draw(0, 6);

		cmdBuff->EndRenderPass();
		cmdBuff->SyncPreviousRenderPass(RHI::OutputColorPipelineStage, RHI::FragmentPipelineStage);
	}
	return true;
}

//----------------------------------------------------------------------------

TMemoryView<const SSamplableRenderTarget>	CDownSampleTexture::GetSamplableRenderTargets() const
{
	return m_DownscaledTextures;
}

//----------------------------------------------------------------------------

TMemoryView<const CInt2>	CDownSampleTexture::GetDownscaledSizes() const
{
	return m_DownscaledSizes;
}

//----------------------------------------------------------------------------

bool	CDownSampleTexture::CreateRenderStates(	CShaderLoader &loader,
												ECopyCombination firstCombination,
												ECopyCombination otherCombination)
{
	m_FirstCopyCombination = firstCombination;
	m_FirstCopyDownscale = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("First Copy Down Render state"));
	m_CopyCombination = otherCombination;
	m_CopyDownscale = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("Copy Down Render state"));

	if (m_FirstCopyDownscale == null || m_CopyDownscale == null)
		return false;

	CShaderLoader::SShadersPaths	shadersPaths;
	shadersPaths.m_Vertex = QUAD_VERTEX_SHADER;
	shadersPaths.m_Fragment = COPY_FRAGMENT_SHADER;

	// Render pass bloom
	RHI::SRenderState		downScaleRenderState;

	downScaleRenderState.m_PipelineState.m_DynamicScissor = true;
	downScaleRenderState.m_PipelineState.m_DynamicViewport = true;
	if (!downScaleRenderState.m_InputVertexBuffers.PushBack().Valid())
		return false;
	downScaleRenderState.m_InputVertexBuffers.Last().m_Stride = sizeof(CFloat2);

	// First CopyDownScale
	FillCopyShaderBindings(m_FirstCopyCombination, downScaleRenderState.m_ShaderBindings, m_ConstSetLayoutDownScale);

	if (loader.LoadShader(downScaleRenderState, shadersPaths, m_ApiManager) == false)
		return false;

	m_FirstCopyDownscale->m_RenderState = downScaleRenderState;

	// CopyDownScale
	FillCopyShaderBindings(m_CopyCombination, downScaleRenderState.m_ShaderBindings, m_ConstSetLayoutDownScale);

	if (loader.LoadShader(downScaleRenderState, shadersPaths, m_ApiManager) == false)
		return false;

	m_CopyDownscale->m_RenderState = downScaleRenderState;
	return BakeRenderStates();
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
