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

#include <pk_rhi/include/FwdInterfaces.h>
#include <pk_rhi/include/interfaces/IFrameBuffer.h>

#include <PK-SampleLib/ShaderDefinitions/UnitTestsShaderDefinitions.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

struct	SToneMappingInfo
{
	CFloat4	m_Common;		// packed parameters: (width, height, aspectRatio, precomputeLuma)
	CFloat4	m_ToneMapping;	// packed parameters: (exposure, gamma, saturation, dithering)
	CFloat4	m_Vignetting;	// packed parameters: (color-intensity, desaturation-intensity, roundness, smoothness)
	CFloat4	m_VignetteColor; // alpha is ignored

	SToneMappingInfo() : m_Common(CFloat4::ZERO), m_ToneMapping(CFloat4(1, 1, 1, 0)), m_Vignetting(CFloat4::ZERO), m_VignetteColor(CFloat4::ZERO) {}
};

//----------------------------------------------------------------------------

class	CPostFxToneMapping
{
public:
	struct	SInOutRenderTargets
	{
		CGuid							m_ToTonemapRtIdx;
		const SSamplableRenderTarget	*m_ToTonemapRt;

		CGuid							m_OutputRtIdx;
	};

	CPostFxToneMapping();
	~CPostFxToneMapping();

	bool		Init(	const RHI::PApiManager &apiManager,
						const RHI::PGpuBuffer &fullScreenQuadVbo,
						const RHI::PConstantSampler &samplerRT,
						const RHI::SConstantSetLayout &samplerRTLayout);
	bool		UpdateFrameBuffer(	const TMemoryView<RHI::PFrameBuffer> &frameBuffers,
									TArray<RHI::ELoadRTOperation> &loadOP,
									TArray<RHI::SFrameBufferClearValue> &clearValues,
									const SInOutRenderTargets *inOut);
	bool		AddSubPasses(	const RHI::PRenderPass &renderPass,
								const SInOutRenderTargets *inOut);
	bool		CreateRenderStates(	const TMemoryView<const RHI::SRenderTargetDesc> &frameBufferLayout,
									CShaderLoader &shaderLoader,
									const RHI::PRenderPass &renderPass);

	void		SetExposure(float exposure) { m_Parameters.m_ToneMapping.x() = expf(exposure)*1.4f; }
	void		SetGamma(float gamma) { m_Parameters.m_ToneMapping.y() = gamma; }
	void		SetSaturation(float saturation) { m_Parameters.m_ToneMapping.z() = saturation; }
	void		SetDithering(bool dithering) { m_Parameters.m_ToneMapping.w() = dithering ? 1.f : 0.f; }
	void		SetPrecomputeLuma(bool precomputeLuma) { m_Parameters.m_Common.w() = precomputeLuma ? 1.f : 0.f; m_PrecomputeLuma = precomputeLuma;}
	void		SetScreenSize(u32 width, u32 height)
	{
		PK_ASSERT(width > 0 && height > 0);
		m_Parameters.m_Common.x() = (float)width;
		m_Parameters.m_Common.y() = (float)height;
		m_Parameters.m_Common.z() = (float)height / (float)width;
	}
	void		SetVignettingColor(CFloat3 color) { m_Parameters.m_VignetteColor = color.xyz1(); }
	void		SetVignettingColorIntensity(float intensity) { m_Parameters.m_Vignetting.x() = intensity; }
	void		SetVignettingDesaturationIntensity(float intensity) { m_Parameters.m_Vignetting.y() = intensity; }
	void		SetVignettingRoundness(float roundness) { m_Parameters.m_Vignetting.z() = roundness; }
	void		SetVignettingSmoothness(float smoothness) { m_Parameters.m_Vignetting.w() = smoothness; }

	bool		Draw(const RHI::PCommandBuffer &commandBuffer);
	bool		Draw_JustCopy(const RHI::PCommandBuffer &commandBuffer);

	SSamplableRenderTarget			m_OutputRenderTarget;
	CGuid							m_OutputRenderTargetIdx;

private:
	SSamplableRenderTarget			m_ToTonemapRenderTarget;

	RHI::PConstantSampler			m_SamplerRT;
	RHI::SConstantSetLayout			m_SamplerRTLayout;

	CGuid							m_SubPassIdx;

	RHI::PRenderState				m_RenderState;
	RHI::PRenderState				m_RenderStateSkip;
	RHI::PRenderState				m_RenderStateSkipPrecomputeLuma;

	SToneMappingInfo				m_Parameters;

	RHI::PConstantSet				m_BlueNoise;

	// This class does not own that
	RHI::PApiManager				m_ApiManager;
	RHI::PGpuBuffer					m_FullScreenQuadVbo;

	bool							m_PrecomputeLuma;
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
