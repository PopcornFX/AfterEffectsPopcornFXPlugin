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

#include <PK-SampleLib/ShaderDefinitions/BasicSceneShaderDefinitions.h>

#include <pk_rhi/include/RHI.h>
#include <pk_rhi/include/AllInterfaces.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

class	CGBuffer
{
public:
	struct	SInOutRenderTargets
	{
		// ------------------------------------
		// Render targets needed in the frame buffer before calling the UpdateFrameBuffer method
		// ------------------------------------
		// Render targets sampled for the lighting render-pass:
		CGuid									m_SamplableDepthRtIdx;
		const SSamplableRenderTarget			*m_SamplableDepthRt;

		CGuid									m_SamplableDiffuseRtIdx;
		const SSamplableRenderTarget			*m_SamplableDiffuseRt;

		CGuid									m_SamplableShadowRtIdx;
		const SSamplableRenderTarget			*m_SamplableShadowRt;

		// All the RT to sample for the merging render-pass (that will also be the output of the opaque render-pass)
		TMemoryView<const CGuid>				m_OpaqueRTsIdx;
		TMemoryView<SSamplableRenderTarget>		m_OpaqueRTs;

		// ------------------------------------
		// Merge output buffer, if m_OutputRtIdx == CGuid::INVALID then the output buffer is created
		// otherwise the merge output is done in this frame buffer RT
		// ------------------------------------
		CGuid									m_OutputRtIdx;

		SInOutRenderTargets()
			: m_SamplableDepthRt(null)
			, m_SamplableDiffuseRt(null)
		{
		}
	};

	CGBuffer();
	~CGBuffer();

	// samplableDepthBuffIdx is the idx depth buffer that we can sample while having the real depth buffer bound:
	// If we could avoid that and have just one depth buffer it would be a lot better
	bool		Init(	const RHI::PApiManager &apiManager,
						const RHI::PConstantSampler &samplerRT,
						const RHI::SConstantSetLayout &samplerRTLayout,
						const RHI::SConstantSetLayout &sceneInfoLayout);

	bool		UpdateFrameBuffer(	const TMemoryView<const RHI::PFrameBuffer> &frameBuffers,
									TArray<RHI::ELoadRTOperation> &loadOP,
									TArray<RHI::SFrameBufferClearValue> &clearValues,
									bool clearMergingBuffer,
									const SInOutRenderTargets *inOut);

	bool		AddSubPasses(const RHI::PRenderPass &renderPass, const SInOutRenderTargets *inOut);

	bool		CreateRenderStates(	const TMemoryView<const RHI::SRenderTargetDesc> &frameBufferLayout,
									CShaderLoader &shaderLoader,
									const RHI::PRenderPass &renderPass);

	void		SetClearMergeBufferColor(const CFloat4 &clearColor);

private:
	bool		CreateLightSphere(u32 subdivs);
	bool		CreateDecalCube();
	bool		CreateGBufferRenderStates(const TMemoryView<const RHI::SRenderTargetDesc> &frameBufferLayout, CShaderLoader &shaderLoader, const RHI::PRenderPass &renderPass);
	bool		CreateLightingRenderStates(const TMemoryView<const RHI::SRenderTargetDesc> &frameBufferLayout, CShaderLoader &shaderLoader, const RHI::PRenderPass &renderPass);
	bool		CreateParticleLightingRenderStates(const TMemoryView<const RHI::SRenderTargetDesc> &frameBufferLayout, CShaderLoader &shaderLoader, const RHI::PRenderPass &renderPass);
	bool		CreateMergingRenderStates(const TMemoryView<const RHI::SRenderTargetDesc> &frameBufferLayout, CShaderLoader &shaderLoader, const RHI::PRenderPass &renderPass);

	bool		AddGBufferSubPass(const RHI::PRenderPass &beforePostFX, const TMemoryView<const CGuid> &outputRTs);
	bool		AddDecalSubPass(const RHI::PRenderPass &beforePostFX, const TMemoryView<const CGuid> &outputRTs, CGuid samplableDepthBufferIdx);
	bool		AddLightingSubPass(const RHI::PRenderPass &beforePostFX, CGuid samplableDepthBufferIdx, CGuid samplableDiffuseBufferIdx);
	bool		AddMergingSubPass(const RHI::PRenderPass &beforePostFX, const TMemoryView<const CGuid> &mergingToSampleRT);

	RHI::PApiManager							m_ApiManager;

public:
	static const RHI::EPixelFormat	s_MergeBufferFormat;
	static const RHI::EPixelFormat	s_LightAccumBufferFormat;
	static const RHI::EPixelFormat	s_DepthBufferFormat;
	static const RHI::EPixelFormat	s_NormalRoughMetalBufferFormat;
	// -----------------------------------------------
	// Render targets indices:
	// -----------------------------------------------
	// Always present in the deferred pass:
	CGuid							m_MergeBufferIndex;
	SSamplableRenderTarget			m_Merge;
	CGuid							m_DepthBufferIndex;
	SSamplableRenderTarget			m_Depth;
	CGuid							m_LightAccumBufferIndex;
	SSamplableRenderTarget			m_LightAccu;
	CGuid							m_NormalRoughMetalBufferIndex;
	SSamplableRenderTarget			m_NormalRoughMetal;
	// -----------------------------------------------

	// -----------------------------------------------
	// Subpasses idx:
	// -----------------------------------------------
	CGuid							m_GBufferSubPassIdx;
	CGuid							m_DecalSubPassIdx;
	CGuid							m_LightingSubPassIdx;
	CGuid							m_MergingSubPassIdx;
	// -----------------------------------------------

	u32								m_InitializedRP;

	// Full screen quad buffer
	Utils::GpuBufferViews			m_QuadBuffers;

	RHI::PConstantSampler			m_SamplerRT;
	RHI::SConstantSetLayout			m_SamplerRTLayout;

	RHI::SConstantSetLayout			m_SceneInfoLayout;

	// -----------------------------------------------
	// IF InitRP_GBuffer:
	// -----------------------------------------------
	// - In the sub-pass "G-Buffer" :
	RHI::SConstantSetLayout			m_GBufferMeshInfoConstLayout[GBufferCombination_Count]; // Diffuse - Diffuse, Normal - Diffuse, Spec - Diffuse, Normal, Spec
	RHI::PRenderState				m_GBufferMaterials[GBufferCombination_Count];
	// - In the sub-pass "Lighting" :
	RHI::SConstantSetLayout			m_LightingSamplersConstLayout;
	RHI::PConstantSet				m_LightingSamplersSet;
	RHI::SConstantSetLayout			m_LightInfoConstLayout;
	RHI::SConstantSetLayout			m_ShadowsInfoConstLayout;
	RHI::PRenderState				m_LightsRenderState;
	RHI::SConstantSetLayout			m_EnvironmentMapLayout;
	RHI::SConstantSetLayout			m_BRDFLUTLayout;
	// - Particle specific shaders (this should replace the previous lighting shaders):
	RHI::PRenderState				m_ParticlePointLightRenderState;
	// - In the sub-pass "Merging"
	RHI::SConstantSetLayout			m_MergingSamplersConstLayout;
	RHI::PConstantSet				m_MergingSamplersSet;
	RHI::PRenderState				m_MergingRenderState;

	RHI::SFrameBufferClearValue		m_MergeClearValue;
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
