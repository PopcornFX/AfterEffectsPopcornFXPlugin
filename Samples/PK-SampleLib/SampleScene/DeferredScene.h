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

#include <stdint.h>
#include <PK-SampleLib/SampleScene/AbstractGraphicScene.h>
#include <PK-SampleLib/Camera.h>
#include <PK-SampleLib/SampleUtils.h>
#include <PK-SampleLib/RenderPasses/DownSampleTexture.h>
#include <PK-SampleLib/RenderPasses/PostFxBloom.h>
#include <PK-SampleLib/RenderPasses/PostFxToneMapping.h>
#include <PK-SampleLib/RenderPasses/GBuffer.h>

#include <PK-SampleLib/RHIRenderParticleSceneHelpers.h>

#include <PK-SampleLib/ShaderLoader.h>
#include <PK-SampleLib/SampleUtils.h>

#include <pk_rhi/include/RHI.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

class	CDeferredScene : public CAbstractGraphicScene
{
public:
	CDeferredScene();
	virtual ~CDeferredScene();

	// G Buffer layout / material

	RHI::PRenderState							GetGBufferMaterial(EGBufferCombination matCombination) const;
	TMemoryView<const RHI::SConstantSetLayout>	GetGBufferSamplersConstLayouts() const { return m_GBuffer.m_GBufferMeshInfoConstLayout; }

	const SSamplableRenderTarget				*GetSamplableDiffuse() const { return &m_GBufferRTs[0]; }
	const SSamplableRenderTarget				*GetSamplableDepth() const { return &m_GBufferRTs[1]; }
	const SSamplableRenderTarget				*GetSamplableNormalRoughMetal() const { return &m_GBuffer.m_NormalRoughMetal; }
	const SSamplableRenderTarget				*GetSamplableToneMapping() const { return &m_ToneMapping.m_OutputRenderTarget; }

	// Light Info Layout
	const RHI::SConstantSetLayout				&GetLightInfoConstLayout() const { return m_GBuffer.m_LightInfoConstLayout; }
	const RHI::SConstantSetLayout				&ShadowInfoConstLayout() const { return m_GBuffer.m_ShadowsInfoConstLayout; }
	const RHI::PConstantSet						GetLightingSamplerSet() const { return m_GBuffer.m_LightingSamplersSet; }

//	CCameraRotate								&GetCamera() { return m_Camera; }
	const CCameraRotate							&GetCamera() const { return m_Camera; }

	TMemoryView<const RHI::SRenderTargetDesc>	GetFrameBufferBeforeBloomLayout() const { return m_BeforeBloomFrameBuffer->GetLayout(); }
	TMemoryView<const RHI::SRenderTargetDesc>	GetSwapChainFrameBufferLayout() const { return m_FinalFrameBuffers.First()->GetLayout(); }

	RHI::PRenderPass							GetRenderPass_BeforeBloom() const { return m_BeforeBloomRenderPass; }
	RHI::PRenderPass							GetRenderPass_Final() const { return m_FinalRenderPass; }

	const CGBuffer								&GetGBuffer() const { return m_GBuffer; }

	const RHI::PConstantSet						&GetSceneInfoConstantSet() const { return m_SceneInfoConstantSet; }
	const RHI::PConstantSet						&GetBRDFLUTSamplerSet() const { return m_BRDFLUTConstantSet; }
	const RHI::PConstantSet						&GetDummyEnvMapSamplerSet() const { return m_DummyCubeConstantSet; }
	const RHI::PConstantSampler					&GetDefaultSampler() const { return m_DefaultSampler; }

	const RHI::PGpuBuffer						&GetSceneInfoBuffer() const { return m_SceneInfoConstantBuffer; }

	const RHI::PTexture							&GetDummyWhiteTexture() const { return m_DummyWhite; }

	bool							InitIFN();

	enum	ERenderTarget
	{
		RenderTarget_Unknown = -1,
		RenderTarget_Depth = 0,
		RenderTarget_Diffuse,
		RenderTarget_Specular,
		RenderTarget_Normal,
		RenderTarget_LightAccum,
		RenderTarget_Scene,

		__MaxRenderTargets
	};

	// Debug mode: TODO

private:
	virtual bool					CreateRenderTargets(bool recreateSwapChain) override;
	virtual bool					CreateRenderPasses() override;
	virtual bool					CreateRenderStates() override;
	virtual bool					CreateFrameBuffers(bool recreateSwapChain) override;

	virtual void					FillCommandBuffer(const RHI::PCommandBuffer &cmdBuff, u32 swapImgIdx) override;

	// Default Render states
	bool							CreateDebugCopyRenderStates();

protected:
	// Pre Render, right after command list started
	virtual void					StartCommandBuffer(const RHI::PCommandBuffer &cmdBuff) = 0;
	// Render your opaque mesh backdrops, if any
	virtual void					RenderMeshBackdrops(const RHI::PCommandBuffer &cmdBuff) = 0;
	// Submit any command lists before the Post Opaque render pass starts (ie. dispatch compute shaders)
	virtual void					PostOpaque(const RHI::PCommandBuffer &cmdBuff) = 0;
	// Render your opaque pass in G Buffer
	virtual void					RenderOpaquePass(const RHI::PCommandBuffer &cmdBuff) = 0;

	// Render your custom decals in GBuffer
	virtual void					RenderDecals(const RHI::PCommandBuffer &cmdBuff) = 0;
	// Render your custom lights in Light Accumulation buffer
	virtual void					RenderLights(const RHI::PCommandBuffer &cmdBuff) = 0;
	// Render your custom rendering post merge (transparent pass, ...)
	virtual void					RenderPostMerge(const RHI::PCommandBuffer &cmdBuff) = 0;
	// Render your distortion map
	virtual void					RenderDistortionMap(const RHI::PCommandBuffer &cmdBuff) = 0;
	// Render your distortion map
	virtual void					RenderPostDistortion(const RHI::PCommandBuffer &cmdBuff) = 0;
	// Render your additional composition (eg. UI)
	virtual void					RenderFinalComposition(const RHI::PCommandBuffer &cmdBuff) = 0;

	virtual void					DrawHUD() override;

	// Camera
	float							m_CameraGUI_Distance;
	CFloat3							m_CameraGUI_Angles;
	CFloat3							m_CameraGUI_LookatPosition;
	bool							m_CameraFlag_ResetProj;		// (eg. on coordinate frame change)

	float							m_DeferredMergingMinAlpha = 1.0f;

	void							UpdateCamera();
	void							UpdateSceneInfo();

	// Background
	CFloat3							m_BackgroundColorRGB;

	void							_UpdateDisplayFps();
	float							_GetDisplayFps() const;

private:
	void							UpdateMaxBloomRTs();

	// Default single sampler constant set layout to sample render targets:
	RHI::SConstantSetLayout			m_DefaultSamplerConstLayout;
	RHI::PConstantSampler			m_DefaultSampler;

	// Scene info constant set:
	PKSample::SSceneInfoData		m_SceneInfoData; // current scene info
	RHI::SConstantSetLayout			m_SceneInfoConstantSetLayout;
	RHI::PConstantSet				m_SceneInfoConstantSet;
	RHI::PGpuBuffer					m_SceneInfoConstantBuffer;

	RHI::PConstantSet				m_BRDFLUTConstantSet;

	RHI::PTexture					m_DummyWhite;
	RHI::PTexture					m_DummyCube;
	RHI::PConstantSet				m_DummyCubeConstantSet;
	RHI::PConstantSampler			m_DummyCubeSampler;

	// Pipeline: Main Render passes
	// -> Pass 1: Differed
	CGBuffer						m_GBuffer;
	CPostFxDistortion				m_Distortion;
	CPostFxBloom					m_Bloom;
	CPostFxToneMapping				m_ToneMapping;

	TStaticArray<SSamplableRenderTarget, SPassDescription::__GBufferRTCount>	m_GBufferRTs;
	TStaticArray<CGuid, SPassDescription::__GBufferRTCount>						m_GBufferRTsIdx;

	TArray<RHI::SFrameBufferClearValue>	m_BeforeBloomClearValues;
	RHI::PRenderPass					m_BeforeBloomRenderPass;
	RHI::PFrameBuffer					m_BeforeBloomFrameBuffer;

	enum	EFinalFrameBuffer
	{
		kMaxSwapChainSize = 3,
	};

	TArray<RHI::SFrameBufferClearValue>									m_FinalClearValues;
	RHI::PRenderPass													m_FinalRenderPass;
	TSemiDynamicArray<RHI::PFrameBuffer, kMaxSwapChainSize>				m_FinalFrameBuffers;

	// Default Render-states

	RHI::PRenderState				m_CopyColorRenderState;
	RHI::PRenderState				m_CopyAlphaRenderState;

	RHI::PRenderState				m_OverdrawHeatmapRenderState;	// Overdraw
	RHI::PConstantSet				m_OverdrawConstantSet;			// Overdraw


	TArray<RHI::PCommandBuffer>		m_CommandBuffers;

	bool							m_IsInit;

	IFileSystem						*m_Controller;

	CCameraRotate					m_Camera;
	CInt2							m_PrevMousePosition;

	// Bloom, these method are still needed ?
	s32								m_UserMaxBloomRenderPass;
	s32								m_MaxBloomRenderPass;

	float							m_DisplayFpsAccTime;
	u32								m_DisplayFpsAccFrames;
	float							m_DisplayFps;

protected:
	// Use for debug intermediate rendering
	ERenderTarget					m_RtToDraw;
	bool							m_ShowAlpha;

	SParticleSceneOptions			m_SceneOptions;

	CConstantNoiseTexture			m_NoiseTexture;
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
