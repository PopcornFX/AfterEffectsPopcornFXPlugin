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

class	CPostFxFXAA
{
public:
	struct	SInOutRenderTargets
	{
		CGuid							m_InputRtIdx;
		const SSamplableRenderTarget	*m_InputRt;

		CGuid							m_OutputRtIdx;
	};

	CPostFxFXAA();
	~CPostFxFXAA();

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

	bool		Draw(const RHI::PCommandBuffer &commandBuffer);
	bool		Draw_JustCopy(const RHI::PCommandBuffer &commandBuffer);

	void		SetLumaInAlpha(const bool lumaInAlpha);
	bool		LumaInAlpha() { return m_LumaInAlpha; }

	SSamplableRenderTarget			m_OutputRenderTarget;
	CGuid							m_OutputRenderTargetIdx;

private:
	SSamplableRenderTarget			m_InputRenderTarget;

	RHI::PConstantSampler			m_SamplerRT;
	RHI::SConstantSetLayout			m_SamplerRTLayout;
	RHI::PConstantSet				m_ConstantSet;

	CGuid							m_SubPassIdx;

	RHI::PRenderState				m_RenderState;
	RHI::PRenderState				m_RenderStateSkip;

	CUint2							m_FrameBufferSize;

	bool							m_LumaInAlpha;

	// This class does not own that
	RHI::PApiManager				m_ApiManager;
	RHI::PGpuBuffer					m_FullScreenQuadVbo;
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
