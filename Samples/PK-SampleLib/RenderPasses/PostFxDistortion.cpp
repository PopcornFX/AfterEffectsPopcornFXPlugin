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

#include "PostFxDistortion.h"

#include <PK-SampleLib/ShaderDefinitions/UnitTestsShaderDefinitions.h>
#include <PK-SampleLib/ShaderDefinitions/BasicSceneShaderDefinitions.h>

#include <pk_rhi/include/AllInterfaces.h>

#define	QUAD_VERTEX_SHADER				"./Shaders/FullScreenQuad.vert"

#define	COPY_FRAGMENT_SHADER			"./Shaders/Copy.frag"
#define	BLUR_FRAGMENT_SHADER			"./Shaders/GaussianBlur.frag"
#define	DISTORTION_FRAGMENT_SHADER		"./Shaders/Distortion.frag"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

const RHI::EPixelFormat	CPostFxDistortion::s_DistortionBufferFormat = RHI::FormatFloat16RGBA;

//----------------------------------------------------------------------------

CPostFxDistortion::CPostFxDistortion()
: m_SamplerRT(null)
, m_ConstSetProcessToDistord(null)
, m_ConstSetProcessOutput(null)
, m_RenderStateOffset(null)
, m_RenderStateBlurH(null)
, m_RenderStateBlurV(null)
, m_RenderStateSkip(null)
, m_DistortionIntensity(1.f)
, m_ChromaticAberrationMultipliers(CFloat4(0.01f, 0.0125f, 0.015f, 0.0175f))
, m_PushMultipliers(m_DistortionIntensity * m_ChromaticAberrationMultipliers)
{
}

//----------------------------------------------------------------------------

CPostFxDistortion::~CPostFxDistortion()
{
}

//----------------------------------------------------------------------------

bool	CPostFxDistortion::Init(	const RHI::PApiManager &apiManager,
									const RHI::PGpuBuffer &fullScreenQuadVbo,
									const RHI::PConstantSampler &samplerRT,
									const RHI::SConstantSetLayout &samplerRTLayout)
{
	m_FullScreenQuadVbo = fullScreenQuadVbo;
	m_ApiManager = apiManager;
	m_SamplerRT = samplerRT;
	m_SamplerRTLayout = samplerRTLayout;
	return true;
}

//----------------------------------------------------------------------------

bool		CPostFxDistortion::UpdateFrameBuffer(	const TMemoryView<const RHI::PFrameBuffer> &frameBuffers,
													TArray<RHI::ELoadRTOperation> &loadOP,
													TArray<RHI::SFrameBufferClearValue> &clearValues,
													const SInOutRenderTargets *inOut)
{
	CUint2					frameBufferSize = frameBuffers.First()->GetSize();

	m_ToDistordRt = *inOut->m_ToDistordRt;
	// Distortion map RT
	if (!PK_VERIFY(m_DistoRenderTarget.CreateRenderTarget(RHI::SRHIResourceInfos("Distortion Render Target"), m_ApiManager, m_SamplerRT, s_DistortionBufferFormat, frameBufferSize, m_SamplerRTLayout)))
		return false;
	// Output
	if (!PK_VERIFY(m_OutputRenderTarget.CreateRenderTarget(RHI::SRHIResourceInfos("Distortion Output Render Target"), m_ApiManager, m_SamplerRT, s_DistortionBufferFormat, frameBufferSize, m_SamplerRTLayout)))
		return false;

	for (u32 i = 0; i < frameBuffers.Count(); ++i)
	{
		PK_ASSERT(frameBufferSize == frameBuffers[i]->GetSize());

		// Distortion map RT
		m_DistoBufferIdx = frameBuffers[i]->GetRenderTargets().Count();
		if (!PK_VERIFY(frameBuffers[i]->AddRenderTarget(m_DistoRenderTarget.m_RenderTarget)))
			return false;
		// Output
		m_OutputBufferIdx = frameBuffers[i]->GetRenderTargets().Count();
		if (!PK_VERIFY(frameBuffers[i]->AddRenderTarget(m_OutputRenderTarget.m_RenderTarget)))
			return false;
	}

	// Distortion map RT
	if (!PK_VERIFY(	loadOP.PushBack(RHI::LoadClear).Valid() &&
					clearValues.PushBack(RHI::SFrameBufferClearValue(0.f, 0.f, 0.f, 0.f)).Valid()))
		return false;
	// Output
	if (!PK_VERIFY(loadOP.PushBack(RHI::LoadClear).Valid() &&
		clearValues.PushBack(RHI::SFrameBufferClearValue(0.f, 0.f, 0.f, 0.f)).Valid()))
		return false;

	CreateProcessTextureConstSetLayouts(m_ConstSetLayoutProcess);

	m_ConstSetProcessToDistord = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Distort Constant Set"), m_ConstSetLayoutProcess);
	m_ConstSetProcessOutput = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Distort Output Constant Set"), m_ConstSetLayoutProcess);

	if (m_ConstSetProcessToDistord == null || m_ConstSetProcessOutput == null)
		return false;

	RHI::PTexture	sampleRT = m_DistoRenderTarget.m_RenderTarget->GetTexture();

	m_ConstSetProcessToDistord->SetConstants(m_SamplerRT, m_ToDistordRt.m_RenderTarget->GetTexture(), 0);
	m_ConstSetProcessToDistord->SetConstants(m_SamplerRT, sampleRT, 1);
	m_ConstSetProcessToDistord->UpdateConstantValues();

	m_ConstSetProcessOutput->SetConstants(m_SamplerRT, m_OutputRenderTarget.m_RenderTarget->GetTexture(), 0);
	m_ConstSetProcessOutput->SetConstants(m_SamplerRT, sampleRT, 1);
	m_ConstSetProcessOutput->UpdateConstantValues();
	return true;
}

//----------------------------------------------------------------------------

bool		CPostFxDistortion::AddSubPasses(const RHI::PRenderPass &renderPass, const SInOutRenderTargets *inOut)
{
	bool	success = true;

	m_DistoSubPassIdx = renderPass->GetSubPassDefinitions().Count();

	// Render the disto texture:
	RHI::SSubPassDefinition	subPass;
	subPass.m_DepthStencilRenderTarget = inOut->m_DepthRtIdx;
	for (u32 i = 0; i < inOut->m_ParticleInputs.Count(); ++i)
		subPass.m_InputRenderTargets.PushBack(inOut->m_ParticleInputs[i]);
	subPass.m_OutputRenderTargets.PushBack(m_DistoBufferIdx);
	success &= renderPass->AddRenderSubPass(subPass);

	// Distord the samplableRenderOutputToDistordIdx in the m_OutputRenderTarget
	RHI::SSubPassDefinition		subPassPost;
	subPassPost.m_OutputRenderTargets.PushBack(m_OutputBufferIdx);
	subPassPost.m_InputRenderTargets.PushBack(inOut->m_ToDistordRtIdx);
	subPassPost.m_InputRenderTargets.PushBack(m_DistoBufferIdx);
	success &= renderPass->AddRenderSubPass(subPassPost);

	// HBlur m_OutputRenderTarget -> samplableRenderOutputToDistordIdx
	RHI::SSubPassDefinition		subPassBlurH;
	subPassBlurH.m_OutputRenderTargets.PushBack(inOut->m_ToDistordRtIdx);
	subPassBlurH.m_InputRenderTargets.PushBack(m_OutputBufferIdx);
	subPassBlurH.m_InputRenderTargets.PushBack(m_DistoBufferIdx);
	success &= renderPass->AddRenderSubPass(subPassBlurH);

	// VBlur samplableRenderOutputToDistordIdx -> m_OutputRenderTarget
	RHI::SSubPassDefinition		subPassBlurV;
	if (inOut->m_OutputRtIdx.Valid())
		subPassBlurV.m_OutputRenderTargets.PushBack(inOut->m_OutputRtIdx);
	else
		subPassBlurV.m_OutputRenderTargets.PushBack(m_OutputBufferIdx);
	subPassBlurV.m_InputRenderTargets.PushBack(inOut->m_ToDistordRtIdx);
	subPassBlurV.m_InputRenderTargets.PushBack(m_DistoBufferIdx);
	success &= renderPass->AddRenderSubPass(subPassBlurV);

	m_PostDistortionSubPassIdx = renderPass->GetSubPassDefinitions().Count();

	// Render particles post distortion -> m_OutputRenderTarget
	RHI::SSubPassDefinition		postDistortion;
	postDistortion.m_DepthStencilRenderTarget = inOut->m_DepthRtIdx;
	for (u32 i = 0; i < inOut->m_ParticleInputs.Count(); ++i)
		postDistortion.m_InputRenderTargets.PushBack(inOut->m_ParticleInputs[i]);
	if (inOut->m_OutputRtIdx.Valid())
		postDistortion.m_OutputRenderTargets.PushBack(inOut->m_OutputRtIdx);
	else
		postDistortion.m_OutputRenderTargets.PushBack(m_OutputBufferIdx);
	success &= renderPass->AddRenderSubPass(postDistortion);
	return success;
}

//----------------------------------------------------------------------------

bool		CPostFxDistortion::CreateRenderStates(	const TMemoryView<const RHI::SRenderTargetDesc> &frameBufferLayout,
													CShaderLoader &shaderLoader,
													const RHI::PRenderPass &renderPass)
{
	m_RenderStateOffset = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("Distortion Offset Render State"));
	m_RenderStateBlurH = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("Distortion BlurH Render State"));
	m_RenderStateBlurV = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("Distortion BlurV Render State"));
	m_RenderStateSkip = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("Distortion Skip Render State"));

	if (m_RenderStateOffset == null || m_RenderStateSkip == null ||
		m_RenderStateBlurH == null || m_RenderStateBlurV == null)
		return false;

	// renderState Offset

	CShaderLoader::SShadersPaths	shadersPathsDistortion;
	shadersPathsDistortion.m_Vertex = QUAD_VERTEX_SHADER;
	shadersPathsDistortion.m_Fragment = DISTORTION_FRAGMENT_SHADER;

	RHI::SRenderState		renderState;
	renderState.m_PipelineState.m_DynamicScissor = true;
	renderState.m_PipelineState.m_DynamicViewport = true;
	if (!renderState.m_InputVertexBuffers.PushBack().Valid())
		return false;
	renderState.m_InputVertexBuffers.Last().m_Stride = sizeof(CFloat2);

	FillDistortionShaderBindings(renderState.m_ShaderBindings, m_ConstSetLayoutProcess);

	if (shaderLoader.LoadShader(renderState, shadersPathsDistortion, m_ApiManager) == false)
		return false;
	m_RenderStateOffset->m_RenderState = renderState;

	// Skip renderState Blur

	CShaderLoader::SShadersPaths	shadersPathsBlur;
	shadersPathsBlur.m_Vertex = QUAD_VERTEX_SHADER;
	shadersPathsBlur.m_Fragment = BLUR_FRAGMENT_SHADER;

	RHI::SRenderState		renderStateBlur;
	renderStateBlur.m_PipelineState.m_DynamicScissor = true;
	renderStateBlur.m_PipelineState.m_DynamicViewport = true;
	if (!renderStateBlur.m_InputVertexBuffers.PushBack().Valid())
		return false;
	renderStateBlur.m_InputVertexBuffers.Last().m_Stride = sizeof(CFloat2);

	FillGaussianBlurShaderBindings(	GaussianBlurCombination_13_Tap,
									renderStateBlur.m_ShaderBindings,
									m_ConstSetLayoutProcess);

	if (shaderLoader.LoadShader(renderStateBlur, shadersPathsBlur, m_ApiManager) == false)
		return false;
	m_RenderStateBlurH->m_RenderState = renderStateBlur;
	m_RenderStateBlurV->m_RenderState = renderStateBlur;

	// Skip renderState

	CShaderLoader::SShadersPaths	shadersPathsCopy;
	shadersPathsCopy.m_Vertex = QUAD_VERTEX_SHADER;
	shadersPathsCopy.m_Fragment = COPY_FRAGMENT_SHADER;

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
	return	m_ApiManager->BakeRenderState(m_RenderStateOffset, frameBufferLayout, renderPass, m_DistoSubPassIdx + 1) &&
			m_ApiManager->BakeRenderState(m_RenderStateBlurH, frameBufferLayout, renderPass, m_DistoSubPassIdx + 2) &&
			m_ApiManager->BakeRenderState(m_RenderStateBlurV, frameBufferLayout, renderPass, m_DistoSubPassIdx + 3) &&
			m_ApiManager->BakeRenderState(m_RenderStateSkip, frameBufferLayout, renderPass, m_DistoSubPassIdx + 1);
}

//----------------------------------------------------------------------------

bool	CPostFxDistortion::Draw(const RHI::PCommandBuffer &cmdBuff)
{
	bool result = true;
	result &= _Draw_UVoffset(cmdBuff, m_ConstSetProcessToDistord);
	cmdBuff->NextRenderSubPass();
	result &= _Draw_Blur(cmdBuff, m_ConstSetProcessOutput, true);
	cmdBuff->NextRenderSubPass();
	result &= _Draw_Blur(cmdBuff, m_ConstSetProcessToDistord, false);
	return result;
}

//----------------------------------------------------------------------------

bool	CPostFxDistortion::_Draw_UVoffset(const RHI::PCommandBuffer &cmdBuff, const RHI::PConstantSet &inputSampler)
{
	PK_NAMEDSCOPEDPROFILE("Distortion pass (UV offset)");

	cmdBuff->BindRenderState(m_RenderStateOffset);
	cmdBuff->BindVertexBuffers(TMemoryView<const RHI::PGpuBuffer>(m_FullScreenQuadVbo));

	cmdBuff->BindConstantSets(TMemoryView<const RHI::PConstantSet>(inputSampler));

	SDistortionInfo distortionInfo(m_PushMultipliers);

	cmdBuff->PushConstant(&distortionInfo, 0);
	cmdBuff->Draw(0, 6);

	return true;
}

//----------------------------------------------------------------------------

bool	CPostFxDistortion::_Draw_Blur(const RHI::PCommandBuffer &cmdBuff, const RHI::PConstantSet &inputSampler, bool isDirHortizontal)
{
	PK_NAMEDSCOPEDPROFILE("Distortion pass (Blur)");

	cmdBuff->BindRenderState(isDirHortizontal ? m_RenderStateBlurH : m_RenderStateBlurV);
	cmdBuff->BindVertexBuffers(TMemoryView<const RHI::PGpuBuffer>(m_FullScreenQuadVbo));

	const CUint2	contextSize = m_OutputRenderTarget.m_RenderTarget->GetSize();
	const SBlurInfo	blurInfo(contextSize, isDirHortizontal ? CFloat2(1.f, 0.f) : CFloat2(0.f, 1.f));

	cmdBuff->PushConstant(&blurInfo, 0);

	cmdBuff->BindConstantSets(TMemoryView<const RHI::PConstantSet>(inputSampler));

	cmdBuff->Draw(0, 6);

	return true;
}

//----------------------------------------------------------------------------

bool	CPostFxDistortion::Draw_JustCopy(const RHI::PCommandBuffer &cmdBuff)
{
	PK_NAMEDSCOPEDPROFILE("Distortion Skip pass (just a copy)");

	cmdBuff->BindRenderState(m_RenderStateSkip);
	cmdBuff->BindVertexBuffers(TMemoryView<const RHI::PGpuBuffer>(m_FullScreenQuadVbo));

	cmdBuff->BindConstantSets(TMemoryView<const RHI::PConstantSet>(m_ToDistordRt.m_SamplerConstantSet));
	cmdBuff->Draw(0, 6);

	cmdBuff->NextRenderSubPass();
	cmdBuff->NextRenderSubPass();

	return true;
}

void CPostFxDistortion::SetChromaticAberrationIntensity(float intensity)
{
	m_DistortionIntensity = intensity;
	m_PushMultipliers = m_DistortionIntensity * m_ChromaticAberrationMultipliers;
}

void CPostFxDistortion::SetAberrationMultipliers(CFloat4 multipliers)
{
	m_ChromaticAberrationMultipliers = multipliers;
	m_PushMultipliers = m_DistortionIntensity * m_ChromaticAberrationMultipliers;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
