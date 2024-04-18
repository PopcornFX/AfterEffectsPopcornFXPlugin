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

#include <PK-SampleLib/ShaderDefinitions/UnitTestsShaderDefinitions.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

struct	SMulAddInfo
{
	CFloat4	m_MulValue;
	CFloat4	m_AddValue;

	SMulAddInfo() : m_MulValue(CFloat4::ZERO), m_AddValue(CFloat4::ZERO) {}
	SMulAddInfo(const CFloat4 &mul, const CFloat4 &add) : m_MulValue(mul), m_AddValue(add) {}
};

//----------------------------------------------------------------------------

class	CDownSampleTexture
{
public:
	CDownSampleTexture();
	~CDownSampleTexture();

	bool		Init(	const RHI::PApiManager &apiManager,
						const RHI::PGpuBuffer &fullScreenQuadVbo);

	bool		CreateRenderTargets(const RHI::PConstantSampler &sampler,
									u32 maxDownsampleCount,
									const CUint2 &textureSize,
									RHI::EPixelFormat textureFormat,
									const bool forcePowerOfTwoSizes = false);

	bool		CreateRenderPass();
	bool		CreateRenderStates(	CShaderLoader &loader,
									ECopyCombination firstCombination,
									ECopyCombination otherCombination);
	bool		CreateFrameBuffers();

	bool		Draw(	const RHI::PCommandBuffer &commandBuffer,
						const RHI::PConstantSet &hdrFrameSampler,
						const SMulAddInfo &firstPass = SMulAddInfo(),
						const SMulAddInfo &otherPasses = SMulAddInfo());

	TMemoryView<const SSamplableRenderTarget>	GetSamplableRenderTargets() const;
	TMemoryView<const CInt2>					GetDownscaledSizes() const;

private:
	bool		BakeRenderStates();

	RHI::PApiManager					m_ApiManager;

	TArray<SSamplableRenderTarget>		m_DownscaledTextures;
	TArray<RHI::PFrameBuffer>			m_FrameBuffers;
	TArray<CInt2>						m_DownscaledSizes;

	RHI::PGpuBuffer						m_FullScreenQuadVbo;
	RHI::PRenderPass					m_RenderPassDownScale;
	RHI::SConstantSetLayout				m_ConstSetLayoutDownScale;

	ECopyCombination					m_CopyCombination;
	RHI::PRenderState					m_CopyDownscale;

	ECopyCombination					m_FirstCopyCombination;
	RHI::PRenderState					m_FirstCopyDownscale;

	RHI::SRenderTargetDesc				m_FrameBufferLayout;
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
