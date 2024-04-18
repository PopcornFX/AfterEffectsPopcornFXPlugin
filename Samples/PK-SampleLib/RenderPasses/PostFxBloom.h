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
#include <PK-SampleLib/SampleUtils.h>
#include <PK-SampleLib/ShaderLoader.h>
#include <PK-SampleLib/RenderPasses/DownSampleTexture.h>

#include <pk_rhi/include/FwdInterfaces.h>
#include <pk_rhi/include/interfaces/IFrameBuffer.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

class	CPostFxBloom
{
public:
	struct	SInOutRenderTargets
	{
		const SSamplableRenderTarget				*m_InputRenderTarget;
		TMemoryView<const RHI::PRenderTarget>		m_OutputRenderTargets;
	};

	CPostFxBloom();
	~CPostFxBloom();

	bool		Init(	const RHI::PApiManager &apiManager,
						const RHI::PGpuBuffer &fullScreenQuadVbo);

	bool		UpdateFrameBuffer(	const RHI::PConstantSampler &sampler,
									u32 maxDownSampleBloom,
									const SInOutRenderTargets *inOut);

	bool		CreateRenderPass();
	bool		CreateRenderStates(CShaderLoader &loader, EGaussianBlurCombination blurTap);
	bool		CreateFrameBuffers();

	void		SetSubtractValue(float subtractValue);
	void		SetIntensity(float intensity);
	void		SetAttenuation(float attenuation);

	u32			DownsampleCount() const { return m_DownSampledBrightPassRTs.Count(); }

	bool		Draw(const RHI::PCommandBuffer &commandBuffer, u32 swapChainIdx);

private:
	bool		BakeRenderStates();

	RHI::PApiManager					m_ApiManager;

	struct	SBlurRenderTargets
	{
		SSamplableRenderTarget	m_TmpBlurRT;
		SSamplableRenderTarget	m_FinalBlurRT;
	};

	RHI::SConstantSetLayout				m_CopyBlurConstSetLayout;

	RHI::PGpuBuffer						m_FullScreenQuadVbo;

	// Render targets:
	TArray<SSamplableRenderTarget>				m_TmpBlurRTs;				// Tmp RTs for the blur
	TMemoryView<const SSamplableRenderTarget>	m_DownSampledBrightPassRTs;	// Down sampled RTs after the bright pass
	// Fed by user:
	TMemoryView<const RHI::PRenderTarget>		m_SwapChainRTs;				// Out RT
	SSamplableRenderTarget				m_InputRenderTarget;

	CDownSampleTexture					m_DownSampler;

	// Frame buffers
	TArray<RHI::PFrameBuffer>			m_FrameBuffersBloom;
	TArray<RHI::PFrameBuffer>			m_FrameBuffersMerge;
	// FrameBuffer Layouts
	RHI::SRenderTargetDesc				m_FrameBufferBloomLayout[2];
	RHI::SRenderTargetDesc				m_FrameBufferMergeLayout[1];

	RHI::PRenderPass					m_RenderPassBloom;
	RHI::PRenderPass					m_RenderPassMerge;

	// Bloom render pass:
	RHI::PRenderState					m_AdditiveBlitRS;
	RHI::PRenderState					m_HorizontalBlurRS;
	RHI::PRenderState					m_VerticalBlurRS;
	// Merge render pass:
	RHI::PRenderState					m_FinalBlitRS;
	RHI::PRenderState					m_FinalAdditiveBlitRS;

	CFloat4								m_SubtractValue;
	CFloat4								m_Intensity;
	CFloat4								m_Attenuation;
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
