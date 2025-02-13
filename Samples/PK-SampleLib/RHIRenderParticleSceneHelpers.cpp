#include "precompiled.h"

#include "RHIRenderParticleSceneHelpers.h"

#include "PK-SampleLib/RenderIntegrationRHI/RHIGraphicResources.h"
#include "PK-SampleLib/RenderIntegrationRHI/RendererCache.h"
#include "PK-SampleLib/ShaderDefinitions/BasicSceneShaderDefinitions.h"
#include "PK-SampleLib/ShaderDefinitions/SampleLibShaderDefinitions.h"
#include "PK-SampleLib/RenderIntegrationRHI/MaterialToRHI.h"
#include "PK-SampleLib/BRDFLUT.h"

#include <pk_rhi/include/AllInterfaces.h>
#include <pk_rhi/include/interfaces/SApiContext.h>
#include <pk_rhi/include/PixelFormatFallbacks.h>

#define	GBUFFER_VERTEX_SHADER_PATH					"./Shaders/SolidMesh.vert"
#define	GBUFFER_FRAGMENT_SHADER_PATH				"./Shaders/GBuffer.frag"
#define	DITHERING_PATTERNS_TEXTURE_PATH				"./Textures/DitheringPatterns.png"


__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

void	SBackdropsData::CLight::SetAmbient(const CFloat3 &color, float intensity)
{
	m_Type = SLightRenderPass::Ambient;
	m_Color = ConvertSRGBToLinear(color);
	m_Intensity = intensity;
}

void	SBackdropsData::CLight::SetDirectional(const CFloat3 &direction, const CFloat3 &color, float intensity)
{
	m_Type = SLightRenderPass::Directional;
	m_Direction = direction;
	m_Color = ConvertSRGBToLinear(color);
	m_Intensity = intensity;
}

void	SBackdropsData::CLight::SetSpot(const CFloat3 &position, const CFloat3 &direction, float angle, float coneFalloff, const CFloat3 &color, float intensity)
{
	m_Type = SLightRenderPass::Spot;
	m_Position = position;
	m_Direction = direction;
	m_Angle = angle;
	m_ConeFalloff = coneFalloff;
	m_Color = ConvertSRGBToLinear(color);
	m_Intensity = intensity;
}

void	SBackdropsData::CLight::SetPoint(const CFloat3 &position, const CFloat3 &color, float intensity)
{
	m_Type = SLightRenderPass::Point;
	m_Position = position;
	m_Color = ConvertSRGBToLinear(color);
	m_Intensity = intensity;
}

//----------------------------------------------------------------------------

CRHIParticleSceneRenderHelper::CRHIParticleSceneRenderHelper()
:	m_ShaderLoader(null)
,	m_ResourceManager(null)
,	m_CurrentPackResourceManager(null)
,	m_EnableParticleRender(true)
,	m_EnableBackdropRender(true)
,	m_EnableOverdrawRender(false)
,	m_EnableBrushBackground(true)
,	m_EnablePostFX(true)
,	m_EnableDistortion(true)
,	m_EnableBloom(true)
,	m_BlurTap(GaussianBlurCombination_5_Tap)
,	m_EnableToneMapping(true)
,	m_EnableColorRemap(false)
,	m_EnableFXAA(true)
,	m_CoordinateFrame(CCoordinateFrame::GlobalFrame())
,	m_GridIdxCount(0)
,	m_GridSubdivIdxCount(0)
{
}

//----------------------------------------------------------------------------

CRHIParticleSceneRenderHelper::~CRHIParticleSceneRenderHelper()
{
}

//----------------------------------------------------------------------------

bool	CRHIParticleSceneRenderHelper::Init(const RHI::PApiManager	&apiManager,
											CShaderLoader			*shaderLoader,
											CResourceManager		*resourceManager,
											u32						initRP)
{
	m_ApiManager = apiManager;
	m_ShaderLoader = shaderLoader;
	m_ResourceManager = resourceManager;
	m_InitializedRP = initRP;

	if (STextureKey::s_DefaultResourceID.Valid() ||
		SGeometryKey::s_DefaultResourceID.Valid())
	{
		const SCreateArg	args(apiManager, resourceManager);

		if (STextureKey::s_DefaultResourceID.Valid())
			CTextureManager::RenderThread_ResolveResource(STextureKey::s_DefaultResourceID, args);
		if (SGeometryKey::s_DefaultResourceID.Valid())
			CGeometryManager::RenderThread_ResolveResource(SGeometryKey::s_DefaultResourceID, args);
	}

	// Create default sampler:
	CreateSimpleSamplerConstSetLayouts(m_DefaultSamplerConstLayout, false);
	if (m_DefaultSampler == null)
		m_DefaultSampler = m_ApiManager->CreateConstantSampler(	RHI::SRHIResourceInfos("Default Linear Sampler"),
																RHI::SampleLinear, RHI::SampleLinear,
																RHI::SampleClampToEdge, RHI::SampleClampToEdge, RHI::SampleClampToEdge, 1);
	if (m_DefaultSampler == null)
		return false;
	if (m_DefaultSamplerNearest == null)
		m_DefaultSamplerNearest = m_ApiManager->CreateConstantSampler(	RHI::SRHIResourceInfos("Default nearest Sampler"),
																		RHI::SampleNearest, RHI::SampleNearest,
																		RHI::SampleClampToEdge, RHI::SampleClampToEdge, RHI::SampleClampToEdge, 1);
	if (m_DefaultSamplerNearest == null)
		return false;

	u32			white = 0xFFFFFFFF;
	u32			black = 0x00000000;
	u32			normal = 0xFFFF7F7F;
	CImageMap	dummyWhite(CUint3::ONE, &white, sizeof(u32));
	CImageMap	dummyBlack(CUint3::ONE, &black, sizeof(u32));
	CImageMap	dummyNormal(CUint3::ONE, &normal, sizeof(u32));

	// Init the dummy textures:
	m_DummyBlack = m_ApiManager->CreateTexture(RHI::SRHIResourceInfos("Dummy Black Texture"), TMemoryView<const CImageMap>(dummyBlack), RHI::FormatUnorm8RGBA);
	m_DummyWhite = m_ApiManager->CreateTexture(RHI::SRHIResourceInfos("Dummy White Texture"), TMemoryView<const CImageMap>(dummyWhite), RHI::FormatUnorm8RGBA);
	m_DummyNormal = m_ApiManager->CreateTexture(RHI::SRHIResourceInfos("Dummy Normal Texture"), TMemoryView<const CImageMap>(dummyNormal), RHI::FormatUnorm8RGBA);

	if (!PK_VERIFY(m_DummyBlack != null && m_DummyWhite != null && m_DummyNormal != null))
		return false;

	m_DummyWhiteConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Default White Texture Constant Set"), m_DefaultSamplerConstLayout);
	m_DummyBlackConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Default Black Texture Constant Set"), m_DefaultSamplerConstLayout);

	if (!PK_VERIFY(m_DummyWhiteConstantSet != null && m_DummyBlackConstantSet != null))
		return false;

	m_DummyWhiteConstantSet->SetConstants(m_DefaultSampler, m_DummyWhite, 0);
	m_DummyWhiteConstantSet->UpdateConstantValues();
	m_DummyBlackConstantSet->SetConstants(m_DefaultSampler, m_DummyBlack, 0);
	m_DummyBlackConstantSet->UpdateConstantValues();

	// Init the environmentMap
	if (!PK_VERIFY(m_EnvironmentMap.Init(m_ApiManager, shaderLoader)))
	return false;

	// Init the atlas dummy constant-set
	PK_ASSERT(SConstantAtlasKey::GetAtlasConstantSetLayout().m_Constants.Count() == 1);
	m_DummyAtlas.m_AtlasConstSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Dummy Atlas Constant Set"), SConstantAtlasKey::GetAtlasConstantSetLayout());
	m_DummyAtlas.m_AtlasBuffer = m_ApiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("Dummy Atlas Buffer"), RHI::RawBuffer, sizeof(u32) + sizeof(CFloat4), RHI::UsageStaticDraw);

	if (!PK_VERIFY(m_DummyAtlas.m_AtlasConstSet != null) || !PK_VERIFY(m_DummyAtlas.m_AtlasBuffer != null))
		return false;

	{
		void			*ptr = m_ApiManager->MapCpuView(m_DummyAtlas.m_AtlasBuffer);
		if (PK_VERIFY(ptr != null))
		{
			const u32		count = 1;
			const CFloat4	rect = CFloat4(1.0f, 1.0f, 0.0f, 0.0f);
			Mem::Copy(ptr, &count, sizeof(u32));
			ptr = Mem::AdvanceRawPointer(ptr, sizeof(u32));
			Mem::Copy(ptr, &rect, sizeof(CFloat4));
		}
		m_ApiManager->UnmapCpuView(m_DummyAtlas.m_AtlasBuffer);
	}

	m_DummyAtlas.m_AtlasConstSet->SetConstants(m_DummyAtlas.m_AtlasBuffer, 0);
	m_DummyAtlas.m_AtlasConstSet->UpdateConstantValues();

	// Init noise texture
	RHI::PConstantSampler sampler = m_ApiManager->CreateConstantSampler(	RHI::SRHIResourceInfos("Dithering Sampler"),
																			RHI::SampleNearest,
																			RHI::SampleNearestMipmapNearest,
																			RHI::SampleClampToEdge,
																			RHI::SampleClampToEdge,
																			RHI::SampleClampToEdge,
																			1);

	STextureKey	textureKey;
	textureKey.m_Path = DITHERING_PATTERNS_TEXTURE_PATH;
	CTextureManager::CResourceId textureId = CTextureManager::UpdateThread_GetResource(textureKey, SPrepareArg(null, null, null));
	if (!textureId.Valid())
		return false;
	RHI::PTexture ditheringtexture = CTextureManager::RenderThread_ResolveResource(textureId, SCreateArg(m_ApiManager, m_ResourceManager));
	if (ditheringtexture == null)
		return false;

	m_NoiseTexture.m_NoiseTexture = ditheringtexture;
	m_NoiseTexture.m_NoiseTextureConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Dithering Constant Set"), SConstantNoiseTextureKey::GetNoiseTextureConstantSetLayout());
	m_NoiseTexture.m_NoiseTextureConstantSet->SetConstants(sampler, m_NoiseTexture.m_NoiseTexture, 0);
	m_NoiseTexture.m_NoiseTextureConstantSet->UpdateConstantValues();

	// Init the BRDF LUT
	// Load BRDF LUT
	const CImageMap	mipmaps[] = { CImageMap(CUint3(64, 64, 1), (void*)BRDFLUTArray, BRDFLUTArraySize) };
	PK_STATIC_ASSERT(BRDFLUTArraySize == 64 * 64 * 2 * 2);

	RHI::PTexture	BRDFLUT = apiManager->CreateTexture(RHI::SRHIResourceInfos("BRDFLUT Texture"), mipmaps, RHI::FormatFloat16RG);
	if (!PK_VERIFY(BRDFLUT != null))
		return false;

	// Create scene info:
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
	}

	// Just for the full screen quad...
	if (!m_GBuffer.Init(m_ApiManager, m_DefaultSampler, m_DefaultSamplerConstLayout, m_SceneInfoConstantSetLayout))
		return false;

	if (!PK_VERIFY(m_Lights.m_DirectionalShadows.Init(	m_ApiManager,
														m_SceneInfoConstantSetLayout,
														m_DefaultSampler,
														m_DefaultSamplerConstLayout,
														m_GBuffer.m_QuadBuffers.m_VertexBuffers[0])))
		return false;

	if (!PK_VERIFY(m_Lights.Init(m_ApiManager, m_GBuffer.m_LightInfoConstLayout, m_GBuffer.m_ShadowsInfoConstLayout, m_DefaultSampler, m_DummyWhite)))
		return false;

	if ((m_InitializedRP & InitRP_Distortion) != 0)
	{
		// Init render-pass: Distortion
		if (!m_Distortion.Init(m_ApiManager, m_GBuffer.m_QuadBuffers.m_VertexBuffers[0], m_DefaultSampler, m_DefaultSamplerConstLayout))
			return false;
	}
	if ((m_InitializedRP & InitRP_Bloom) != 0)
	{
		// Init render-pass: Bloom
		if (!m_Bloom.Init(apiManager, m_GBuffer.m_QuadBuffers.m_VertexBuffers[0]))
			return false;
	}
	if ((m_InitializedRP & InitRP_ToneMapping) != 0)
	{
		// Init render-pass: Tonemap
		if (!m_ToneMapping.Init(apiManager, m_GBuffer.m_QuadBuffers.m_VertexBuffers[0], m_DefaultSampler, m_DefaultSamplerConstLayout))
			return false;
	}
	if ((m_InitializedRP & InitRP_ColorRemap) != 0)
	{
		// Init render-pass: Color Remap
		if (!m_ColorRemap.Init(apiManager, m_GBuffer.m_QuadBuffers.m_VertexBuffers[0], m_DefaultSampler, m_DefaultSamplerConstLayout))
			return false;
	}
	if ((m_InitializedRP & InitRP_FXAA) != 0)
	{
		// Init render-pass: FXAA
		if (!m_FXAA.Init(apiManager, m_GBuffer.m_QuadBuffers.m_VertexBuffers[0], m_DefaultSampler, m_DefaultSamplerConstLayout))
			return false;
	}
	// Create Gizmo rendering
	{
		if (!PK_VERIFY(m_GizmoDrawer.CreateVertexBuffer(m_ApiManager)))
			return false;
	}

	// Create the default textures used for lighting:
	// BRDF LookUp:
	// Create the sampler
	RHI::PConstantSampler BRDFLUTSampler = m_ApiManager->CreateConstantSampler(	RHI::SRHIResourceInfos("BRDFLUT Sampler"),
																				RHI::SampleLinear,
																				RHI::SampleLinearMipmapLinear,
																				RHI::SampleClampToEdge,
																				RHI::SampleClampToEdge,
																				RHI::SampleClampToEdge,
																				1);
	// Create the constant set
	m_BRDFLUTConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("BRDFLUT Constant Set"), m_GBuffer.m_BRDFLUTLayout);
	if (!PK_VERIFY(m_BRDFLUTConstantSet != null))
		return false;
	m_BRDFLUTConstantSet->SetConstants(BRDFLUTSampler, BRDFLUT, 0);
	m_BRDFLUTConstantSet->UpdateConstantValues();

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIParticleSceneRenderHelper::Resize(TMemoryView<const RHI::PRenderTarget> finalRts)
{
	if (!PK_VERIFY(!finalRts.Empty()))
		return false;

	m_ViewportSize = finalRts.First()->GetSize();

	// We assert that all the render targets are the same size, just in case...
	PK_ONLY_IF_ASSERTS(
		for (u32 i = 1; i < finalRts.Count(); ++i)
		{
			PK_ASSERT(finalRts[i]->GetSize() == m_ViewportSize);
		}
	);

	TArray<RHI::ELoadRTOperation>		beforeBloomLoadOp;
	TArray<RHI::ELoadRTOperation>		finalLoadOp;

	// Clear the old FBOs:
	m_BeforeBloomClearValues.Clear();
	m_BeforeBloomFrameBuffer = null;
	m_BeforeBloomRenderPass = null;
	m_FinalClearValues.Clear();
	m_FinalFrameBuffers.Clear();
	m_FinalRenderPass = null;

	m_BeforeDebugOutputRtIdx.Clear();
	m_BasicRenderingRTIdx.Clear();
	m_BasicRenderingSubPassIdx.Clear();

	// ------------------------------------------
	// We create the new frame buffer / FBO info here:
	// ------------------------------------------
	RHI::PRenderPass					currentRenderPass;
	TMemoryView<RHI::PFrameBuffer>		currentFrameBuffers;
	TArray<RHI::SFrameBufferClearValue>	*currentClearValues;
	TArray<RHI::ELoadRTOperation>		*currentLoadOp;

	const bool	hasBeforeBloomRenderPass = (m_InitializedRP & InitRP_Bloom) != 0;
	const bool	hasAfterBloomRenderPass = !hasBeforeBloomRenderPass || !_IsLastRenderPass(InitRP_Bloom);

	if (hasAfterBloomRenderPass)
	{
		// Create the final frame buffers:
		if (!m_FinalFrameBuffers.Resize(finalRts.Count()))
			return false;
		for (u32 i = 0; i < finalRts.Count(); ++i)
		{
			// Create frame buffer and add it
			m_FinalFrameBuffers[i] = m_ApiManager->CreateFrameBuffer(RHI::SRHIResourceInfos("Final Frame Buffer"));
			if (!PK_VERIFY(m_FinalFrameBuffers[i] != null))
				return false;
			if (!m_FinalFrameBuffers[i]->AddRenderTarget(finalRts[i]))
				return false;
		}
		if (!PK_VERIFY(finalLoadOp.PushBack(RHI::LoadDontCare).Valid()))
			return false;
		// And the final render pass:
		m_FinalRenderPass = m_ApiManager->CreateRenderPass(RHI::SRHIResourceInfos("Final Render Pass"));
		if (m_FinalRenderPass == null)
			return false;
	}
	else
	{
		// Here we just keep the ref on the final render targets to give them as the bloom output
		if (!m_FinalRenderTargets.Resize(finalRts.Count()))
			return false;
		for (u32 i = 0; i < finalRts.Count(); ++i)
		{
			// Copy render target
			m_FinalRenderTargets[i] = finalRts[i];
		}
	}

	// We check if we need 2 frame buffers:
	if (hasBeforeBloomRenderPass)
	{
		// IFN we create the before bloom frame buffer:
		m_BeforeBloomFrameBuffer = m_ApiManager->CreateFrameBuffer(RHI::SRHIResourceInfos("Before Bloom Frame Buffer"));
		if (!PK_VERIFY(m_BeforeBloomFrameBuffer != null))
			return false;
		m_BeforeBloomRenderPass = m_ApiManager->CreateRenderPass(RHI::SRHIResourceInfos("Before Bloom Render Pass"));
		if (!PK_VERIFY(m_BeforeBloomRenderPass != null))
			return false;
		currentFrameBuffers = TMemoryView<RHI::PFrameBuffer>(m_BeforeBloomFrameBuffer);
		currentRenderPass = m_BeforeBloomRenderPass;
		currentClearValues = &m_BeforeBloomClearValues;
		currentLoadOp = &beforeBloomLoadOp;
	}
	else
	{
		currentFrameBuffers = m_FinalFrameBuffers;
		currentRenderPass = m_FinalRenderPass;
		currentClearValues = &m_FinalClearValues;
		currentLoadOp = &finalLoadOp;
	}

	// In and out render targets
	if ((m_InitializedRP & InitRP_GBuffer) != 0)
	{
		// ------------------------------------------
		//
		// GBUFFER
		//
		// ------------------------------------------

		// ------------------------------------------
		// Create and add the render targets:
		// ------------------------------------------
		for (u32 i = 0; i < m_GBufferRTs.Count(); ++i)
		{
			m_GBufferRTsIdx[i] = currentFrameBuffers.First()->GetRenderTargets().Count();
			if (!PK_VERIFY(m_GBufferRTs[i].CreateRenderTarget(	RHI::SRHIResourceInfos("GBuffer Render Target"),
																m_ApiManager,
																m_DefaultSampler,
																SPassDescription::s_GBufferDefaultFormats[i],
																m_ViewportSize,
																m_DefaultSamplerConstLayout)))
			{
				return false;
			}
			for (u32 j = 0; j < currentFrameBuffers.Count(); ++j)
			{
				if (!PK_VERIFY(currentFrameBuffers[j]->AddRenderTarget(m_GBufferRTs[i].m_RenderTarget)))
					return false;
			}
		}

		// ------------------------------------------
		// Add the clear operations:
		// ------------------------------------------
		// GBuffer 0: Diffuse HDR
		if (!PK_VERIFY(currentClearValues->PushBack(RHI::SFrameBufferClearValue(0.f, 0.f, 0.f, 0.f)).Valid()))
			return false;
		if (!PK_VERIFY(currentLoadOp->PushBack(RHI::LoadClear).Valid()))
			return false;
		// GBuffer 1: Depth
		if (!PK_VERIFY(currentClearValues->PushBack(RHI::SFrameBufferClearValue(1.f, 1.f, 1.f, 1.f)).Valid()))
			return false;
		if (!PK_VERIFY(currentLoadOp->PushBack(RHI::LoadClear).Valid()))
			return false;
		// GBuffer 2: Emissive HDR
		if (!PK_VERIFY(currentClearValues->PushBack(RHI::SFrameBufferClearValue(0.f, 0.f, 0.f, 0.f)).Valid()))
			return false;
		if (!PK_VERIFY(currentLoadOp->PushBack(RHI::LoadClear).Valid()))
			return false;

		CGBuffer::SInOutRenderTargets	inOut;

		_FillGBufferInOut(inOut);
		if (!PK_VERIFY(m_GBuffer.UpdateFrameBuffer(	currentFrameBuffers,
													*currentLoadOp,
													*currentClearValues,
													false,
													&inOut)))
			return false;

		// ------------------------------------------
		// Create render passes:
		// ------------------------------------------
		if (!PK_VERIFY(m_GBuffer.AddSubPasses(currentRenderPass, &inOut)))
			return false;
	}
	else if (m_InitializedRP != 0)
	{
		// ------------------------------------------
		//
		// BASIC TRANSPARENT RENDERING
		//
		// ------------------------------------------
		// No GBuffer, just a simple HDR buffer for the transparent particles:
		RHI::EPixelFormat	basicRTPxlFormat = RHI::EPixelFormat::FormatFloat16RGBA;

		// ------------------------------------------
		// Create and add the render targets:
		// ------------------------------------------
		if (!PK_VERIFY(m_BasicRenderingRT.CreateRenderTarget(	RHI::SRHIResourceInfos("BasicRendering Render Target"),
																m_ApiManager,
																m_DefaultSampler,
																basicRTPxlFormat,
																m_ViewportSize,
																m_DefaultSamplerConstLayout)))
			return false;
		for (u32 i = 0; i < currentFrameBuffers.Count(); ++i)
		{
			m_BasicRenderingRTIdx = currentFrameBuffers[i]->GetRenderTargets().Count();
			if (!PK_VERIFY(currentFrameBuffers[i]->AddRenderTarget(m_BasicRenderingRT.m_RenderTarget)))
				return false;
		}

		// ------------------------------------------
		// Add the clear operations:
		// ------------------------------------------
		if (!PK_VERIFY(currentClearValues->PushBack(RHI::SFrameBufferClearValue(0.f, 0.f, 0.f, 0.f)).Valid()))
			return false;
		if (!PK_VERIFY(currentLoadOp->PushBack(RHI::LoadClear).Valid()))
			return false;

		// ------------------------------------------
		// Create render passes:
		// ------------------------------------------
		RHI::SSubPassDefinition		subPass;

		m_BasicRenderingSubPassIdx = currentRenderPass->GetSubPassDefinitions().Count();
		if (!PK_VERIFY(subPass.m_OutputRenderTargets.PushBack(m_BasicRenderingRTIdx).Valid()))
			return false;
		if (!PK_VERIFY(currentRenderPass->AddRenderSubPass(subPass)))
			return false;
	}
	else // If m_InitializedRP == 0, then we can just render the particles directly in the swap chain render target
	{
		// ------------------------------------------
		// Create render passes:
		// ------------------------------------------
		RHI::SSubPassDefinition		subPass;

		m_BasicRenderingSubPassIdx = currentRenderPass->GetSubPassDefinitions().Count();
		if (!PK_VERIFY(subPass.m_OutputRenderTargets.PushBack(0).Valid())) // We just render in the swap-chain render target
			return false;
		if (!PK_VERIFY(currentRenderPass->AddRenderSubPass(subPass)))
			return false;
	}

	// ------------------------------------------
	//
	// DISTORTION
	//
	// ------------------------------------------
	if ((m_InitializedRP & InitRP_Distortion) != 0)
	{
		CPostFxDistortion::SInOutRenderTargets	inOut;

		_FillDistortionInOut(inOut);
		if (!PK_VERIFY(m_Distortion.UpdateFrameBuffer(	currentFrameBuffers,
														*currentLoadOp,
														*currentClearValues,
														&inOut)))
			return false;

		if (!PK_VERIFY(m_Distortion.AddSubPasses(currentRenderPass, &inOut)))
			return false;
	}

	// Change current frame buffer and current render pass if bloom is enabled:
	if (hasAfterBloomRenderPass)
	{
		currentFrameBuffers = m_FinalFrameBuffers;
		currentRenderPass = m_FinalRenderPass;
		currentClearValues = &m_FinalClearValues;
		currentLoadOp = &finalLoadOp;
	}

	// ------------------------------------------
	//
	// TONE-MAPPING
	//
	// ------------------------------------------
	if ((m_InitializedRP & InitRP_ToneMapping) != 0)
	{
		CPostFxToneMapping::SInOutRenderTargets		inOut;

		_FillTonemappingInOut(inOut);
		if (!PK_VERIFY(m_ToneMapping.UpdateFrameBuffer(currentFrameBuffers, *currentLoadOp, *currentClearValues, &inOut)))
			return false;

		if (!PK_VERIFY(m_ToneMapping.AddSubPasses(currentRenderPass, &inOut)))
			return false;
	}

	// ------------------------------------------
	//
	// COLOR-REMAP
	// 
	// ------------------------------------------

	if ((m_InitializedRP & InitRP_ColorRemap) != 0)
	{
		CPostFxColorRemap::SInOutRenderTargets	inOut;

		_FillColorRemapInOut(inOut);
		m_ColorRemap.SetRemapTexture(m_DummyWhite, true); // Placeholder
		if (!PK_VERIFY(m_ColorRemap.UpdateFrameBuffer(currentFrameBuffers, *currentLoadOp, &inOut)))
			return false;

		if (!PK_VERIFY(m_ColorRemap.AddSubPasses(currentRenderPass, &inOut)))
			return false;		
	}


	// ------------------------------------------
	//
	// FXAA
	//
	// ------------------------------------------
	if ((m_InitializedRP & InitRP_FXAA) != 0)
	{
		CPostFxFXAA::SInOutRenderTargets		inOut;

		_FillFXAAInOut(inOut);
		if (!PK_VERIFY(m_FXAA.UpdateFrameBuffer(currentFrameBuffers,
												*currentLoadOp,
												*currentClearValues,
												&inOut)))
			return false;

		if (!PK_VERIFY(m_FXAA.AddSubPasses(currentRenderPass, &inOut)))
			return false;
	}

	// ------------------------------------------
	//
	// DEBUG
	//
	// ------------------------------------------
	if ((m_InitializedRP & InitRP_Debug) != 0)
	{
		m_ParticleDebugSubpassIdx = currentRenderPass->GetSubPassDefinitions().Count();

		// First sub pass: depth test + output in prev render pass output render target
		// ---------------------------
		_FillDebugInOut(m_BeforeDebugOutputRt, m_BeforeDebugOutputRtIdx);

		RHI::SSubPassDefinition		debugSubPass;

		if (!m_BeforeDebugOutputRtIdx.Valid())
		{
			// Then we need to add this render target to the current frame buffer to write in it
			m_BeforeDebugOutputRtIdx = currentFrameBuffers.First()->GetRenderTargets().Count();
			for (u32 i = 0; i < currentFrameBuffers.Count(); ++i)
			{
				if (!PK_VERIFY(currentFrameBuffers[i]->AddRenderTarget(m_BeforeDebugOutputRt.m_RenderTarget)))
					return false;
			}
			if (!PK_VERIFY(currentLoadOp->PushBack(RHI::LoadKeepValue).Valid()))
				return false;

		}
		if (!PK_VERIFY(debugSubPass.m_OutputRenderTargets.PushBack(m_BeforeDebugOutputRtIdx).Valid()))
			return false;

		if ((m_InitializedRP & InitRP_GBuffer) != 0)
		{
			if (hasBeforeBloomRenderPass)
			{
				// Then we need to add the GBuffer depth render target to the current frame buffer to depth-test
				debugSubPass.m_DepthStencilRenderTarget = currentFrameBuffers.First()->GetRenderTargets().Count();
				for (u32 i = 0; i < currentFrameBuffers.Count(); ++i)
				{
					if (!PK_VERIFY(currentFrameBuffers[i]->AddRenderTarget(m_GBuffer.m_Depth.m_RenderTarget)))
						return false;
				}
				if (!PK_VERIFY(currentLoadOp->PushBack(RHI::LoadKeepValue).Valid()))
					return false;
			}
			else
			{
				debugSubPass.m_DepthStencilRenderTarget = m_GBuffer.m_DepthBufferIndex;
			}
		}

		if (!PK_VERIFY(currentRenderPass->AddRenderSubPass(debugSubPass)))
			return false;

		// Second sub pass: depth test + output in swapchain render target + samples the prev pass output render target
		// ---------------------------
		RHI::SSubPassDefinition		finalSubPass;

		if (!PK_VERIFY(finalSubPass.m_OutputRenderTargets.PushBack(0).Valid()))
			return false;
		finalSubPass.m_DepthStencilRenderTarget = debugSubPass.m_DepthStencilRenderTarget;
		if (!PK_VERIFY(finalSubPass.m_InputRenderTargets.PushBack(m_BeforeDebugOutputRtIdx).Valid()))
			return false;

		if (!PK_VERIFY(currentRenderPass->AddRenderSubPass(finalSubPass)))
			return false;
	}

	// ------------------------------------------
	//
	// BAKE THE FRAME BUFFERS AND THE RENDER PASSES
	//
	// ------------------------------------------
	if (hasBeforeBloomRenderPass)
	{
		// Bake the render pass:
		if (!PK_VERIFY(m_BeforeBloomRenderPass->BakeRenderPass(m_BeforeBloomFrameBuffer->GetLayout(), beforeBloomLoadOp)))
			return false;
		// Bake the before bloom frame buffer:
		if (!PK_VERIFY(m_BeforeBloomFrameBuffer->BakeFrameBuffer(m_BeforeBloomRenderPass)))
			return false;
	}

	// If the bloom is the last render pass, then we do not need the final frame buffers and final render pass
	// as it directly writes in the swap chain render targets
	if (hasAfterBloomRenderPass)
	{
		// Bake the render pass:
		if (!PK_VERIFY(m_FinalRenderPass->BakeRenderPass(m_FinalFrameBuffers.First()->GetLayout(), finalLoadOp)))
			return false;

		for (u32 i = 0; i < m_FinalFrameBuffers.Count(); ++i)
		{
			// Bake the before bloom frame buffer:
			if (!PK_VERIFY(m_FinalFrameBuffers[i]->BakeFrameBuffer(m_FinalRenderPass)))
				return false;
		}
	}

	// ------------------------------------------
	//
	// CREATE THE RENDER STATES
	//
	// ------------------------------------------
	// Before bloom:
	if ((m_InitializedRP & InitRP_GBuffer) != 0)
	{
		const RHI::PFrameBuffer		&frameBuffer = hasBeforeBloomRenderPass ? m_BeforeBloomFrameBuffer : m_FinalFrameBuffers.First();
		const RHI::PRenderPass		&renderPass = hasBeforeBloomRenderPass ? m_BeforeBloomRenderPass : m_FinalRenderPass;
		if (!PK_VERIFY(m_GBuffer.CreateRenderStates(frameBuffer->GetLayout(), *m_ShaderLoader, renderPass)))
			return false;
		// The opaque backdrops are rendered in the GBuffer opaque render pass:
		if (!PK_VERIFY(_CreateOpaqueBackdropRenderStates(frameBuffer->GetLayout(), renderPass, m_GBuffer.m_GBufferSubPassIdx)))
			return false;
		// The transparent backdrops are rendered in the GBuffer merging render pass:
		if (!PK_VERIFY(_CreateTransparentBackdropRenderStates(frameBuffer->GetLayout(), renderPass, m_GBuffer.m_MergingSubPassIdx)))
			return false;
	}
	else
	{
		const RHI::PFrameBuffer		&frameBuffer = hasBeforeBloomRenderPass ? m_BeforeBloomFrameBuffer : m_FinalFrameBuffers.First();
		const RHI::PRenderPass		&renderPass = hasBeforeBloomRenderPass ? m_BeforeBloomRenderPass : m_FinalRenderPass;
		// The transparent backdrops are rendered in the basic render pass:
		if (!PK_VERIFY(_CreateTransparentBackdropRenderStates(frameBuffer->GetLayout(), renderPass, m_BasicRenderingSubPassIdx)))
			return false;
	}
	if ((m_InitializedRP & InitRP_Distortion) != 0)
	{
		const RHI::PFrameBuffer		&frameBuffer = hasBeforeBloomRenderPass ? m_BeforeBloomFrameBuffer : m_FinalFrameBuffers.First();
		const RHI::PRenderPass		&renderPass = hasBeforeBloomRenderPass ? m_BeforeBloomRenderPass : m_FinalRenderPass;
		if (!PK_VERIFY(m_Distortion.CreateRenderStates(frameBuffer->GetLayout(), *m_ShaderLoader, renderPass)))
			return false;
	}
	// After bloom:
	if ((m_InitializedRP & InitRP_ToneMapping) != 0)
	{
		if (!PK_VERIFY(m_ToneMapping.CreateRenderStates(m_FinalFrameBuffers.First()->GetLayout(), *m_ShaderLoader, m_FinalRenderPass)))
			return false;
	}
	if ((m_InitializedRP & InitRP_ColorRemap) != 0)
	{
		if (!PK_VERIFY(m_ColorRemap.CreateRenderStates(m_FinalFrameBuffers.First()->GetLayout(), *m_ShaderLoader, m_FinalRenderPass)))
			return false;
	}
	if ((m_InitializedRP & InitRP_FXAA) != 0)
	{
		if (!PK_VERIFY(m_FXAA.CreateRenderStates(m_FinalFrameBuffers.First()->GetLayout(), *m_ShaderLoader, m_FinalRenderPass)))
			return false;
	}
	if ((m_InitializedRP & InitRP_Debug) != 0)
	{
		// Final subpass is the subpass just after the particle debug subpass:
		if (!PK_VERIFY(_CreateFinalRenderStates(m_FinalFrameBuffers.First()->GetLayout(), m_FinalRenderPass, m_ParticleDebugSubpassIdx, m_BeforeDebugOutputRt)))
			return false;
	}

	// Add the render passes to the graphic resource manager:
	if (!_AddRenderPassesToGraphicResourceManagerIFN())
		return false;
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIParticleSceneRenderHelper::SetupPostFX_Distortion(const SParticleSceneOptions::SDistortion &config, bool /*firstInit*/)
{
	if ((m_InitializedRP & InitRP_Distortion) != 0)
	{
		m_EnableDistortion = config.m_Enable;
		m_Distortion.SetChromaticAberrationIntensity(config.m_DistortionIntensity);
		m_Distortion.SetAberrationMultipliers(config.m_ChromaticAberrationMultipliers);
	}
		
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIParticleSceneRenderHelper::SetupPostFX_Bloom(const SParticleSceneOptions::SBloom &config, bool firstInit, bool swapChainChanged)
{
	if ((m_InitializedRP & InitRP_Bloom) != 0)
	{
		EGaussianBlurCombination	blurTap = static_cast<EGaussianBlurCombination>(config.m_BlurTap);

		m_EnableBloom = config.m_Enable;
		m_Bloom.SetSubtractValue(config.m_BrightPassValue);
		m_Bloom.SetIntensity(config.m_Intensity);
		m_Bloom.SetAttenuation(config.m_Attenuation);

		if (!swapChainChanged && !firstInit)
			return true; // quick exit

		bool							success = true;

		CPostFxBloom::SInOutRenderTargets	inOut;

		_FillBloomInOut(inOut);
		success &= m_Bloom.UpdateFrameBuffer(m_DefaultSampler, config.m_RenderPassCount, &inOut);
		if (firstInit)
		{
			success &= m_Bloom.CreateRenderPass();
			success &= m_Bloom.CreateRenderStates(*m_ShaderLoader, blurTap);
		}
		else if (m_BlurTap != blurTap)
		{
			success &= m_Bloom.CreateRenderStates(*m_ShaderLoader, blurTap);
			m_BlurTap = blurTap;
		}
		success &= m_Bloom.CreateFrameBuffers();
		if (!success)
		{
			CLog::Log(PK_ERROR, "Could not re-create the bloom");
			return false;
		}
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIParticleSceneRenderHelper::SetupPostFX_ToneMapping(	const SParticleSceneOptions::SToneMapping &config,
																const SParticleSceneOptions::SVignetting &configVignetting,
																bool dithering,
																bool /*firstInit*/,
																bool precomputeLuma /* = true*/)
{
	if ((m_InitializedRP & InitRP_ToneMapping) != 0)
	{
		m_EnableToneMapping = config.m_Enable;
		m_ToneMapping.SetExposure(config.m_Exposure);
		m_ToneMapping.SetSaturation(config.m_Saturation);

		m_ToneMapping.SetVignettingColor(configVignetting.m_Color);
		m_ToneMapping.SetVignettingColorIntensity(configVignetting.m_ColorIntensity);
		m_ToneMapping.SetVignettingDesaturationIntensity(configVignetting.m_DesaturationIntensity);
		m_ToneMapping.SetVignettingRoundness(configVignetting.m_Roundness);
		m_ToneMapping.SetVignettingSmoothness(configVignetting.m_Smoothness);

		m_ToneMapping.SetDithering(dithering);

		m_ToneMapping.SetPrecomputeLuma(precomputeLuma);

		// Gamma correction is needed only if the render targets are in linear-space.
		bool isSRGB = true;
		PK_ASSERT(!m_FinalFrameBuffers.Empty());
		PK_ASSERT(!m_FinalFrameBuffers[0]->GetRenderTargets().Empty());
		RHI::PTexture	texture = m_FinalFrameBuffers[0]->GetRenderTargets()[0]->GetTexture();
		if (texture != null)
			isSRGB = (u32(texture->GetFormat()) & RHI::FlagSrgb) != 0;
		m_ToneMapping.SetGamma(isSRGB ? 1.f : 2.2f);
		m_ToneMapping.SetScreenSize(m_FinalFrameBuffers[0]->GetRenderTargets()[0]->GetSize().x(), m_FinalFrameBuffers[0]->GetRenderTargets()[0]->GetSize().y());
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIParticleSceneRenderHelper::SetupPostFX_ColorRemap(	const SParticleSceneOptions::SColorRemap &config,
																bool firstInit)
{
	(void)firstInit;

	RHI::PTexture	colorRemapTexture = m_DummyWhite;
	CUint3			dimensions = {256, 16, 1};
	if ((m_InitializedRP & InitRP_ColorRemap) != 0)
	{
		m_EnableColorRemap = config.m_Enable;
		if (!config.m_RemapTexturePath.Empty())
		{
			PK_ASSERT(m_CurrentPackResourceManager != null);
			CString	resourcePath = config.m_RemapTexturePath;
			TResourcePtr<CImage>	inputImage = m_CurrentPackResourceManager->Load<CImage>(resourcePath);
						
		
			if (inputImage != null && !inputImage->Empty() && !inputImage->m_Frames.Empty() && !inputImage->m_Frames[0].m_Mipmaps.Empty())
			{
				// CImage from ressource
				CImageMap	&resourceMipmap	= inputImage->m_Frames[0].m_Mipmaps[0];
				CImageMap	dstMipmap = resourceMipmap;

				const bool	canSkip = (inputImage->m_Format == CImage::Format_BGRA8_sRGB && config.m_ForceSRGBToLinear) ||
									  (inputImage->m_Format == CImage::Format_BGRA8 && !config.m_ForceSRGBToLinear);
				if (!canSkip)
				{
					dstMipmap.m_Dimensions = resourceMipmap.m_Dimensions;
					dstMipmap.m_RawBuffer = CRefCountedMemoryBuffer::Alloc(dstMipmap.PixelBufferSize(inputImage->m_Format));
					Mem::Copy(dstMipmap.m_RawBuffer.Get(), resourceMipmap.m_RawBuffer.Get(), dstMipmap.PixelBufferSize(inputImage->m_Format));

					// Convert to BGRA8
					CImageSurface		surface(dstMipmap, inputImage->m_Format);

					if (!surface.Convert(CImage::Format_BGRA8))
					{
						CLog::Log(PK_ERROR, "Color remap post-effect: Could not linearize LUT image \"%s\"", resourcePath.Data());
						return false;
					}
					if (config.m_ForceSRGBToLinear)
					{
						surface.m_Format = inputImage->GammaCorrected() ? CImage::Format_BGRA8 : CImage::Format_BGRA8_sRGB;
						if (!surface.Convert(inputImage->GammaCorrected() ? CImage::Format_BGRA8_sRGB : CImage::Format_BGRA8))
						{
							CLog::Log(PK_ERROR, "Color remap post-effect: Could not linearize LUT image \"%s\"", resourcePath.Data());
							return false;
						}
					}
					dstMipmap.m_RawBuffer = surface.m_RawBuffer;
				}
				if (dstMipmap.m_RawBuffer != null)
				{
					RHI::PTexture resourceTexture = m_ApiManager->CreateTexture(RHI::SRHIResourceInfos(resourcePath), TMemoryView<CImageMap>(dstMipmap), RHI::FormatUnorm8BGRA);
					dimensions = dstMipmap.m_Dimensions;
					if (resourceTexture != null)
						colorRemapTexture = resourceTexture;
					else
						CLog::Log(PK_ERROR, "Color remap post-effect: Failed loading LUT image \"%s\"", resourcePath.Data());
				}
			}		
		}
		m_ColorRemap.SetRemapTexture(colorRemapTexture, colorRemapTexture == m_DummyWhite, dimensions);
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIParticleSceneRenderHelper::SetupPostFX_FXAA(const SParticleSceneOptions::SFXAA &config, bool /*firstInit*/, bool lumaInAlpha  /* = true*/)
{
	if ((m_InitializedRP & InitRP_FXAA) != 0)
	{
		const bool	oldLumaInAlpha = m_FXAA.LumaInAlpha();
		m_EnableFXAA = config.m_Enable;
		// If lumaInAlpha is true, make sure the tonemap pass computes it.
		// If tonemap pass is not initialized, luma is computed in the FXAA pass anyway
		m_FXAA.SetLumaInAlpha(lumaInAlpha && (m_InitializedRP & InitRP_ToneMapping) != 0);

		if (oldLumaInAlpha != m_FXAA.LumaInAlpha())
		{
			if (!PK_VERIFY(m_FXAA.CreateRenderStates(m_FinalFrameBuffers.First()->GetLayout(), *m_ShaderLoader, m_FinalRenderPass)))
				return false;
		}
	}
	return true;
}

//----------------------------------------------------------------------------

bool CRHIParticleSceneRenderHelper::SetupPostFX_Overdraw(const SParticleSceneOptions::SOverdraw &config, bool /*firstInit*/)
{
	PK_ASSERT(config.m_OverdrawUpperRange >= 1u);
	m_OverdrawScaleFactor = 1.0f / config.m_OverdrawUpperRange;

	RHI::PTexture	overdrawTexture = m_HeatmapTexture;
	if ((m_InitializedRP & InitRP_ColorRemap) != 0)
	{
		if (!config.m_OverdrawTexturePath.Empty())
		{
			PK_ASSERT(m_CurrentPackResourceManager != null);
			CString					resourcePath = config.m_OverdrawTexturePath;
			TResourcePtr<CImage>	inputImage = m_CurrentPackResourceManager->Load<CImage>(resourcePath);


			if (inputImage != null && !inputImage->Empty() && !inputImage->m_Frames.Empty() && !inputImage->m_Frames[0].m_Mipmaps.Empty())
			{
				// CImage from ressource
				CImageMap	&resourceMipmap = inputImage->m_Frames[0].m_Mipmaps[0];
				CImageMap	dstMipmap = resourceMipmap;

				const bool	canSkip = inputImage->m_Format == CImage::Format_BGRA8_sRGB;
				if (!canSkip)
				{
					dstMipmap.m_Dimensions = resourceMipmap.m_Dimensions;
					dstMipmap.m_RawBuffer = CRefCountedMemoryBuffer::Alloc(dstMipmap.PixelBufferSize(inputImage->m_Format));
					Mem::Copy(dstMipmap.m_RawBuffer.Get(), resourceMipmap.m_RawBuffer.Get(), dstMipmap.PixelBufferSize(inputImage->m_Format));

					// Convert to BGRA8
					CImageSurface		surface(dstMipmap, inputImage->m_Format);

					if (!surface.Convert(CImage::Format_BGRA8))
					{
						CLog::Log(PK_ERROR, "Overdraw color palette: Could not linearize LUT image \"%s\"", resourcePath.Data());
						return false;
					}

					surface.m_Format = inputImage->GammaCorrected() ? CImage::Format_BGRA8 : CImage::Format_BGRA8_sRGB;
					if (!surface.Convert(inputImage->GammaCorrected() ? CImage::Format_BGRA8_sRGB : CImage::Format_BGRA8))
					{
						CLog::Log(PK_ERROR, "Overdraw color palette: Could not linearize LUT image \"%s\"", resourcePath.Data());
						return false;
					}
					dstMipmap.m_RawBuffer = surface.m_RawBuffer;
				}
				if (dstMipmap.m_RawBuffer != null)
				{
					RHI::PTexture resourceTexture = m_ApiManager->CreateTexture(RHI::SRHIResourceInfos(resourcePath), TMemoryView<CImageMap>(dstMipmap), RHI::FormatUnorm8BGRA);
					if (resourceTexture != null)
					{
						overdrawTexture = resourceTexture;
					}
					else
					{
						CLog::Log(PK_ERROR, "Overdraw color palette: Failed loading LUT image \"%s\"", resourcePath.Data());
					}
				}
			}
		}
		m_OverdrawConstantSet->SetConstants(m_DefaultSampler, overdrawTexture, 1);
		m_OverdrawConstantSet->UpdateConstantValues();
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIParticleSceneRenderHelper::SetupShadows()
{
	m_Lights.m_DirectionalShadows.SetCascadeShadowsSettings(0,
															m_ShadowOptions.m_CascadeShadowSceneRangeRatios.x(),
															m_ShadowOptions.m_CascadeShadowMinDistances.x());
	m_Lights.m_DirectionalShadows.SetCascadeShadowsSettings(1,
															m_ShadowOptions.m_CascadeShadowSceneRangeRatios.y(),
															m_ShadowOptions.m_CascadeShadowMinDistances.y());
	m_Lights.m_DirectionalShadows.SetCascadeShadowsSettings(2,
															m_ShadowOptions.m_CascadeShadowSceneRangeRatios.z(),
															m_ShadowOptions.m_CascadeShadowMinDistances.z());
	m_Lights.m_DirectionalShadows.SetCascadeShadowsSettings(3,
															m_ShadowOptions.m_CascadeShadowSceneRangeRatios.w(),
															m_ShadowOptions.m_CascadeShadowMinDistances.w());
	m_Lights.m_DirectionalShadows.SetShadowsInfo(	m_ShadowOptions.m_ShadowBias,
													m_ShadowOptions.m_ShadowVarianceExponent,
													m_ShadowOptions.m_EnableShadows,
													m_ShadowOptions.m_EnableVarianceShadows,
													m_ShadowOptions.m_EnableDebugShadows,
													m_ShadowOptions.m_CascadeShadowResolution);

	bool	success = true;

	success &= m_Lights.m_DirectionalShadows.UpdateShadowsSettings();
	success &= m_Lights.UpdateShadowMaps(m_DefaultSampler);
	return success;
}

//----------------------------------------------------------------------------

bool	CRHIParticleSceneRenderHelper::SetSceneInfo(const SSceneInfoData &sceneInfoData, ECoordinateFrame coordinateFrame)
{
	if (m_CoordinateFrame != coordinateFrame)
	{
		m_CoordinateFrame = coordinateFrame;
		if ((m_InitializedRP & InitRP_GBuffer) != 0)
		{
			const bool					hasDifferentRenderPasses = (m_InitializedRP & InitRP_Bloom) != 0;
			const RHI::PFrameBuffer		&frameBuffer = hasDifferentRenderPasses ? m_BeforeBloomFrameBuffer : m_FinalFrameBuffers.First();
			const RHI::PRenderPass		&renderPass = hasDifferentRenderPasses ? m_BeforeBloomRenderPass : m_FinalRenderPass;

			_CreateOpaqueBackdropRenderStates(frameBuffer->GetLayout(), renderPass, m_GBuffer.m_GBufferSubPassIdx);
		}
	}

	m_SceneInfoData = sceneInfoData;

	if (!PK_VERIFY(m_SceneInfoConstantBuffer != null))
		return false;

	// Fill the GPU buffer:
	SSceneInfoData	*sceneInfo = static_cast<SSceneInfoData*>(m_ApiManager->MapCpuView(m_SceneInfoConstantBuffer));
	if (!PK_VERIFY(sceneInfo != null))
		return false;

	*sceneInfo = sceneInfoData;
	if (!PK_VERIFY(m_ApiManager->UnmapCpuView(m_SceneInfoConstantBuffer)))
		return false;
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIParticleSceneRenderHelper::SetBackdropInfo(const SBackdropsData &backdropData, ECoordinateFrame coordinateFrame)
{
	// FIXME(Julien): This is a very bad way of handling this.
	// Setting it each frame and comparing with the last state to see if something has changed...

	bool	success = true;

	// Environment map
	if (backdropData.m_EnvironmentMapPath != m_BackdropsData.m_EnvironmentMapPath)
	{
		// We should try to reload envmap even if EnvironmentMapAffectsAmbient is false,
		// envmap might be used for background
		if (!backdropData.m_EnvironmentMapPath.Empty())
			success &= m_EnvironmentMap.Load(backdropData.m_EnvironmentMapPath.Data(), m_CurrentPackResourceManager);
		else
			m_EnvironmentMap.Reset();
	}

	if (backdropData.m_EnvironmentMapRotation != m_BackdropsData.m_EnvironmentMapRotation)
		m_EnvironmentMap.SetRotation(backdropData.m_EnvironmentMapRotation);


	if ((m_InitializedRP & InitRP_GBuffer) != 0)
	{
		m_MeshBackdrop.m_Roughness = backdropData.m_MeshRoughness;
		m_MeshBackdrop.m_Metalness = backdropData.m_MeshMetalness;
		m_MeshBackdropFilteredSubmeshes = backdropData.m_MeshFilteredSubmeshes;

		// Some code paths provide null paths (i.e. editor-only debug textures, no path is specified but the data changes)
		const bool	diffuseChanged =	(m_MeshBackdrop.m_DiffuseMap.m_Path != backdropData.m_MeshDiffusePath) ||
										(m_MeshBackdrop.m_DiffuseMap.m_Path.Empty() && (m_MeshBackdrop.m_DiffuseMap.m_ImageData != backdropData.m_MeshDiffuseData));
		const bool	normalChanged =		(m_MeshBackdrop.m_NormalMap.m_Path != backdropData.m_MeshNormalPath) ||
										(m_MeshBackdrop.m_NormalMap.m_Path.Empty() && (m_MeshBackdrop.m_NormalMap.m_ImageData != backdropData.m_MeshNormalData));
		const bool	roughMetalChanged =	(m_MeshBackdrop.m_RoughMetalMap.m_Path != backdropData.m_MeshRoughMetalPath) ||
										(m_MeshBackdrop.m_RoughMetalMap.m_Path.Empty() && (m_MeshBackdrop.m_RoughMetalMap.m_ImageData != backdropData.m_MeshRoughMetalData));

		if (m_MeshBackdrop.m_MeshPath != backdropData.m_MeshPath ||
			m_MeshBackdrop.m_MeshLOD != backdropData.m_MeshLOD ||
			diffuseChanged ||
			normalChanged ||
			roughMetalChanged ||
			backdropData.m_MeshVertexColorsSet != m_BackdropsData.m_MeshVertexColorsSet ||
			backdropData.m_MeshVertexColorsMode != m_BackdropsData.m_MeshVertexColorsMode ||
			m_MeshBackdrop.IsDirty())
		{
			Mem::Reinit(m_MeshBackdrop);

			m_MeshBackdrop.m_MeshPath = backdropData.m_MeshPath;
			m_MeshBackdrop.m_MeshLOD = backdropData.m_MeshLOD;
			m_MeshBackdrop.m_DiffuseMap.m_Path = backdropData.m_MeshDiffusePath;
			m_MeshBackdrop.m_NormalMap.m_Path = backdropData.m_MeshNormalPath;
			m_MeshBackdrop.m_RoughMetalMap.m_Path = backdropData.m_MeshRoughMetalPath;
			m_MeshBackdrop.m_DiffuseMap.m_ImageData = backdropData.m_MeshDiffuseData;
			m_MeshBackdrop.m_NormalMap.m_ImageData = backdropData.m_MeshNormalData;
			m_MeshBackdrop.m_RoughMetalMap.m_ImageData = backdropData.m_MeshRoughMetalData;

			const RHI::SConstantSetLayout constLayouts[] =
			{
				m_MeshConstSetLayout,
				m_MeshConstSetLayout,
				m_MeshConstSetLayout,
				m_MeshConstSetLayout
			};

			if (!m_MeshBackdrop.m_MeshPath.Empty())
				success &= m_MeshBackdrop.Load(	m_ApiManager,
												constLayouts,
												m_CurrentPackResourceManager,
												backdropData.m_MeshVertexColorsSet,
												backdropData.m_MeshVertexColorsMode == 2,
												m_DummyWhite,
												m_DummyNormal,
												m_DefaultSampler);
		}

		if (backdropData.m_FollowInstances)
		{
			m_MeshBackdrop.m_Transforms = backdropData.m_FXInstancesTransforms;
		}
		else
		{
			if (!PK_VERIFY(m_MeshBackdrop.m_Transforms.Resize(1)))
				return false;
			m_MeshBackdrop.m_Transforms.First() = backdropData.m_MeshBackdropTransforms;
		}

		// If there is skinned mesh data, transfer to VBuffers
		if (!m_MeshBackdrop.RefreshSkinnedDatas(m_ApiManager, backdropData.m_FXInstancesSkinnedDatas))
			return false;

		// And we update the lights
		PK_VERIFY(_UpdateBackdropLights(backdropData));
	}

	if (m_GridVertices == null || m_GridIndices == null ||
		!m_BackdropsData.IsGridSameGeometry(backdropData) ||
		m_BackdropsData.m_BackdropGridVersion != backdropData.m_BackdropGridVersion)
	{
		// Create the grid geometry:
		success &= _CreateGridGeometry(backdropData.m_GridSize, backdropData.m_GridSubdivisions, backdropData.m_GridSubSubdivisions, backdropData.m_GridTransforms, coordinateFrame);
	}

	m_BackdropsData = backdropData;

	return success;
}

//----------------------------------------------------------------------------

void	CRHIParticleSceneRenderHelper::SetCurrentPackResourceManager(CResourceManager *resourceManager)
{
	m_CurrentPackResourceManager = resourceManager;
}

//----------------------------------------------------------------------------

void	CRHIParticleSceneRenderHelper::SetBackGroundColor(const CFloat3 &top, const CFloat3 &bottom)
{
	m_BackdropsData.m_BackgroundColorTop = top;
	m_BackdropsData.m_BackgroundColorBottom = bottom;
}

//----------------------------------------------------------------------------

void	CRHIParticleSceneRenderHelper::SetBackGroundTexture(const RHI::PTexture &background)
{
	m_BackgroundTexture = background;
	if (m_BackgroundTexture != null && m_BackgroundTextureConstantSet != null)
	{
		m_BackgroundTextureConstantSet->SetConstants(m_DefaultSampler, m_BackgroundTexture, 0);
		m_BackgroundTextureConstantSet->UpdateConstantValues();
	}
}

//----------------------------------------------------------------------------

CGBuffer	&CRHIParticleSceneRenderHelper::GetDeferredSetup()
{
	return m_GBuffer;
}

//----------------------------------------------------------------------------

bool	CRHIParticleSceneRenderHelper::RenderScene(	ERenderTargetDebug		renderTargetDebug,
													const SRHIDrawOutputs	&drawOutputs,
													u32						finalRtIdx)
{
	PK_SCOPEDPROFILE();

	RHI::PCommandBuffer	preOpaqueCmdBuff = m_ApiManager->CreateCommandBuffer(RHI::SRHIResourceInfos("PK-RHI Pre Opaque command buffer"));
	RHI::PCommandBuffer	postOpaqueCmdBuff = m_ApiManager->CreateCommandBuffer(RHI::SRHIResourceInfos("PK-RHI Post Opaque command buffer"));

	if (!PK_VERIFY(preOpaqueCmdBuff != null && postOpaqueCmdBuff != null))
		return false;

	preOpaqueCmdBuff->Start();

	if (!PK_VERIFY(m_EnvironmentMap.GenerateCubemap(preOpaqueCmdBuff)))
		return false;

	if ((m_InitializedRP & InitRP_Debug) != 0)
	{
		PK_NAMEDSCOPEDPROFILE_GPU(preOpaqueCmdBuff->ProfileEventContext(), "ImGUI");
		if (!GenerateGUI(drawOutputs))
			return false;
	}

	if (m_BackdropsData.m_ShowMesh && !m_MeshBackdrop.m_MeshBatches.Empty())
		_UpdateMeshBackdropInfos();

	if (!m_PreRender.empty())
		m_PreRender(preOpaqueCmdBuff);

	const bool										hasBloom = (m_InitializedRP & InitRP_Bloom) != 0;
	const RHI::PFrameBuffer							&frameBuffer = hasBloom ? m_BeforeBloomFrameBuffer : m_FinalFrameBuffers[finalRtIdx];
	const RHI::PRenderPass							&renderPass = hasBloom ? m_BeforeBloomRenderPass : m_FinalRenderPass;
	const TMemoryView<RHI::SFrameBufferClearValue>	&clearValues = hasBloom ? m_BeforeBloomClearValues : m_FinalClearValues;

	// Render pass shadows:
	if (!m_Lights.m_DirectionalLights.Empty() && m_Lights.m_DirectionalShadows.GetShadowEnabled())
	{
		const SLightRenderPass::SPerLightData	&curLight = m_Lights.m_DirectionalLights.First();
		const CFloat3							lightDir = curLight.m_LightParam0.xyz();
		bool									hasShadows = false;

		m_Lights.m_DirectionalShadows.UpdateSceneInfo(m_SceneInfoData);
		m_Lights.m_DirectionalShadows.InitFrameUpdateSceneInfo(lightDir, m_CoordinateFrame, *m_ShaderLoader, m_MeshBackdrop, m_BackdropsData.m_CastShadows);
		_UpdateShadowsBBox(m_Lights.m_DirectionalShadows, drawOutputs.m_DrawCalls);
		m_Lights.m_DirectionalShadows.FinalizeFrameUpdateSceneInfo();
		m_Lights.UpdateShadowsInfo(m_ApiManager);
		for (u32 j = 0; j < m_Lights.m_DirectionalShadows.CascadeShadowCount(); ++j)
		{
			if (m_Lights.m_DirectionalShadows.IsSliceValidForDraw(j))
			{
				m_Lights.m_DirectionalShadows.BeginDrawShadowRenderPass(preOpaqueCmdBuff, j, m_BackdropsData.m_CastShadows ? &m_MeshBackdrop : null);
				_RenderParticles(false, ParticlePass_OpaqueShadow, drawOutputs.m_DrawCalls, preOpaqueCmdBuff, m_Lights.m_DirectionalShadows.GetSceneInfoConstSet(j));
				m_Lights.m_DirectionalShadows.EndDrawShadowRenderPass(preOpaqueCmdBuff, j);
				hasShadows = true;
			}
		}
		if (hasShadows)
			preOpaqueCmdBuff->SyncPreviousRenderPass(RHI::OutputColorPipelineStage, RHI::TopOfPipePipelineStage);
	}
	else
	{
		// Used to diable shadows in lighting render pass:
		m_Lights.UpdateShadowsInfo(m_ApiManager);
	}

	// -- START RENDER PASS: Deferred -------------------
	{
		PK_NAMEDSCOPEDPROFILE("START RENDER PASS: Deferred");
		preOpaqueCmdBuff->BeginRenderPass(renderPass, frameBuffer, clearValues);
		preOpaqueCmdBuff->SetViewport(CInt2(0, 0), frameBuffer->GetSize(), CFloat2(0, 1));
		preOpaqueCmdBuff->SetScissor(CInt2(0), frameBuffer->GetSize());
	}

	if ((m_InitializedRP & InitRP_GBuffer) != 0)
	{
		// Sub-pass: GBuffer
		{
			PK_NAMEDSCOPEDPROFILE("Sub-pass 0: GBuffer");
			PK_NAMEDSCOPEDPROFILE_GPU(preOpaqueCmdBuff->ProfileEventContext(), "Sub-pass 0: GBuffer");
			if (m_BackdropsData.m_ShowMesh && m_EnableBackdropRender)
				_RenderMeshBackdrop(preOpaqueCmdBuff);
		}

		preOpaqueCmdBuff->Stop();
		m_ApiManager->SubmitCommandBufferDirect(preOpaqueCmdBuff);

		postOpaqueCmdBuff->Start();

		// -- START RENDER PASS: Deferred -------------------
		{
			PK_NAMEDSCOPEDPROFILE("START RENDER PASS: Deferred");
			postOpaqueCmdBuff->BeginRenderPass(renderPass, frameBuffer, TMemoryView<RHI::SFrameBufferClearValue>()); // don't clear
			postOpaqueCmdBuff->SetViewport(CInt2(0, 0), frameBuffer->GetSize(), CFloat2(0, 1));
			postOpaqueCmdBuff->SetScissor(CInt2(0), frameBuffer->GetSize());
		}

		// Sub-pass: Opaque particles
		{
			PK_NAMEDSCOPEDPROFILE("Sub-pass 1: Opaque particles");
			PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "Opaque particles");

			if (m_EnableParticleRender)
				_RenderParticles(false, ParticlePass_Opaque, drawOutputs.m_DrawCalls, postOpaqueCmdBuff);
		}
		// Sub-pass: Decal
		{
			PK_NAMEDSCOPEDPROFILE("Sub-pass 2: Decals");
			PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "Decal particles");
			postOpaqueCmdBuff->NextRenderSubPass();
			if (m_EnableParticleRender)
				_RenderParticles(false, ParticlePass_Decal, drawOutputs.m_DrawCalls, postOpaqueCmdBuff);
		}
		// Sub-pass: backdrop directional light + light particles:
		{
			PK_NAMEDSCOPEDPROFILE("Sub-pass 3: backdrop directional light + light particles");
			PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "backdrop directional light + light particles");
			postOpaqueCmdBuff->NextRenderSubPass();

			_RenderBackdropLights(postOpaqueCmdBuff, m_Lights, m_GBuffer.m_LightsRenderState);

			if (m_EnableParticleRender)
			{
				PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "Light particles");
				_RenderParticles(false, ParticlePass_Lighting, drawOutputs.m_DrawCalls, postOpaqueCmdBuff);
			}
		}

		// Sub-pass: Merging
		{
			PK_NAMEDSCOPEDPROFILE("Sub-pass 4: Merging");
			PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "Merging");
			postOpaqueCmdBuff->NextRenderSubPass();

			_RenderBackground(postOpaqueCmdBuff, m_EnableOverdrawRender || !m_EnableBrushBackground);

			{
				PK_NAMEDSCOPEDPROFILE("Merge");
				PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "Merge");
				postOpaqueCmdBuff->BindRenderState(m_GBuffer.m_MergingRenderState);
				postOpaqueCmdBuff->BindVertexBuffers(m_GBuffer.m_QuadBuffers.m_VertexBuffers);
				postOpaqueCmdBuff->BindConstantSets(TMemoryView<const RHI::PConstantSet>(m_GBuffer.m_MergingSamplersSet));
				postOpaqueCmdBuff->PushConstant(&m_DeferredMergingMinAlpha, 0);
				postOpaqueCmdBuff->Draw(0, 6);
			}

			// -> Render the grid IFN:
			if (m_BackdropsData.m_ShowGrid && !m_EnableOverdrawRender)
			{
				_RenderGridBackdrop(postOpaqueCmdBuff);
			}
			// -> Render Particles
			if (m_EnableParticleRender)
			{
				PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "Transparent particles");
				_RenderParticles(false, ParticlePass_Transparent, drawOutputs.m_DrawCalls, postOpaqueCmdBuff);
			}

			_RenderEditorMisc(postOpaqueCmdBuff);
		}
	}
	else
	{
		_RenderBackground(postOpaqueCmdBuff, m_EnableOverdrawRender || !m_EnableBrushBackground);
		// Sub-pass: Basic rendering
		// -> Render Particles
		if (m_EnableParticleRender)
		{
			PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "Transparent particles");
			_RenderParticles(false, ParticlePass_Transparent, drawOutputs.m_DrawCalls, postOpaqueCmdBuff);
		}

		_RenderEditorMisc(postOpaqueCmdBuff);
	}

	if (_EndRenderPass(InitRP_GBuffer, postOpaqueCmdBuff))
	{
		postOpaqueCmdBuff->Stop();
		m_ApiManager->SubmitCommandBufferDirect(postOpaqueCmdBuff);
		return true;
	}

	// Sub-pass: Distortion-Map
	if ((m_InitializedRP & InitRP_Distortion) != 0)
	{
		// Sub-pass 4-5-6: Distortion/Blur-PostFX
		// Input: m_DeferredSetup.m_Merge
		// Output: m_DeferredSetup.m_DistortionPostFX.m_OutputRenderTarget

		PK_NAMEDSCOPEDPROFILE("Sub-pass: Distortion-Map");
		postOpaqueCmdBuff->NextRenderSubPass();
		if (PostFXEnabled_Distortion() && m_EnableParticleRender && !m_EnableOverdrawRender)
		{
			PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "Distortion particles");
			_RenderParticles(false, ParticlePass_Distortion, drawOutputs.m_DrawCalls, postOpaqueCmdBuff);
		}

		{
			PK_NAMEDSCOPEDPROFILE("Sub-passes: Distortion-PostFX");
			PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "Sub-passes: Distortion-PostFX");
			postOpaqueCmdBuff->NextRenderSubPass();
			if (PostFXEnabled_Distortion() && m_EnableParticleRender && !m_EnableOverdrawRender)
				m_Distortion.Draw(postOpaqueCmdBuff);
			else
				m_Distortion.Draw_JustCopy(postOpaqueCmdBuff);
		}

		{
			PK_NAMEDSCOPEDPROFILE("Sub-passes: Transparent post distortion");
			PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "Sub-passes: Transparent post distortion");
			postOpaqueCmdBuff->NextRenderSubPass();
			if (m_EnableParticleRender && !m_EnableOverdrawRender)
			{
				ESampleLibGraphicResources_RenderPass	renderPasses[] =
				{
					ParticlePass_Tint,
					ParticlePass_TransparentPostDisto
				};

				_RenderParticles(false, renderPasses, drawOutputs.m_DrawCalls, postOpaqueCmdBuff);
			}
		}

		if (_EndRenderPass(InitRP_Distortion, postOpaqueCmdBuff))
		{
			postOpaqueCmdBuff->Stop();
			m_ApiManager->SubmitCommandBufferDirect(postOpaqueCmdBuff);
			return true;
		}
	}

	// -- START RENDER PASS: Bloom Post-FX -------------------------
	// Input: m_DeferredSetup.m_DistortionPostFX.m_OutputRenderTarget
	// Output: m_DeferredSetup.m_DistortionPostFX.m_OutputRenderTarget
	if ((m_InitializedRP & InitRP_Bloom) != 0)
	{
		PK_NAMEDSCOPEDPROFILE("START RENDER PASS: Bloom Post-FX");
		PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "Bloom Post-FX");
		postOpaqueCmdBuff->EndRenderPass();
		postOpaqueCmdBuff->SyncPreviousRenderPass(RHI::FragmentPipelineStage, RHI::FragmentPipelineStage);
		if (PostFXEnabled_Bloom() && !m_EnableOverdrawRender)
		{
			m_Bloom.Draw(postOpaqueCmdBuff, finalRtIdx);
			postOpaqueCmdBuff->SyncPreviousRenderPass(RHI::FragmentPipelineStage, RHI::FragmentPipelineStage);
		}
		if (_IsLastRenderPass(InitRP_Bloom))
		{
			postOpaqueCmdBuff->Stop();
			m_ApiManager->SubmitCommandBufferDirect(postOpaqueCmdBuff);
			return true;
		}
	}

	// -- START RENDER PASS: Final Image -------------------------
	if (hasBloom)
	{
		PK_NAMEDSCOPEDPROFILE("START RENDER PASS: Final Image");
		postOpaqueCmdBuff->BeginRenderPass(m_FinalRenderPass, m_FinalFrameBuffers[finalRtIdx], m_FinalClearValues);
		postOpaqueCmdBuff->SetViewport(CInt2(0, 0), m_FinalFrameBuffers.First()->GetSize(), CFloat2(0, 1));
		postOpaqueCmdBuff->SetScissor(CInt2(0), m_FinalFrameBuffers.First()->GetSize());
	}
	else
	{
		postOpaqueCmdBuff->NextRenderSubPass();
	}

	if ((m_InitializedRP & InitRP_ToneMapping) != 0)
	{
		// Sub-pass 0: Tone-Mapping
		// Input: m_DeferredSetup.m_DistortionPostFX.m_OutputRenderTarget
		// Output: m_ToneMapping.m_OutputRenderTarget
		{
			PK_NAMEDSCOPEDPROFILE("Sub-pass 0: Tone-Mapping");
			PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "Tone-Mapping");

			if (PostFXEnabled_ToneMapping() && !m_EnableOverdrawRender)
				m_ToneMapping.Draw(postOpaqueCmdBuff);
			else
				m_ToneMapping.Draw_JustCopy(postOpaqueCmdBuff);
		}

		if (_EndRenderPass(InitRP_ToneMapping, postOpaqueCmdBuff))
		{
			postOpaqueCmdBuff->Stop();
			m_ApiManager->SubmitCommandBufferDirect(postOpaqueCmdBuff);
			return true;
		}

		postOpaqueCmdBuff->NextRenderSubPass();
	}
	if ((m_InitializedRP & InitRP_ColorRemap) != 0)
	{
		{
			PK_NAMEDSCOPEDPROFILE("Sub-pass 1: Color Remap");
			PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "Color Remap");

			if (PostFXEnabled_ColorRemap() && m_ColorRemap.m_HasTexture && !m_EnableOverdrawRender)
				m_ColorRemap.Draw(postOpaqueCmdBuff);
			else
				m_ColorRemap.Draw_JustCopy(postOpaqueCmdBuff);		
		}
		if (_EndRenderPass(InitRP_ColorRemap, postOpaqueCmdBuff))
		{
			postOpaqueCmdBuff->Stop();
			m_ApiManager->SubmitCommandBufferDirect(postOpaqueCmdBuff);
			return true;
		}
		postOpaqueCmdBuff->NextRenderSubPass();
	}

	// Sub-pass: FXAA
	if ((m_InitializedRP & InitRP_FXAA) != 0)
	{
		{
			PK_NAMEDSCOPEDPROFILE("Sub-pass 2: FXAA");
			PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "FXAA");
			if (PostFXEnabled_FXAA() && !m_EnableOverdrawRender)
				m_FXAA.Draw(postOpaqueCmdBuff);
			else
				m_FXAA.Draw_JustCopy(postOpaqueCmdBuff);
		}
		if (_EndRenderPass(InitRP_FXAA, postOpaqueCmdBuff))
		{
			postOpaqueCmdBuff->Stop();
			m_ApiManager->SubmitCommandBufferDirect(postOpaqueCmdBuff);
			return true;
		}
		postOpaqueCmdBuff->NextRenderSubPass();
	}

	if ((m_InitializedRP & InitRP_Debug) != 0)
	{
		// Sub-pass 1: Debug-Particles
		{
			PK_NAMEDSCOPEDPROFILE("Sub-pass 1: Debug-Particles");
			PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "Debug-Particles");

			if (m_BackdropsData.m_ShowMesh)
				_RenderMeshBackdropDebug(postOpaqueCmdBuff);

			_RenderParticles(true, ParticlePass_Debug, drawOutputs.m_DrawCalls, postOpaqueCmdBuff);
		}

		if (!DrawOnDebugBuffer(postOpaqueCmdBuff))
			return false;

		// Sub-pass 2: Final Copy (and swap-chain as output)
		{
			PK_NAMEDSCOPEDPROFILE("Sub-pass 2: Final Copy (and swap-chain as output)");
			PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "Final Copy");
			postOpaqueCmdBuff->NextRenderSubPass();
			{
				if (m_EnableOverdrawRender)
				{
					const SOverdrawInfo infos(m_OverdrawScaleFactor);

					postOpaqueCmdBuff->BindRenderState(m_OverdrawHeatmapRenderState);
					postOpaqueCmdBuff->BindVertexBuffers(m_GBuffer.m_QuadBuffers.m_VertexBuffers);
					postOpaqueCmdBuff->BindConstantSets(TMemoryView<RHI::PConstantSet>(m_OverdrawConstantSet));
					postOpaqueCmdBuff->PushConstant(&infos, 0);
					postOpaqueCmdBuff->Draw(0, 6);
				}
				else if (renderTargetDebug == RenderTargetDebug_ShadowCascades)
				{
					const CUint2	cascadeViewportSize = frameBuffer->GetSize() / 2;
					for (u32 i = 0; i < 4; ++i)
					{
						const CInt2	viewportOffset = CInt2(cascadeViewportSize.x() * (i % 2), cascadeViewportSize.y() * (i / 2));
						TStaticCountedArray<RHI::PConstantSet, 2> 	constantSets;
						constantSets.PushBack(m_Lights.m_DirectionalShadows.GetDepthConstSet(i));
						postOpaqueCmdBuff->SetViewport(viewportOffset, cascadeViewportSize, CFloat2(0, 1));
						postOpaqueCmdBuff->BindRenderState(m_CopyColorRenderState);
						postOpaqueCmdBuff->BindVertexBuffers(m_GBuffer.m_QuadBuffers.m_VertexBuffers);
						postOpaqueCmdBuff->BindConstantSets(constantSets);
						postOpaqueCmdBuff->Draw(0, 6);
					}
				}
				else
				{
					RHI::PRenderState							renderState = m_CopyColorRenderState;
					TStaticCountedArray<RHI::PConstantSet, 2>	constantSets;

					if (renderTargetDebug == RenderTargetDebug_NoDebug)
					{
						// Display final render target:
						constantSets.PushBack(m_BeforeDebugOutputRt.m_SamplerConstantSet);
					}
					else if (renderTargetDebug == RenderTargetDebug_Distortion)
					{
						// Display distortion render target:
						if ((m_InitializedRP & InitRP_Distortion) == 0)
							constantSets.PushBack(m_BeforeDebugOutputRt.m_SamplerConstantSet);
						else
							constantSets.PushBack(m_Distortion.m_DistoRenderTarget.m_SamplerConstantSet);
					}
					else
					{
						// Display one of the GBuffer render targets:
						if ((m_InitializedRP & InitRP_GBuffer) == 0)
							constantSets.PushBack(m_BeforeDebugOutputRt.m_SamplerConstantSet);
						else
						{
							switch (renderTargetDebug)
							{
							case RenderTargetDebug_Diffuse:
								constantSets.PushBack(GBufferRT(SPassDescription::GBufferRT_Diffuse).m_SamplerConstantSet);
								break;
							case RenderTargetDebug_Depth:
								constantSets.PushBack(GBufferRT(SPassDescription::GBufferRT_Depth).m_SamplerConstantSet);
								renderState = m_CopyDepthRenderState;
								break;
							case RenderTargetDebug_Normal:
								constantSets.PushBack(m_GBuffer.m_NormalRoughMetal.m_SamplerConstantSet);
								renderState = m_CopyNormalRenderState;
								break;
							case RenderTargetDebug_NormalUnpacked:
								constantSets.PushBack(m_GBuffer.m_NormalRoughMetal.m_SamplerConstantSet);
								constantSets.PushBack(m_SceneInfoConstantSet);
								renderState = m_CopyNormalUnpackedRenderState;
								break;
							case RenderTargetDebug_Roughness:
								constantSets.PushBack(m_GBuffer.m_NormalRoughMetal.m_SamplerConstantSet);
								renderState = m_CopySpecularRenderState;
								break;
							case RenderTargetDebug_Metalness:
								constantSets.PushBack(m_GBuffer.m_NormalRoughMetal.m_SamplerConstantSet);
								renderState = m_CopyAlphaRenderState;
								break;
							case RenderTargetDebug_LightAccum:
								constantSets.PushBack(m_GBuffer.m_LightAccu.m_SamplerConstantSet);
								break;
							case RenderTargetDebug_PostMerge:
								constantSets.PushBack(m_GBuffer.m_Merge.m_SamplerConstantSet);
								break;
							default:
								constantSets.PushBack(m_BeforeDebugOutputRt.m_SamplerConstantSet);
								break;
							}
						}
					}

					postOpaqueCmdBuff->BindRenderState(renderState);
					postOpaqueCmdBuff->BindVertexBuffers(m_GBuffer.m_QuadBuffers.m_VertexBuffers);
					if (renderTargetDebug == RenderTargetDebug_Depth)
						postOpaqueCmdBuff->PushConstant(&m_SceneInfoData.m_ZBufferLimits, 0);
					postOpaqueCmdBuff->BindConstantSets(constantSets);
					postOpaqueCmdBuff->Draw(0, 6);
				}
			}
		}

		// Draw Selection
		{
			PK_NAMEDSCOPEDPROFILE("Draw Selection");
			PK_NAMEDSCOPEDPROFILE_GPU(postOpaqueCmdBuff->ProfileEventContext(), "Selection");
			_RenderParticles(true, ParticlePass_Compositing, drawOutputs.m_DrawCalls, postOpaqueCmdBuff);
			// We should render the selected light particles here. Render the light spheres wire-frames...
		}

		if (!DrawOnFinalBuffer(postOpaqueCmdBuff))
			return false;

		if (_EndRenderPass(InitRP_Debug, postOpaqueCmdBuff))
		{
			postOpaqueCmdBuff->Stop();
			m_ApiManager->SubmitCommandBufferDirect(postOpaqueCmdBuff);
			return true;
		}
	}
	PK_ASSERT_NOT_REACHED_MESSAGE("The last render pass in the InitRP bitmask should be InitRP_Debug");
	return false;
}

//----------------------------------------------------------------------------

void	CRHIParticleSceneRenderHelper::_RenderMeshBackdropDebug(const RHI::PCommandBuffer &cmdBuff)
{
	(void)cmdBuff;
}

//----------------------------------------------------------------------------

bool	CRHIParticleSceneRenderHelper::_CullParticleDraw(	bool debug,
															PKSample::ESampleLibGraphicResources_RenderPass renderPass,
															const PKSample::SRHIDrawCall &drawCall,
															bool validRenderState)
{
	(void)debug; (void)renderPass; (void)drawCall; (void)validRenderState;
	return false;
}

//----------------------------------------------------------------------------

RHI::PRenderState	CRHIParticleSceneRenderHelper::_GetDebugRenderState(PKSample::ESampleLibGraphicResources_RenderPass renderPass,
																		const PKSample::SRHIDrawCall &drawCall,
																		PKSample::EShaderOptions shaderOptions,
																		PKSample::ESampleLibGraphicResources_RenderPass *cacheRenderPass)
{
	(void)renderPass; (void)drawCall; (void)shaderOptions; (void)cacheRenderPass;
	return null;
}

//----------------------------------------------------------------------------

bool	CRHIParticleSceneRenderHelper::_CreateFinalDebugRenderStates(	TMemoryView<const RHI::SRenderTargetDesc> frameBufferLayout,
																		RHI::PRenderPass renderPass,
																		CGuid mergeSubPassIdx,
																		CGuid finalSubPassIdx)
{
	(void)frameBufferLayout; (void)renderPass; (void)mergeSubPassIdx; (void)finalSubPassIdx;
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIParticleSceneRenderHelper::_BindDebugVertexBuffer(const PKSample::SRHIDrawCall &drawCall, const RHI::PCommandBuffer &cmdBuff)
{
	(void)drawCall; (void)cmdBuff;
	return false;
}

//----------------------------------------------------------------------------

bool	CRHIParticleSceneRenderHelper::_BindDebugConstantSets(const RHI::PRenderState &renderState, const PKSample::SRHIDrawCall &drawCall, TStaticCountedArray<RHI::PConstantSet, 0x10> &constantSets)
{
	(void)drawCall; (void)constantSets; (void)renderState;
	return true;
}

//----------------------------------------------------------------------------

void	CRHIParticleSceneRenderHelper::_RenderParticles(bool											debugMode,
														CRenderPassArray								renderPasses,
														const TArray<SRHIDrawCall>						&drawCalls,
														const RHI::PCommandBuffer						&cmdBuff,
														const RHI::PConstantSet							&sceneInfo)
{
	PK_SCOPEDPROFILE();
	RHI::PRenderState							renderState = null;
	const void									*lastBatch = null;

	for (const SRHIDrawCall &dc : drawCalls)
	{
		// Here we either get the render state from the renderer cache or we choose between the debug render states:
		EShaderOptions							shaderOptions = static_cast<EShaderOptions>(dc.m_ShaderOptions);
		ESampleLibGraphicResources_RenderPass	cacheRenderPass = __MaxParticlePass;
		u32										neededConstants = 0;
		PCRendererCacheInstance					cacheInstance = null;
		RHI::PRenderState						newRenderState = null;
		const bool								isGeomShaderBB = !!(shaderOptions & Option_GeomBillboarding);
		const bool								isVertexShaderBB =	(shaderOptions & Option_VertexBillboarding) ||
																	(shaderOptions & Option_TriangleVertexBillboarding) ||
																	(shaderOptions & Option_RibbonVertexBillboarding);
		const bool								gpuStorage = (shaderOptions & PKSample::Option_GPUStorage) != 0;
		const bool								GPUMesh = (shaderOptions & PKSample::Option_GPUMesh) != 0;

		if (debugMode && gpuStorage)
		{
			if ((shaderOptions & Option_RibbonVertexBillboarding) != 0)
				continue; // Ribbon GPU particles debug draw not currently supported
		}

		for (ESampleLibGraphicResources_RenderPass renderPass : renderPasses)
		{
			//------------------------------------------------------
			// Retrieve renderer cache instance:
			//------------------------------------------------------
			{
				CRendererCacheInstance_UpdateThread		*srcRendererCache = dc.m_RendererCacheInstance.Get();
				if (srcRendererCache != null)
				{
					if (renderPass == ParticlePass_OpaqueShadow && !srcRendererCache->CastShadows())
						continue;
					cacheInstance = srcRendererCache->RenderThread_GetCacheInstance();
					if (cacheInstance != null)
					{
						const PRendererCache	layout = cacheInstance->m_Cache;
						if (layout != null)
							newRenderState = layout->GetRenderState(shaderOptions, renderPass, &neededConstants);
					}
				}
				else
					CLog::Log(PK_ERROR, "The renderer does not have a cache instance: some graphical resource creation must have failed");
			}

			//------------------------------------------------------
			// Cull particle draw
			//------------------------------------------------------
			if (_CullParticleDraw(debugMode, renderPass, dc, newRenderState != null))
			{
				continue;
			}

			if (debugMode)
			{
				newRenderState = _GetDebugRenderState(renderPass, dc, shaderOptions, &cacheRenderPass);
				if (cacheRenderPass != renderPass)
					continue;
			}

			if (newRenderState == null)
			{
				continue;
			}

			//------------------------------------------------------
			// Bind render state and constants:
			//------------------------------------------------------
			if (renderState != newRenderState)
			{
				renderState = newRenderState;
				cmdBuff->BindRenderState(newRenderState);
			}

			// First we add the scene info constant set and then the renderer cache constant sets:
			TStaticCountedArray<RHI::PConstantSet, 0x10>	constantSets;
			if (!debugMode)
			{
				PK_ASSERT(cacheInstance != null);
				if ((m_InitializedRP & InitRP_GBuffer) != 0)
				{
					if (neededConstants & MaterialToRHI::NeedSampleDepth)
						constantSets.PushBack(GBufferRT(SPassDescription::GBufferRT_Depth).m_SamplerConstantSet);
					if (neededConstants & MaterialToRHI::NeedSampleNormalRoughMetal)
						constantSets.PushBack(m_GBuffer.m_NormalRoughMetal.m_SamplerConstantSet);
					if (neededConstants & MaterialToRHI::NeedSampleDiffuse)
						constantSets.PushBack(GBufferRT(SPassDescription::GBufferRT_Diffuse).m_SamplerConstantSet);
				}
				else
				{
					if (neededConstants & MaterialToRHI::NeedSampleDepth)
						constantSets.PushBack(m_DummyWhiteConstantSet);
					if (neededConstants & MaterialToRHI::NeedSampleNormalRoughMetal)
						constantSets.PushBack(m_DummyBlackConstantSet);
					if (neededConstants & MaterialToRHI::NeedSampleDiffuse)
						constantSets.PushBack(m_DummyBlackConstantSet);
				}
				if (neededConstants & MaterialToRHI::NeedLightingInfo)
				{
					constantSets.PushBack(m_Lights.m_LightsInfoConstantSet);
					if (renderPass == ParticlePass_OpaqueShadow)
						constantSets.PushBack(m_Lights.m_DummyShadowsInfoConstantSet);
					else
						constantSets.PushBack(m_Lights.m_ShadowsInfoConstantSet);
					constantSets.PushBack(m_BRDFLUTConstantSet);
					const bool	envMapAffectsAmbient = (!m_EnvironmentMap.IsValid() || m_BackdropsData.m_EnvironmentMapPath.Empty()) ? false : m_BackdropsData.m_EnvironmentMapAffectsAmbient;
					constantSets.PushBack(envMapAffectsAmbient ? m_EnvironmentMap.GetIBLCubemapConstantSet() : m_EnvironmentMap.GetWhiteEnvMapConstantSet());
				}
				if (neededConstants & MaterialToRHI::NeedAtlasInfo)
				{
					// If the atlas dimension is 1 * 1, it's null in the cache (see LoadRendererAtlas() in
					// RHIGraphicResources) but the atlas feature is not disabled in the shader, so we load
					// a valid 1 * 1 atlas definition.
					if (cacheInstance->m_Atlas != null)
						constantSets.PushBack(cacheInstance->m_Atlas->m_AtlasConstSet);
					else
						constantSets.PushBack(m_DummyAtlas.m_AtlasConstSet);
				}

				if (neededConstants & MaterialToRHI::NeedDitheringPattern)
					constantSets.PushBack(m_NoiseTexture.m_NoiseTextureConstantSet);

				constantSets.PushBack(sceneInfo == null ? m_SceneInfoConstantSet : sceneInfo);

				if (cacheInstance->m_ConstSet != null)
					constantSets.PushBack(cacheInstance->m_ConstSet);
			}
			else
			{
				constantSets.PushBack(sceneInfo == null ? m_SceneInfoConstantSet : sceneInfo);
			}

			if (isGeomShaderBB || isVertexShaderBB)
			{
				if (!gpuStorage)
				{
					PK_ASSERT(dc.m_UBSemanticsPtr[SRHIDrawCall::UBSemantic_GPUBillboard] != null);

					// TODO: Fix this, we should avoid creating the constant set per draw call..
					// But if disabled, artefacts on d3d12
					PK_NAMEDSCOPEDPROFILE("Create constant set for draw requests");
					RHI::PConstantSet	drawRequestsConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Draw Requests Constant Set"), SConstantDrawRequests::GetConstantSetLayout(dc.m_RendererType));
					if (!PK_VERIFY(drawRequestsConstantSet != null))
						return;

					drawRequestsConstantSet->SetConstants(dc.m_UBSemanticsPtr[SRHIDrawCall::UBSemantic_GPUBillboard], 0);
					drawRequestsConstantSet->UpdateConstantValues();
					constantSets.PushBack(drawRequestsConstantSet);
				}
				if (isVertexShaderBB)
				{
					if (debugMode)
					{
						if (!_BindDebugConstantSets(renderState, dc, constantSets))
							return;
					}
					else
					{
						if (gpuStorage)
						{
							if (!PK_VERIFY(constantSets.PushBack(dc.m_GPUStorageOffsetsConstantSet).Valid()))
								return;
						}
						// Full constant set (Positions, Sizes, .. All additional inputs)
						if (!PK_VERIFY(constantSets.PushBack(dc.m_GPUStorageSimDataConstantSet).Valid()))
							return;
					}
				}
			}
			else if (GPUMesh)
			{
				if (debugMode)
				{
					if (!_BindDebugConstantSets(renderState, dc, constantSets))
						return;
				}
				else
				{
					if (!PK_VERIFY(constantSets.PushBack(dc.m_GPUStorageOffsetsConstantSet).Valid()))
						return;
					// Full constant set (Positions, Sizes, .. All additional inputs)
					if (!PK_VERIFY(constantSets.PushBack(dc.m_GPUStorageSimDataConstantSet).Valid()))
						return;
				}
			}

			for (u32 i = 0; i < dc.m_PushConstants.Count(); ++i)
				cmdBuff->PushConstant(&dc.m_PushConstants[i], i);

			cmdBuff->BindConstantSets(constantSets.View());

			//------------------------------------------------------
			// Render
			//------------------------------------------------------
			if (dc.m_Batch != lastBatch || !dc.m_VertexOffsets.Empty())
			{
				if (dc.m_IndexBuffer != null)
					cmdBuff->BindIndexBuffer(dc.m_IndexBuffer, 0, dc.m_IndexSize);

				if (!debugMode)
				{
					// Takes all the generated vertex inputs:
					cmdBuff->BindVertexBuffers(dc.m_VertexBuffers, dc.m_VertexOffsets);
				}
				else
				{
					_BindDebugVertexBuffer(dc, cmdBuff);
				}
				//lastBatch = dc->m_Batch; // Fixme
			}

			// Draw:
			if (dc.m_Type == SRHIDrawCall::DrawCall_Regular)
			{
				if (dc.m_IndexBuffer != null)
					cmdBuff->DrawIndexed(dc.m_IndexOffset, dc.m_VertexOffset, dc.m_IndexCount);
				else
					cmdBuff->Draw(dc.m_VertexOffset, dc.m_VertexCount);
			}
			else if (dc.m_Type == SRHIDrawCall::DrawCall_IndexedInstanced)
			{
				cmdBuff->DrawIndexedInstanced(dc.m_IndexOffset, 0, dc.m_IndexCount, dc.m_InstanceCount);
			}
			else if (dc.m_Type == SRHIDrawCall::DrawCall_InstancedIndirect)
			{
				cmdBuff->DrawInstancedIndirect(dc.m_IndirectBuffer, dc.m_IndirectBufferOffset);
			}
			else if (dc.m_Type == SRHIDrawCall::DrawCall_IndexedInstancedIndirect)
			{
				cmdBuff->DrawIndexedInstancedIndirect(dc.m_IndirectBuffer, dc.m_IndirectBufferOffset);
			}
			else
			{
				PK_ASSERT_NOT_REACHED();
			}
		}
	}
}

//----------------------------------------------------------------------------

void	CRHIParticleSceneRenderHelper::_RenderGridBackdrop(const RHI::PCommandBuffer &cmdBuff)
{
	PK_SCOPEDPROFILE();
	PK_NAMEDSCOPEDPROFILE_GPU(cmdBuff->ProfileEventContext(), "Grid");
	const CFloat4	bigGridColor = ConvertSRGBToLinear(m_BackdropsData.m_GridColor);
	const CFloat4	smallGridColor = ConvertSRGBToLinear(m_BackdropsData.m_GridSubColor);

	cmdBuff->BindRenderState(m_GridRenderState);
	cmdBuff->BindConstantSets(TMemoryView<RHI::PConstantSet>(m_SceneInfoConstantSet));

	cmdBuff->BindVertexBuffers(TMemoryView<RHI::PGpuBuffer>(m_GridVertices));
	cmdBuff->BindIndexBuffer(m_GridIndices, 0, RHI::IndexBuffer16Bit);

	cmdBuff->PushConstant(&smallGridColor, 0);
	cmdBuff->DrawIndexed(m_GridIdxCount, 0, m_GridSubdivIdxCount);

	cmdBuff->PushConstant(&bigGridColor, 0);
	cmdBuff->DrawIndexed(0, 0, m_GridIdxCount);
}

//----------------------------------------------------------------------------

void	CRHIParticleSceneRenderHelper::_RenderEditorMisc(const RHI::PCommandBuffer &cmdBuff)
{
	PK_SCOPEDPROFILE();
	PK_NAMEDSCOPEDPROFILE_GPU(cmdBuff->ProfileEventContext(), "Debug draw lines");
	_DrawDebugLines(cmdBuff, m_MiscLinesOpaque_Position, m_MiscLinesOpaque_Color, m_LinesPointsColorBufferOpaque, m_LinesRenderStateOpaque);
	_DrawDebugLines(cmdBuff, m_MiscLinesAdditive_Position, m_MiscLinesAdditive_Color, m_LinesPointsColorBufferAdditive, m_LinesRenderStateAdditive);
}

//----------------------------------------------------------------------------

void	CRHIParticleSceneRenderHelper::BindMiscLinesOpaque(const TMemoryView<const CFloat3> &positions, const TMemoryView<const CFloat4> &colors)
{
	m_MiscLinesOpaque_Position = positions;
	m_MiscLinesOpaque_Color = colors;
}

//----------------------------------------------------------------------------

void	CRHIParticleSceneRenderHelper::BindMiscLinesAdditive(const TMemoryView<const CFloat3> &positions, const TMemoryView<const CFloat4> &colors)
{
	m_MiscLinesAdditive_Position = positions;
	m_MiscLinesAdditive_Color = colors;
}

//----------------------------------------------------------------------------

void	CRHIParticleSceneRenderHelper::ClearMiscGeom()
{
	m_MiscLinesOpaque_Position.Clear();
	m_MiscLinesOpaque_Color.Clear();
	m_MiscLinesAdditive_Position.Clear();
	m_MiscLinesAdditive_Color.Clear();
}

//----------------------------------------------------------------------------

void	CRHIParticleSceneRenderHelper::_UpdateMeshBackdropInfos()
{
	PK_ASSERT(m_MeshBackdrop.m_MeshInfo != null);
	SMeshFragmentConstant		*meshFragmentConstant = static_cast<SMeshFragmentConstant*>(m_ApiManager->MapCpuView(m_MeshBackdrop.m_MeshInfo));

	if (meshFragmentConstant == null)
		return;

	meshFragmentConstant->m_Roughness = m_MeshBackdrop.m_Roughness;
	meshFragmentConstant->m_Metalness = m_MeshBackdrop.m_Metalness;

	if (!m_ApiManager->UnmapCpuView(m_MeshBackdrop.m_MeshInfo))
		return;
}

//----------------------------------------------------------------------------

bool	CRHIParticleSceneRenderHelper::_UpdateBackdropLights(const SBackdropsData &backdrop)
{
	const float	kDeg2Rad = TNumericConstants<float>::Pi() / 180.0f;
	CFloat3		ambientColor = CFloat3::ZERO;
	u32			dirLightIdx = 0;
	u32			spotLightIdx = 0;
	u32			pointLightIdx = 0;

	const bool	sameShadows = (m_ShadowOptions == backdrop.m_ShadowOptions);
	if (!sameShadows)
	{
		m_ShadowOptions = backdrop.m_ShadowOptions;
		SetupShadows();
	}

	for (u32 i = 0; i < backdrop.m_Lights.Count(); ++i)
	{
		const CFloat3	lightColor = backdrop.m_Lights[i].Color() * backdrop.m_Lights[i].Intensity();

		if (backdrop.m_Lights[i].Type() == SLightRenderPass::Ambient)
		{
			ambientColor += lightColor;
		}
		else if (backdrop.m_Lights[i].Type() == SLightRenderPass::Directional)
		{
			if (dirLightIdx >= m_Lights.m_DirectionalLights.Count())
			{
				if (!PK_VERIFY(m_Lights.m_DirectionalLights.PushBack().Valid()))
					return false;
			}
			m_Lights.m_DirectionalLights[dirLightIdx] = SLightRenderPass::SPerLightData(backdrop.m_Lights[i].Direction(), lightColor);
			++dirLightIdx;
		}
		else if (backdrop.m_Lights[i].Type() == SLightRenderPass::Spot)
		{
			if (spotLightIdx >= m_Lights.m_SpotLights.Count())
			{
				if (!PK_VERIFY(m_Lights.m_SpotLights.PushBack().Valid()))
					return false;
			}
			m_Lights.m_SpotLights[spotLightIdx] = SLightRenderPass::SPerLightData(	backdrop.m_Lights[i].Position(),
																					backdrop.m_Lights[i].Direction(),
																					lightColor,
																					cosf(backdrop.m_Lights[i].Angle() * kDeg2Rad),
																					backdrop.m_Lights[i].ConeFalloff());
			++spotLightIdx;
		}
		else if (backdrop.m_Lights[i].Type() == SLightRenderPass::Point)
		{
			if (pointLightIdx >= m_Lights.m_PointLights.Count())
			{
				if (!PK_VERIFY(m_Lights.m_PointLights.PushBack().Valid()))
					return false;
			}
			m_Lights.m_PointLights[pointLightIdx] = SLightRenderPass::SPerLightData(backdrop.m_Lights[i].Position(), lightColor);
			++pointLightIdx;
		}
	}

	bool	success = true;

	if (dirLightIdx < m_Lights.m_DirectionalLights.Count())
		success &= m_Lights.m_DirectionalLights.Resize(dirLightIdx);
	if (spotLightIdx < m_Lights.m_SpotLights.Count())
		success &= m_Lights.m_SpotLights.Resize(spotLightIdx);
	if (pointLightIdx < m_Lights.m_PointLights.Count())
		success &= m_Lights.m_PointLights.Resize(pointLightIdx);

	if (!PK_VERIFY(success))
		return false;

	// We update the ambient lighting (add all the ambient backdrop lights):
	const bool		envMapAffectsAmbient = (!m_EnvironmentMap.IsValid() || backdrop.m_EnvironmentMapPath.Empty()) ? false : backdrop.m_EnvironmentMapAffectsAmbient;
	ambientColor = envMapAffectsAmbient ? backdrop.m_EnvironmentMapColor * backdrop.m_EnvironmentMapIntensity : ambientColor;

	m_Lights.m_LightsInfoData.m_AmbientColor = ambientColor;

	if (!PK_VERIFY(m_Lights.Update(m_ApiManager)))
		return false;
	return true;
}

//----------------------------------------------------------------------------

void	CRHIParticleSceneRenderHelper::_RenderBackdropLights(	const RHI::PCommandBuffer &cmdBuff,
																const SLightRenderPass &lightInfo,
																const RHI::PRenderState &renderState)
{
	PK_NAMEDSCOPEDPROFILE("Draw light backdrop");
	PK_NAMEDSCOPEDPROFILE_GPU(cmdBuff->ProfileEventContext(), "Directional light");

	cmdBuff->BindRenderState(renderState);
	cmdBuff->BindVertexBuffers(m_GBuffer.m_QuadBuffers.m_VertexBuffers);

	const bool	envMapAffectsAmbient = (!m_EnvironmentMap.IsValid() || m_BackdropsData.m_EnvironmentMapPath.Empty()) ? false : m_BackdropsData.m_EnvironmentMapAffectsAmbient;

	const RHI::PConstantSet		lightConstSets[] =
	{
		m_SceneInfoConstantSet,
		lightInfo.m_LightsInfoConstantSet,
		lightInfo.m_ShadowsInfoConstantSet,
		m_BRDFLUTConstantSet,
		envMapAffectsAmbient ? m_EnvironmentMap.GetIBLCubemapConstantSet() : m_EnvironmentMap.GetWhiteEnvMapConstantSet(),
		m_GBuffer.m_LightingSamplersSet,
	};

	cmdBuff->BindConstantSets(lightConstSets);
	cmdBuff->Draw(0, 6);
}

//----------------------------------------------------------------------------

void	CRHIParticleSceneRenderHelper::_RenderMeshBackdrop(const RHI::PCommandBuffer &cmdBuff)
{
	PK_NAMEDSCOPEDPROFILE_GPU(cmdBuff->ProfileEventContext(), "Mesh backdrop");
	if (m_MeshBackdrop.m_MeshBatches.Empty())
		return;

	const bool	useVertexColors = m_MeshBackdrop.m_HasVertexColors && m_BackdropsData.m_MeshVertexColorsMode != 0u;

	if (useVertexColors)
		cmdBuff->BindRenderState(m_MeshRenderStateVertexColor);
	else
		cmdBuff->BindRenderState(m_MeshRenderState);

	const RHI::PConstantSet		constSets[] =
	{
		m_SceneInfoConstantSet,
		m_MeshBackdrop.m_ConstantSet
	};

	cmdBuff->BindConstantSets(constSets);

	SMeshVertexConstant	meshVertexConstant;
	const u32			instanceCount = m_MeshBackdrop.m_Transforms.Count();
	for (u32 iInstance = 0; iInstance < instanceCount; ++iInstance)
	{
		// Model matrix
		const CFloat4x4	meshTransforms = m_MeshBackdrop.m_Transforms[iInstance];
		meshVertexConstant.m_ModelMatrix = meshTransforms;
		// Normal matrix (same as model but without scale and translate)
		const CFloat4x4	modelNoTranslate = CFloat4x4(meshTransforms.XAxis(), meshTransforms.YAxis(), meshTransforms.ZAxis(), CFloat4::WAXIS);
		meshVertexConstant.m_NormalMatrix = modelNoTranslate.Inverse().Transposed();

		cmdBuff->PushConstant(&meshVertexConstant, 0);
		for (u32 j = 0, stop = (m_MeshBackdropFilteredSubmeshes.Empty() ? m_MeshBackdrop.m_MeshBatches.Count() : m_MeshBackdropFilteredSubmeshes.Count()); j < stop; ++j)
		{
			const u32							smidx = m_MeshBackdropFilteredSubmeshes.Empty() ? j : m_MeshBackdropFilteredSubmeshes[j];
			if (smidx >= m_MeshBackdrop.m_MeshBatches.Count()) // This can happen the frame the backdrop LOD changes in the mesh viewer
				continue;

			const PKSample::SMesh::SMeshBatch	&curBatch = m_MeshBackdrop.m_MeshBatches[smidx];
			RHI::PGpuBuffer						vBuffer = null;

			// curBatch can be either static or skinned batch, most of the time it won't be animated
			if (!PK_VERIFY(curBatch.m_Instances.Empty() || iInstance < curBatch.m_Instances.Count()))
				continue;
			// If this instance has valid skinned data, render with those vb, otherwise render the bindpose
			if (curBatch.m_Instances.Empty() || !curBatch.m_Instances[iInstance].m_HasValidSkinnedData)
				vBuffer = curBatch.m_BindPoseVertexBuffers;
			else
				vBuffer = curBatch.m_Instances[iInstance].m_SkinnedVertexBuffers;

			if (!PK_VERIFY(vBuffer != null && curBatch.m_BindPoseVertexBuffers != null))
				continue;

			const RHI::PGpuBuffer		vertexBuffersWithColors[] = { vBuffer, vBuffer, vBuffer, curBatch.m_BindPoseVertexBuffers, curBatch.m_BindPoseVertexBuffers };
			const u32					offsetsWithColors[] = { curBatch.m_PositionsOffset, curBatch.m_NormalsOffset, curBatch.m_TangentsOffset, curBatch.m_TexCoordsOffset, curBatch.m_ColorsOffset };

			const RHI::PGpuBuffer		vertexBuffers[] = { vBuffer, vBuffer, vBuffer, curBatch.m_BindPoseVertexBuffers};
			const u32					offsets[] = { curBatch.m_PositionsOffset, curBatch.m_NormalsOffset, curBatch.m_TangentsOffset, curBatch.m_TexCoordsOffset};

			if (useVertexColors)
				cmdBuff->BindVertexBuffers(vertexBuffersWithColors, offsetsWithColors);
			else
				cmdBuff->BindVertexBuffers(vertexBuffers, offsets);

			cmdBuff->BindIndexBuffer(curBatch.m_IndexBuffer, 0, curBatch.m_IndexSize);

			cmdBuff->DrawIndexed(0, 0, curBatch.m_IndexCount);
		}
	}
}

//----------------------------------------------------------------------------

void	CRHIParticleSceneRenderHelper::_RenderBackground(const RHI::PCommandBuffer &cmdBuff, bool clearOnly /*= false */)
{
	PK_NAMEDSCOPEDPROFILE("Draw background");
	PK_NAMEDSCOPEDPROFILE_GPU(cmdBuff->ProfileEventContext(), "Background");
	if (m_BackgroundTexture != null && !clearOnly)
	{
		cmdBuff->BindRenderState(m_CopyBackgroundColorRenderState);
		cmdBuff->BindVertexBuffers(m_GBuffer.m_QuadBuffers.m_VertexBuffers);
		cmdBuff->BindConstantSets(TMemoryView<RHI::PConstantSet>(m_BackgroundTextureConstantSet));
		cmdBuff->Draw(0, 6);
	}
	else
	{
		cmdBuff->BindRenderState(m_BrushBackgroundRenderState);
		cmdBuff->BindVertexBuffers(m_GBuffer.m_QuadBuffers.m_VertexBuffers);

		// Update brush info data:
		SBrushInfo	*infos = (SBrushInfo*)m_ApiManager->MapCpuView(m_BrushInfoData);
		if (!PK_VERIFY(infos != null))
			return;

		infos->m_TopColor = clearOnly ? CFloat4(0.f, 0.f, 0.f, m_DeferredMergingMinAlpha) : CFloat4(ConvertSRGBToLinear(m_BackdropsData.m_BackgroundColorTop), m_DeferredMergingMinAlpha);
		infos->m_BottomColor = clearOnly ? CFloat4(0.f, 0.f, 0.f, m_DeferredMergingMinAlpha) : CFloat4(ConvertSRGBToLinear(m_BackdropsData.m_BackgroundColorBottom), m_DeferredMergingMinAlpha);
		infos->m_CameraPosition = m_SceneInfoData.m_InvView.WAxis();
		CCoordinateFrame::BuildTransitionFrame(CCoordinateFrame::GlobalFrame(), Frame_RightHand_Y_Up, infos->m_UserToRHY);
		infos->m_InvViewProj = m_SceneInfoData.m_InvViewProj;
		infos->m_EnvironmentMapColor = CFloat4(ConvertSRGBToLinear(m_BackdropsData.m_EnvironmentMapColor) * m_BackdropsData.m_EnvironmentMapIntensity, 0);
		infos->m_EnvironmentMapMipLvl = m_BackdropsData.m_EnvironmentMapBlur * m_EnvironmentMap.GetBackgroundMipmapCount();
		const bool	envMapVisible = (!m_EnvironmentMap.IsValid() || m_BackdropsData.m_EnvironmentMapPath.Empty()) ? false : m_BackdropsData.m_BackgroundUsesEnvironmentMap;
		infos->m_EnvironmentMapVisible = clearOnly ? 0 : static_cast<u32>(envMapVisible);

		m_ApiManager->UnmapCpuView(m_BrushInfoData);
		// ------------

		const RHI::PConstantSet		brushConstantSets[] = { m_BrushInfoConstSet, envMapVisible ? m_EnvironmentMap.GetBackgroundCubemapConstantSet() : m_EnvironmentMap.GetIBLCubemapConstantSet() };
		cmdBuff->BindConstantSets(brushConstantSets, 0);
		cmdBuff->Draw(0, 6);
	}
}

//----------------------------------------------------------------------------

void	CRHIParticleSceneRenderHelper::_UpdateShadowsBBox(CDirectionalShadows &shadow, const TArray<SRHIDrawCall> &drawCalls)
{
	PK_SCOPEDPROFILE();
	for (u32 dcIdx = 0; dcIdx < drawCalls.Count(); ++dcIdx)
	{
		u32										neededConstants = 0;
		PCRendererCacheInstance					cacheInstance = null;
		RHI::PRenderState						newRenderState = null;
		EShaderOptions							shaderOptions = static_cast<EShaderOptions>(drawCalls[dcIdx].m_ShaderOptions);
		bool									castShadows = false;

		//------------------------------------------------------
		// Retrieve renderer cache instance:
		//------------------------------------------------------
		{
			CRendererCacheInstance_UpdateThread		*srcRendererCache = drawCalls[dcIdx].m_RendererCacheInstance.Get();
			if (srcRendererCache != null)
			{
				castShadows = srcRendererCache->CastShadows();
				cacheInstance = srcRendererCache->RenderThread_GetCacheInstance();
				if (cacheInstance != null)
				{
					const PRendererCache	layout = cacheInstance->m_Cache;
					if (layout != null)
						newRenderState = layout->GetRenderState(shaderOptions, ParticlePass_Opaque, &neededConstants);
				}
			}
			else
				CLog::Log(PK_ERROR, "The renderer does not have a cache instance: some graphical resource creation must have failed");
		}
		if (newRenderState != null)
			shadow.AddBBox(drawCalls[dcIdx].m_BBox, CFloat4x4::IDENTITY, castShadows);
	}
}

//----------------------------------------------------------------------------

bool	CRHIParticleSceneRenderHelper::_CreateFinalRenderStates(	const TMemoryView<const RHI::SRenderTargetDesc> frameBufferLayout,
																	const RHI::PRenderPass &renderPass,
																	u32 subPassIdx,
																	const SSamplableRenderTarget &prevPassOut)
{
	if (!_CreateFinalDebugRenderStates(frameBufferLayout, renderPass, subPassIdx, subPassIdx + 1))
		return false;

	// Final basic copy
	if (!_CreateCopyRenderState(m_CopyColorRenderState, CopyCombination_Basic, frameBufferLayout, renderPass, subPassIdx + 1))
		return false;
	if (!_CreateCopyRenderState(m_CopyDepthRenderState, CopyCombination_Depth, frameBufferLayout, renderPass, subPassIdx + 1))
		return false;
	if (!_CreateCopyRenderState(m_CopyNormalRenderState, CopyCombination_Normal, frameBufferLayout, renderPass, subPassIdx + 1))
		return false;
	if (!_CreateCopyRenderState(m_CopyNormalUnpackedRenderState, CopyCombination_UnpackedNormal, frameBufferLayout, renderPass, subPassIdx + 1))
		return false;
	if (!_CreateCopyRenderState(m_CopySpecularRenderState, CopyCombination_Specular, frameBufferLayout, renderPass, subPassIdx + 1))
		return false;
	if (!_CreateCopyRenderState(m_CopyAlphaRenderState, CopyCombination_Alpha, frameBufferLayout, renderPass, subPassIdx + 1))
		return false;
	if (!_CreateCopyRenderState(m_CopyMulAddRenderState, CopyCombination_MulAdd, frameBufferLayout, m_FinalRenderPass, 2))
		return false;

	// Heat-map
	{
		m_OverdrawHeatmapRenderState = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("Overdraw Render State"));
		if (m_OverdrawHeatmapRenderState == null)
			return false;

		RHI::SRenderState		&renderState = m_OverdrawHeatmapRenderState->m_RenderState;
		renderState.m_PipelineState.m_DynamicScissor = true;
		renderState.m_PipelineState.m_DynamicViewport = true;

		if (!renderState.m_InputVertexBuffers.PushBack().Valid())
			return false;
		renderState.m_InputVertexBuffers.Last().m_Stride = sizeof(CFloat2);

		RHI::SConstantSetLayout		setLayout(RHI::FragmentShaderMask);
		setLayout.AddConstantsLayout(RHI::SConstantSamplerDesc("IntensityMap", RHI::SamplerTypeSingle));
		setLayout.AddConstantsLayout(RHI::SConstantSamplerDesc("OverdrawLUT", RHI::SamplerTypeSingle));

		FillEditorHeatmapOverdrawBindings(renderState.m_ShaderBindings, setLayout);

		CShaderLoader::SShadersPaths shadersPaths;
		shadersPaths.m_Vertex = FULL_SCREEN_QUAD_VERTEX_SHADER_PATH;
		shadersPaths.m_Fragment = OVERDRAW_HEATMAP_FRAGMENT_SHADER_PATH;

		if (m_ShaderLoader->LoadShader(renderState, shadersPaths, m_ApiManager) == false)
			return false;
		if (!m_ApiManager->BakeRenderState(m_OverdrawHeatmapRenderState, frameBufferLayout, renderPass, subPassIdx + 1))
			return false;

		m_OverdrawConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Overdraw Constant Set") ,setLayout);
		if (m_OverdrawConstantSet == null)
			return false;

		STextureKey	textureKey;
		textureKey.m_Path = OVERDRAW_HEATMAP_LUT_TEXTURE_PATH;
		CTextureManager::CResourceId textureId = CTextureManager::UpdateThread_GetResource(textureKey, SPrepareArg(null, null, null));
		if (!textureId.Valid())
			return false;
		m_HeatmapTexture = CTextureManager::RenderThread_ResolveResource(textureId, SCreateArg(m_ApiManager, m_ResourceManager));
		if (m_HeatmapTexture == null)
			return false;
		m_OverdrawConstantSet->SetConstants(m_DefaultSampler, prevPassOut.m_RenderTarget->GetTexture(), 0);
		m_OverdrawConstantSet->SetConstants(m_DefaultSampler, m_HeatmapTexture, 1);
		m_OverdrawConstantSet->UpdateConstantValues();
	}

	// Gizmo
	if (!m_GizmoDrawer.CreateRenderStates(m_ApiManager, *m_ShaderLoader, frameBufferLayout, renderPass, subPassIdx + 1))
		return false;
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIParticleSceneRenderHelper::_CreateTransparentBackdropRenderStates(TMemoryView<const RHI::SRenderTargetDesc> frameBufferLayout, const RHI::PRenderPass &renderPass, u32 subPassIdx)
{
	// Background texture copy:
	// ----------------------------------------------------------
	{
		if (m_ApiManager->ApiName() == RHI::GApi_OpenGL || m_ApiManager->ApiName() == RHI::GApi_OES)
		{
			if (!_CreateCopyRenderState(m_CopyBackgroundColorRenderState, CopyCombination_FlippedBasic, frameBufferLayout, renderPass, subPassIdx))
				return false;
		}
		else
		{
			if (!_CreateCopyRenderState(m_CopyBackgroundColorRenderState, CopyCombination_Basic, frameBufferLayout, renderPass, subPassIdx))
				return false;
		}

		m_BackgroundTextureConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Background Texture Constant Set"), m_DefaultSamplerConstLayout);

		if (m_BackgroundTextureConstantSet == null)
			return false;

		if (m_BackgroundTexture != null)
		{
			m_BackgroundTextureConstantSet->SetConstants(m_DefaultSampler, m_BackgroundTexture, 0);
			m_BackgroundTextureConstantSet->UpdateConstantValues();
		}
	}

	// Brush background render state
	// ----------------------------------------------------------
	{
		m_BrushBackgroundRenderState = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("Brush Background Render State"));
		if (m_BrushBackgroundRenderState == null)
			return false;

		RHI::SRenderState		&renderState = m_BrushBackgroundRenderState->m_RenderState;
		renderState.m_PipelineState.m_DynamicScissor = true;
		renderState.m_PipelineState.m_DynamicViewport = true;
		renderState.m_PipelineState.m_DepthWrite = false;
		renderState.m_PipelineState.m_DepthTest = RHI::Equal;

		if (!renderState.m_InputVertexBuffers.PushBack().Valid())
			return false;
		renderState.m_InputVertexBuffers.Last().m_Stride = sizeof(CFloat2);

		RHI::SConstantSetLayout brushInfoConstSetLayout;

		CreateBrushBackdropInfoConstantSetLayout(brushInfoConstSetLayout);
		FillBrushBackdropShaderBindings(renderState.m_ShaderBindings, brushInfoConstSetLayout, m_GBuffer.m_EnvironmentMapLayout);

		CShaderLoader::SShadersPaths shadersPaths;
		shadersPaths.m_Vertex = FULL_SCREEN_QUAD_VERTEX_SHADER_PATH;
		shadersPaths.m_Fragment = BRUSH_BACKDROP_FRAGMENT_SHADER_PATH;

		if (!m_ShaderLoader->LoadShader(renderState, shadersPaths, m_ApiManager) ||
			!m_ApiManager->BakeRenderState(m_BrushBackgroundRenderState, frameBufferLayout, renderPass, subPassIdx))
			return false;

		// We also create the brush backrop info buffer:
		m_BrushInfoConstSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Brush Info Constant Set"), brushInfoConstSetLayout);
		m_BrushInfoData = m_ApiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("Brush Info Buffer"), RHI::ConstantBuffer, sizeof(SBrushInfo));
		m_BrushInfoConstSet->SetConstants(m_BrushInfoData, 0);
		m_BrushInfoConstSet->UpdateConstantValues();
	}

	// Grid:
	// ----------------------------------------------------------
	{
		m_GridRenderState = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("Grid Render State"));

		if (m_GridRenderState == null)
			return false;

		m_GridRenderState->m_RenderState.m_PipelineState.m_DynamicScissor = true;
		m_GridRenderState->m_RenderState.m_PipelineState.m_DynamicViewport = true;
		m_GridRenderState->m_RenderState.m_PipelineState.m_Blending = false;
		m_GridRenderState->m_RenderState.m_PipelineState.m_DepthWrite = true;
		m_GridRenderState->m_RenderState.m_PipelineState.m_DepthTest = RHI::LessOrEqual;
		m_GridRenderState->m_RenderState.m_PipelineState.m_RasterizerMode = RHI::RasterizeSolid;
		m_GridRenderState->m_RenderState.m_PipelineState.m_DrawMode = RHI::DrawModeLine;

		FillEditorDebugDrawShaderBindings(m_GridRenderState->m_RenderState.m_ShaderBindings, false, false);

		if (!m_GridRenderState->m_RenderState.m_InputVertexBuffers.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat3))).Valid())
			return false;

		CShaderLoader::SShadersPaths	paths;
		paths.m_Vertex = DEBUG_DRAW_VERTEX_SHADER_PATH;
		paths.m_Fragment = DEBUG_DRAW_FRAGMENT_SHADER_PATH;

		if (!m_ShaderLoader->LoadShader(m_GridRenderState->m_RenderState, paths, m_ApiManager) ||
			!m_ApiManager->BakeRenderState(m_GridRenderState, frameBufferLayout, renderPass, subPassIdx))
			return false;
	}

	// Misc:
	// m_LinesRenderStateAdditive
	{
		m_LinesRenderStateAdditive = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("DebugDrawLinesAdditive Render State"));
		if (m_LinesRenderStateAdditive == null)
			return false;
		RHI::SRenderState	&renderState = m_LinesRenderStateAdditive->m_RenderState;

		renderState.m_PipelineState.m_DynamicViewport = true;
		renderState.m_PipelineState.m_DynamicScissor = true;
		renderState.m_PipelineState.m_DepthTest = RHI::NoTest;
		renderState.m_PipelineState.m_DepthWrite = false;
		renderState.m_PipelineState.m_Blending = true;
		renderState.m_PipelineState.m_ColorBlendingEquation = RHI::BlendAdd;
		renderState.m_PipelineState.m_ColorBlendingSrc = RHI::BlendOne;
		renderState.m_PipelineState.m_ColorBlendingDst = RHI::BlendOne;
		renderState.m_PipelineState.m_DrawMode = RHI::DrawModeLine;

		PK_ASSERT(renderState.m_InputVertexBuffers.Empty());
		if (!PK_VERIFY(renderState.m_InputVertexBuffers.Resize(2)))
			return false;
		renderState.m_InputVertexBuffers[0].m_Stride = sizeof(CFloat3);
		renderState.m_InputVertexBuffers[1].m_Stride = sizeof(CFloat4);

		PKSample::FillEditorDebugDrawShaderBindings(renderState.m_ShaderBindings, true, false);
		PKSample::CShaderLoader::SShadersPaths	paths;
		paths.m_Vertex = DEBUG_DRAW_VERTEX_SHADER_PATH;
		paths.m_Fragment = DEBUG_DRAW_FRAGMENT_SHADER_PATH;
		if (!m_ShaderLoader->LoadShader(m_LinesRenderStateAdditive->m_RenderState, paths, m_ApiManager) ||
			!m_ApiManager->BakeRenderState(m_LinesRenderStateAdditive, frameBufferLayout, renderPass, subPassIdx))
			return false;
	}

	// m_LinesRenderStateOpaque
	{
		m_LinesRenderStateOpaque = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("DebugDrawLinesOpaque Render State"));
		if (m_LinesRenderStateOpaque == null)
			return false;
		RHI::SRenderState	&renderState = m_LinesRenderStateOpaque->m_RenderState;

		renderState.m_PipelineState.m_DynamicViewport = true;
		renderState.m_PipelineState.m_DynamicScissor = true;
		renderState.m_PipelineState.m_DepthTest = RHI::LessOrEqual;
		renderState.m_PipelineState.m_DepthWrite = true;
		renderState.m_PipelineState.m_Blending = false;
		renderState.m_PipelineState.m_DrawMode = RHI::DrawModeLine;

		PK_ASSERT(renderState.m_InputVertexBuffers.Empty());
		if (!PK_VERIFY(renderState.m_InputVertexBuffers.Resize(2)))
			return false;
		renderState.m_InputVertexBuffers[0].m_Stride = sizeof(CFloat3);
		renderState.m_InputVertexBuffers[1].m_Stride = sizeof(CFloat4);

		PKSample::FillEditorDebugDrawShaderBindings(renderState.m_ShaderBindings, true, false);
		PKSample::CShaderLoader::SShadersPaths	paths;
		paths.m_Vertex = DEBUG_DRAW_VERTEX_SHADER_PATH;
		paths.m_Fragment = DEBUG_DRAW_FRAGMENT_SHADER_PATH;
		if (!m_ShaderLoader->LoadShader(m_LinesRenderStateOpaque->m_RenderState, paths, m_ApiManager) ||
			!m_ApiManager->BakeRenderState(m_LinesRenderStateOpaque, frameBufferLayout, renderPass, subPassIdx))
			return false;
	}

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIParticleSceneRenderHelper::_CreateOpaqueBackdropRenderStates(TMemoryView<const RHI::SRenderTargetDesc> frameBufferLayout, const RHI::PRenderPass &renderPass, u32 subPassIdx)
{
	// Mesh:
	// ----------------------------------------------------------
	// We need the constant set layout to create the render state:
	CShaderLoader::SShadersPaths	shadersPaths;
	shadersPaths.m_Vertex = GBUFFER_VERTEX_SHADER_PATH;
	shadersPaths.m_Fragment = GBUFFER_FRAGMENT_SHADER_PATH;

	// This constant set doesn't change wether vertex colors are used or not
	CreateGBufferConstSetLayouts(GBufferCombination_Diffuse_RoughMetal_Normal, m_MeshConstSetLayout);

	m_MeshRenderState = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("Mesh Backdrop Render State"));
	m_MeshRenderStateVertexColor = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("Mesh Backdrop Vertex Color Render State"));

	if (m_MeshRenderState == null || m_MeshRenderStateVertexColor == null)
		return false;

	// Pipeline state
	{
		const ECoordinateFrame	drawFrame = m_CoordinateFrame;
		const ECoordinateFrame	meshFrame = m_MeshBackdrop.m_MeshBatchCoordinateFrame;
		const bool				drawRightHanded = CCoordinateFrame::IsRightHanded(drawFrame);
		const bool				flippedHandedness = CCoordinateFrame::IsRightHanded(meshFrame) != drawRightHanded;
		
		m_MeshRenderState->m_RenderState.m_PipelineState.m_DynamicScissor = true;
		m_MeshRenderState->m_RenderState.m_PipelineState.m_DynamicViewport = true;
		m_MeshRenderState->m_RenderState.m_PipelineState.m_DepthWrite = true;
		m_MeshRenderState->m_RenderState.m_PipelineState.m_DepthTest = RHI::Less;
		m_MeshRenderState->m_RenderState.m_PipelineState.m_CullMode = RHI::CullBackFaces;
		m_MeshRenderState->m_RenderState.m_PipelineState.m_PolyOrder = (drawRightHanded ^ flippedHandedness) ? RHI::FrontFaceCounterClockWise : RHI::FrontFaceClockWise;

		if (!m_MeshRenderState->m_RenderState.m_InputVertexBuffers.Resize(4))
			return false;
		
		m_MeshRenderState->m_RenderState.m_InputVertexBuffers[0].m_Stride = sizeof(CFloat3);
		m_MeshRenderState->m_RenderState.m_InputVertexBuffers[1].m_Stride = sizeof(CFloat3);
		m_MeshRenderState->m_RenderState.m_InputVertexBuffers[2].m_Stride = sizeof(CFloat4);
		m_MeshRenderState->m_RenderState.m_InputVertexBuffers[3].m_Stride = sizeof(CFloat2);

		FillGBufferShaderBindings(m_MeshRenderState->m_RenderState.m_ShaderBindings, m_SceneInfoConstantSetLayout, m_MeshConstSetLayout, false, true);

		m_MeshRenderState->m_RenderState.m_ShaderBindings.m_InputAttributes[0].m_BufferIdx = 0;
		m_MeshRenderState->m_RenderState.m_ShaderBindings.m_InputAttributes[1].m_BufferIdx = 1;
		m_MeshRenderState->m_RenderState.m_ShaderBindings.m_InputAttributes[2].m_BufferIdx = 2;
		m_MeshRenderState->m_RenderState.m_ShaderBindings.m_InputAttributes[3].m_BufferIdx = 3;

		if (!m_ShaderLoader->LoadShader(m_MeshRenderState->m_RenderState, shadersPaths, m_ApiManager))
			return false;
		if (!m_ApiManager->BakeRenderState(m_MeshRenderState, frameBufferLayout, renderPass, subPassIdx))
			return false;
	}

	// Pipeline state vertex color
	{
		m_MeshRenderStateVertexColor->m_RenderState = m_MeshRenderState->m_RenderState;

		if (!m_MeshRenderStateVertexColor->m_RenderState.m_InputVertexBuffers.Resize(5))
			return false;

		m_MeshRenderStateVertexColor->m_RenderState.m_InputVertexBuffers[0].m_Stride = sizeof(CFloat3);
		m_MeshRenderStateVertexColor->m_RenderState.m_InputVertexBuffers[1].m_Stride = sizeof(CFloat3);
		m_MeshRenderStateVertexColor->m_RenderState.m_InputVertexBuffers[2].m_Stride = sizeof(CFloat4);
		m_MeshRenderStateVertexColor->m_RenderState.m_InputVertexBuffers[3].m_Stride = sizeof(CFloat2);
		m_MeshRenderStateVertexColor->m_RenderState.m_InputVertexBuffers[4].m_Stride = sizeof(CFloat4);

		FillGBufferShaderBindings(m_MeshRenderStateVertexColor->m_RenderState.m_ShaderBindings, m_SceneInfoConstantSetLayout, m_MeshConstSetLayout, true, true);

		m_MeshRenderStateVertexColor->m_RenderState.m_ShaderBindings.m_InputAttributes[0].m_BufferIdx = 0;
		m_MeshRenderStateVertexColor->m_RenderState.m_ShaderBindings.m_InputAttributes[1].m_BufferIdx = 1;
		m_MeshRenderStateVertexColor->m_RenderState.m_ShaderBindings.m_InputAttributes[2].m_BufferIdx = 2;
		m_MeshRenderStateVertexColor->m_RenderState.m_ShaderBindings.m_InputAttributes[3].m_BufferIdx = 3;
		m_MeshRenderStateVertexColor->m_RenderState.m_ShaderBindings.m_InputAttributes[4].m_BufferIdx = 4;

		if (!m_ShaderLoader->LoadShader(m_MeshRenderStateVertexColor->m_RenderState, shadersPaths, m_ApiManager))
			return false;
		if (!m_ApiManager->BakeRenderState(m_MeshRenderStateVertexColor, frameBufferLayout, renderPass, subPassIdx))
			return false;
	}

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIParticleSceneRenderHelper::_CreateCopyRenderState(	RHI::PRenderState &renderState,
																ECopyCombination combination,
																const TMemoryView<const RHI::SRenderTargetDesc> &frameBufferLayout,
																const RHI::PRenderPass &renderPass,
																u32 subPassIdx)
{
	renderState = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("Copy Render State"));
	if (renderState == null)
		return false;
	RHI::SRenderState		&debugCopyRenderState = renderState->m_RenderState;
	debugCopyRenderState.m_PipelineState.m_DynamicScissor = true;
	debugCopyRenderState.m_PipelineState.m_DynamicViewport = true;

	if (!debugCopyRenderState.m_InputVertexBuffers.PushBack().Valid())
		return false;
	debugCopyRenderState.m_InputVertexBuffers.Last().m_Stride = sizeof(CFloat2);

	FillCopyShaderBindings(combination, debugCopyRenderState.m_ShaderBindings, m_DefaultSamplerConstLayout);

	CShaderLoader::SShadersPaths shadersPaths;
	shadersPaths.m_Vertex = FULL_SCREEN_QUAD_VERTEX_SHADER_PATH;
	shadersPaths.m_Fragment = COPY_FRAGMENT_SHADER_PATH;

	if (m_ShaderLoader->LoadShader(debugCopyRenderState, shadersPaths, m_ApiManager) == false)
		return false;
	if (!m_ApiManager->BakeRenderState(renderState, frameBufferLayout, renderPass, subPassIdx))
		return false;
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIParticleSceneRenderHelper::_CreateGridGeometry(float size, u32 subdiv, u32 secondSubdiv, const CFloat4x4 &transforms, ECoordinateFrame coordinateFrame)
{
	bool	success = true;

	const u32	gridVtxCount = 4 + 4 * subdiv;
	const u32	gridSubdivVtxCount = 4 * subdiv * secondSubdiv;
	m_GridVertices = m_ApiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("Grid Vertex Buffer"), RHI::VertexBuffer, (gridVtxCount + gridSubdivVtxCount) * sizeof(CFloat3));

	m_GridIdxCount = 8 + 4 * subdiv;
	m_GridSubdivIdxCount = 4 * subdiv * secondSubdiv;

	// FIXME(Julien): Will break down as soon as 'subdiv + subdiv * secondSubdiv' >= 16383
	m_GridIndices = m_ApiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("Grid Index Buffer"), RHI::IndexBuffer, (m_GridIdxCount + m_GridSubdivIdxCount) * sizeof(u16));

	if (m_GridVertices == null || m_GridIndices == null)
		return false;

	// We map the buffers:
	CFloat3		*vertices = reinterpret_cast<CFloat3*>(m_ApiManager->MapCpuView(m_GridVertices));
	u16			*indices = reinterpret_cast<u16*>(m_ApiManager->MapCpuView(m_GridIndices));

	if (vertices == null || indices == null)
		return false;

	const float		halfSize = size / 2.0f;

	// We start by creating the frame of the grid:
	const CFloat3	&halfSizeForward = CCoordinateFrame::Axis(Axis_Forward, coordinateFrame) * halfSize;
	const CFloat3	&halfSizeSide = CCoordinateFrame::Axis(Axis_Right, coordinateFrame) * halfSize;

	vertices[0] = transforms.TransformVector(-halfSizeSide - halfSizeForward);
	vertices[1] = transforms.TransformVector(-halfSizeSide + halfSizeForward);
	vertices[2] = transforms.TransformVector( halfSizeSide + halfSizeForward);
	vertices[3] = transforms.TransformVector( halfSizeSide - halfSizeForward);

	// Quad:
	indices[0] = 0;
	indices[1] = 1;
	indices[2] = 1;
	indices[3] = 2;
	indices[4] = 2;
	indices[5] = 3;
	indices[6] = 3;
	indices[7] = 0;

	u32			currentVertexIdx = 4;
	u32			currentIndexIdx = 8;

	// We divide each side by the subdiv count:
	for (u32 i = 0; i < subdiv; ++i)
	{
		float	currentLerpRatio = (1.0f / static_cast<float>(subdiv)) * (i + 1);

		// Then we lerp between the 4 sides:
		// Horizontal side = 0 - 3 and 1 - 2
		vertices[currentVertexIdx + 0] = vertices[0].Lerp(vertices[3], currentLerpRatio);
		vertices[currentVertexIdx + 1] = vertices[1].Lerp(vertices[2], currentLerpRatio);
		// Link those 2 points:
		indices[currentIndexIdx + 0] = currentVertexIdx + 0;
		indices[currentIndexIdx + 1] = currentVertexIdx + 1;
		// Vertical side = 1 - 0 and 2 - 3
		vertices[currentVertexIdx + 2] = vertices[1].Lerp(vertices[0], currentLerpRatio);
		vertices[currentVertexIdx + 3] = vertices[2].Lerp(vertices[3], currentLerpRatio);
		// Link those 2 points:
		indices[currentIndexIdx + 2] = currentVertexIdx + 2;
		indices[currentIndexIdx + 3] = currentVertexIdx + 3;
		// Increment
		currentVertexIdx += 4;
		currentIndexIdx += 4;
	}

	// Second subdivision:
	CFloat3		previousSubdiv[4] =
	{
		vertices[0],
		vertices[1],
		vertices[1],
		vertices[2],
	};

	for (u32 i = 0; i < subdiv; ++i)
	{
		CFloat3		currentSubdiv[4];
		float		currentLerpRatio = (1.0f / static_cast<float>(subdiv)) * (i + 1);

		// Then we lerp between the 4 sides:
		// Horizontal side = 0 - 3 and 1 - 2
		currentSubdiv[0] = vertices[0].Lerp(vertices[3], currentLerpRatio);
		currentSubdiv[1] = vertices[1].Lerp(vertices[2], currentLerpRatio);
		// Vertical side = 1 - 0 and 2 - 3
		currentSubdiv[2] = vertices[1].Lerp(vertices[0], currentLerpRatio);
		currentSubdiv[3] = vertices[2].Lerp(vertices[3], currentLerpRatio);

		// Create the second subdivision:
		for (u32 j = 0; j < secondSubdiv; ++j)
		{
			float		secondaryLerpRatio = (1.0f / static_cast<float>(secondSubdiv)) * (j + 1);
			vertices[currentVertexIdx + 0] = previousSubdiv[0].Lerp(currentSubdiv[0], secondaryLerpRatio);
			vertices[currentVertexIdx + 1] = previousSubdiv[1].Lerp(currentSubdiv[1], secondaryLerpRatio);
			indices[currentIndexIdx + 0] = currentVertexIdx + 0;
			indices[currentIndexIdx + 1] = currentVertexIdx + 1;
			vertices[currentVertexIdx + 2] = previousSubdiv[2].Lerp(currentSubdiv[2], secondaryLerpRatio);
			vertices[currentVertexIdx + 3] = previousSubdiv[3].Lerp(currentSubdiv[3], secondaryLerpRatio);
			indices[currentIndexIdx + 2] = currentVertexIdx + 2;
			indices[currentIndexIdx + 3] = currentVertexIdx + 3;
			// Increment
			currentVertexIdx += 4;
			currentIndexIdx += 4;
		}
		for (u32 j = 0; j < 4; ++j)
		{
			previousSubdiv[j] = currentSubdiv[j];
		}
	}

	PK_ASSERT(m_GridIdxCount + m_GridSubdivIdxCount == currentIndexIdx);
	PK_ASSERT(gridVtxCount + gridSubdivVtxCount == currentVertexIdx);

	success &= m_ApiManager->UnmapCpuView(m_GridVertices);
	success &= m_ApiManager->UnmapCpuView(m_GridIndices);
	return success;
}

//----------------------------------------------------------------------------

void	CRHIParticleSceneRenderHelper::_DrawDebugLines(const RHI::PCommandBuffer &cmdBuff, const TMemoryView<const CFloat3> &positions, const TMemoryView<const CFloat4> &colors, SLinePointsColorBuffer &buffer, RHI::PRenderState &renderState)
{
	if (!PK_VERIFY(positions.Count() == colors.Count()) ||
		positions.Empty())
		return;

	const u32	bufferBytePositionsCount = positions.CoveredBytes();
	const u32	bufferByteColorCount = colors.CoveredBytes();

	if (buffer.m_LinesPointsBuffer == null || buffer.m_LinesPointsBuffer->GetByteSize() < bufferBytePositionsCount)
		buffer.m_LinesPointsBuffer = m_ApiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("Lines Points Vertex Buffer"), RHI::VertexBuffer, bufferBytePositionsCount);

	if (buffer.m_LinesColorBuffer == null || buffer.m_LinesColorBuffer->GetByteSize() < bufferByteColorCount)
		buffer.m_LinesColorBuffer = m_ApiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("Lines Colors Vertex Buffer"), RHI::VertexBuffer, bufferByteColorCount);

	if (buffer.m_LinesPointsBuffer != null && buffer.m_LinesColorBuffer != null)
	{
		{
			void	*mappedPtr = m_ApiManager->MapCpuView(buffer.m_LinesPointsBuffer);
			if (PK_VERIFY(mappedPtr != null))
			{
				Mem::Copy(mappedPtr, positions.Data(), bufferBytePositionsCount);
				m_ApiManager->UnmapCpuView(buffer.m_LinesPointsBuffer);
			}
		}
		{
			void	*mappedPtr = m_ApiManager->MapCpuView(buffer.m_LinesColorBuffer);
			if (PK_VERIFY(mappedPtr != null))
			{
				Mem::Copy(mappedPtr, colors.Data(), bufferByteColorCount);
				m_ApiManager->UnmapCpuView(buffer.m_LinesColorBuffer);
			}
		}

		cmdBuff->BindRenderState(renderState);
		cmdBuff->BindConstantSets(TMemoryView<RHI::PConstantSet>(m_SceneInfoConstantSet));
		cmdBuff->BindVertexBuffers(TMemoryView<RHI::PGpuBuffer>(&buffer.m_LinesPointsBuffer, 2));

		cmdBuff->Draw(0, positions.Count());
	}
}

//----------------------------------------------------------------------------

bool	CRHIParticleSceneRenderHelper::_AddRenderPassesToGraphicResourceManagerIFN()
{
	if (SPassDescription::s_PassDescriptions.Empty())
	{
		const bool					hasDifferentRenderPasses = (m_InitializedRP & InitRP_Bloom) != 0;
		const RHI::PFrameBuffer		&beforeBloomFrameBuffer = hasDifferentRenderPasses ? m_BeforeBloomFrameBuffer : m_FinalFrameBuffers.First();
		const RHI::PRenderPass		&beforeBloomRenderPass = hasDifferentRenderPasses ? m_BeforeBloomRenderPass : m_FinalRenderPass;

		SPassDescription::s_PassDescriptions.Resize(__MaxParticlePass);
		if ((m_InitializedRP & InitRP_GBuffer) != 0)
		{
			// Default Opaque
			SPassDescription::s_PassDescriptions[ParticlePass_Opaque].m_FrameBufferLayout = beforeBloomFrameBuffer->GetLayout();
			SPassDescription::s_PassDescriptions[ParticlePass_Opaque].m_RenderPass = beforeBloomRenderPass;
			SPassDescription::s_PassDescriptions[ParticlePass_Opaque].m_SubPassIdx = m_GBuffer.m_GBufferSubPassIdx;
			// Default Opaque Shadow
			SPassDescription::s_PassDescriptions[ParticlePass_OpaqueShadow].m_FrameBufferLayout = m_Lights.m_DirectionalShadows.GetFrameBufferLayout();
			SPassDescription::s_PassDescriptions[ParticlePass_OpaqueShadow].m_RenderPass = m_Lights.m_DirectionalShadows.GetRenderPass();
			SPassDescription::s_PassDescriptions[ParticlePass_OpaqueShadow].m_SubPassIdx = m_Lights.m_DirectionalShadows.GetShadowSubpassIdx();
			// Default Decals
			SPassDescription::s_PassDescriptions[ParticlePass_Decal].m_FrameBufferLayout = beforeBloomFrameBuffer->GetLayout();
			SPassDescription::s_PassDescriptions[ParticlePass_Decal].m_RenderPass = m_BeforeBloomRenderPass;
			SPassDescription::s_PassDescriptions[ParticlePass_Decal].m_SubPassIdx = m_GBuffer.m_DecalSubPassIdx;
			// Pass Lighting
			SPassDescription::s_PassDescriptions[ParticlePass_Lighting].m_FrameBufferLayout = beforeBloomFrameBuffer->GetLayout();
			SPassDescription::s_PassDescriptions[ParticlePass_Lighting].m_RenderPass = beforeBloomRenderPass;
			SPassDescription::s_PassDescriptions[ParticlePass_Lighting].m_SubPassIdx = m_GBuffer.m_LightingSubPassIdx;
			// Pass Additive
			SPassDescription::s_PassDescriptions[ParticlePass_Transparent].m_FrameBufferLayout = beforeBloomFrameBuffer->GetLayout();
			SPassDescription::s_PassDescriptions[ParticlePass_Transparent].m_RenderPass = beforeBloomRenderPass;
			SPassDescription::s_PassDescriptions[ParticlePass_Transparent].m_SubPassIdx = m_GBuffer.m_MergingSubPassIdx;
		}
		else
		{
			// Default Opaque
			SPassDescription::s_PassDescriptions[ParticlePass_Opaque].m_FrameBufferLayout.Clear();
			SPassDescription::s_PassDescriptions[ParticlePass_Opaque].m_RenderPass = null;
			SPassDescription::s_PassDescriptions[ParticlePass_Opaque].m_SubPassIdx.Clear();
			// Default Decals
			SPassDescription::s_PassDescriptions[ParticlePass_Decal].m_FrameBufferLayout.Clear();
			SPassDescription::s_PassDescriptions[ParticlePass_Decal].m_RenderPass = null;
			SPassDescription::s_PassDescriptions[ParticlePass_Decal].m_SubPassIdx.Clear();
			// Pass Lighting
			SPassDescription::s_PassDescriptions[ParticlePass_Lighting].m_FrameBufferLayout.Clear();
			SPassDescription::s_PassDescriptions[ParticlePass_Lighting].m_RenderPass = null;
			SPassDescription::s_PassDescriptions[ParticlePass_Lighting].m_SubPassIdx.Clear();
			// Pass Additive
			SPassDescription::s_PassDescriptions[ParticlePass_Transparent].m_FrameBufferLayout = beforeBloomFrameBuffer->GetLayout();
			SPassDescription::s_PassDescriptions[ParticlePass_Transparent].m_RenderPass = beforeBloomRenderPass;
			SPassDescription::s_PassDescriptions[ParticlePass_Transparent].m_SubPassIdx = m_BasicRenderingSubPassIdx;
		}

		if ((m_InitializedRP & InitRP_Distortion) != 0)
		{
			// Pass Distortion
			SPassDescription::s_PassDescriptions[ParticlePass_Distortion].m_FrameBufferLayout = beforeBloomFrameBuffer->GetLayout();
			SPassDescription::s_PassDescriptions[ParticlePass_Distortion].m_RenderPass = beforeBloomRenderPass;
			SPassDescription::s_PassDescriptions[ParticlePass_Distortion].m_SubPassIdx = m_Distortion.m_DistoSubPassIdx;

			// After distortion:
			SPassDescription::s_PassDescriptions[ParticlePass_TransparentPostDisto].m_FrameBufferLayout = beforeBloomFrameBuffer->GetLayout();
			SPassDescription::s_PassDescriptions[ParticlePass_TransparentPostDisto].m_RenderPass = beforeBloomRenderPass;
			SPassDescription::s_PassDescriptions[ParticlePass_TransparentPostDisto].m_SubPassIdx = m_Distortion.m_PostDistortionSubPassIdx;

			// Tinting:
			SPassDescription::s_PassDescriptions[ParticlePass_Tint].m_FrameBufferLayout = beforeBloomFrameBuffer->GetLayout();
			SPassDescription::s_PassDescriptions[ParticlePass_Tint].m_RenderPass = beforeBloomRenderPass;
			SPassDescription::s_PassDescriptions[ParticlePass_Tint].m_SubPassIdx = m_Distortion.m_PostDistortionSubPassIdx;
		}
		else
		{
			// Pass Distortion
			SPassDescription::s_PassDescriptions[ParticlePass_Distortion].m_FrameBufferLayout.Clear();
			SPassDescription::s_PassDescriptions[ParticlePass_Distortion].m_RenderPass = null;
			SPassDescription::s_PassDescriptions[ParticlePass_Distortion].m_SubPassIdx.Clear();

			SPassDescription::s_PassDescriptions[ParticlePass_TransparentPostDisto].m_FrameBufferLayout.Clear();
			SPassDescription::s_PassDescriptions[ParticlePass_TransparentPostDisto].m_RenderPass = null;
			SPassDescription::s_PassDescriptions[ParticlePass_TransparentPostDisto].m_SubPassIdx.Clear();

			SPassDescription::s_PassDescriptions[ParticlePass_Tint].m_FrameBufferLayout.Clear();
			SPassDescription::s_PassDescriptions[ParticlePass_Tint].m_RenderPass = null;
			SPassDescription::s_PassDescriptions[ParticlePass_Tint].m_SubPassIdx.Clear();
		}

		if ((m_InitializedRP & InitRP_Debug) != 0)
		{
			// Pass Debug
			SPassDescription::s_PassDescriptions[ParticlePass_Debug].m_FrameBufferLayout = m_FinalFrameBuffers.First()->GetLayout();
			SPassDescription::s_PassDescriptions[ParticlePass_Debug].m_RenderPass = m_FinalRenderPass;
			SPassDescription::s_PassDescriptions[ParticlePass_Debug].m_SubPassIdx = m_ParticleDebugSubpassIdx;
			// Pass Compositing / UI
			SPassDescription::s_PassDescriptions[ParticlePass_Compositing].m_FrameBufferLayout = m_FinalFrameBuffers.First()->GetLayout();
			SPassDescription::s_PassDescriptions[ParticlePass_Compositing].m_RenderPass = m_FinalRenderPass;
			SPassDescription::s_PassDescriptions[ParticlePass_Compositing].m_SubPassIdx = m_ParticleDebugSubpassIdx + 1;
		}
		else
		{
			// Pass Debug
			SPassDescription::s_PassDescriptions[ParticlePass_Debug].m_FrameBufferLayout.Clear();
			SPassDescription::s_PassDescriptions[ParticlePass_Debug].m_RenderPass = null;
			SPassDescription::s_PassDescriptions[ParticlePass_Debug].m_SubPassIdx.Clear();
			// Pass Compositing / UI
			SPassDescription::s_PassDescriptions[ParticlePass_Compositing].m_FrameBufferLayout.Clear();
			SPassDescription::s_PassDescriptions[ParticlePass_Compositing].m_RenderPass = null;
			SPassDescription::s_PassDescriptions[ParticlePass_Compositing].m_SubPassIdx.Clear();
		}
	}

	return true;
}

//----------------------------------------------------------------------------

void	CRHIParticleSceneRenderHelper::_FillGBufferInOut(CGBuffer::SInOutRenderTargets &inOut)
{
	inOut.m_SamplableDiffuseRtIdx = GBufferRTIdx(SPassDescription::GBufferRT_Diffuse);
	inOut.m_SamplableDiffuseRt = &GBufferRT(SPassDescription::GBufferRT_Diffuse);
	inOut.m_SamplableDepthRtIdx = GBufferRTIdx(SPassDescription::GBufferRT_Depth);
	inOut.m_SamplableDepthRt = &GBufferRT(SPassDescription::GBufferRT_Depth);
	inOut.m_OpaqueRTsIdx = m_GBufferRTsIdx;
	inOut.m_OpaqueRTs = m_GBufferRTs;
	if (_IsLastRenderPass(InitRP_GBuffer))
	{
		// We can directly merge in the swap chain render targets
		inOut.m_OutputRtIdx = 0;
	}
}

//----------------------------------------------------------------------------

void	CRHIParticleSceneRenderHelper::_FillDistortionInOut(CPostFxDistortion::SInOutRenderTargets &inOut)
{
	if ((m_InitializedRP & InitRP_GBuffer) != 0)
	{
		inOut.m_ToDistordRtIdx = m_GBuffer.m_MergeBufferIndex;
		inOut.m_ToDistordRt = &m_GBuffer.m_Merge;
		inOut.m_DepthRtIdx = m_GBuffer.m_DepthBufferIndex;
		inOut.m_ParticleInputs = m_GBufferRTsIdx;
	}
	else
	{
		inOut.m_ToDistordRtIdx = m_BasicRenderingRTIdx;
		inOut.m_ToDistordRt = &m_BasicRenderingRT;
		// No depth buffre because no opaque geom + no particle inputs because no GBuffers to sample
	}
	if (_IsLastRenderPass(InitRP_Distortion))
	{
		// We can directly output the distorded buffer in the swap chain render target
		inOut.m_OutputRtIdx = 0;
	}
}

//----------------------------------------------------------------------------

void	CRHIParticleSceneRenderHelper::_FillBloomInOut(CPostFxBloom::SInOutRenderTargets &inOut)
{
	if ((m_InitializedRP & InitRP_Distortion) != 0)
	{
		inOut.m_InputRenderTarget = &m_Distortion.m_OutputRenderTarget;
	}
	else if ((m_InitializedRP & InitRP_GBuffer) != 0)
	{
		inOut.m_InputRenderTarget = &m_GBuffer.m_Merge;
	}
	else
	{
		inOut.m_InputRenderTarget = &m_BasicRenderingRT;
	}
	if (_IsLastRenderPass(InitRP_Bloom))
	{
		inOut.m_OutputRenderTargets = m_FinalRenderTargets;
	}
	else
	{
		inOut.m_OutputRenderTargets = TMemoryView<const RHI::PRenderTarget>(inOut.m_InputRenderTarget->m_RenderTarget);
	}
}

//----------------------------------------------------------------------------

void	CRHIParticleSceneRenderHelper::_FillTonemappingInOut(CPostFxToneMapping::SInOutRenderTargets &inOut)
{
	const bool	isDifferentRenderPass = (m_InitializedRP & InitRP_Bloom) != 0;

	if ((m_InitializedRP & InitRP_Distortion) != 0)
	{
		if (!isDifferentRenderPass)
			inOut.m_ToTonemapRtIdx = m_Distortion.m_OutputBufferIdx;
		inOut.m_ToTonemapRt = &m_Distortion.m_OutputRenderTarget;
	}
	else if ((m_InitializedRP & InitRP_GBuffer) != 0)
	{
		if (!isDifferentRenderPass)
			inOut.m_ToTonemapRtIdx = m_GBuffer.m_MergeBufferIndex;
		inOut.m_ToTonemapRt = &m_GBuffer.m_Merge;
	}
	else
	{
		if (!isDifferentRenderPass)
			inOut.m_ToTonemapRtIdx = m_BasicRenderingRTIdx;
		inOut.m_ToTonemapRt = &m_BasicRenderingRT;
	}
	if (_IsLastRenderPass(InitRP_ToneMapping))
	{
		// We can directly output the tonemapped buffer in the swap chain render target
		inOut.m_OutputRtIdx = 0;
	}
}

//----------------------------------------------------------------------------

void	CRHIParticleSceneRenderHelper::_FillColorRemapInOut(CPostFxColorRemap::SInOutRenderTargets &inOut)
{
	if ((m_InitializedRP & InitRP_ToneMapping) != 0)
	{
		inOut.m_InputRenderTargetIdx = m_ToneMapping.m_OutputRenderTargetIdx;
		inOut.m_InputRenderTarget = &m_ToneMapping.m_OutputRenderTarget;
	}
	else if ((m_InitializedRP & InitRP_Distortion) != 0)
	{
		inOut.m_InputRenderTargetIdx = m_Distortion.m_DistoBufferIdx;
		inOut.m_InputRenderTarget = &m_Distortion.m_OutputRenderTarget;
	}
	else if ((m_InitializedRP & InitRP_GBuffer) != 0)
	{
		inOut.m_InputRenderTargetIdx = m_GBuffer.m_MergeBufferIndex;;
		inOut.m_InputRenderTarget = &m_GBuffer.m_Merge;
	}
	else
	{
		inOut.m_InputRenderTargetIdx = m_BasicRenderingRTIdx;
		inOut.m_InputRenderTarget = &m_BasicRenderingRT;
	}

	if (_IsLastRenderPass(InitRP_ColorRemap))
	{
		// We can directly output the color remapped buffer in the swap chain render target
		inOut.m_OutputRenderTargetIdx = 0;
	}
}

//----------------------------------------------------------------------------

void	CRHIParticleSceneRenderHelper::_FillDebugInOut(SSamplableRenderTarget &prevPassOut, CGuid &prevPassOutIdx)
{
	const bool	isDifferentRenderPass = (m_InitializedRP & InitRP_Bloom) != 0;

	if ((m_InitializedRP & InitRP_FXAA) != 0)
	{
		prevPassOutIdx = m_FXAA.m_OutputRenderTargetIdx;
		prevPassOut = m_FXAA.m_OutputRenderTarget;
	}
	else if ((m_InitializedRP & InitRP_ColorRemap) != 0)
	{
		prevPassOutIdx = m_ColorRemap.m_OutputRenderTargetIdx;
		prevPassOut = m_ColorRemap.m_OutputRenderTarget;
	}
	else if ((m_InitializedRP & InitRP_ToneMapping) != 0)
	{
		prevPassOutIdx = m_ToneMapping.m_OutputRenderTargetIdx;
		prevPassOut = m_ToneMapping.m_OutputRenderTarget;
	}
	else if ((m_InitializedRP & InitRP_Distortion) != 0)
	{
		if (!isDifferentRenderPass)
			prevPassOutIdx = m_Distortion.m_OutputBufferIdx;
		prevPassOut = m_Distortion.m_OutputRenderTarget;
	}
	else if ((m_InitializedRP & InitRP_GBuffer) != 0)
	{
		if (!isDifferentRenderPass)
			prevPassOutIdx = m_GBuffer.m_MergeBufferIndex;
		prevPassOut = m_GBuffer.m_Merge;
	}
	else
	{
		if (!isDifferentRenderPass)
			prevPassOutIdx = m_BasicRenderingRTIdx;
		prevPassOut = m_BasicRenderingRT;
	}
}

//----------------------------------------------------------------------------

void	CRHIParticleSceneRenderHelper::_FillFXAAInOut(CPostFxFXAA::SInOutRenderTargets &inOut)
{
	const bool	isDifferentRenderPass = (m_InitializedRP & InitRP_FXAA) != 0;

	if ((m_InitializedRP & InitRP_ColorRemap) != 0)
	{
		inOut.m_InputRtIdx = m_ColorRemap.m_OutputRenderTargetIdx;
		inOut.m_InputRt = &m_ColorRemap.m_OutputRenderTarget;
	}
	else if ((m_InitializedRP & InitRP_ToneMapping) != 0)
	{
		inOut.m_InputRtIdx = m_ToneMapping.m_OutputRenderTargetIdx;
		inOut.m_InputRt = &m_ToneMapping.m_OutputRenderTarget;
	}
	else if ((m_InitializedRP & InitRP_Distortion) != 0)
	{
		if (!isDifferentRenderPass)
			inOut.m_InputRtIdx = m_Distortion.m_OutputBufferIdx;
		inOut.m_InputRt = &m_Distortion.m_OutputRenderTarget;
	}
	else if ((m_InitializedRP & InitRP_GBuffer) != 0)
	{
		if (!isDifferentRenderPass)
			inOut.m_InputRtIdx = m_GBuffer.m_MergeBufferIndex;
		inOut.m_InputRt = &m_GBuffer.m_Merge;
	}
	else
	{
		if (!isDifferentRenderPass)
			inOut.m_InputRtIdx = m_BasicRenderingRTIdx;
		inOut.m_InputRt = &m_BasicRenderingRT;
	}
	if (_IsLastRenderPass(InitRP_FXAA))
	{
		inOut.m_OutputRtIdx = 0;
	}
}

//----------------------------------------------------------------------------

bool	CRHIParticleSceneRenderHelper::_IsLastRenderPass(EInitRenderPasses renderPass) const
{
	const u32	bitIdx = IntegerTools::Log2(static_cast<u32>(renderPass));
	const u32	nextRenderPasses = (~0U) << (bitIdx + 1);
	return (m_InitializedRP & nextRenderPasses) == 0;
}

//----------------------------------------------------------------------------

bool	CRHIParticleSceneRenderHelper::_EndRenderPass(EInitRenderPasses renderPass, const RHI::PCommandBuffer &cmdBuff) const
{
	if (_IsLastRenderPass(renderPass))
	{
		cmdBuff->EndRenderPass();
		return true;
	}
	return false;
}

//----------------------------------------------------------------------------

RHI::PRenderTarget	CRHIParticleSceneRenderHelper::_GetRenderTarget(ERenderTargetDebug target)
{
	RHI::PRenderTarget rt = null;
	switch (target)
	{
	case RenderTargetDebug_Diffuse:
		rt = GBufferRT(SPassDescription::GBufferRT_Diffuse).m_RenderTarget;
		break;
	case RenderTargetDebug_Depth:
		rt = GBufferRT(SPassDescription::GBufferRT_Depth).m_RenderTarget;
		break;
	case RenderTargetDebug_Normal:
		rt = m_GBuffer.m_NormalRoughMetal.m_RenderTarget;
		break;
	case RenderTargetDebug_NormalUnpacked:
		rt = m_GBuffer.m_NormalRoughMetal.m_RenderTarget;
		break;
	case RenderTargetDebug_Roughness:
		rt = m_GBuffer.m_NormalRoughMetal.m_RenderTarget;
		break;
	case RenderTargetDebug_Metalness:
		rt = m_GBuffer.m_NormalRoughMetal.m_RenderTarget;
		break;
	case RenderTargetDebug_LightAccum:
		rt = m_GBuffer.m_LightAccu.m_RenderTarget;
		break;
	case RenderTargetDebug_PostMerge:
		rt = m_GBuffer.m_Merge.m_RenderTarget;
		break;
	case	RenderTargetDebug_Distortion:
		rt = m_Distortion.m_DistoRenderTarget.m_RenderTarget;
		break;
	default:
		rt = m_BeforeDebugOutputRt.m_RenderTarget;
		break;
	}

	if (!PK_VERIFY(m_ApiManager != null))
		return null;

	return rt;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
