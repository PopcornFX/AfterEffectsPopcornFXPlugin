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

#include "LightEntity.h"

#include <pk_rhi/include/AllInterfaces.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

SLightRenderPass::SLightRenderPass()
:	m_LightsInfoConstantSet(null)
,	m_LightsInfoBuffer(null)
,	m_DirectionalLightsBuffer(null)
,	m_SpotLightsBuffer(null)
,	m_PointLightsBuffer(null)
,	m_ShadowsInfoConstantSet(null)
,	m_DummyShadowsInfoConstantSet(null)
,	m_ShadowsInfoBuffer(null)
{
}

//----------------------------------------------------------------------------

bool	SLightRenderPass::Init(	const RHI::PApiManager &apiManager,
								const RHI::SConstantSetLayout &lightInfoLayout,
								const RHI::SConstantSetLayout &shadowsInfoLayout,
								const RHI::PConstantSampler &samplerShadows,
								const RHI::PTexture &defaultShadowTex)
{
	m_LightsInfoConstantSet = apiManager->CreateConstantSet(RHI::SRHIResourceInfos("LightsInfo Constant Set"), lightInfoLayout);
	m_ShadowsInfoConstantSet = apiManager->CreateConstantSet(RHI::SRHIResourceInfos("ShadowsInfo Constant Set"), shadowsInfoLayout);
	m_DummyShadowsInfoConstantSet = apiManager->CreateConstantSet(RHI::SRHIResourceInfos("ShadowsInfo Constant Set (dummy)"), shadowsInfoLayout);

	if (!PK_VERIFY(	m_LightsInfoConstantSet != null &&
					m_ShadowsInfoConstantSet != null &&
					m_DummyShadowsInfoConstantSet != null))
		return false;

	if (m_LightsInfoBuffer == null)
	{
		m_LightsInfoBuffer = apiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("LightsInfos Constant Buffer"), RHI::ConstantBuffer, sizeof(SLightsInfo));
		if (m_LightsInfoBuffer == null)
			return false;
	}
	if (m_ShadowsInfoBuffer == null)
	{
		m_ShadowsInfoBuffer = apiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("ShadowsInfos Constant Buffer"), RHI::ConstantBuffer, sizeof(SPerShadowData));
		if (m_ShadowsInfoBuffer == null)
			return false;
	}

	bool	success = true;

	success &= m_DummyShadowsInfoConstantSet->SetConstants(m_ShadowsInfoBuffer, 0);
	success &= m_DummyShadowsInfoConstantSet->SetConstants(samplerShadows, defaultShadowTex, 1);
	success &= m_DummyShadowsInfoConstantSet->SetConstants(samplerShadows, defaultShadowTex, 2);
	success &= m_DummyShadowsInfoConstantSet->SetConstants(samplerShadows, defaultShadowTex, 3);
	success &= m_DummyShadowsInfoConstantSet->SetConstants(samplerShadows, defaultShadowTex, 4);
	success &= m_DummyShadowsInfoConstantSet->UpdateConstantValues();

	if (!PK_VERIFY(success))
		return false;
	return true;
}

//----------------------------------------------------------------------------

bool	SLightRenderPass::Update(const RHI::PApiManager &apiManager)
{
	const u32	directionalLightsDataSize = PKMax(m_DirectionalLights.Count(), 1U) * sizeof(SPerLightData);
	const u32	spotLightsDataSize = PKMax(m_SpotLights.Count(), 1U) * sizeof(SPerLightData);
	const u32	pointLightsDataSize = PKMax(m_PointLights.Count(), 1U) * sizeof(SPerLightData);
	const bool	needReinitDirectional =	m_DirectionalLightsBuffer == null || directionalLightsDataSize > m_DirectionalLightsBuffer->GetByteSize();
	const bool	needReinitSpot = m_SpotLightsBuffer == null || spotLightsDataSize > m_SpotLightsBuffer->GetByteSize();
	const bool	needReinitPoint = m_PointLightsBuffer == null || pointLightsDataSize > m_PointLightsBuffer->GetByteSize();
	const bool	needInitialize = needReinitDirectional || needReinitSpot || needReinitPoint;

	if (needInitialize)
	{
		if (needReinitDirectional)
		{
			m_DirectionalLightsBuffer = apiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("Directional Lights Buffer"), RHI::RawBuffer, directionalLightsDataSize);
			if (m_DirectionalLightsBuffer == null)
				return false;
		}
		if (needReinitSpot)
		{
			m_SpotLightsBuffer = apiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("Spot Lights Buffer"), RHI::RawBuffer, spotLightsDataSize);
			if (m_SpotLightsBuffer == null)
				return false;
		}
		if (needReinitPoint)
		{
			m_PointLightsBuffer = apiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("Point Lights Buffer"), RHI::RawBuffer, pointLightsDataSize);
			if (m_PointLightsBuffer == null)
				return false;
		}

		bool	success = true;

		success &= m_LightsInfoConstantSet->SetConstants(m_LightsInfoBuffer, 0);
		success &= m_LightsInfoConstantSet->SetConstants(m_DirectionalLightsBuffer, 1);
		success &= m_LightsInfoConstantSet->SetConstants(m_SpotLightsBuffer, 2);
		success &= m_LightsInfoConstantSet->SetConstants(m_PointLightsBuffer, 3);
		success &= m_LightsInfoConstantSet->UpdateConstantValues();

		if (!PK_VERIFY(success))
			return false;
	}

	// Fill light info data:
	m_LightsInfoData.m_DirectionalLightsCount = m_DirectionalLights.Count();
	m_LightsInfoData.m_SpotLightsCount = m_SpotLights.Count();
	m_LightsInfoData.m_PointLightsCount = m_PointLights.Count();

	SLightsInfo		*lightConstants = static_cast<SLightsInfo*>(apiManager->MapCpuView(m_LightsInfoBuffer));

	if (lightConstants == null)
		return false;

	*lightConstants = m_LightsInfoData;

	if (!apiManager->UnmapCpuView(m_LightsInfoBuffer))
		return false;

	// Fill data for each light:
	if (!m_DirectionalLights.Empty())
	{
		void		*directionalLightsPtr = apiManager->MapCpuView(m_DirectionalLightsBuffer, 0, directionalLightsDataSize);

		if (directionalLightsPtr == null)
			return false;

		Mem::Copy(directionalLightsPtr, m_DirectionalLights.RawDataPointer(), m_DirectionalLights.CoveredBytes());

		if (!apiManager->UnmapCpuView(m_DirectionalLightsBuffer))
			return false;
	}
	if (!m_SpotLights.Empty())
	{
		void		*spotLightsPtr = apiManager->MapCpuView(m_SpotLightsBuffer, 0, spotLightsDataSize);

		if (spotLightsPtr == null)
			return false;

		Mem::Copy(spotLightsPtr, m_SpotLights.RawDataPointer(), m_SpotLights.CoveredBytes());

		if (!apiManager->UnmapCpuView(m_SpotLightsBuffer))
			return false;
	}
	if (!m_PointLights.Empty())
	{
		void		*pointLightsPtr = apiManager->MapCpuView(m_PointLightsBuffer, 0, pointLightsDataSize);

		if (pointLightsPtr == null)
			return false;

		Mem::Copy(pointLightsPtr, m_PointLights.RawDataPointer(), m_PointLights.CoveredBytes());

		if (!apiManager->UnmapCpuView(m_PointLightsBuffer))
			return false;
	}
	return true;
}

//----------------------------------------------------------------------------

bool		SLightRenderPass::UpdateShadowMaps(const RHI::PConstantSampler &samplerShadows)
{
	bool	success = true;

	success &= m_ShadowsInfoConstantSet->SetConstants(m_ShadowsInfoBuffer, 0);
	success &= m_ShadowsInfoConstantSet->SetConstants(samplerShadows, m_DirectionalShadows.GetDepthTexture(0), 1);
	success &= m_ShadowsInfoConstantSet->SetConstants(samplerShadows, m_DirectionalShadows.GetDepthTexture(1), 2);
	success &= m_ShadowsInfoConstantSet->SetConstants(samplerShadows, m_DirectionalShadows.GetDepthTexture(2), 3);
	success &= m_ShadowsInfoConstantSet->SetConstants(samplerShadows, m_DirectionalShadows.GetDepthTexture(3), 4);
	success &= m_ShadowsInfoConstantSet->UpdateConstantValues();
	return success;
}

//----------------------------------------------------------------------------

bool		SLightRenderPass::UpdateShadowsInfo(const RHI::PApiManager &apiManager)
{
	// Fill shadow info data:
	m_ShadowData.m_WorldToShadow0 = m_DirectionalShadows.GetWorldToShadow(0);
	m_ShadowData.m_WorldToShadow1 = m_DirectionalShadows.GetWorldToShadow(1);
	m_ShadowData.m_WorldToShadow2 = m_DirectionalShadows.GetWorldToShadow(2);
	m_ShadowData.m_WorldToShadow3 = m_DirectionalShadows.GetWorldToShadow(3);
	m_ShadowData.m_ShadowsRanges.x() = m_DirectionalShadows.GetDepthRange(0);
	m_ShadowData.m_ShadowsRanges.y() = m_DirectionalShadows.GetDepthRange(1);
	m_ShadowData.m_ShadowsRanges.z() = m_DirectionalShadows.GetDepthRange(2);
	m_ShadowData.m_ShadowsRanges.w() = m_DirectionalShadows.GetDepthRange(3);
	m_ShadowData.m_ShadowsAspectRatio0 = m_DirectionalShadows.GetAspectRatio(0);
	m_ShadowData.m_ShadowsAspectRatio1 = m_DirectionalShadows.GetAspectRatio(1);
	m_ShadowData.m_ShadowsAspectRatio2 = m_DirectionalShadows.GetAspectRatio(2);
	m_ShadowData.m_ShadowsAspectRatio3 = m_DirectionalShadows.GetAspectRatio(3);
	m_ShadowData.m_ShadowsConstants.x() = m_DirectionalShadows.GetShadowBias();
	m_ShadowData.m_ShadowsConstants.y() = m_DirectionalShadows.GetShadowVariancePower();

	m_ShadowData.m_ShadowsFlags = 0;
	if (m_DirectionalShadows.GetShadowEnabled())
		m_ShadowData.m_ShadowsFlags |= 1;
	if (m_DirectionalShadows.GetShadowVarianceEnabled())
		m_ShadowData.m_ShadowsFlags |= 2;
	if (m_DirectionalShadows.GetDebugShadowEnabled())
		m_ShadowData.m_ShadowsFlags |= 4;

	SPerShadowData *shadowConstants = static_cast<SPerShadowData *>(apiManager->MapCpuView(m_ShadowsInfoBuffer));

	if (shadowConstants == null)
		return false;

	*shadowConstants = m_ShadowData;

	if (!apiManager->UnmapCpuView(m_ShadowsInfoBuffer))
		return false;

	return true;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
