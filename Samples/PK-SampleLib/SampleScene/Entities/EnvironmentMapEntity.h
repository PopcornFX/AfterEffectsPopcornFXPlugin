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

#include <pk_rhi/include/FwdInterfaces.h>

#include <PK-SampleLib/ShaderLoader.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------
class	CEnvironmentMap
{
public:
	CEnvironmentMap();
	~CEnvironmentMap();

	bool							Init(const RHI::PApiManager &apiManager, CShaderLoader *shaderLoader);
	bool							Load(const CString &resourcePath, CResourceManager *resourceManager);
	bool							GenerateCubemap(const RHI::PCommandBuffer &cmdBuff);
	bool							IsValid(); // Input texture is loaded and cubemap generation compute shader has been dispatched.
	void							Reset();
	void							SetRotation(float angle);

	RHI::PConstantSet				GetIBLCubemapConstantSet();
	RHI::PConstantSet				GetBackgroundCubemapConstantSet();
	RHI::PConstantSet				GetWhiteEnvMapConstantSet();
	u32								GetBackgroundMipmapCount() { return m_MipmapCount; }

	void							SetProgressiveProcessing(bool progressiveProcessing);

private:
	float							MipLevelToBlurAngle(u32 srcMip);

private:
	bool							m_ProgressiveProcessing;
	CString							m_InputPath;
	RHI::PTexture					m_InputTexture;
	RHI::PConstantSampler			m_InputSampler;
	bool							m_InputIsLatLong;
	// m_MustRegisterCompute: Has still dispatch to register for generation/filtering
	bool							m_MustRegisterCompute;
	// m_LoadIsValid: given source texture and associated RHI objects exist.
	// It doesn't mean the output texture is filled with the final data, but it's allocated
	// and will be filled next time we can register some dispatch.
	bool							m_LoadIsValid;
	// m_IsUsable: we have started registering some dispatch, so the output texture has usable data on
	// each mip even if not fully processed. Used to return a white cubemap constant set when
	// registering dispatch has not occured.
	bool							m_IsUsable;
	RHI::SConstantSetLayout			m_CubemapSamplerConstLayout;

	TArray<RHI::PRenderTarget>		m_CubemapRenderTargets;
	TArray<RHI::PRenderTarget>		m_IBLCubeRenderTargets;
	TArray<RHI::PRenderTarget>		m_BackgroundCubeRenderTargets;
	TArray<RHI::PRenderTarget>		m_FacesForBlurRenderTargets;

	RHI::PConstantSet				m_CubemapConstantSet;
	RHI::PConstantSet				m_IBLCubemapConstantSet;
	RHI::PConstantSet				m_BackgroundCubemapConstantSet;

	RHI::PTexture					m_CubemapTexture;
	RHI::PTexture					m_IBLCubemapTexture;
	RHI::PTexture					m_BackgroundCubemapTexture;

	RHI::PConstantSampler			m_CubemapSampler;
	RHI::PConstantSampler			m_IBLCubemapSampler;
	RHI::PConstantSampler			m_BackgroundCubemapSampler;

	RHI::PComputeState				m_ComputeLatLongState;
	RHI::PComputeState				m_ComputeCubeState;
	RHI::PComputeState				m_ComputeMipMapState;
	RHI::PComputeState				m_FilterCubemapState;
	RHI::PComputeState				m_BlurCubemapState;
	RHI::PComputeState				m_RenderCubemapFaceState;

	RHI::SConstantSetLayout			m_ComputeLatLongConstantSetLayout;
	RHI::SConstantSetLayout			m_ComputeCubeConstantSetLayout;
	RHI::SConstantSetLayout			m_ComputeMipMapConstantSetLayout;
	RHI::PConstantSampler			m_ComputeMipMapSampler;
	RHI::SConstantSetLayout			m_FilterCubemapConstantSetLayout;
	RHI::SConstantSetLayout			m_BlurCubemapConstantSetLayout;
	RHI::SConstantSetLayout			m_RenderCubemapFaceConstantSetLayout;
	RHI::PConstantSampler			m_BlurFaceSampler;

	RHI::PConstantSet				m_WhiteEnvMapConstantSet;

	RHI::EPixelFormat				m_OutputTextureFormat;

	RHI::SConstantSetLayout			m_ComputeMipMapCubemapConstantSetLayout;
	u32								m_ProgressiveCounter;
	u32								m_MipmapCount;
	u32								m_MipmapCountIBL;

	RHI::PGpuBuffer					m_CubemapRotation;

	RHI::PApiManager				m_ApiManager;
};
//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
