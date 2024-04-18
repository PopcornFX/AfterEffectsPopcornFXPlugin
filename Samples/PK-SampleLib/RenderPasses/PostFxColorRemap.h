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

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

class CPostFxColorRemap
{
public:
	struct SInOutRenderTargets
	{
		const SSamplableRenderTarget				*m_InputRenderTarget;
		CGuid										m_InputRenderTargetIdx;
		CGuid										m_OutputRenderTargetIdx;
	};

	struct SColorRemapInfo
	{
		float	m_LutHeight;

		SColorRemapInfo(const float &lutHeight) : m_LutHeight(lutHeight) {};
	};

	CPostFxColorRemap();
	~CPostFxColorRemap();

	bool		Init(	const RHI::PApiManager			&apiManager,
						const RHI::PGpuBuffer			&fullScreenQuadVbo,
						const RHI::PConstantSampler		&samplerRT,
						const RHI::SConstantSetLayout	&samplerRTLayout);

	bool		UpdateFrameBuffer(	TMemoryView<RHI::PFrameBuffer>	currentFrameBuffers,
									TArray<RHI::ELoadRTOperation>	&loadOP,
									const SInOutRenderTargets		*inOut);

	bool		AddSubPasses(const RHI::PRenderPass &renderPass, const SInOutRenderTargets *inOut);
	bool		CreateRenderStates(const TMemoryView<const RHI::SRenderTargetDesc> &frameBufferLayout, CShaderLoader &loader, const RHI::PRenderPass &renderPass);

	void		SetRemapTexture(const RHI::PTexture texture, bool isPlaceholder, CUint3 dimensions = {128, 16, 1});

	bool		Draw(const RHI::PCommandBuffer &commandBuffer);
	bool		Draw_JustCopy(const RHI::PCommandBuffer &commandBuffer);

	SSamplableRenderTarget						m_OutputRenderTarget;
	CGuid										m_OutputRenderTargetIdx;

	bool										m_HasTexture;
private:
	RHI::PApiManager							m_ApiManager;
	RHI::PConstantSet							m_ColorLUTSet;
	RHI::SConstantSetLayout						m_ColorLUTSetLayout;
	SSamplableRenderTarget						m_InputRenderTarget;	
	RHI::PConstantSampler						m_SamplerRT;
	RHI::SConstantSetLayout						m_SamplerRTLayout;
	RHI::PGpuBuffer								m_FullScreenQuadVbo;
	RHI::PTexture								m_RemapTexture;
	RHI::PConstantSampler						m_RemapTextureSampler;
	float										m_ColorLutHeight;
	CGuid										m_SubPassIdx;
	RHI::PRenderState							m_RenderState;
	RHI::PRenderState							m_RenderStateSkip;
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
