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

#include "PostFxColorRemap.h"

#include <PK-SampleLib/ShaderDefinitions/UnitTestsShaderDefinitions.h>
#include <PK-SampleLib/ShaderDefinitions/BasicSceneShaderDefinitions.h>

#include <pk_rhi/include/AllInterfaces.h>

#define QUAD_VERTEX_SHADER				"./Shaders/FullScreenQuad.vert"

#define COLOR_REMAP_FRAGMENT_SHADER		"./Shaders/ColorRemap.frag"
#define	COPY_FRAGMENT_SHADER			"./Shaders/Copy.frag"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

CPostFxColorRemap::CPostFxColorRemap()
:	m_HasTexture(false)
{
}

CPostFxColorRemap::~CPostFxColorRemap()
{
}

//----------------------------------------------------------------------------

bool CPostFxColorRemap::Init(	const RHI::PApiManager &apiManager,
								const RHI::PGpuBuffer &fullScreenQuadVbo,
								const RHI::PConstantSampler &samplerRT,
								const RHI::SConstantSetLayout &samplerRTLayout)
{
	m_FullScreenQuadVbo = fullScreenQuadVbo;
	m_ApiManager = apiManager;
	m_SamplerRT = samplerRT;
	m_SamplerRTLayout = samplerRTLayout;

	m_ColorLUTSetLayout.Reset();
	m_ColorLUTSetLayout.m_ShaderStagesMask = RHI::FragmentShaderMask;

	m_RemapTextureSampler = apiManager->CreateConstantSampler(	RHI::SRHIResourceInfos("Color Remap Texture Sampler"),
																RHI::EFilteringMode::SampleNearest, // Interpolation done in the shader
																RHI::EFilteringMode::SampleNearest,
																RHI::EWrapMode::SampleClampToEdge,
																RHI::EWrapMode::SampleClampToEdge,
																RHI::EWrapMode::SampleClampToEdge,
																1,
																false);

	// FullScreen sampler:
	RHI::SConstantSamplerDesc	fullscreenSamplerLayout("Texture", RHI::SamplerTypeSingle);
	m_ColorLUTSetLayout.AddConstantsLayout(fullscreenSamplerLayout);
	
	// Color LUT sampler :
	RHI::SConstantSamplerDesc	distortionSamplerLayout("LookUp", RHI::SamplerTypeSingle);
	m_ColorLUTSetLayout.AddConstantsLayout(distortionSamplerLayout);

	m_ColorLUTSet = apiManager->CreateConstantSet(RHI::SRHIResourceInfos("Color Remap Constant Set"), m_ColorLUTSetLayout);
	
	return true;
}

//----------------------------------------------------------------------------

bool CPostFxColorRemap::UpdateFrameBuffer(	TMemoryView<RHI::PFrameBuffer> frameBuffers,
											TArray<RHI::ELoadRTOperation> &loadOP,
											const SInOutRenderTargets *inOut)
{
	const CUint2	frameBuffersSize = frameBuffers.First()->GetSize();

	if (!inOut->m_OutputRenderTargetIdx.Valid())
	{
		if (!PK_VERIFY(m_OutputRenderTarget.CreateRenderTarget(RHI::SRHIResourceInfos("Color Remap Output Render Target"), m_ApiManager, m_SamplerRT,  RHI::FormatFloat16RGBA, frameBuffersSize, m_SamplerRTLayout)))
			return false;

		if (!PK_VERIFY(loadOP.PushBack(RHI::LoadDontCare).Valid()))
			return false;

		for (u32 i = 0; i < frameBuffers.Count(); ++i)
		{
			PK_ASSERT(frameBuffersSize == frameBuffers[i]->GetSize());

			m_OutputRenderTargetIdx = frameBuffers[i]->GetRenderTargets().Count();
			if (!PK_VERIFY(frameBuffers[i]->AddRenderTarget(m_OutputRenderTarget.m_RenderTarget)))
				return false;
		}
	}

	m_InputRenderTarget = *inOut->m_InputRenderTarget;
	
	m_ColorLUTSet->SetConstants(m_SamplerRT, m_InputRenderTarget.m_RenderTarget->GetTexture(), 0);

	if (m_InputRenderTarget.m_RenderTarget != null && m_RemapTexture != null)
		m_ColorLUTSet->UpdateConstantValues();

	return true;
}

//----------------------------------------------------------------------------

bool CPostFxColorRemap::AddSubPasses(const RHI::PRenderPass &renderPass, const SInOutRenderTargets *inOut)
{
	bool						success = true;
	RHI::SSubPassDefinition		colorRemapSubPass;

	m_SubPassIdx = renderPass->GetSubPassDefinitions().Count();

	if (inOut->m_OutputRenderTargetIdx.Valid())
		success &= colorRemapSubPass.m_OutputRenderTargets.PushBack(inOut->m_OutputRenderTargetIdx).Valid();
	else
		success &= colorRemapSubPass.m_OutputRenderTargets.PushBack(m_OutputRenderTargetIdx).Valid();
	if (inOut->m_InputRenderTargetIdx.Valid())
		success &= colorRemapSubPass.m_InputRenderTargets.PushBack(inOut->m_InputRenderTargetIdx).Valid();

	success &= renderPass->AddRenderSubPass(colorRemapSubPass);
	return success;
}

//----------------------------------------------------------------------------

bool CPostFxColorRemap::CreateRenderStates(	const TMemoryView<const RHI::SRenderTargetDesc> &frameBufferLayout,
											CShaderLoader &loader,
											const RHI::PRenderPass &renderPass)
{
	m_RenderState = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("Color Remap Render State"));
	m_RenderStateSkip = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("Color Remap Skip Render State"));

	if (m_RenderState == null || m_RenderStateSkip == null)
		return false;

	// Color Remap Render State
	RHI::SRenderState		remapRenderState;
	remapRenderState.m_PipelineState.m_DynamicScissor = true;
	remapRenderState.m_PipelineState.m_DynamicViewport = true;
	if (!remapRenderState.m_InputVertexBuffers.PushBack().Valid())
		return false;

	remapRenderState.m_InputVertexBuffers.Last().m_Stride = sizeof(CFloat2);

	CShaderLoader::SShadersPaths	shadersPaths;
	shadersPaths.m_Vertex = QUAD_VERTEX_SHADER;
	shadersPaths.m_Fragment = COLOR_REMAP_FRAGMENT_SHADER;

	FillColorRemapShaderBindings(remapRenderState.m_ShaderBindings, m_ColorLUTSetLayout);

	if (loader.LoadShader(remapRenderState, shadersPaths, m_ApiManager) == false)
		return false;

	m_RenderState->m_RenderState = remapRenderState;

	// Skip Render State
	RHI::SRenderState		renderStateSkip;
	renderStateSkip.m_PipelineState.m_DynamicScissor = true;
	renderStateSkip.m_PipelineState.m_DynamicViewport = true;
	if (!renderStateSkip.m_InputVertexBuffers.PushBack().Valid())
		return false;
	renderStateSkip.m_InputVertexBuffers.Last().m_Stride = sizeof(CFloat2);

	FillCopyShaderBindings(CopyCombination_Basic, renderStateSkip.m_ShaderBindings, m_SamplerRTLayout);

	shadersPaths.m_Fragment = COPY_FRAGMENT_SHADER;
	if (loader.LoadShader(renderStateSkip, shadersPaths, m_ApiManager) == false)
		return false;
	m_RenderStateSkip->m_RenderState = renderStateSkip;

	return	m_ApiManager->BakeRenderState(m_RenderState, frameBufferLayout, renderPass, m_SubPassIdx) &&
			m_ApiManager->BakeRenderState(m_RenderStateSkip, frameBufferLayout, renderPass, m_SubPassIdx);
}

//----------------------------------------------------------------------------

void CPostFxColorRemap::SetRemapTexture(const RHI::PTexture texture, bool isPlaceholder, CUint3 dimensions)
{
	m_HasTexture = !isPlaceholder;
	m_ColorLUTSet->SetConstants(m_RemapTextureSampler, texture, 1);

	if(dimensions.x() != dimensions.y() * dimensions.y() && !isPlaceholder)
		CLog::Log(PK_WARN, "Color Remap input texture dimensions are expected to be height*height x height");

	m_ColorLutHeight = dimensions.y();
	
	if (m_InputRenderTarget.m_RenderTarget != null && texture != null)
		m_ColorLUTSet->UpdateConstantValues();

	m_RemapTexture = texture;
}

//----------------------------------------------------------------------------

bool	CPostFxColorRemap::Draw(const RHI::PCommandBuffer &commandBuffer)
{
	PK_NAMEDSCOPEDPROFILE("Color Remap Pass");

	commandBuffer->BindRenderState(m_RenderState);
	commandBuffer->BindVertexBuffers(TMemoryView<const RHI::PGpuBuffer>(m_FullScreenQuadVbo));
	commandBuffer->BindConstantSets(TMemoryView<const RHI::PConstantSet>(m_ColorLUTSet));

	SColorRemapInfo		colorRemapInfo(m_ColorLutHeight);
	commandBuffer->PushConstant(&m_ColorLutHeight, 0);
	commandBuffer->Draw(0, 6);
	
	return true;
}

//----------------------------------------------------------------------------

bool	CPostFxColorRemap::Draw_JustCopy(const RHI::PCommandBuffer &cmdBuff)
{
	PK_NAMEDSCOPEDPROFILE("Tone-Mapping Skip pass (just a copy)");

	cmdBuff->BindRenderState(m_RenderStateSkip);
	cmdBuff->BindVertexBuffers(TMemoryView<const RHI::PGpuBuffer>(m_FullScreenQuadVbo));
	cmdBuff->BindConstantSets(TMemoryView<const RHI::PConstantSet>(m_InputRenderTarget.m_SamplerConstantSet));

	cmdBuff->Draw(0, 6);
	return true;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END	
