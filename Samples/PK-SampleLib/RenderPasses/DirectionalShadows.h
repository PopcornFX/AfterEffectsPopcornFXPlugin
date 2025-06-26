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
#include <PK-SampleLib/RenderPasses/DownSampleTexture.h>
#include <PK-SampleLib/SampleScene/Entities/MeshEntity.h>
#include <PK-SampleLib/RenderIntegrationRHI/RHIGraphicResources.h>

#include <pk_rhi/include/FwdInterfaces.h>
#include <pk_rhi/include/interfaces/IFrameBuffer.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

class	CDirectionalShadows
{
public:
	CDirectionalShadows();
	~CDirectionalShadows();

	bool		Init(	const RHI::PApiManager &apiManager,
						const RHI::SConstantSetLayout &sceneInfoLayout,
						const RHI::PConstantSampler &samplerRT,
						const RHI::SConstantSetLayout &samplerRTLayout,
						const RHI::PGpuBuffer &fullScreenQuadVbo);
	bool		UpdateSceneInfo(const PKSample::SSceneInfoData &sceneInfoData);
	bool		InitFrameUpdateSceneInfo(	const CFloat3 &lightDir,
 											ECoordinateFrame drawFrame,
											CShaderLoader &loader,
											const PKSample::SMesh &backdrop,
											bool castShadows);
	void		AddBBox(const CAABB &bbox, const CFloat4x4 &transform, bool castShadows);
	bool		FinalizeFrameUpdateSceneInfo();

	bool		IsSliceValidForDraw(u32 sliceIdx) const { return m_CascadedShadows[sliceIdx].m_IsValid; }
	bool		IsAnySliceValidForDraw() const;
	bool		BeginDrawShadowRenderPass(const RHI::PCommandBuffer &commandBuffer, u32 cascadeIdx, const PKSample::SMesh *backdrop = null);
	bool		EndDrawShadowRenderPass(const RHI::PCommandBuffer &commandBuffer, u32 cascadeIdx);

	void		SetShadowsInfo(float bias, float variancePower, bool enableShadows, bool enableVariance, bool debugShadows, const CUint2 &resolution);
	void		SetCascadeShadowsSettings(u32 cascadeIdx, float sceneRangeRatio, float minDistance);
	bool		UpdateShadowsSettings();

	const RHI::PConstantSet	&GetDepthConstSet(u32 sliceIdx) const { return m_CascadedShadows[sliceIdx].m_ShadowMap.m_SamplerConstantSet; }
	const RHI::PTexture		&GetDepthTexture(u32 sliceIdx) const { return m_CascadedShadows[sliceIdx].m_ShadowMap.m_RenderTarget->GetTexture(); }
	const CFloat4x4			&GetWorldToShadow(u32 sliceIdx) const { return m_CascadedShadows[sliceIdx].m_WorldToShadow; }
	const RHI::PRenderPass	&GetRenderPass() const { return m_RenderPass; }
	u32						GetShadowSubpassIdx() const { return 0; }
	const RHI::PConstantSet	&GetSceneInfoConstSet(u32 sliceIdx) const { return m_CascadedShadows[sliceIdx].m_SceneInfoConstantSet; }
	float					GetDepthRange(u32 sliceIdx) const { return m_CascadedShadows[sliceIdx].m_DepthRangeMax; }
	CFloat2					GetAspectRatio(u32 sliceIdx) const { return m_CascadedShadows[sliceIdx].m_AspectRatio; }
	float					GetShadowBias() const { return m_ShadowBias; }
	float					GetShadowVariancePower() const { return m_ShadowVariancePower; }
	float					GetShadowEnabled() const { return m_EnableShadows; }
	float					GetShadowVarianceEnabled() const { return m_EnableVariance; }
	float					GetDebugShadowEnabled() const { return m_DebugShadows; }
	u32						CascadeShadowCount() const { return m_CascadedShadows.Count(); }
	TMemoryView<const RHI::SRenderTargetDesc>	GetFrameBufferLayout() const;

private:
	struct	SCascadedShadowSlice
	{
		RHI::PConstantSet					m_SceneInfoConstantSet;
		RHI::PGpuBuffer						m_SceneInfoBuffer;

		RHI::PFrameBuffer					m_FrameBuffer;

		RHI::PConstantSet					m_BlurHConstantSet;
		RHI::PConstantSet					m_BlurVConstantSet;

		SSamplableRenderTarget				m_ShadowMap;
		SSamplableRenderTarget				m_DepthMap;
		SSamplableRenderTarget				m_IntermediateRt;

		CFloat4x4							m_View;
		CFloat4x4							m_InvView;
		CFloat4x4							m_Proj;
		CFloat4x4							m_InvProj;
		CFloat4x4							m_WorldToShadow;

		CAABB								m_ShadowSliceViewAABB;
		CFloat2								m_AspectRatio = CFloat2(1);

		float								m_SceneRangeRatio;
		float								m_MinDistance;

		float								m_DepthRangeMin;
		float								m_DepthRangeMax;
		// Shadow valid only if there is something to render:
		bool								m_IsValid;

		SCascadedShadowSlice()
		:	m_SceneInfoConstantSet(null)
		,	m_SceneInfoBuffer(null)
		,	m_FrameBuffer(null)
		,	m_BlurHConstantSet(null)
		,	m_BlurVConstantSet(null)
		,	m_View(CFloat4x4::IDENTITY)
		,	m_InvView(CFloat4x4::IDENTITY)
		,	m_Proj(CFloat4x4::IDENTITY)
		,	m_InvProj(CFloat4x4::IDENTITY)
		,	m_WorldToShadow(CFloat4x4::IDENTITY)
		,	m_ShadowSliceViewAABB(CAABB::DEGENERATED)
		,	m_AspectRatio(1)
		,	m_SceneRangeRatio(0)
		,	m_MinDistance(0)
		,	m_DepthRangeMin(0)
		,	m_DepthRangeMax(0)
		,	m_IsValid(false)
		{
		}
	};

	bool	_CreateRenderPass();
	bool	_CreateMeshBackdropRenderStates(CShaderLoader &loader, ECoordinateFrame drawFrame, ECoordinateFrame meshFrame);
	bool	_CreateSliceData(SCascadedShadowSlice &slice);
	CAABB	_GetFrustumBBox(CFloat4x4 invViewProj, float nearPlane, float farPlane, const CFloat4x4 &postProjTransform = CFloat4x4::IDENTITY);
	CSphere	_GetFrustumBSphere(CFloat4x4 invViewProj, float farPlane, const CFloat4x4 &postProjTransform = CFloat4x4::IDENTITY);
	void	_ExtendCasterBBox(CAABB &caster, const CAABB &receiver, const CFloat3 &lightDirection) const;

	RHI::PApiManager					m_ApiManager;

	PKSample::SSceneInfoData			m_SceneInfoData;
	CAABB								m_WorldCameraFrustumBBox;

	RHI::PRenderState					m_BackdropShadowRenderState;
	RHI::SConstantSetLayout				m_SceneInfoLayout;

	RHI::SConstantSetLayout				m_BlurConstSetLayout;
	RHI::PRenderState					m_BlurHRenderState;
	RHI::PRenderState					m_BlurVRenderState;

	RHI::PRenderPass					m_RenderPass;

	RHI::PConstantSampler				m_SamplerRT;
	RHI::SConstantSetLayout				m_SamplerRTLayout;

	RHI::PGpuBuffer						m_FullScreenQuadBuffer;

	TStaticArray<SCascadedShadowSlice, 4>	m_CascadedShadows;

	CFloat3								m_LightDir;

	ECoordinateFrame					m_DrawCoordFrame;
	ECoordinateFrame					m_BackdropCoordFrame;
	CFloat4x4							m_MeshToDrawFrame;

	CFloat4x4							m_LightTransform;
	CAABB								m_CasterLightAlignedBBox;

	CAABB								m_ReceiverViewAlignedBBox;
	CAABB								m_ReceiverWorldAlignedBBox;
	CAABB								m_CasterViewAlignedBBox;
	CAABB								m_CasterWorldAlignedBBox;

	float								m_ShadowBias;
	float								m_ShadowVariancePower;
	bool								m_EnableShadows;
	bool								m_EnableVariance;
	bool								m_DebugShadows;
	CUint2								m_ShadowMapResolution = CUint2(0);

	RHI::EPixelFormat					m_IdealDepthFormat;
	RHI::EPixelFormat					m_IdealShadowFormat;
	RHI::SRenderTargetDesc				m_FrameBufferLayout[3];
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
