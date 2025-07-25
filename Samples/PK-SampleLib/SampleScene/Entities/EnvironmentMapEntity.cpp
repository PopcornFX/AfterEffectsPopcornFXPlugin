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

#include "EnvironmentMapEntity.h"

#include <pk_rhi/include/AllInterfaces.h>
#include <pk_rhi/include/PixelFormatFallbacks.h>
#include <pk_rhi/include/interfaces/IApiManager.h>
#include <pk_rhi/include/interfaces/SApiContext.h>

#include <pk_kernel/include/kr_resources.h>

#include <pk_imaging/include/im_image.h>

#include <PK-SampleLib/ShaderDefinitions/BasicSceneShaderDefinitions.h>
#include <PK-SampleLib/ShaderDefinitions/SampleLibShaderDefinitions.h>

#include <PK-SampleLib/RenderIntegrationRHI/RHIRenderIntegrationConfig.h>
#include <pk_maths/include/pk_maths_transforms.h>
#include <pk_kernel/include/kr_units.h>
#include <pk_imaging/include/im_codecs.h>

#define	COMPUTE_CUBEMAP_SHADER_PATH	"./Shaders/ComputeCubemap.comp"
#define	COMPUTE_MIPMAP_SHADER_PATH	"./Shaders/ComputeMipMap.comp"
#define	FILTER_CUBEMAP_SHADER_PATH	"./Shaders/FilterCubemap.comp"
#define	BLUR_CUBEMAP_SHADER_PATH	"./Shaders/BlurCubemap.comp"
#define	RENDER_FACE_SHADER_PATH		"./Shaders/RenderCubemapFace.comp"

#define	FACECOUNT 6
#define MAXFACESIZE 1024 // Mip map count will be computed from this.
// For IBL, we want to specify mip map count since this is hardcoded in shaders
// So it doesn't necessarily goes to the 1*1 level
#define MAXFACESIZEIBL 512
#define MIPMAPCOUNTIBL 7
#define IBLSAMPLECOUNT 1024 // For montecarlo sampling of radiance
#define PI 3.14159f

PK_PLUGIN_DECLARE(CImagePKIMCodec);

__PK_SAMPLE_API_BEGIN


//----------------------------------------------------------------------------

CEnvironmentMap::CEnvironmentMap()
:	m_ProgressiveProcessing(false)
,	m_InputPath(null)
,	m_CachePath(null)
,	m_InputTexture(null)
,	m_InputSampler(null)
,	m_InputIsLatLong(false)
,	m_MustRegisterCompute(false)
,	m_LoadIsValid(false)
,	m_IsUsable(false)
,	m_CubemapConstantSet(null)
,	m_IBLCubemapConstantSet(null)
,	m_BackgroundCubemapConstantSet(null)
,	m_CubemapTexture(null)
,	m_IBLCubemapTexture(null)
,	m_BackgroundCubemapTexture(null)
,	m_CubemapSampler(null)
,	m_IBLCubemapSampler(null)
,	m_BackgroundCubemapSampler(null)
,	m_ComputeLatLongState(null)
,	m_ComputeCubeState(null)
,	m_ComputeMipMapState(null)
,	m_FilterCubemapState(null)
,	m_BlurCubemapState(null)
,	m_RenderCubemapFaceState(null)
,	m_ComputeMipMapSampler(null)
,	m_BlurFaceSampler(null)
,	m_WhiteEnvMapConstantSet(null)
,	m_OutputTextureFormat(RHI::FormatFloat16RGBA)
,	m_ProgressiveCounter(0u)
,	m_MipmapCount(0u)
,	m_MipmapCountIBL(0u)
,	m_CubemapRotation(null)
{
}

//----------------------------------------------------------------------------

CEnvironmentMap::~CEnvironmentMap()
{
}

//----------------------------------------------------------------------------

u32	_TexelRangeFromAngle(float angle, u32 faceSize)
{
	/*
	We want to compute distance (X) (texel range at most deformed region)
	for a given angle (radius of the kernel) of (a).
	We are also at edge in the third dimension, meaning the angle (c) between
	center of edge and corner is smaller than 45 degrees.
	The following diagram ignores the half texel offset necessary to work
	from texel center.

	+-----X----------D----------------------------+
	| \          \         |                      |
	|   \         \        |     c = a + b        |
	|     \        \       |     1 = X + D        |
	|       \       \      |                      |
	|         \      \     F                      |
	|           \     \    |                      |
	|             \  a \ b |                      |
	|               \   \  |                      |
	|                 \  \ |                      |
	|                   \ \|                      |
	|                     \|                      |
	|                                             |
	|                                             |
	|                                             |
	|                                             |
	|                                             |
	|                                             |
	|                                             |
	|                                             |
	|                                             |
	+---------------------------------------------+
	*/

	// If face size is 1, this won't be use,
	// we can blur down to a smaller texture
	if (faceSize <= 1 )
		return 0u;

	// Coordinate of the first texel in one dimension, in -1 +1 range (= face seam with half a texel offset)
	const float		halfTexelNormalized = ((0.5f / static_cast<float>(faceSize)) - 0.5f) * 2.0f;

	const CFloat3	ray = CFloat3(halfTexelNormalized, halfTexelNormalized, 1.0f).Normalized();

	const CFloat3	F = CFloat3(0.f, halfTexelNormalized, 1.0f);
	const float		c = acos(ray.Dot(F.Normalized()));
	const float		b = c - angle;
	const float		D = tan(b) * F.Length();
	const float		X = (1. - D);

	const u32		range = u32(X * faceSize * 0.5);

	/*
	{
		CLog::Log(PK_INFO, "Size is %i, angle is %f, range is %i ", faceSize, angle, range);
		float rangeFloat = X * faceSize * 0.5;
		float check = acos(ray.Dot(CFloat3(((0.5 + rangeFloat) - (faceSize*0.5))/(0.5*faceSize), halfTexelNormalized, 1.0f).Normalized()));
		CLog::Log(PK_INFO, "Check: angle was %f rad", check);
	}
	*/

	return range;
}

//----------------------------------------------------------------------------

// For background blurring, heuristic mapping from src mip level to
// kernel blur angle (radians).
float	CEnvironmentMap::MipLevelToBlurAngle(u32 srcMipLevel)
{
	// Cursor t in 0 - 1 range. Is = 1 at the before last mipmap, meaning it's processed with the max blur
	// into the last mipmap.
	const float	t = PKMin(1.0f, static_cast<float>(srcMipLevel) / static_cast<float>(m_MipmapCount - 2));

	// Heuristic: make small cones for first blurs since very visible.
	// Do not start with blur of 0., which would mean no blur.
	return	(PI / 2.0f) * PKLerp(0.01f, 1.0f, pow(t, 1.8f));
}

//----------------------------------------------------------------------------

bool CEnvironmentMap::TryLoadFromCache(const CString &resourcePath, CResourceManager *resourceManager)
{
	const CString			cachePath = m_CachePath / m_InputPath + ".cube.pkim";
	const CString			cachePathIBL = m_CachePath / m_InputPath + ".ibl.pkim";
	TResourcePtr<CImage>	cachedImage = resourceManager->Load<CImage>(cachePath);
	TResourcePtr<CImage>	cachedImageIBL = resourceManager->Load<CImage>(cachePathIBL);

	if (cachedImage != null && cachedImageIBL != null)
	{
		// Check for invalid/corrupted cache file
		if (cachedImage->m_Frames.Count() != 1 &&
			cachedImage->m_Frames.First().m_Mipmaps.Count() != FACECOUNT * m_MipmapCount &&
			cachedImageIBL->m_Frames.Count() != 1 &&
			cachedImageIBL->m_Frames.First().m_Mipmaps.Count() != FACECOUNT * m_MipmapCountIBL)
			return false;

		RHI::PTexture		initialCubemap = m_BackgroundCubemapTexture;
		RHI::PTexture		initialCubemapIBL = m_IBLCubemapTexture;

		m_BackgroundCubemapTexture = RHI::PixelFormatFallbacks::CreateTextureAndFallbackIFN(m_ApiManager, *cachedImage, false, resourcePath.Data());
		m_IBLCubemapTexture = RHI::PixelFormatFallbacks::CreateTextureAndFallbackIFN(m_ApiManager, *cachedImageIBL, false, resourcePath.Data());

		bool				success = true;

		success &= m_BackgroundCubemapConstantSet->SetConstants(m_BackgroundCubemapSampler, m_BackgroundCubemapTexture, 0);
		success &= m_BackgroundCubemapConstantSet->UpdateConstantValues();
		success &= m_IBLCubemapConstantSet->SetConstants(m_IBLCubemapSampler, m_IBLCubemapTexture, 0);
		success &= m_IBLCubemapConstantSet->UpdateConstantValues();

		if (!success)
		{
			m_BackgroundCubemapTexture = initialCubemap;
			m_IBLCubemapTexture = initialCubemapIBL;

			m_BackgroundCubemapConstantSet->SetConstants(m_BackgroundCubemapSampler, m_BackgroundCubemapTexture, 0);
			m_BackgroundCubemapConstantSet->UpdateConstantValues();
			m_IBLCubemapConstantSet->UpdateConstantValues();
			m_IBLCubemapConstantSet->SetConstants(m_IBLCubemapSampler, m_IBLCubemapTexture, 0);
		}

		return success;
	}

	return false;
}

//----------------------------------------------------------------------------

bool	CEnvironmentMap::Init(const RHI::PApiManager &apiManager, CShaderLoader *shaderLoader)
{
	m_MustRegisterCompute = false;
	m_LoadIsValid = false;
	m_IsUsable = false;
	m_MipmapCount = IntegerTools::Log2(MAXFACESIZE) + 1; 
	m_MipmapCountIBL = MIPMAPCOUNTIBL;
	m_ApiManager = apiManager;

	// Create output texture constant set layout
	{
		m_CubemapSamplerConstLayout.Reset();
		CreateCubemapSamplerConstantSetLayout(m_CubemapSamplerConstLayout);
	}

	// Cubemap rotation (used for both IBL, background, and dummy cubemap constant sets)
	{
		m_CubemapRotation = apiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("CubemapRotation"), RHI::ConstantBuffer, sizeof(TMatrix<float, 2, 4>));
		if (!PK_VERIFY(m_CubemapRotation != null))
			return false;
		SetRotation(0.0f);
	}

	// Create dummy ressources for fallback
	{
		u32						white = 0xFFFFFFFF;
		CImageMap				dummyWhite(CUint3::ONE, &white, sizeof(u32));
		CImageMap				dummyCube[6];

		for (u32 i = 0; i < 6; i++)
			dummyCube[i] = dummyWhite;

		RHI::PTexture	dummyOutputTexture = apiManager->CreateTexture(RHI::SRHIResourceInfos("Dummy Environment Map Texture"), dummyCube, RHI::FormatUnorm8RGBA, RHI::kDefaultComponentSwizzle, RHI::TextureCubemap);
		if (!PK_VERIFY(dummyOutputTexture != null))
			return false;

		RHI::PConstantSampler dummyOutputSampler = apiManager->CreateConstantSampler(	RHI::SRHIResourceInfos("Dummy Environment Map Sampler"),
																						RHI::SampleLinear,
																						RHI::SampleLinearMipmapLinear,
																						RHI::SampleClampToEdge,
																						RHI::SampleClampToEdge,
																						RHI::SampleClampToEdge,
																						dummyOutputTexture->GetMipmapCount(),
																						false);
		if (!PK_VERIFY(dummyOutputSampler != null))
			return false;

		m_WhiteEnvMapConstantSet = apiManager->CreateConstantSet(RHI::SRHIResourceInfos("Dummy Environment Map Constant Set"), m_CubemapSamplerConstLayout);
		if (!PK_VERIFY(m_WhiteEnvMapConstantSet != null))
			return false;

		m_WhiteEnvMapConstantSet->SetConstants(dummyOutputSampler, dummyOutputTexture, 0);
		m_WhiteEnvMapConstantSet->SetConstants(m_CubemapRotation, 1);
		m_WhiteEnvMapConstantSet->UpdateConstantValues();
	}

	// Select cubemap output format
	{
		if (apiManager->GetApiContext()->m_GPUCaps.IsPixelFormatSupported(RHI::FormatFloat16RGBA, RHI::FormatUsage_ShaderSampling | RHI::FormatUsage_RenderTarget))
		{
			m_OutputTextureFormat = RHI::FormatFloat16RGBA;
		}
		else
		{
			CLog::Log(PK_WARN, "The pixel format %s is not supported by your GPU.", RHI::PixelFormatHelpers::PixelFormatToString_NoAssert(RHI::FormatFloat16RGBA));
			m_OutputTextureFormat = RHI::FormatUnorm8RGBA;
		}
	}

	// Create cubemap resources:
	// - Intermediate cubemap (mipmapped without specific processing, local to this class)
	// - Background cubemap
	// - Cubemap for image based lighting
	{
		// Simple mipmapped cubemap
		{
			TArray<CImageMap>	blackCube;
			TArray<CFloat4>		blackTexels;
			if (!PK_VERIFY(blackCube.Resize(FACECOUNT * m_MipmapCount)) ||
				!PK_VERIFY(blackTexels.Resize(MAXFACESIZE * MAXFACESIZE)))
				return false;
			Mem::Clear(blackTexels.RawDataPointer(), blackTexels.CoveredBytes());

			if (!PK_VERIFY(m_CubemapRenderTargets.Resize(FACECOUNT * m_MipmapCount)))
				return false;

			for (u32 face = 0; face < FACECOUNT; face++)
			{
				u32	faceSize = MAXFACESIZE;
				for (u32 level = 0; level < m_MipmapCount; level++)
				{
					const u32	id = level + face * m_MipmapCount;

					// add a face to the dummy texture
					const CImageMap	dummy = CImageMap(CUint3(faceSize, faceSize, 1), blackTexels.RawDataPointer(), faceSize * faceSize * sizeof(float) * 4);
					blackCube[id] = dummy;

					m_CubemapRenderTargets[id] = apiManager->CreateRenderTarget(RHI::SRHIResourceInfos("Environment Map Render Target"), m_OutputTextureFormat, CUint2(faceSize, faceSize), true, RHI::SampleCount1, true);
					if (!PK_VERIFY(m_CubemapRenderTargets[id] != null))
						return false;

					faceSize /= 2;
				}
			} 

			m_CubemapTexture = apiManager->CreateTexture(RHI::SRHIResourceInfos("Environment Map Texture"), blackCube, m_OutputTextureFormat, RHI::kDefaultComponentSwizzle, RHI::TextureCubemap);
			if (!PK_VERIFY(m_CubemapTexture != null))
				return false;

			m_CubemapSampler = apiManager->CreateConstantSampler(	RHI::SRHIResourceInfos("Environment Map Sampler"),
																	RHI::SampleLinear,
																	RHI::SampleLinearMipmapLinear,
																	RHI::SampleClampToEdge,
																	RHI::SampleClampToEdge,
																	RHI::SampleClampToEdge,
																	m_CubemapTexture->GetMipmapCount(),
																	false);
			if (!PK_VERIFY(m_CubemapSampler != null))
				return false;

			m_CubemapConstantSet = apiManager->CreateConstantSet(RHI::SRHIResourceInfos("Environment Map Constant Set"), m_CubemapSamplerConstLayout);
			if (!PK_VERIFY(m_CubemapConstantSet != null))
				return false;

			m_CubemapConstantSet->SetConstants(m_CubemapSampler, m_CubemapTexture, 0);
			m_CubemapConstantSet->SetConstants(m_CubemapRotation, 1);
			m_CubemapConstantSet->UpdateConstantValues();
		}

		// IBL cubemap (Radiance sum prefiltering)
		{
			TArray<CImageMap>	blackCube;
			TArray<CFloat4>		blackTexels;
			if (!PK_VERIFY(blackCube.Resize(FACECOUNT * m_MipmapCountIBL)) ||
				!PK_VERIFY(blackTexels.Resize(MAXFACESIZEIBL * MAXFACESIZEIBL)))
				return false;
			Mem::Clear(blackTexels.RawDataPointer(), blackTexels.CoveredBytes());

			if (!PK_VERIFY(m_IBLCubeRenderTargets.Resize(FACECOUNT * m_MipmapCountIBL)))
				return false;

			for (u32 face = 0; face < FACECOUNT; face++)
			{
				u32	faceSize = MAXFACESIZEIBL;
				for (u32 level = 0; level < m_MipmapCountIBL; level++)
				{
					const u32	id = level + face * m_MipmapCountIBL;

					// add a face to the dummy texture
					const CImageMap	dummy = CImageMap(CUint3(faceSize, faceSize, 1), blackTexels.RawDataPointer(), faceSize * faceSize * sizeof(float) * 4);
					blackCube[id] = dummy;

					m_IBLCubeRenderTargets[id] = apiManager->CreateRenderTarget(RHI::SRHIResourceInfos("IBL Render Target"), m_OutputTextureFormat, CUint2(faceSize, faceSize), true, RHI::SampleCount1, true);
					if (!PK_VERIFY(m_IBLCubeRenderTargets[id] != null))
						return false;

					faceSize /= 2;
				}
			} 

			m_IBLCubemapTexture = apiManager->CreateTexture(RHI::SRHIResourceInfos("IBL Texture"), blackCube, m_OutputTextureFormat, RHI::kDefaultComponentSwizzle, RHI::TextureCubemap);
			if (!PK_VERIFY(m_IBLCubemapTexture != null))
				return false;

			m_IBLCubemapSampler = apiManager->CreateConstantSampler(	RHI::SRHIResourceInfos("IBL Sampler"),
																		RHI::SampleLinear,
																		RHI::SampleLinearMipmapLinear,
																		RHI::SampleClampToEdge,
																		RHI::SampleClampToEdge,
																		RHI::SampleClampToEdge,
																		m_IBLCubemapTexture->GetMipmapCount(),
																		false);
			if (!PK_VERIFY(m_IBLCubemapSampler != null))
				return false;

			m_IBLCubemapConstantSet = apiManager->CreateConstantSet(RHI::SRHIResourceInfos("IBL Constant Set"), m_CubemapSamplerConstLayout);
			if (!PK_VERIFY(m_IBLCubemapConstantSet != null))
				return false;

			m_IBLCubemapConstantSet->SetConstants(m_IBLCubemapSampler, m_IBLCubemapTexture, 0);
			m_IBLCubemapConstantSet->SetConstants(m_CubemapRotation, 1);
			m_IBLCubemapConstantSet->UpdateConstantValues();
		}

		// Background cubemap (blurred cubemap for display)
		{
			TArray<CImageMap>	blackCube;
			TArray<CFloat4>		blackTexels;
			if (!PK_VERIFY(blackCube.Resize(FACECOUNT * m_MipmapCount)) ||
				!PK_VERIFY(blackTexels.Resize(MAXFACESIZE * MAXFACESIZE)))
				return false;
			Mem::Clear(blackTexels.RawDataPointer(), blackTexels.CoveredBytes());

			if (!PK_VERIFY(m_BackgroundCubeRenderTargets.Resize(FACECOUNT * m_MipmapCount)) ||
				!PK_VERIFY(m_FacesForBlurRenderTargets.Resize(FACECOUNT * m_MipmapCount)))
				return false;

			for (u32 face = 0; face < FACECOUNT; face++)
			{
				u32	faceSize = MAXFACESIZE;
				for (u32 level = 0; level < m_MipmapCount; level++)
				{
					const u32	blurRadius = _TexelRangeFromAngle(MipLevelToBlurAngle(level), faceSize);
					const u32	id = level + face * m_MipmapCount;

					// add a face to the dummy texture
					const CImageMap	dummy = CImageMap(CUint3(faceSize, faceSize, 1), blackTexels.RawDataPointer(), faceSize * faceSize * sizeof(float) * 4);
					blackCube[id] = dummy;

					m_BackgroundCubeRenderTargets[id] = apiManager->CreateRenderTarget(RHI::SRHIResourceInfos("Blurred Cubemap Render Target"), m_OutputTextureFormat, CUint2(faceSize, faceSize), true, RHI::SampleCount1, true);
					m_FacesForBlurRenderTargets[id] = apiManager->CreateRenderTarget(RHI::SRHIResourceInfos("Blurred Face Render Target"), m_OutputTextureFormat, CUint2(faceSize + 2 * (blurRadius), faceSize + 2 * (blurRadius)), true, RHI::SampleCount1, true);
					if (!PK_VERIFY(m_BackgroundCubeRenderTargets[id] != null) ||
						!PK_VERIFY(m_FacesForBlurRenderTargets[id] != null))
						return false;

					faceSize /= 2;
				}
			} 

			m_BackgroundCubemapTexture = apiManager->CreateTexture(RHI::SRHIResourceInfos("Blurred Texture"), blackCube, m_OutputTextureFormat, RHI::kDefaultComponentSwizzle, RHI::TextureCubemap);
			if (!PK_VERIFY(m_BackgroundCubemapTexture != null))
				return false;

			m_BackgroundCubemapSampler = apiManager->CreateConstantSampler(	RHI::SRHIResourceInfos("Blurred Sampler"),
																			RHI::SampleLinear,
																			RHI::SampleLinearMipmapLinear,
																			RHI::SampleClampToEdge,
																			RHI::SampleClampToEdge,
																			RHI::SampleClampToEdge,
																			m_BackgroundCubemapTexture->GetMipmapCount(),
																			false);
			if (!PK_VERIFY(m_BackgroundCubemapSampler != null))
				return false;

			m_BackgroundCubemapConstantSet = apiManager->CreateConstantSet(RHI::SRHIResourceInfos("Blurred Constant Set"), m_CubemapSamplerConstLayout);
			if (!PK_VERIFY(m_IBLCubemapConstantSet != null))
				return false;

			m_BackgroundCubemapConstantSet->SetConstants(m_BackgroundCubemapSampler, m_BackgroundCubemapTexture, 0);
			m_BackgroundCubemapConstantSet->SetConstants(m_CubemapRotation, 1);
			m_BackgroundCubemapConstantSet->UpdateConstantValues();
		}

		// Create sampler for mip map generation
		m_ComputeMipMapSampler = apiManager->CreateConstantSampler(	RHI::SRHIResourceInfos("Compute Mip Sampler"),
																	RHI::SampleLinear,
																	RHI::SampleLinearMipmapLinear ,
																	RHI::SampleClampToEdge,
																	RHI::SampleClampToEdge,
																	RHI::SampleClampToEdge,
																	1,
																	false);
		if (!PK_VERIFY(m_ComputeMipMapSampler != null))
				return false;

		// Create sampler for face blur
		m_BlurFaceSampler = apiManager->CreateConstantSampler(	RHI::SRHIResourceInfos("Blur Face Sampler"),
																RHI::SampleNearest,
																RHI::SampleNearestMipmapNearest,
																RHI::SampleClampToEdge,
																RHI::SampleClampToEdge,
																RHI::SampleClampToEdge,
																1,
																false);
		if (!PK_VERIFY(m_BlurFaceSampler != null))
			return false;
	}

	// Create compute state and constant set layouts
	{
		// For latlong input texture
		m_ComputeLatLongState = apiManager->CreateComputeState(RHI::SRHIResourceInfos("Compute LatLong Compute State"));
		if (!PK_VERIFY(m_ComputeLatLongState != null))
			return false;
		FillComputeCubemapShaderBindings(m_ComputeLatLongState->m_ComputeState.m_ShaderBindings, true);
		if (!PK_VERIFY(shaderLoader->LoadShader(m_ComputeLatLongState->m_ComputeState, COMPUTE_CUBEMAP_SHADER_PATH, apiManager)))
			return false;
		if (!PK_VERIFY(apiManager->BakeComputeState(m_ComputeLatLongState)))
			return false;
		CreateComputeCubemapConstantSetLayout(m_ComputeLatLongConstantSetLayout, true);
			
		// For cube input texture
		m_ComputeCubeState = apiManager->CreateComputeState(RHI::SRHIResourceInfos("Cube Compute State"));
		if (!PK_VERIFY(m_ComputeCubeState != null))
			return false;
		FillComputeCubemapShaderBindings(m_ComputeCubeState->m_ComputeState.m_ShaderBindings, false);
		if (!PK_VERIFY(shaderLoader->LoadShader(m_ComputeCubeState->m_ComputeState, COMPUTE_CUBEMAP_SHADER_PATH, apiManager)))
			return false;
		if (!PK_VERIFY(apiManager->BakeComputeState(m_ComputeCubeState)))
			return false;
		CreateComputeCubemapConstantSetLayout(m_ComputeCubeConstantSetLayout, false);

		// For mipmap generation
		m_ComputeMipMapState = apiManager->CreateComputeState(RHI::SRHIResourceInfos("MipMap Compute State"));
		if (!PK_VERIFY(m_ComputeMipMapState != null))
			return false;
		FillComputeMipMapShaderBindings(m_ComputeMipMapState->m_ComputeState.m_ShaderBindings);
		if (!PK_VERIFY(shaderLoader->LoadShader(m_ComputeMipMapState->m_ComputeState, COMPUTE_MIPMAP_SHADER_PATH, apiManager)))
			return false;
		if (!PK_VERIFY(apiManager->BakeComputeState(m_ComputeMipMapState)))
			return false;
		CreateComputeMipMapConstantSetLayout(m_ComputeMipMapConstantSetLayout);

		// For IBL filtering compute shader
		m_FilterCubemapState = apiManager->CreateComputeState(RHI::SRHIResourceInfos("Filter Compute State"));
		if (!PK_VERIFY(m_FilterCubemapState != null))
			return false;
		FillFilterCubemapShaderBindings(m_FilterCubemapState->m_ComputeState.m_ShaderBindings);
		if (!PK_VERIFY(shaderLoader->LoadShader(m_FilterCubemapState->m_ComputeState, FILTER_CUBEMAP_SHADER_PATH, apiManager)))
			return false;
		if (!PK_VERIFY(apiManager->BakeComputeState(m_FilterCubemapState)))
			return false;
		CreateFilterCubemapConstantSetLayout(m_FilterCubemapConstantSetLayout);

		// For background blurring face pre-rendering compute shader
		m_RenderCubemapFaceState = apiManager->CreateComputeState(RHI::SRHIResourceInfos("Render Cubemap Compute State"));
		if (!PK_VERIFY(m_RenderCubemapFaceState != null))
			return false;
		FillBlurCubemapRenderFaceShaderBindings(m_RenderCubemapFaceState->m_ComputeState.m_ShaderBindings);
		if (!PK_VERIFY(shaderLoader->LoadShader(m_RenderCubemapFaceState->m_ComputeState, RENDER_FACE_SHADER_PATH, apiManager)))
			return false;
		if (!PK_VERIFY(apiManager->BakeComputeState(m_RenderCubemapFaceState)))
			return false;
		CreateBlurCubemapRenderFaceConstantSetLayout(m_RenderCubemapFaceConstantSetLayout);

		// For background blurring compute shader
		m_BlurCubemapState = apiManager->CreateComputeState(RHI::SRHIResourceInfos("Blue Cubemap Compute State"));
		if (!PK_VERIFY(m_BlurCubemapState != null))
			return false;
		FillBlurCubemapProcessShaderBindings(m_BlurCubemapState->m_ComputeState.m_ShaderBindings);
		if (!PK_VERIFY(shaderLoader->LoadShader(m_BlurCubemapState->m_ComputeState, BLUR_CUBEMAP_SHADER_PATH, apiManager)))
			return false;
		if (!PK_VERIFY(apiManager->BakeComputeState(m_BlurCubemapState)))
			return false;
		CreateBlurCubemapProcessConstantSetLayout(m_BlurCubemapConstantSetLayout);
	}

	return true;
}

//----------------------------------------------------------------------------

bool	CEnvironmentMap::Load(const CString &resourcePath, CResourceManager *resourceManager)
{
	if (m_InputPath == resourcePath)
		return true;

	m_LoadIsValid = false;
	m_IsUsable = false;
	m_ProgressiveCounter = 0u;
	m_InputPath = resourcePath;

	if (TryLoadFromCache(resourcePath, resourceManager))
	{
		m_MustRegisterCompute = false;
		m_LoadIsValid = true;
		m_IsUsable = true;

		return true;
	}

	// Create input texture ressources
	TResourcePtr<CImage>	inputImage = resourceManager->Load<CImage>(resourcePath, false, SResourceLoadCtl(false, true));
	if (inputImage == null || inputImage->Empty())
		return false;

	m_InputTexture = RHI::PixelFormatFallbacks::CreateTextureAndFallbackIFN(m_ApiManager, *inputImage, inputImage->GammaCorrected() || !inputImage->FloatingPoint(), resourcePath.Data());
	m_InputSampler = m_ApiManager->CreateConstantSampler(	RHI::SRHIResourceInfos("Input Sampler"),
														RHI::SampleLinear,
														RHI::SampleLinearMipmapLinear,
														RHI::SampleClampToEdge,
														RHI::SampleClampToEdge,
														RHI::SampleClampToEdge,
														m_InputTexture->GetMipmapCount(),
														false);
	if (!PK_VERIFY(m_InputSampler != null))
		return false;

	// Get input type (latlong or cube)
	m_InputIsLatLong = (m_InputTexture->GetType() != RHI::ETextureType::TextureCubemap);

	m_MustRegisterCompute = true;
	m_LoadIsValid = true;

	return true; 
}

//----------------------------------------------------------------------------

#if	(PK_IMAGING_ENABLE_WRITE_CODECS != 0)

// These would be defined in the dds_codec.h header, which might not be redistributed
// in the SDKs. Ideally we would #include "../Plugins/CodecImage_DDS/include/dds_codec.h",
// but it wouldn't compile in some SDK configs. So here, just repro it here and keep it synced
enum	EExportCodecFlags
{
	ExportFlag_ExplicitWriteFlags	= 0x1,
	ExportFlag_WriteTexels			= 0x2,
	ExportFlag_WriteDensity			= 0x4,
};

bool CEnvironmentMap::ExportCubemap(CResourceManager *resourceManager)
{
	TArray<PImage>	images;
	if (!PK_VERIFY(images.Resize(m_ReadBackTextures.Count())))
		return false;

	for (u32 i = 0; i < m_ReadBackTextures.Count(); i++)
	{
		PImage		image = m_ApiManager->CreateImageFromReadBackTexture(m_ReadBackTextures[i]);
		if (!PK_VERIFY(image != null) ||	// 'CreateImageFromReadBackTexture()' failed
			!PK_VERIFY(!image->m_Frames.Empty()) ||
			!PK_VERIFY(!image->m_Frames.First().m_Mipmaps.Empty()) ||
			!PK_VERIFY(image->m_Frames.First().m_Mipmaps.First().m_RawBuffer != null))	// invalid image produced by 'CreateImageFromReadBackTexture()'
			return false;

		images[i] = image;
	}
	PImage			imageCube = PK_NEW(CImage);
	PImage			imageIBL = PK_NEW(CImage);

	if (!PK_VERIFY(imageCube->m_Frames.Resize(1)) || !PK_VERIFY(imageIBL->m_Frames.Resize(1)))
		return false;

	imageCube->m_Flags |= CImage::Flag_Cubemap;
	imageCube->m_Format = images.First()->m_Format;
	imageIBL->m_Flags |= CImage::Flag_Cubemap;
	imageIBL->m_Format = images.First()->m_Format;

	for (u32 level = 0; level < m_MipmapCount; level++)
	{
		for (u32 face = 0; face < FACECOUNT; face++)
		{
			if (!PK_VERIFY(imageCube->m_Frames[0].m_Mipmaps.PushBack(images[FACECOUNT * level + face]->m_Frames[0].m_Mipmaps[0]).Valid()))
				return false;
		}
	}
	for (u32 level = 0; level < m_MipmapCountIBL; level++)
	{
		for (u32 face = 0; face < FACECOUNT; face++)
		{
			if (!PK_VERIFY(imageIBL->m_Frames[0].m_Mipmaps.PushBack(images[FACECOUNT * m_MipmapCount + FACECOUNT * level + face]->m_Frames[0].m_Mipmaps[0]).Valid()))
				return false;
		}
	}

	IFileSystem				*fileController = resourceManager->FileController();

	const CString			cachePath = m_CachePath / m_InputPath + ".cube.pkim";
	const CString			cachePathIBL = m_CachePath / m_InputPath + ".ibl.pkim";
	IImageCodec				*imageCodec = checked_cast<IImageCodec*>(GetPlugin_CImagePKIMCodec());
	if (imageCodec == null)
		return false;

	CMessageStream			exportReport;
	SImageCodecWriteConfig	writeCfg;

	writeCfg.m_CodecData[0] = EExportCodecFlags::ExportFlag_WriteTexels;

	if (!imageCodec->FileSave(fileController, *imageCube, cachePath, false, writeCfg, exportReport))
	{
		fileController->FileDelete(cachePath, true);
	}

	if (!imageCodec->FileSave(fileController, *imageIBL, cachePathIBL, false, writeCfg, exportReport))
	{
		fileController->FileDelete(cachePathIBL, true);
	}
	m_ReadBackTextures.Clear();

	return !exportReport.HasErrors();
}
#endif

//----------------------------------------------------------------------------

// Register cubemap generation (compute dispatch + copy) on the given commandbuffer, if needed
bool	CEnvironmentMap::UpdateCubemap(const RHI::PCommandBuffer &cmdBuff, CResourceManager *resourceManager)
{
#if	(PK_IMAGING_ENABLE_WRITE_CODECS != 0)
	if (!m_MustRegisterCompute)
	{
		if (m_ReadBackTextures.Count() == 0)
			return true;

		// Wait for all readback textures to be ready
		for (u32 i = 0; i < m_ReadBackTextures.Count(); i++)
		{
			if (!m_ReadBackTextures[i]->IsReadable())
				return true;
		}

		if (ExportCubemap(resourceManager))
			return true;
	}

	return GenerateCubemap(cmdBuff);
#else
	(void)resourceManager;

	if (m_MustRegisterCompute)
		return GenerateCubemap(cmdBuff);
	return true;
#endif
}

//----------------------------------------------------------------------------

bool	CEnvironmentMap::GenerateCubemap(const RHI::PCommandBuffer &cmdBuff)
{
	bool	success = true;

	// Start registering commands
	if (m_ProgressiveCounter == 0u)
	{
		// Register compute dispatch
		success &= cmdBuff->BeginComputePass();

		// Sampling source texture (cube or latlong) without specific processing,
		// filling 6 max resolution face textures.
		for (u32 face = 0; face < FACECOUNT; face++)
		{
			RHI::PComputeState		computeState;
			RHI::PConstantSet		constantSet;

			if (m_InputIsLatLong)
			{
				computeState = m_ComputeLatLongState;
				constantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("LatLong Constant Set"), m_ComputeLatLongConstantSetLayout);
			}
			else
			{
				computeState = m_ComputeCubeState;
				constantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Cube Constant Set"), m_ComputeCubeConstantSetLayout);
			}
			if (!PK_VERIFY(constantSet != null))
				return false;

			RHI::PGpuBuffer			faceInfo = m_ApiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("FaceInfo Constant Buffer"), RHI::ConstantBuffer, sizeof(u32) * 2);
			if (!PK_VERIFY(faceInfo != null))
				return false;
			u32						*data = static_cast<u32*>(m_ApiManager->MapCpuView(faceInfo));
			data[0] = face;
			data[1] = MAXFACESIZE;
			m_ApiManager->UnmapCpuView(faceInfo);

			constantSet->SetConstants(faceInfo, 0);
			constantSet->SetConstants(m_InputSampler, m_InputTexture, 1);
			constantSet->SetConstants(null, m_CubemapRenderTargets[0 + face * m_MipmapCount]->GetTexture(), 2);
			constantSet->UpdateConstantValues();

			success &= cmdBuff->BindComputeState(computeState);

			success &= cmdBuff->BindConstantSet(constantSet);
			success &= cmdBuff->Dispatch(	(MAXFACESIZE + (PK_RH_GPU_THREADGROUP_SIZE_2D - 1)) / PK_RH_GPU_THREADGROUP_SIZE_2D,
											(MAXFACESIZE + (PK_RH_GPU_THREADGROUP_SIZE_2D - 1)) / PK_RH_GPU_THREADGROUP_SIZE_2D,
											1);
		}

		// Compute mip maps (2*2 box blur) without specific processing
		u32	faceSize = MAXFACESIZE / 2;
		for (u32 level = 1; level < m_MipmapCount; level++)
		{
			for (u32 face = 0; face < FACECOUNT; face++)
			{
				RHI::PConstantSet	constantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("MipMap Constant Set"), m_ComputeMipMapConstantSetLayout);
				if (!PK_VERIFY(constantSet != null))
					return false;

				RHI::PGpuBuffer	mipmapData = m_ApiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("LatLong Constant Set"), RHI::ConstantBuffer, sizeof(CFloat2));
				if (!PK_VERIFY(mipmapData != null))
					return false;
				CFloat2			*data = static_cast<CFloat2*>(m_ApiManager->MapCpuView(mipmapData));
				data[0] = 1.0f / CFloat2(faceSize, faceSize); // Texel size
				m_ApiManager->UnmapCpuView(mipmapData);

				RHI::PTexture	inputTexture = m_CubemapRenderTargets[(level - 1) + face * m_MipmapCount]->GetTexture();
				RHI::PTexture	outputTexture = m_CubemapRenderTargets[level + face * m_MipmapCount]->GetTexture();
				constantSet->SetConstants(mipmapData, 0);
				constantSet->SetConstants(m_ComputeMipMapSampler, inputTexture, 1);
				constantSet->SetConstants(null, outputTexture, 2);
				constantSet->UpdateConstantValues();

				// Register compute dispatch
				success &= cmdBuff->BeginComputePass();
				success &= cmdBuff->BindComputeState(m_ComputeMipMapState);

				success &= cmdBuff->BindConstantSet(constantSet);
				success &= cmdBuff->Dispatch(	(faceSize + (PK_RH_GPU_THREADGROUP_SIZE_2D - 1)) / PK_RH_GPU_THREADGROUP_SIZE_2D,
												(faceSize + (PK_RH_GPU_THREADGROUP_SIZE_2D - 1)) / PK_RH_GPU_THREADGROUP_SIZE_2D,
												1);
			}
			faceSize /= 2;	
		}

		success &= cmdBuff->EndComputePass();

		// Copy faces to cubemap
		faceSize = MAXFACESIZE;
		for (u32 level = 0; level < m_MipmapCount; level++)
		{
			for (u32 face = 0; face < FACECOUNT; face++)
			{
				// Register copy to output texture
				success &= cmdBuff->CopyTexture(m_CubemapRenderTargets[level + face * m_MipmapCount]->GetTexture(), m_CubemapTexture, 0, level, 0, face, CUint3::ZERO, CUint3::ZERO, CUint3(faceSize, faceSize, 1));
			}
			faceSize /= 2;
		}

		// First mip: register copy form our simple mipmapped resource
		// to output textures (background cubemap and IBL cubemap)
		for (u32 face = 0; face < FACECOUNT; face++)
		{
			// For background, we have the same mip count as source cubemap
			success &= cmdBuff->CopyTexture(m_CubemapRenderTargets[0 + face * m_MipmapCount]->GetTexture(), m_BackgroundCubemapTexture, 0, 0, 0, face, CUint3::ZERO, CUint3::ZERO, CUint3(MAXFACESIZE, MAXFACESIZE, 1));
			// For IBL, dst can have smaller so we don't necessary copy mip 0
			const u32	mipCountDelta = IntegerTools::Log2(MAXFACESIZE) - IntegerTools::Log2(MAXFACESIZEIBL);
			success &= cmdBuff->CopyTexture(m_CubemapRenderTargets[mipCountDelta + face * m_MipmapCount]->GetTexture(), m_IBLCubemapTexture, 0, 0, 0, face, CUint3::ZERO, CUint3::ZERO, CUint3(MAXFACESIZEIBL, MAXFACESIZEIBL, 1));
		}
		
		// Background texture processing: blur
		faceSize = MAXFACESIZE / 2;
		for (u32 level = 1; level < m_MipmapCount; level++)
		{
			const float	blurAngle = MipLevelToBlurAngle(level - 1); // Takes src level as input
			const u32	blurRadius = _TexelRangeFromAngle(blurAngle, faceSize * 2); // Takes src face size as input

			success &= cmdBuff->BeginComputePass();

			for (u32 face = 0; face < FACECOUNT; face++)
			{
				{
					RHI::PConstantSet	constantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Render Cubemap Face Constant Set"), m_RenderCubemapFaceConstantSetLayout);
					if (!PK_VERIFY(constantSet != null))
						return false;
					RHI::PGpuBuffer		faceInfo = m_ApiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("FaceInfo Constant Buffer"), RHI::ConstantBuffer, sizeof(u32) * 4);
					if (!PK_VERIFY(faceInfo != null))
						return false;
					u32					*data = static_cast<u32*>(m_ApiManager->MapCpuView(faceInfo));
					data[0] = face;
					data[1] = level;
					data[2] = faceSize * 2;
					data[3] = blurRadius;
					m_ApiManager->UnmapCpuView(faceInfo);

					constantSet->SetConstants(faceInfo, 0);
					constantSet->SetConstants(m_BackgroundCubemapSampler, m_BackgroundCubemapTexture, 1);
					constantSet->SetConstants(null, m_FacesForBlurRenderTargets[(level - 1) + face * m_MipmapCount]->GetTexture(), 2);
					constantSet->UpdateConstantValues();

					success &= cmdBuff->BindComputeState(m_RenderCubemapFaceState);
					success &= cmdBuff->BindConstantSet(constantSet);
					u32	size = faceSize * 2 + 2 * blurRadius;
					success &= cmdBuff->Dispatch(	(size + (PK_RH_GPU_THREADGROUP_SIZE_2D - 1)) / PK_RH_GPU_THREADGROUP_SIZE_2D,
													(size + (PK_RH_GPU_THREADGROUP_SIZE_2D - 1)) / PK_RH_GPU_THREADGROUP_SIZE_2D,
													1);
				}

				{
					RHI::PConstantSet	constantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Blur Cubemap Constant Set"), m_BlurCubemapConstantSetLayout);
					if (!PK_VERIFY(constantSet != null))
						return false;
					RHI::PGpuBuffer		faceInfo = m_ApiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("FaceInfo Constant Buffer"), RHI::ConstantBuffer, sizeof(u32) * 3);
					if (!PK_VERIFY(faceInfo != null))
						return false;
					void				*data = m_ApiManager->MapCpuView(faceInfo);
					static_cast<u32*>(data)[0] = faceSize;
					static_cast<u32*>(data)[1] = blurRadius;
					static_cast<float*>(data)[2] = blurAngle;
					m_ApiManager->UnmapCpuView(faceInfo);
					constantSet->SetConstants(faceInfo, 0);
					constantSet->SetConstants(m_BlurFaceSampler, m_FacesForBlurRenderTargets[(level - 1) + face * m_MipmapCount]->GetTexture(), 1);
					constantSet->SetConstants(null, m_BackgroundCubeRenderTargets[level + face * m_MipmapCount]->GetTexture(), 2);
					constantSet->UpdateConstantValues();

					success &= cmdBuff->BeginComputePass();
					success &= cmdBuff->BindComputeState(m_BlurCubemapState);
					success &= cmdBuff->BindConstantSet(constantSet);
					success &= cmdBuff->Dispatch(	(faceSize + (PK_RH_GPU_THREADGROUP_SIZE_2D - 1)) / PK_RH_GPU_THREADGROUP_SIZE_2D,
													(faceSize + (PK_RH_GPU_THREADGROUP_SIZE_2D - 1)) / PK_RH_GPU_THREADGROUP_SIZE_2D,
													1);
				}
			}
			success &= cmdBuff->EndComputePass();

			for (u32 face = 0; face < FACECOUNT; face++)
				success &= cmdBuff->CopyTexture(m_BackgroundCubeRenderTargets[level + face * m_MipmapCount]->GetTexture(), m_BackgroundCubemapTexture, 0, level, 0, face, CUint3::ZERO, CUint3::ZERO, CUint3(faceSize, faceSize, 1));

			faceSize /= 2;
		}
	}

	// Processings other levels of the output IBL texture
	const u32	totalSampleCount = IBLSAMPLECOUNT;
	const u32	progressiveSteps = m_ProgressiveProcessing ? 64u : 1u;
	const u32	sampleCount = totalSampleCount / progressiveSteps;
	u32			faceSize = MAXFACESIZEIBL / 2;
	success &= cmdBuff->BeginComputePass();
	for (u32 level = 1; level < m_MipmapCountIBL; level++)
	{
		for (u32 face = 0; face < FACECOUNT; face++)
		{
			RHI::PConstantSet	constantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Filter Cubemap Constant Set"), m_FilterCubemapConstantSetLayout);
			if (!PK_VERIFY(constantSet != null))
				return false;

			RHI::PGpuBuffer	faceInfo = m_ApiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("FaceInfo Constant Buffer"), RHI::ConstantBuffer, sizeof(u32) * 6);
			if (!PK_VERIFY(faceInfo != null))
				return false;
			u32				*data = static_cast<u32*>(m_ApiManager->MapCpuView(faceInfo));
			data[0] = face;
			reinterpret_cast<float*>(data)[1] = level / (m_MipmapCountIBL - 1.0f); // Roughness
			data[2] = faceSize;
			data[3] = sampleCount; // Sample count this batch
			data[4] = sampleCount * m_ProgressiveCounter; // Already performed sample count
			data[5] = totalSampleCount; // Total sample count
			m_ApiManager->UnmapCpuView(faceInfo);

			constantSet->SetConstants(faceInfo, 0);
			constantSet->SetConstants(m_IBLCubemapSampler, m_CubemapTexture, 1);
			constantSet->SetConstants(null, m_IBLCubeRenderTargets[level + face * m_MipmapCountIBL]->GetTexture(), 2);
			constantSet->UpdateConstantValues();


			success &= cmdBuff->BindComputeState(m_FilterCubemapState);

			success &= cmdBuff->BindConstantSet(constantSet);
			success &= cmdBuff->Dispatch(	(faceSize + (PK_RH_GPU_THREADGROUP_SIZE_2D - 1)) / PK_RH_GPU_THREADGROUP_SIZE_2D,
											(faceSize + (PK_RH_GPU_THREADGROUP_SIZE_2D - 1)) / PK_RH_GPU_THREADGROUP_SIZE_2D,
											1);

		}
		faceSize /= 2;
	}
	success &= cmdBuff->EndComputePass();

	// Register copy to IBL output cube texture
	faceSize = MAXFACESIZEIBL / 2;
	for (u32 level = 1; level < m_MipmapCountIBL; level++)
	{
		for (u32 face = 0; face < FACECOUNT; face++)
		{
			success &= cmdBuff->CopyTexture(m_IBLCubeRenderTargets[level + face * m_MipmapCountIBL]->GetTexture(), m_IBLCubemapTexture, 0, level, 0, face, CUint3::ZERO, CUint3::ZERO, CUint3(faceSize, faceSize, 1));
		}
		faceSize /= 2;
	}

#if	(PK_IMAGING_ENABLE_WRITE_CODECS != 0)
	if (!PK_VERIFY(m_ReadBackTextures.Resize(FACECOUNT * m_MipmapCount + FACECOUNT * m_MipmapCountIBL)))
		return false;

	for (u32 level = 0; level < m_MipmapCount; level++)
	{
		for (u32 face = 0; face < FACECOUNT; face++)
		{
			const RHI::PRenderTarget &target = level == 0 ? m_CubemapRenderTargets[0 + face * m_MipmapCount] : m_BackgroundCubeRenderTargets[level + face * m_MipmapCount];

			m_ReadBackTextures[level + face * m_MipmapCount] =
				m_ApiManager->CreateReadBackTexture(RHI::SRHIResourceInfos(CString::Format("Cubemap_LOD_Face%d_Level%d", face, level)), target);
			success &= cmdBuff->ReadBackRenderTarget(target, m_ReadBackTextures[level + face * m_MipmapCount]);
		}
	}

	for (u32 level = 0; level < m_MipmapCountIBL; level++)
	{
		for (u32 face = 0; face < FACECOUNT; face++)
		{
			const u32					mipCountDelta = IntegerTools::Log2(MAXFACESIZE) - IntegerTools::Log2(MAXFACESIZEIBL);
			const RHI::PRenderTarget	&target = level == 0 ? m_CubemapRenderTargets[mipCountDelta + face * m_MipmapCount] : m_IBLCubeRenderTargets[level + face * m_MipmapCountIBL];

			m_ReadBackTextures[FACECOUNT * m_MipmapCount + level + face * m_MipmapCountIBL] =
				m_ApiManager->CreateReadBackTexture(RHI::SRHIResourceInfos(CString::Format("CubemapIBL_LOD_Face%d_Level%d", face, level)), target);
			success &= cmdBuff->ReadBackRenderTarget(target, m_ReadBackTextures[FACECOUNT * m_MipmapCount + level + face * m_MipmapCountIBL]);
		}
	}
#endif

	m_ProgressiveCounter += 1u;

	// We have dispatched filtering for each mip level,
	// we can start using the texture even if not fully processed.
	m_IsUsable = true;

	if (m_ProgressiveCounter >= progressiveSteps)
		m_MustRegisterCompute = false;

	return success;
}

//----------------------------------------------------------------------------

RHI::PConstantSet	CEnvironmentMap::GetIBLCubemapConstantSet()
{
	return m_LoadIsValid && m_IsUsable ? m_IBLCubemapConstantSet : m_WhiteEnvMapConstantSet;
}

//----------------------------------------------------------------------------

RHI::PConstantSet	CEnvironmentMap::GetBackgroundCubemapConstantSet()
{
	return m_LoadIsValid && m_IsUsable ? m_BackgroundCubemapConstantSet : m_WhiteEnvMapConstantSet;
}

//----------------------------------------------------------------------------

bool	CEnvironmentMap::IsValid()
{
	return m_LoadIsValid;
}

//----------------------------------------------------------------------------

void	CEnvironmentMap::Reset()
{
	m_InputPath = null;
	m_InputTexture = null;
	m_InputSampler = null;
	m_InputIsLatLong = false;
	m_MustRegisterCompute = false;
	m_LoadIsValid = false;
	m_IsUsable = false;
}

//----------------------------------------------------------------------------

RHI::PConstantSet	CEnvironmentMap::GetWhiteEnvMapConstantSet()
{
	return m_WhiteEnvMapConstantSet;
}

//----------------------------------------------------------------------------

void	CEnvironmentMap::SetProgressiveProcessing(bool progressiveProcessing)
{
	m_ProgressiveProcessing = progressiveProcessing;
	m_ProgressiveCounter = 0u;
	m_MustRegisterCompute = true;
	m_IsUsable = false;
}

//----------------------------------------------------------------------------

void CEnvironmentMap::SetCachePath(const CString &path)
{
	m_CachePath = path;
}

//----------------------------------------------------------------------------

void	CEnvironmentMap::SetRotation(float angle)
{
	angle = Units::DegreesToRadians(angle);
	const float	c = cosf(angle);
	const float	s = sinf(angle);

	if (!PK_VERIFY(m_CubemapRotation != null))
		return;
	// std140: array stride must be 16 byte so the mat 2x2 is uploaded as a mat 2x4
	TMatrix<float, 2, 4>	*data = static_cast<TMatrix<float, 2, 4>*>(m_ApiManager->MapCpuView(m_CubemapRotation));
	*data = TMatrix<float, 2, 4>(CFloat4(c, -s, 0, 0), CFloat4(s, c, 0, 0));
	m_ApiManager->UnmapCpuView(m_CubemapRotation);
}

//----------------------------------------------------------------------------

__PK_SAMPLE_API_END
