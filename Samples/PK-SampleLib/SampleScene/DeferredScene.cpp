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

#include "DeferredScene.h"
#include "SampleUtils.h"
#include "WindowContext/AWindowContext.h"
#include "ShaderDefinitions/SampleLibShaderDefinitions.h"
#include "ShaderDefinitions/UnitTestsShaderDefinitions.h"
#include "RenderIntegrationRHI/RendererCache.h"
#include "BRDFLUT.h"
#include "ImguiRhiImplem.h"
#include "imgui.h"

#include <pk_rhi/include/AllInterfaces.h>

//----------------------------------------------------------------------------

#define	FULL_SCREEN_QUAD_VERTEX_SHADER_PATH			"./Shaders/FullScreenQuad.vert"
#define	COPY_FRAGMENT_SHADER_PATH					"./Shaders/Copy.frag"
#define	DITHERING_PATTERNS_TEXTURE_PATH				"./Textures/DitheringPatterns.png"

#define	CAMERA_NEAR_PLANE				1.0e-1f
#define	CAMERA_FAR_PLANE				1.0e+3f

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

CDeferredScene::CDeferredScene()
:	m_CameraGUI_Distance(0)
,	m_CameraGUI_Angles(0)
,	m_CameraGUI_LookatPosition(0)
,	m_CameraFlag_ResetProj(false)
,	m_BackgroundColorRGB(CFloat3::ZERO)
,	m_IsInit(false)
,	m_Controller(null)
,	m_PrevMousePosition(0)
,	m_UserMaxBloomRenderPass(6)
,	m_MaxBloomRenderPass(0)
,	m_DisplayFpsAccTime(0)
,	m_DisplayFpsAccFrames(0)
,	m_DisplayFps(TNumericTraits<float>::PositiveInfinity())
,	m_RtToDraw(RenderTarget_Scene)
,	m_ShowAlpha(false)
{
	m_Controller = File::NewFileSystem();
	m_Controller->MountPack("UnitTests");
	m_ShaderLoader.SetDefaultFSController(m_Controller);
}

//----------------------------------------------------------------------------

CDeferredScene::~CDeferredScene()
{
	PKSample::CRendererCacheInstance_UpdateThread::RenderThread_DestroyAllResources();
	PK_DELETE(m_Controller);
}

//----------------------------------------------------------------------------

RHI::PRenderState	CDeferredScene::GetGBufferMaterial(EGBufferCombination matCombination) const
{
	if (matCombination < GBufferCombination_Count)
		return m_GBuffer.m_GBufferMaterials[matCombination];
	return null;
}

//----------------------------------------------------------------------------

bool	CDeferredScene::InitIFN()
{
	if (m_IsInit)
		return true;

	// --------------------------------------
	// Create default sampler
	// --------------------------------------
	CreateSimpleSamplerConstSetLayouts(m_DefaultSamplerConstLayout, false);
	if (m_DefaultSampler == null)
		m_DefaultSampler = m_ApiManager->CreateConstantSampler(RHI::SRHIResourceInfos("Default Sampler"), RHI::SampleLinear, RHI::SampleLinear, RHI::SampleClampToEdge, RHI::SampleClampToEdge, RHI::SampleClampToEdge, 1);
	if (m_DefaultSampler == null)
		return false;
	// --------------------------------------

	// --------------------------------------
	// Create scene info
	// --------------------------------------
	{
		if (!CreateSceneInfoConstantLayout(m_SceneInfoConstantSetLayout))
			return false;

		const RHI::SConstantBufferDesc	&sceneInfoBufferDesc = m_SceneInfoConstantSetLayout.m_Constants.First().m_ConstantBuffer;

		m_SceneInfoConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("SceneInfos Constant Set"), m_SceneInfoConstantSetLayout);
		if (!PK_VERIFY(m_SceneInfoConstantSet != null))
			return false;

		const u32	constantBufferSize = sceneInfoBufferDesc.m_ConstantBufferSize; // We could also create the new one based on the default one created
		PK_ASSERT(constantBufferSize == sizeof(PKSample::SSceneInfoData));
		m_SceneInfoConstantBuffer = m_ApiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("SceneInfos Constant Buffer"), RHI::ConstantBuffer, constantBufferSize);
		if (!PK_VERIFY(m_SceneInfoConstantBuffer != null))
			return false;
		if (!PK_VERIFY(m_SceneInfoConstantSet->SetConstants(m_SceneInfoConstantBuffer, 0)))
			return false;
		if (!PK_VERIFY(m_SceneInfoConstantSet->UpdateConstantValues()))
			return false;
		// Update scene info:
		Utils::SetupSceneInfoData(m_SceneInfoData, GetCamera().Camera(), CCoordinateFrame::GlobalFrame());
	}
	// --------------------------------------

	// Init render-pass: gbuffer
	if (!m_GBuffer.Init(m_ApiManager, m_DefaultSampler, m_DefaultSamplerConstLayout, m_SceneInfoConstantSetLayout))
		return false;

	// Init render-pass: distortion
	m_Distortion.SetChromaticAberrationIntensity(m_SceneOptions.m_Distortion.m_DistortionIntensity);
	m_Distortion.SetAberrationMultipliers(m_SceneOptions.m_Distortion.m_ChromaticAberrationMultipliers);
	if (!m_Distortion.Init(m_ApiManager, m_GBuffer.m_QuadBuffers.m_VertexBuffers[0], m_DefaultSampler, m_DefaultSamplerConstLayout))
		return false;

	// Init render-pass: Bloom
	if (!m_Bloom.Init(m_ApiManager, m_GBuffer.m_QuadBuffers.m_VertexBuffers[0]))
		return false;

	// Init render-pass: tone-mapping (+ vignetting)
	m_ToneMapping.SetExposure(m_SceneOptions.m_ToneMapping.m_Exposure);
	m_ToneMapping.SetSaturation(m_SceneOptions.m_ToneMapping.m_Saturation);
	if (!m_ToneMapping.Init(m_ApiManager, m_GBuffer.m_QuadBuffers.m_VertexBuffers[0], m_DefaultSampler, m_DefaultSamplerConstLayout))
		return false;

	// Initialize the camera
	m_Camera.SetProj(60, CFloat2(m_WindowContext->GetDrawableSize()), CAMERA_NEAR_PLANE, CAMERA_FAR_PLANE);
	UpdateCamera();

	m_PrevMousePosition = m_WindowContext->GetMousePosition();

	// Load default assets, fixme: use default manager and unload
	if (STextureKey::s_DefaultResourceID.Valid() ||
		SGeometryKey::s_DefaultResourceID.Valid())
	{
		const CString		rootPath = m_Controller->VirtualToPhysical(CString::EmptyString, IFileSystem::Access_Read);
		const SCreateArg	args(m_ApiManager, null, Resource::DefaultManager(), rootPath);

		if (STextureKey::s_DefaultResourceID.Valid())
			CTextureManager::RenderThread_ResolveResource(STextureKey::s_DefaultResourceID, args);
		if (SGeometryKey::s_DefaultResourceID.Valid())
			CGeometryManager::RenderThread_ResolveResource(SGeometryKey::s_DefaultResourceID, args);
	}

	// --------------------------------------
	// Create default textures used for rendering
	// --------------------------------------
	// Load a white dummy cube texture
	u32			white = 0xFFFFFFFF;
	CImageMap	dummyWhite(CUint3::ONE, &white, sizeof(u32));
	CImageMap	dummyCube[6];
	for (u32 i = 0; i < 6; i++)
		dummyCube[i] = dummyWhite;
	m_DummyCube = m_ApiManager->CreateTexture(RHI::SRHIResourceInfos("Dummy Cube Texture"), dummyCube, RHI::FormatUnorm8RGBA, RHI::kDefaultComponentSwizzle, RHI::TextureCubemap);
	if (!PK_VERIFY(m_DummyCube != null))
		return false;
	m_DummyWhite = m_ApiManager->CreateTexture(RHI::SRHIResourceInfos("Dummy White Texture"), TMemoryView<const CImageMap>(dummyWhite), RHI::FormatUnorm8RGBA, RHI::kDefaultComponentSwizzle, RHI::Texture2D);
	if (!PK_VERIFY(m_DummyWhite != null))
		return false;
	// Create the sampler
	m_DummyCubeSampler = m_ApiManager->CreateConstantSampler(	RHI::SRHIResourceInfos("Dummy Cube Sampler"),
																RHI::SampleLinear,
																RHI::SampleLinearMipmapLinear,
																RHI::SampleRepeat,
																RHI::SampleRepeat,
																RHI::SampleRepeat,
																m_DummyCube->GetMipmapCount());
	// Cubemap rotation matrix
	RHI::PGpuBuffer			cubemapRotation = m_ApiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("CubemapRotation"), RHI::ConstantBuffer, sizeof(TMatrix<float, 2, 4>));
	TMatrix<float, 2, 4>	*data = static_cast<TMatrix<float, 2, 4>*>(m_ApiManager->MapCpuView(cubemapRotation));
	*data = TMatrix<float, 2, 4>(CFloat4(0, 0, 0, 0), CFloat4(0, 0, 0, 0));
	m_ApiManager->UnmapCpuView(cubemapRotation);

	// Create the constant set
	m_DummyCubeConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Dummy Cube Constant Set"), GetGBuffer().m_EnvironmentMapLayout);
	if (!PK_VERIFY(m_DummyCubeConstantSet != null))
		return false;
	m_DummyCubeConstantSet->SetConstants(m_DummyCubeSampler, m_DummyCube, 0);
	m_DummyCubeConstantSet->SetConstants(cubemapRotation, 1);
	m_DummyCubeConstantSet->UpdateConstantValues();

	// Load BRDF LUT
	const CImageMap	mipmaps[] = { CImageMap(CUint3(64, 64, 1), (void*)BRDFLUTArray, BRDFLUTArraySize) };
	PK_STATIC_ASSERT(BRDFLUTArraySize == 64 * 64 * 2 * 2);
	RHI::PTexture	BRDFLUT = m_ApiManager->CreateTexture(RHI::SRHIResourceInfos("BRDFLUT Texture"), mipmaps, RHI::FormatFloat16RG);
	if (!PK_VERIFY(BRDFLUT != null))
		return false;
	// Create the sampler
	RHI::PConstantSampler	BRDFLUTSampler = m_ApiManager->CreateConstantSampler(	RHI::SRHIResourceInfos("BRDFLUT Sampler"),
																				RHI::SampleLinear,
																				RHI::SampleLinearMipmapLinear,
																				RHI::SampleClampToEdge,
																				RHI::SampleClampToEdge,
																				RHI::SampleClampToEdge,
																				1);
	// Create the constant set
	m_BRDFLUTConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("BRDFLUT Constant Set"), GetGBuffer().m_BRDFLUTLayout);
	if (!PK_VERIFY(m_BRDFLUTConstantSet != null))
		return false;
	m_BRDFLUTConstantSet->SetConstants(BRDFLUTSampler, BRDFLUT, 0);
	m_BRDFLUTConstantSet->UpdateConstantValues();

	// Init noise texture
	RHI::PConstantSampler	sampler = m_ApiManager->CreateConstantSampler(RHI::SRHIResourceInfos("Dithering Sampler"),
																		RHI::SampleNearest,
																		RHI::SampleNearest,
																		RHI::SampleClampToEdge,
																		RHI::SampleClampToEdge,
																		RHI::SampleClampToEdge,
																		1);

	STextureKey	textureKey;
	textureKey.m_Path = DITHERING_PATTERNS_TEXTURE_PATH;
	CTextureManager::CResourceId textureId = CTextureManager::UpdateThread_GetResource(textureKey, SPrepareArg(null, null, null));
	if (!textureId.Valid())
		return false;

	const CString		rootPath = m_Controller->VirtualToPhysical(CString::EmptyString, IFileSystem::Access_Read);
	const SCreateArg	args(m_ApiManager, null, Resource::DefaultManager(), rootPath);
	RHI::PTexture		ditheringtexture = CTextureManager::RenderThread_ResolveResource(textureId, args);
	if (ditheringtexture == null)
		return false;

	m_NoiseTexture.m_NoiseTexture = ditheringtexture;
	m_NoiseTexture.m_NoiseTextureConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Dithering Constant Set"), SConstantNoiseTextureKey::GetNoiseTextureConstantSetLayout());
	m_NoiseTexture.m_NoiseTextureConstantSet->SetConstants(sampler, m_NoiseTexture.m_NoiseTexture, 0);
	m_NoiseTexture.m_NoiseTextureConstantSet->UpdateConstantValues();

	// --------------------------------------

	m_IsInit = true;
	return true;
}

//----------------------------------------------------------------------------

bool	CDeferredScene::CreateRenderTargets(bool recreateSwapChain)
{
	(void)recreateSwapChain;
	return true;
}

//----------------------------------------------------------------------------

bool	CDeferredScene::CreateRenderPasses()
{
	return true;
}

//----------------------------------------------------------------------------

bool	CDeferredScene::CreateRenderStates()
{
	return true;
}

//----------------------------------------------------------------------------

bool	CDeferredScene::CreateFrameBuffers(bool recreateSwapChain)
{
	if (!recreateSwapChain && !InitIFN())
		return false;

	if (!PK_VERIFY(!m_SwapChainRenderTargets.Empty()))
		return false;

	UpdateMaxBloomRTs();

	// Reset the size
	m_Camera.SetProj(60, CFloat2(m_WindowContext->GetDrawableSize()), CAMERA_NEAR_PLANE, CAMERA_FAR_PLANE);
	UpdateCamera();

	const CUint2	frameBufferSize = m_SwapChainRenderTargets[0]->GetSize();

	m_BeforeBloomClearValues.Clear();
	m_FinalClearValues.Clear();
	m_FinalFrameBuffers.Clear();
	m_BeforeBloomFrameBuffer = null;

	// Create the frame buffers and the render passes:
	TArray<RHI::ELoadRTOperation>		beforeBloomLoadOp;
	TArray<RHI::ELoadRTOperation>		finalLoadOp;

	m_BeforeBloomFrameBuffer = m_ApiManager->CreateFrameBuffer(RHI::SRHIResourceInfos("BeforeBloom Frame Buffer"));

	if (!PK_VERIFY(m_BeforeBloomFrameBuffer != null))
		return false;

	if (!PK_VERIFY(m_FinalFrameBuffers.Resize(m_SwapChainRenderTargets.Count())))
		return false;

	for (u32 i = 0; i < m_FinalFrameBuffers.Count(); ++i)
	{
		m_FinalFrameBuffers[i] = m_ApiManager->CreateFrameBuffer(RHI::SRHIResourceInfos("Final Frame Buffer"));
		if (!PK_VERIFY(m_FinalFrameBuffers[i] != null))
			return false;
		if (!PK_VERIFY(m_FinalFrameBuffers[i]->AddRenderTarget(m_SwapChainRenderTargets[i])))
			return false;
	}
	if (!PK_VERIFY(finalLoadOp.PushBack(RHI::LoadDontCare).Valid()))
		return false;

	if (!recreateSwapChain)
	{
		m_BeforeBloomRenderPass = m_ApiManager->CreateRenderPass(RHI::SRHIResourceInfos("BeforeBloom Render Pass"));
		m_FinalRenderPass = m_ApiManager->CreateRenderPass(RHI::SRHIResourceInfos("Final Render Pass"));

		if (!PK_VERIFY(m_BeforeBloomRenderPass != null && m_FinalRenderPass != null))
			return false;
	}

	// ------------------------------------------
	//
	// GBUFFER
	//
	// ------------------------------------------
	for (u32 i = 0; i < m_GBufferRTs.Count(); ++i)
	{
		m_GBufferRTsIdx[i] = m_BeforeBloomFrameBuffer->GetRenderTargets().Count();
		if (!PK_VERIFY(m_GBufferRTs[i].CreateRenderTarget(	RHI::SRHIResourceInfos("GBuffer Render Target"),
															m_ApiManager,
															m_DefaultSampler,
															SPassDescription::s_GBufferDefaultFormats[i],
															frameBufferSize,
															m_DefaultSamplerConstLayout)))
		{
			return false;
		}
		if (!PK_VERIFY(m_BeforeBloomFrameBuffer->AddRenderTarget(m_GBufferRTs[i].m_RenderTarget)))
			return false;
	}

	// ------------------------------------------
	// Add the clear operations:
	// ------------------------------------------
	// GBuffer 0: Diffuse HDR
	if (!PK_VERIFY(m_BeforeBloomClearValues.PushBack(RHI::SFrameBufferClearValue(0.f, 0.f, 0.f, 0.f)).Valid()))
		return false;
	if (!PK_VERIFY(beforeBloomLoadOp.PushBack(RHI::LoadClear).Valid()))
		return false;
	// GBuffer 1: Depth
	if (!PK_VERIFY(m_BeforeBloomClearValues.PushBack(RHI::SFrameBufferClearValue(1.f, 1.f, 1.f, 1.f)).Valid()))
		return false;
	if (!PK_VERIFY(beforeBloomLoadOp.PushBack(RHI::LoadClear).Valid()))
		return false;
	// GBuffer 2: Emissive HDR
	if (!PK_VERIFY(m_BeforeBloomClearValues.PushBack(RHI::SFrameBufferClearValue(0.f, 0.f, 0.f, 0.f)).Valid()))
		return false;
	if (!PK_VERIFY(beforeBloomLoadOp.PushBack(RHI::LoadClear).Valid()))
		return false;

	CGBuffer::SInOutRenderTargets	gbuffInOut;

	gbuffInOut.m_SamplableDiffuseRtIdx = m_GBufferRTsIdx[0];
	gbuffInOut.m_SamplableDiffuseRt = &m_GBufferRTs[0];
	gbuffInOut.m_SamplableDepthRtIdx = m_GBufferRTsIdx[1];
	gbuffInOut.m_SamplableDepthRt = &m_GBufferRTs[1];
	gbuffInOut.m_OpaqueRTsIdx = m_GBufferRTsIdx;
	gbuffInOut.m_OpaqueRTs = m_GBufferRTs;

	m_GBuffer.SetClearMergeBufferColor(m_BackgroundColorRGB.xyz1());
	if (!PK_VERIFY(m_GBuffer.UpdateFrameBuffer(	TMemoryView<const RHI::PFrameBuffer>(m_BeforeBloomFrameBuffer),
												beforeBloomLoadOp,
												m_BeforeBloomClearValues,
												true,
												&gbuffInOut)))
		return false;
	if (!recreateSwapChain)
	{
		if (!PK_VERIFY(m_GBuffer.AddSubPasses(m_BeforeBloomRenderPass, &gbuffInOut)))
			return false;
	}

	// ------------------------------------------
	//
	// DISTORTION
	//
	// ------------------------------------------
	CPostFxDistortion::SInOutRenderTargets	distoInOut;

	distoInOut.m_ToDistordRtIdx = m_GBuffer.m_MergeBufferIndex;
	distoInOut.m_ToDistordRt = &m_GBuffer.m_Merge;
	distoInOut.m_DepthRtIdx = m_GBuffer.m_DepthBufferIndex;
	distoInOut.m_ParticleInputs = m_GBufferRTsIdx;
	if (!PK_VERIFY(m_Distortion.UpdateFrameBuffer(	TMemoryView<const RHI::PFrameBuffer>(m_BeforeBloomFrameBuffer),
													beforeBloomLoadOp,
													m_BeforeBloomClearValues,
													&distoInOut)))
		return false;
	if (!recreateSwapChain)
	{
		if (!PK_VERIFY(m_Distortion.AddSubPasses(m_BeforeBloomRenderPass, &distoInOut)))
			return false;
	}

	// ------------------------------------------
	//
	// BLOOM
	//
	// ------------------------------------------
	CPostFxBloom::SInOutRenderTargets	bloomInOut;

	bloomInOut.m_InputRenderTarget = &m_Distortion.m_OutputRenderTarget;
	bloomInOut.m_OutputRenderTargets = TMemoryView<const RHI::PRenderTarget>(bloomInOut.m_InputRenderTarget->m_RenderTarget);
	if (!PK_VERIFY(m_Bloom.UpdateFrameBuffer(m_DefaultSampler, m_UserMaxBloomRenderPass, &bloomInOut)))
		return false;
	if (!recreateSwapChain)
	{
		if (!PK_VERIFY(m_Bloom.CreateRenderPass()))
			return false;
	}
	if (!PK_VERIFY(m_Bloom.CreateFrameBuffers()))
		return false;

	// ------------------------------------------
	//
	// TONE-MAPPING + VIGNETTING
	//
	// ------------------------------------------
	CPostFxToneMapping::SInOutRenderTargets		tonemapInOut;

	tonemapInOut.m_ToTonemapRt = &m_Distortion.m_OutputRenderTarget;
	if (!PK_VERIFY(m_ToneMapping.UpdateFrameBuffer(m_FinalFrameBuffers, finalLoadOp, m_FinalClearValues, &tonemapInOut)))
		return false;
	if (!recreateSwapChain)
	{
		if (!PK_VERIFY(m_ToneMapping.AddSubPasses(m_FinalRenderPass, &tonemapInOut)))
			return false;
	}

	// ------------------------------------------
	//
	// FINAL COPY / DEBUG PASS
	//
	// ------------------------------------------
	// We add the GBuffer depth in the final frame buffers:
	CGuid		finalFrameBufferDepthIdx = m_FinalFrameBuffers.First()->GetRenderTargets().Count();
	for (u32 i = 0; i < m_FinalFrameBuffers.Count(); ++i)
	{
		if (!PK_VERIFY(m_FinalFrameBuffers[i]->AddRenderTarget(m_GBuffer.m_Depth.m_RenderTarget)))
			return false;
	}
	if (!PK_VERIFY(finalLoadOp.PushBack(RHI::LoadKeepValue).Valid()))
		return false;

	RHI::SSubPassDefinition		finalSubPass;

	if (!PK_VERIFY(finalSubPass.m_InputRenderTargets.PushBack(m_ToneMapping.m_OutputRenderTargetIdx).Valid()))
		return false;
	if (!PK_VERIFY(finalSubPass.m_OutputRenderTargets.PushBack(0).Valid()))
		return false;
	finalSubPass.m_DepthStencilRenderTarget = finalFrameBufferDepthIdx;
	if (!recreateSwapChain)
	{
		if (!PK_VERIFY(m_FinalRenderPass->AddRenderSubPass(finalSubPass)))
			return false;
	}
	// Bake the frame buffers and the render passes:
	if (!recreateSwapChain)
	{
		if (!PK_VERIFY(m_BeforeBloomRenderPass->BakeRenderPass(m_BeforeBloomFrameBuffer->GetLayout(), beforeBloomLoadOp)))
			return false;
	}
	if (!PK_VERIFY(m_BeforeBloomFrameBuffer->BakeFrameBuffer(m_BeforeBloomRenderPass)))
		return false;
	if (!recreateSwapChain)
	{
		if (!PK_VERIFY(m_FinalRenderPass->BakeRenderPass(m_FinalFrameBuffers.First()->GetLayout(), finalLoadOp)))
			return false;
	}
	for (u32 i = 0; i < m_FinalFrameBuffers.Count(); ++i)
	{
		if (!PK_VERIFY(m_FinalFrameBuffers[i]->BakeFrameBuffer(m_FinalRenderPass)))
			return false;
	}

	if (!recreateSwapChain)
	{
		// Create the render states:
		if (!PK_VERIFY(m_GBuffer.CreateRenderStates(m_BeforeBloomFrameBuffer->GetLayout(), m_ShaderLoader, m_BeforeBloomRenderPass)))
			return false;
		if (!PK_VERIFY(m_Distortion.CreateRenderStates(m_BeforeBloomFrameBuffer->GetLayout(), m_ShaderLoader, m_BeforeBloomRenderPass)))
			return false;
		if (!PK_VERIFY(m_Bloom.CreateRenderStates(m_ShaderLoader, m_SceneOptions.m_Bloom.m_BlurTap)))
			return false;
		if (!PK_VERIFY(m_ToneMapping.CreateRenderStates(m_FinalFrameBuffers.First()->GetLayout(), m_ShaderLoader, m_FinalRenderPass)))
			return false;
		// Now we create the render states:
		if (!PK_VERIFY(CreateDebugCopyRenderStates()))
			return false;
	}
	if (!m_RenderOffscreen && !PK_VERIFY(ImGuiPkRHI::CreateRenderInfo(m_ApiManager, m_ShaderLoader, m_FinalFrameBuffers.First()->GetLayout(), m_FinalRenderPass, 1)))
		return false;
#if	PKSAMPLE_HAS_PROFILER_RENDERER
	if (!PK_VERIFY(m_ProfilerRenderer->CreateRenderInfo(m_ApiManager, m_ShaderLoader, m_FinalFrameBuffers.First()->GetLayout(), m_FinalRenderPass, 1)))
		return false;
#endif
	return true;
}

//----------------------------------------------------------------------------

bool	CDeferredScene::CreateDebugCopyRenderStates()
{
	//-------------------------------
	// DEBUG COPY PASS
	//-------------------------------
	m_CopyAlphaRenderState = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("CopyAlpha Render State"));
	m_CopyColorRenderState = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("CopyColor Render State"));
	if (m_CopyAlphaRenderState == null || m_CopyColorRenderState == null)
		return false;

	RHI::SRenderState		&debugCopyColorRenderState = m_CopyColorRenderState->m_RenderState;
	RHI::SRenderState		&debugCopyAlphaRenderState = m_CopyAlphaRenderState->m_RenderState;

	debugCopyColorRenderState.m_PipelineState.m_DynamicScissor = true;
	debugCopyColorRenderState.m_PipelineState.m_DynamicViewport = true;
	debugCopyAlphaRenderState.m_PipelineState.m_DynamicScissor = true;
	debugCopyAlphaRenderState.m_PipelineState.m_DynamicViewport = true;

	if (!debugCopyColorRenderState.m_InputVertexBuffers.PushBack().Valid() ||
		!debugCopyAlphaRenderState.m_InputVertexBuffers.PushBack().Valid())
			return false;
	debugCopyColorRenderState.m_InputVertexBuffers.Last().m_Stride = sizeof(CFloat2);
	debugCopyAlphaRenderState.m_InputVertexBuffers.Last().m_Stride = sizeof(CFloat2);
	FillCopyShaderBindings(CopyCombination_Basic, debugCopyColorRenderState.m_ShaderBindings, m_DefaultSamplerConstLayout);
	FillCopyShaderBindings(CopyCombination_Alpha, debugCopyAlphaRenderState.m_ShaderBindings, m_DefaultSamplerConstLayout);

	CShaderLoader::SShadersPaths shadersPaths;
	shadersPaths.m_Vertex	= FULL_SCREEN_QUAD_VERTEX_SHADER_PATH;
	shadersPaths.m_Fragment	= COPY_FRAGMENT_SHADER_PATH;

	if (!m_ShaderLoader.LoadShader(debugCopyColorRenderState, shadersPaths, m_ApiManager) ||
		!m_ShaderLoader.LoadShader(debugCopyAlphaRenderState, shadersPaths, m_ApiManager))
			return false;
	return	m_ApiManager->BakeRenderState(m_CopyColorRenderState, m_FinalFrameBuffers.First()->GetLayout(), m_FinalRenderPass, 1) &&
			m_ApiManager->BakeRenderState(m_CopyAlphaRenderState, m_FinalFrameBuffers.First()->GetLayout(), m_FinalRenderPass, 1);
}

//----------------------------------------------------------------------------

void	CDeferredScene::FillCommandBuffer(const RHI::PCommandBuffer &cmdBuff, u32 swapImgIdx)
{
	(void)cmdBuff;
	CUint2		contextSize = m_SwapChainRenderTargets[0]->GetSize();

	RHI::PCommandBuffer	preOpaqueCmdBuff = m_ApiManager->CreateCommandBuffer(RHI::SRHIResourceInfos("PK-RHI Pre Opaque command buffer"));
	RHI::PCommandBuffer	postOpaqueCmdBuff = m_ApiManager->CreateCommandBuffer(RHI::SRHIResourceInfos("PK-RHI Post Opaque command buffer"));

	preOpaqueCmdBuff->Start();
	{
		// Pre Render:
		StartCommandBuffer(preOpaqueCmdBuff);

		// -- RENDER PASS: Deferred -------------------

		preOpaqueCmdBuff->BeginRenderPass(m_BeforeBloomRenderPass, m_BeforeBloomFrameBuffer, m_BeforeBloomClearValues);

		preOpaqueCmdBuff->SetViewport(CInt2(0), contextSize, CFloat2(0, 1));
		preOpaqueCmdBuff->SetScissor(CInt2(0), contextSize);

		// Sub-pass 0: GBuffer
		{
			PK_NAMEDSCOPEDPROFILE("Opaque pass");
			PK_NAMEDSCOPEDPROFILE_GPU(preOpaqueCmdBuff->ProfileEventContext(), "Opaque");
			RenderMeshBackdrops(preOpaqueCmdBuff);
		}

		preOpaqueCmdBuff->Stop();
		m_ApiManager->SubmitCommandBufferDirect(preOpaqueCmdBuff);

		postOpaqueCmdBuff->Start();
		PostOpaque(postOpaqueCmdBuff);

		// -- START RENDER PASS: Deferred -------------------
		{
			PK_NAMEDSCOPEDPROFILE("START RENDER PASS: Deferred");
			postOpaqueCmdBuff->BeginRenderPass(m_BeforeBloomRenderPass, m_BeforeBloomFrameBuffer, TMemoryView<RHI::SFrameBufferClearValue>()); // don't clear
			postOpaqueCmdBuff->SetViewport(CInt2(0, 0), m_BeforeBloomFrameBuffer->GetSize(), CFloat2(0, 1));
			postOpaqueCmdBuff->SetScissor(CInt2(0), m_BeforeBloomFrameBuffer->GetSize());
		}

		// Sub-pass 0: Opaque particles
		{
			PK_NAMEDSCOPEDPROFILE("Opaque particles");
			PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "Opaque particles");
			RenderOpaquePass(postOpaqueCmdBuff);
		}

		// Sub-pass 1: Decals
		{
			PK_NAMEDSCOPEDPROFILE("Decals pass");
			PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "Decals");
			postOpaqueCmdBuff->NextRenderSubPass();
			RenderDecals(postOpaqueCmdBuff);
		}

		// Sub-pass 2: Light
		{
			PK_NAMEDSCOPEDPROFILE("Lighting pass");
			PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "Lighting");
			postOpaqueCmdBuff->NextRenderSubPass();

			postOpaqueCmdBuff->BindRenderState(m_GBuffer.m_LightsRenderState);
			postOpaqueCmdBuff->BindVertexBuffers(m_GBuffer.m_QuadBuffers.m_VertexBuffers);
			RenderLights(postOpaqueCmdBuff);
		}

		// Sub-pass 3: Merging
		{
			PK_NAMEDSCOPEDPROFILE("GBuffer and light accumulation merging pass");
			PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "GBuffer and light accumulation merge");
			postOpaqueCmdBuff->NextRenderSubPass();
			postOpaqueCmdBuff->BindRenderState(m_GBuffer.m_MergingRenderState);
			postOpaqueCmdBuff->BindVertexBuffers(m_GBuffer.m_QuadBuffers.m_VertexBuffers);
			postOpaqueCmdBuff->BindConstantSets(TMemoryView<const RHI::PConstantSet>(m_GBuffer.m_MergingSamplersSet));
			postOpaqueCmdBuff->PushConstant(&m_DeferredMergingMinAlpha, 0);
			postOpaqueCmdBuff->Draw(0, 6);
		}

		{
			PK_NAMEDSCOPEDPROFILE("Semi-transparent Forward pass");
			PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "Semi-transparent Forward");
			RenderPostMerge(postOpaqueCmdBuff);
		}

		// Sub-pass 4: Distortion-Map
		{
			PK_NAMEDSCOPEDPROFILE("Distortion-map pass");
			PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "Distortion");
			postOpaqueCmdBuff->NextRenderSubPass();
			RenderDistortionMap(postOpaqueCmdBuff);
		}

		// Sub-pass 5-6-7: Distortion-PostFX
		{
			PK_NAMEDSCOPEDPROFILE("Distortion-postFX pass");
			PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "Distortion-postFX");
			postOpaqueCmdBuff->NextRenderSubPass();
			if (m_SceneOptions.m_Distortion.m_Enable) // TODO, debug modes ...
				m_Distortion.Draw(postOpaqueCmdBuff);
			else
				m_Distortion.Draw_JustCopy(postOpaqueCmdBuff);
		}

		{
			PK_NAMEDSCOPEDPROFILE("Sub-passes: Transparent no distortion");
			PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "Transparent no distortion");
			postOpaqueCmdBuff->NextRenderSubPass();
			RenderPostDistortion(postOpaqueCmdBuff);
		}

		postOpaqueCmdBuff->EndRenderPass();
		postOpaqueCmdBuff->SyncPreviousRenderPass(RHI::FragmentPipelineStage, RHI::FragmentPipelineStage);

		// -- RENDER PASS: Bloom Post-FX -------------------------
		// Input: m_DeferredSetup.m_DistortionPostFX.m_OutputRenderTarget
		// Output: m_DeferredSetup.m_DistortionPostFX.m_OutputRenderTarget
		//m_DownSampler.Draw(postOpaqueCmdBuff, m_DeferredSetup.m_DistortionPostFX.m_SamplerConstantSet); // We do not retrieve the scene illumination yet
		if (m_SceneOptions.m_Bloom.m_Enable)
		{
			PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "Bloom");
			m_Bloom.Draw(postOpaqueCmdBuff, 0);
			postOpaqueCmdBuff->SyncPreviousRenderPass(RHI::FragmentPipelineStage, RHI::FragmentPipelineStage);
		}

		// -- RENDER PASS: Final Image -------------------------

		postOpaqueCmdBuff->BeginRenderPass(m_FinalRenderPass, m_FinalFrameBuffers[swapImgIdx], m_FinalClearValues);

		// Sub-pass 0: Tone-Mapping
		// Input: m_DeferredSetup.m_DistortionPostFX.m_OutputRenderTarget
		// Output: m_ToneMapping.m_OutputRenderTarget
		{
			PK_NAMEDSCOPEDPROFILE("Tone-Mapping post-FX pass");
			PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "Tone-Mapping");
			if (m_SceneOptions.m_ToneMapping.m_Enable) // TODO, debug modes ...
				m_ToneMapping.Draw(postOpaqueCmdBuff);
			else
				m_ToneMapping.Draw_JustCopy(postOpaqueCmdBuff);
		}

		// Sub-pass 1: Final Copy (and swap-chain as output)
		postOpaqueCmdBuff->NextRenderSubPass();
		{
			PK_NAMEDSCOPEDPROFILE("Blit final buffer on screen pass");
			PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "Blit final buffer");

			// ------------ COPY THE CURRENTLY DEBUGGED BUFFER ON SCREEN ---------------
			if (m_RtToDraw != RenderTarget_Unknown)
			{
				if (m_ShowAlpha)
					postOpaqueCmdBuff->BindRenderState(m_CopyAlphaRenderState);
				else
					postOpaqueCmdBuff->BindRenderState(m_CopyColorRenderState);

				if (m_RtToDraw == RenderTarget_Depth)
					postOpaqueCmdBuff->BindConstantSets(TMemoryView<RHI::PConstantSet>(m_GBuffer.m_Depth.m_SamplerConstantSet));
				else if (m_RtToDraw == RenderTarget_Diffuse)
					postOpaqueCmdBuff->BindConstantSets(TMemoryView<RHI::PConstantSet>(m_GBufferRTs[0].m_SamplerConstantSet));
				else if (m_RtToDraw == RenderTarget_Specular)
					postOpaqueCmdBuff->BindConstantSets(TMemoryView<RHI::PConstantSet>(m_GBufferRTs[1].m_SamplerConstantSet));
				else if (m_RtToDraw == RenderTarget_Normal)
					postOpaqueCmdBuff->BindConstantSets(TMemoryView<RHI::PConstantSet>(m_GBufferRTs[1].m_SamplerConstantSet));
				else if (m_RtToDraw == RenderTarget_LightAccum)
					postOpaqueCmdBuff->BindConstantSets(TMemoryView<RHI::PConstantSet>(m_GBuffer.m_LightAccu.m_SamplerConstantSet));
				else if (m_RtToDraw == RenderTarget_Scene)
					postOpaqueCmdBuff->BindConstantSets(TMemoryView<RHI::PConstantSet>(m_ToneMapping.m_OutputRenderTarget.m_SamplerConstantSet));

				postOpaqueCmdBuff->BindVertexBuffers(m_GBuffer.m_QuadBuffers.m_VertexBuffers);
				postOpaqueCmdBuff->Draw(0, 6);
			}
		}

		RenderFinalComposition(postOpaqueCmdBuff);

		// ------------ IMGUI ---------------
		if (!m_RenderOffscreen)
		{
			PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "ImGUI");
			ImDrawData	*drawData = ImGuiPkRHI::GenerateRenderData();
			ImGuiPkRHI::DrawRenderData(drawData, postOpaqueCmdBuff);
			ImGuiPkRHI::EndFrame();
		}

		// ------------ PROFILER ------------
	#if	PKSAMPLE_HAS_PROFILER_RENDERER
		{
			PK_NAMEDSCOPEDPROFILE("Profiler pass");
			PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "Profiler");
			m_ProfilerRenderer->Render(postOpaqueCmdBuff, m_WindowContext->GetDrawableSize(), m_ProfilerData);
		}
	#endif

		postOpaqueCmdBuff->EndRenderPass();
	}

	postOpaqueCmdBuff->Stop();
	m_ApiManager->SubmitCommandBufferDirect(postOpaqueCmdBuff);
	return;
}

//----------------------------------------------------------------------------

namespace
{
	static const float	kCameraDistanceLimits[2] = {    0.f, 100.f };
	static const float	kCameraAzimuthLimits[2]  = { -180.f, 180.f };
	static const float	kCameraAltitudeLimits[2] = {  -90.f,  90.f };
	static const float	kCameraRollLimits[2]     = { -180.f, 180.f };

	static float _Diff(const float kd[2])
	{
		assert(kd[1] >= kd[0]);	// NOTE(Julien): Never call PK_ASSERT() before the popcorn runtime is started
		return PKAbs(kd[0] - kd[1]);
	}

	static const float kCameraDistanceLimitsDiff = _Diff(kCameraDistanceLimits);
	static const float kCameraAzimuthLimitsDiff  = _Diff(kCameraAzimuthLimits);
	static const float kCameraAltitudeLimitsDiff = _Diff(kCameraAltitudeLimits);
	static const float kCameraRollLimitsDiff     = _Diff(kCameraRollLimits);
}

//----------------------------------------------------------------------------

void	CDeferredScene::UpdateCamera()
{
	// Update camera from controller
	if (m_WindowContext != null && m_WindowContext->m_Controller != null)
	{
		static const float	kFactor = 0.001f;
		const float			rightY = m_WindowContext->m_Controller->GetNormalizedAxis(CAbstractController::AxisRightY) * kFactor;
		const float			rightX = m_WindowContext->m_Controller->GetNormalizedAxis(CAbstractController::AxisRightX) * kFactor;
		const float			leftY  = m_WindowContext->m_Controller->GetNormalizedAxis(CAbstractController::AxisLeftY)  * kFactor;

		m_CameraGUI_Distance   += leftY  * kCameraDistanceLimitsDiff;
		m_CameraGUI_Angles.x() += rightY * kCameraAltitudeLimitsDiff;
		m_CameraGUI_Angles.y() += (1.f + rightX) * kCameraAzimuthLimitsDiff;
	}

	// Update camera from mouse
	if (m_WindowContext != null)
	{
		static const float	kFactor = 0.0015f;
		const CInt2		currentMousePosition = m_WindowContext->GetMousePosition();
		const CFloat2	mouseDiff = m_WindowContext->GetPixelRatio() * CFloat2(currentMousePosition - m_PrevMousePosition) * kFactor;
		m_PrevMousePosition = currentMousePosition;

		const CBool3	mouseButtonState = m_WindowContext->GetMouseClick();
		if (mouseButtonState != 0)
		{
			if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
			{
				if (mouseButtonState.xz() == CBool2(true, false))	// only left-click: orbit
				{
					m_CameraGUI_Angles.x() += mouseDiff.y() * kCameraAltitudeLimitsDiff;
					m_CameraGUI_Angles.y() += (1.f + mouseDiff.x()) * kCameraAzimuthLimitsDiff;
				}
				else if (mouseButtonState.xz() == CBool2(true, true))	// left+right click: dolly
				{
					m_CameraGUI_Distance += mouseDiff.y() * kCameraDistanceLimitsDiff;
				}
			}
		}
	}

	m_CameraGUI_Distance = PKClamp(m_CameraGUI_Distance, kCameraDistanceLimits[0], kCameraDistanceLimits[1]);
	m_CameraGUI_Angles.x() = PKClamp(m_CameraGUI_Angles.x(), kCameraAltitudeLimits[0], kCameraAltitudeLimits[1]);
	m_CameraGUI_Angles.y() = fmodf(m_CameraGUI_Angles.y() - kCameraAzimuthLimits[0], kCameraAzimuthLimitsDiff) + kCameraAzimuthLimits[0];

	m_Camera.SetAngles(Units::DegreesToRadians(m_CameraGUI_Angles));
	m_Camera.SetDistance(m_CameraGUI_Distance);
	m_Camera.SetLookAt(m_CameraGUI_LookatPosition);

	if (m_CameraFlag_ResetProj)
	{
		m_Camera.SetProj(60, CFloat2(m_WindowContext->GetDrawableSize()), CAMERA_NEAR_PLANE, CAMERA_FAR_PLANE);
		m_CameraFlag_ResetProj = false;
	}

	m_Camera.Update();
}

//----------------------------------------------------------------------------

void	CDeferredScene::UpdateSceneInfo()
{
	Utils::SetupSceneInfoData(m_SceneInfoData, GetCamera().Camera(), CCoordinateFrame::GlobalFrame());
	PKSample::SSceneInfoData	*sceneInfo = static_cast<PKSample::SSceneInfoData*>(m_ApiManager->MapCpuView(m_SceneInfoConstantBuffer));
	if (PK_VERIFY(sceneInfo != null))
	{
		*sceneInfo = m_SceneInfoData;
		PK_VERIFY(m_ApiManager->UnmapCpuView(m_SceneInfoConstantBuffer));
	}
}

//----------------------------------------------------------------------------

void	CDeferredScene::DrawHUD()
{
	PK_SCOPEDPROFILE_C(CFloat3(1, 1, 0));
	PK_ASSERT(m_WindowContext != null);

	CUint2 			drawSize = m_SwapChainRenderTargets.First()->GetSize();
	float 			pxlRatio = 1.0f;

	if (drawSize != m_WindowContext->GetWindowSize())
	{
		drawSize = m_WindowContext->GetWindowSize();
		pxlRatio = m_WindowContext->GetPixelRatio();
	}

	ImGuiPkRHI::NewFrame(drawSize, m_Dt, pxlRatio);

	// ImGui Additional things
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 2));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(4, 2));
	ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 6);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0);

	if (ImGui::Begin("Deferred scene", null, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::PushItemWidth(130);

		ImGui::SliderFloat("Camera Distance", &m_CameraGUI_Distance, kCameraDistanceLimits[0], kCameraDistanceLimits[1]);
		ImGui::SliderFloat("Camera Azimuth ", &m_CameraGUI_Angles.y(), kCameraAzimuthLimits[0], kCameraAzimuthLimits[1]);
		ImGui::SliderFloat("Camera Altitude", &m_CameraGUI_Angles.x(), kCameraAltitudeLimits[0], kCameraAltitudeLimits[1]);
		ImGui::SliderFloat("Camera Roll    ", &m_CameraGUI_Angles.z(), kCameraRollLimits[0], kCameraRollLimits[1]);

		ImGui::Separator();

		ImGui::Checkbox("Enable distortion", &m_SceneOptions.m_Distortion.m_Enable);

		ImGui::Separator();

		ImGui::Checkbox("Enable bloom", &m_SceneOptions.m_Bloom.m_Enable);

		if (ImGui::SliderFloat("Bloom bright-pass", &m_SceneOptions.m_Bloom.m_BrightPassValue, 0, 3))
		{
			m_Bloom.SetSubtractValue(m_SceneOptions.m_Bloom.m_BrightPassValue);
		}
		if (ImGui::SliderFloat("Bloom intensity", &m_SceneOptions.m_Bloom.m_Intensity, 0, 5))
		{
			m_Bloom.SetIntensity(m_SceneOptions.m_Bloom.m_Intensity);
		}
		if (ImGui::SliderFloat("Bloom attenuation", &m_SceneOptions.m_Bloom.m_Attenuation, 0, 1))
		{
			m_Bloom.SetAttenuation(m_SceneOptions.m_Bloom.m_Attenuation);
		}
		UpdateMaxBloomRTs();
		if (ImGui::SliderInt("Bloom max render pass", &m_UserMaxBloomRenderPass, 0, m_MaxBloomRenderPass))
		{
			bool	success = true;

			CPostFxBloom::SInOutRenderTargets	inOut;

			inOut.m_InputRenderTarget = &m_Distortion.m_OutputRenderTarget;
			inOut.m_OutputRenderTargets = TMemoryView<const RHI::PRenderTarget>(m_Distortion.m_OutputRenderTarget.m_RenderTarget);

			success &= m_Bloom.UpdateFrameBuffer(m_DefaultSampler, m_UserMaxBloomRenderPass, &inOut);
			success &= m_Bloom.CreateFrameBuffers();
			if (!success)
			{
				CLog::Log(PK_ERROR, "Could not re-create the bloom");
			}
		}

		int bloomTap = static_cast<int>(m_SceneOptions.m_Bloom.m_BlurTap);

		static const char	*blurTaps[] =
		{
			"Gaussian Blur 5 Tap",
			"Gaussian Blur 9 Tap",
			"Gaussian Blur 13 Tap",
		};
		if (ImGui::Combo("Bloom tap number", &bloomTap, blurTaps, GaussianBlurCombination_Count))
		{
			bool	success = true;
			success &= m_Bloom.CreateRenderPass();
			success &= m_Bloom.CreateRenderStates(m_ShaderLoader, static_cast<EGaussianBlurCombination>(bloomTap));
			m_SceneOptions.m_Bloom.m_BlurTap = static_cast<EGaussianBlurCombination>(bloomTap);
			if (!success)
			{
				CLog::Log(PK_ERROR, "Could not re-create the bloom");
			}
		}

		ImGui::Separator();

		ImGui::Checkbox("Enable tone-mapping", &m_SceneOptions.m_ToneMapping.m_Enable);

		if (ImGui::SliderFloat("Exposure", &m_SceneOptions.m_ToneMapping.m_Exposure, -5, 5))
		{
			m_ToneMapping.SetExposure(m_SceneOptions.m_ToneMapping.m_Exposure);
		}
		if (ImGui::SliderFloat("Saturation", &m_SceneOptions.m_ToneMapping.m_Saturation, 0, 2))
		{
			m_ToneMapping.SetSaturation(m_SceneOptions.m_ToneMapping.m_Saturation);
		}
		if (ImGui::SliderFloat("Vignetting Color Intensity", &m_SceneOptions.m_Vignetting.m_ColorIntensity, 0, 1))
		{
			m_ToneMapping.SetVignettingColorIntensity(m_SceneOptions.m_Vignetting.m_ColorIntensity);
		}
		if (ImGui::SliderFloat("Vignetting Desaturation Intensity", &m_SceneOptions.m_Vignetting.m_DesaturationIntensity, 0, 1))
		{
			m_ToneMapping.SetVignettingDesaturationIntensity(m_SceneOptions.m_Vignetting.m_DesaturationIntensity);
		}
		if (ImGui::SliderFloat("Vignetting Smoothness", &m_SceneOptions.m_Vignetting.m_Smoothness, 0, 1))
		{
			m_ToneMapping.SetVignettingSmoothness(m_SceneOptions.m_Vignetting.m_Smoothness);
		}

		m_ToneMapping.SetScreenSize(m_Camera.Camera().m_WinSize.x(), m_Camera.Camera().m_WinSize.y());
		ImGui::PopItemWidth();
	}
	ImGui::SetWindowCollapsed(true, ImGuiSetCond_Once);
	ImGui::SetWindowPos(ImVec2(m_WindowContext->GetWindowSize().x() - ImGui::GetWindowWidth(), 0), ImGuiSetCond_Always);
	ImGui::End();

	UpdateCamera();

	if (ImGui::Begin("info", null, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar))
	{
		ImGui::PushItemWidth(100);
		ImGui::LabelText("Frame Time", "%1.3f ms", m_Dt * 1.0e+3f);

		_UpdateDisplayFps();
		ImGui::LabelText("FPS", "%3.1f", _GetDisplayFps());

		ImGui::SetWindowPos(ImVec2(m_WindowContext->GetWindowSize().x() - ImGui::GetWindowWidth(), m_WindowContext->GetWindowSize().y() - ImGui::GetWindowHeight()), ImGuiSetCond_Always);
		ImGui::PopItemWidth();
	}
	ImGui::End();
	ImGui::PopStyleVar(6);

#if	PKSAMPLE_HAS_PROFILER_RENDERER
	if (m_ProfilerRenderer->Enabled())
	{
		if (ImGui::Begin("Profiler node details", null, ImGuiWindowFlags_NoSavedSettings))
		{
			ImGui::SetWindowPos(ImVec2(420, 60), ImGuiCond_Once);
			ImGui::SetWindowSize(ImVec2(800, 200), ImGuiCond_Once);
			for (const auto &msg : m_ProfilerRenderer->GetTooltips())
				ImGui::Text("%s", msg.Data());
		}
		ImGui::End();
	}
#endif
}

//----------------------------------------------------------------------------

void	CDeferredScene::_UpdateDisplayFps()
{
	m_DisplayFpsAccTime += m_Dt;
	m_DisplayFpsAccFrames++;
	if (m_DisplayFpsAccTime > 1.0f)
	{
		m_DisplayFps = m_DisplayFpsAccFrames / m_DisplayFpsAccTime;
		m_DisplayFpsAccFrames = 0;
		m_DisplayFpsAccTime = 0;
	}
}

//----------------------------------------------------------------------------

float	CDeferredScene::_GetDisplayFps() const
{
	if (!TNumericTraits<float>::IsFinite(m_DisplayFps))
		return 1.0f / m_Dt;
	return m_DisplayFps;
}

//----------------------------------------------------------------------------

void	CDeferredScene::UpdateMaxBloomRTs()
{
	const CUint2	ctxSize = m_GBuffer.m_Merge.m_Size;
	const u32		maxDimension = PKMax(ctxSize.x(), ctxSize.y());
	m_MaxBloomRenderPass = IntegerTools::Log2(maxDimension);
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
