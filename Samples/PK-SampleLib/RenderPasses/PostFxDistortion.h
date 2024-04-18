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

class	CPostFxDistortion
{
public:
	struct	SInOutRenderTargets
	{
		// Render target to distord:
		CGuid							m_ToDistordRtIdx;
		const SSamplableRenderTarget	*m_ToDistordRt;

		// Depth buffer
		CGuid							m_DepthRtIdx;

		// Possible particle shader inputs (samplable depth, samplable diffuse...)
		TMemoryView<const CGuid>		m_ParticleInputs;

		// Output render target
		CGuid							m_OutputRtIdx;
	};

	struct SDistortionInfo
	{
		CFloat4 m_PushMultipliers;

		SDistortionInfo(const CFloat4 &multipliers) : m_PushMultipliers(multipliers) {};
	};

	CPostFxDistortion();
	~CPostFxDistortion();

	bool		Init(	const RHI::PApiManager &apiManager,
						const RHI::PGpuBuffer &fullScreenQuadVbo,
						const RHI::PConstantSampler &samplerRT,
						const RHI::SConstantSetLayout &samplerRTLayout);
	bool		UpdateFrameBuffer(	const TMemoryView<const RHI::PFrameBuffer> &frameBuffers,
									TArray<RHI::ELoadRTOperation> &loadOP,
									TArray<RHI::SFrameBufferClearValue> &clearValues,
									const SInOutRenderTargets *inOut);
	bool		AddSubPasses(const RHI::PRenderPass &renderPass, const SInOutRenderTargets *inOut);
	bool		CreateRenderStates(	const TMemoryView<const RHI::SRenderTargetDesc> &frameBufferLayout,
									CShaderLoader &shaderLoader,
									const RHI::PRenderPass &renderPass);

	bool		Draw(const RHI::PCommandBuffer &commandBuffer);
	bool		Draw_JustCopy(const RHI::PCommandBuffer &commandBuffer);

	void		SetChromaticAberrationIntensity(float intensity);
	void		SetAberrationMultipliers(CFloat4 multipliers);

	CGuid							m_DistoBufferIdx;
	SSamplableRenderTarget			m_DistoRenderTarget;
	CGuid							m_OutputBufferIdx;
	SSamplableRenderTarget			m_OutputRenderTarget;

	CGuid							m_DistoSubPassIdx;
	CGuid							m_PostDistortionSubPassIdx;

	static const RHI::EPixelFormat	s_DistortionBufferFormat;

private:
	bool		_Draw_UVoffset(const RHI::PCommandBuffer &commandBuffer, const RHI::PConstantSet &inputSampler);
	bool		_Draw_Blur(const RHI::PCommandBuffer &commandBuffer, const RHI::PConstantSet &inputSampler, bool isHorizontal);

	RHI::PConstantSampler			m_SamplerRT;
	RHI::SConstantSetLayout			m_SamplerRTLayout;

	RHI::PConstantSet				m_ConstSetProcessToDistord;
	RHI::PConstantSet				m_ConstSetProcessOutput;
	RHI::SConstantSetLayout			m_ConstSetLayoutProcess;

	SSamplableRenderTarget			m_ToDistordRt;

	RHI::PRenderState				m_RenderStateOffset;
	RHI::PRenderState				m_RenderStateBlurH;
	RHI::PRenderState				m_RenderStateBlurV;
	RHI::PRenderState				m_RenderStateSkip;

	float							m_DistortionIntensity;
	CFloat4							m_ChromaticAberrationMultipliers;
	CFloat4							m_PushMultipliers;

	// This class does not own that
	RHI::PApiManager				m_ApiManager;
	RHI::PGpuBuffer					m_FullScreenQuadVbo;
	RHI::PRenderPass				m_RenderPass;

};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
