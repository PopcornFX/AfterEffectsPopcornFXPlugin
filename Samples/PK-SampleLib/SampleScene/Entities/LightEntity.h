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
#include <PK-SampleLib/RenderPasses/DirectionalShadows.h>
#include <pk_rhi/include/FwdInterfaces.h>
#include <pk_rhi/include/interfaces/SConstantSetLayout.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

struct	SLightRenderPass
{
	enum	ELightType
	{
		Ambient,
		Directional,
		Point,
		Spot,
	};

	struct	SLightsInfo
	{
		u32			m_DirectionalLightsCount;
		u32			m_SpotLightsCount;
		u32			m_PointLightsCount;
		u32			__padding0;
		CFloat3		m_AmbientColor;
		u32			__padding1;
	};

	struct	SPerLightData
	{
		CFloat4		m_LightParam0;
		CFloat4		m_LightParam1;
		CFloat3		m_Color;
		float		__padding0;

		SPerLightData()
		:	m_LightParam0(CFloat4::ZERO)
		,	m_LightParam1(CFloat4::ZERO)
		,	m_Color(CFloat3::ONE)
		{
		}

		// Directional or Point light
		SPerLightData(const CFloat3 &posDir, const CFloat3 &color)
		:	m_LightParam0(posDir, 0.0f)
		,	m_LightParam1(CFloat4::ZERO)
		,	m_Color(color)
		{
		}

		// Spot light
		SPerLightData(const CFloat3 &position, const CFloat3 &direction, const CFloat3 &color, float angle, float coneFalloff)
		:	m_LightParam0(position, angle)
		,	m_LightParam1(direction.Normalized(), coneFalloff)
		,	m_Color(color)
		{
		}
	};

	struct	SPerShadowData
	{
		CFloat4x4	m_WorldToShadow0;
		CFloat4x4	m_WorldToShadow1;
		CFloat4x4	m_WorldToShadow2;
		CFloat4x4	m_WorldToShadow3;
		CFloat4		m_ShadowsRanges;
		CFloat2		m_ShadowsAspectRatio0;
		CFloat2		m_ShadowsAspectRatio1;
		CFloat2		m_ShadowsAspectRatio2;
		CFloat2		m_ShadowsAspectRatio3;
		CFloat4		m_ShadowsConstants;
		int			m_ShadowsFlags;

		SPerShadowData()
		:	m_WorldToShadow0(CFloat4x4::IDENTITY)
		,	m_WorldToShadow1(CFloat4x4::IDENTITY)
		,	m_WorldToShadow2(CFloat4x4::IDENTITY)
		,	m_WorldToShadow3(CFloat4x4::IDENTITY)
		,	m_ShadowsRanges(CFloat4::ZERO)
		,	m_ShadowsAspectRatio0(CFloat2::ONE)
		,	m_ShadowsAspectRatio1(CFloat2::ONE)
		,	m_ShadowsAspectRatio2(CFloat2::ONE)
		,	m_ShadowsAspectRatio3(CFloat2::ONE)
		,	m_ShadowsConstants(0.005f, 1.2f, 0, 0)
		,	m_ShadowsFlags(0)
		{
		}
	};

	SLightRenderPass();
	~SLightRenderPass() { }

	bool			Init(	const RHI::PApiManager &apiManager,
							const RHI::SConstantSetLayout &lightInfoLayout,
							const RHI::SConstantSetLayout &shadowsInfoLayout,
							const RHI::PConstantSampler &samplerShadows,
							const RHI::PTexture &defaultShadowTex);
	bool			Update(	const RHI::PApiManager &apiManager);
	bool			UpdateShadowMaps(const RHI::PConstantSampler &samplerShadows);
	bool			UpdateShadowsInfo(const RHI::PApiManager &apiManager);

	// Lights data:
	SLightsInfo					m_LightsInfoData;
	TArray<SPerLightData>		m_DirectionalLights;
	TArray<SPerLightData>		m_SpotLights;
	TArray<SPerLightData>		m_PointLights;

	// GPU info updated by Init and Update:
	RHI::PConstantSet			m_LightsInfoConstantSet;
	RHI::PGpuBuffer				m_LightsInfoBuffer;
	RHI::PGpuBuffer				m_DirectionalLightsBuffer;
	RHI::PGpuBuffer				m_SpotLightsBuffer;
	RHI::PGpuBuffer				m_PointLightsBuffer;

	RHI::PConstantSet			m_ShadowsInfoConstantSet;
	RHI::PConstantSet			m_DummyShadowsInfoConstantSet;
	RHI::PGpuBuffer				m_ShadowsInfoBuffer;

	// Shadows:
	CDirectionalShadows			m_DirectionalShadows;
	SPerShadowData				m_ShadowData;
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
