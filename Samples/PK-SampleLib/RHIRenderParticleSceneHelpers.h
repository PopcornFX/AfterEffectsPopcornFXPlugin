#pragma once

#include "PKSample.h"

#include <pk_rhi/include/FwdInterfaces.h>
#include <PK-SampleLib/RenderIntegrationRHI/RHITypePolicy.h>

// Render passes:
#include <PK-SampleLib/RenderPasses/GBuffer.h>
#include <PK-SampleLib/RenderPasses/PostFxDistortion.h>
#include <PK-SampleLib/RenderPasses/PostFxBloom.h>
#include <PK-SampleLib/RenderPasses/PostFxToneMapping.h>
#include <PK-SampleLib/RenderPasses/PostFxFXAA.h>
#include <PK-SampleLib/RenderPasses/PostFxColorRemap.h>
#include <PK-SampleLib/RenderPasses/DirectionalShadows.h>

#include <pk_kernel/include/kr_resources.h>
#include <PK-SampleLib/ShaderDefinitions/EditorShaderDefinitions.h>
#include <PK-SampleLib/SampleScene/Entities/MeshEntity.h>
#include <PK-SampleLib/SampleScene/Entities/LightEntity.h>
#include <PK-SampleLib/Gizmo.h>
#include <PK-SampleLib/SampleScene/Entities/EnvironmentMapEntity.h>

// Debug draw shaders:
#define		DEBUG_DRAW_VERTEX_SHADER_PATH			"./Shaders/DebugDraw.vert"
#define		DEBUG_DRAW_FRAGMENT_SHADER_PATH			"./Shaders/DebugDrawColor.frag"
// Debug draw shaders:
#define		DEBUG_DRAW_VALUE_VERTEX_SHADER_PATH		"./Shaders/DebugDrawValue.vert"
#define		DEBUG_DRAW_VALUE_FRAGMENT_SHADER_PATH	"./Shaders/DebugDrawValue.frag"
#define		DEBUG_DRAW_LINE_VERTEX_SHADER_PATH		"./Shaders/DebugDrawLine.vert"
#define		DEBUG_DRAW_LINE_FRAGMENT_SHADER_PATH	"./Shaders/DebugDrawLine.frag"
// Draw for the post-fx:
#define	FULL_SCREEN_QUAD_VERTEX_SHADER_PATH			"./Shaders/FullScreenQuad.vert"
#define	COPY_FRAGMENT_SHADER_PATH					"./Shaders/Copy.frag"
// Additional resources for the overdraw rendering:
#define	OVERDRAW_HEATMAP_FRAGMENT_SHADER_PATH		"./Shaders/Heatmap.frag"
#define	OVERDRAW_HEATMAP_LUT_TEXTURE_PATH			"./Textures/Overdraw.dds"

#define	BRUSH_BACKDROP_FRAGMENT_SHADER_PATH		"./Shaders/BrushBackdrop.frag"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

struct	SSceneInfoData;

//----------------------------------------------------------------------------

// Overdraw push constants:
struct	SOverdrawInfo
{
	float		m_ScaleFactor;

	SOverdrawInfo(float scaleFactor)
	:	m_ScaleFactor(scaleFactor)
	{
	}
};

struct	SGizmoFlags
{
	bool					m_Enabled;
	EGizmoType				m_Mode;
	bool					m_LocalSpace;
	bool					m_Snap;
	u8						m_SnapPrecision;

	SGizmoFlags()
	:	m_Enabled(true)
	,	m_Mode(GizmoNone)
	,	m_LocalSpace(false)
	,	m_Snap(false)
	,	m_SnapPrecision(3)
	{
	}
};

//----------------------------------------------------------------------------

struct	SParticleSceneOptions
{
	struct	SDistortion
	{
		bool	m_Enable;
		float	m_DistortionIntensity;
		CFloat4 m_ChromaticAberrationMultipliers;

		bool	operator == (const SDistortion &oth) const
		{
			return m_Enable == oth.m_Enable && m_DistortionIntensity == oth.m_DistortionIntensity && m_ChromaticAberrationMultipliers == oth.m_ChromaticAberrationMultipliers;
		}

		SDistortion() : m_Enable(true), m_DistortionIntensity(1.0f), m_ChromaticAberrationMultipliers(0.01f, 0.0125f, 0.015f, 0.0175f) {}

	}	m_Distortion;

	struct	SBloom
	{
		bool						m_Enable;
		float						m_BrightPassValue;
		float						m_Intensity;
		float						m_Attenuation;
		EGaussianBlurCombination	m_BlurTap;
		u32							m_RenderPassCount;

		SBloom()
		:	m_Enable(true)
		,	m_BrightPassValue(1.0f)
		,	m_Intensity(1.0f)
		,	m_Attenuation(1.0f)
		,	m_BlurTap(GaussianBlurCombination_9_Tap)
		,	m_RenderPassCount(6)
		{
		}

		bool	operator == (const SBloom &oth) const
		{
			return	m_Enable == oth.m_Enable &&
					m_BrightPassValue == oth.m_BrightPassValue &&
					m_Intensity == oth.m_Intensity &&
					m_Attenuation == oth.m_Attenuation &&
					m_BlurTap == oth.m_BlurTap &&
					m_RenderPassCount == oth.m_RenderPassCount;
		}
	}	m_Bloom;

	struct	SToneMapping
	{
		bool	m_Enable;
		float	m_Exposure;
		float	m_Saturation;

		SToneMapping() : m_Enable(true), m_Exposure(-0.336472f), m_Saturation(1.f) {} // ln(x / 1.4f)

		bool	operator == (const SToneMapping &oth) const
		{
			return	m_Enable == oth.m_Enable &&
					m_Exposure == oth.m_Exposure &&
					m_Saturation == oth.m_Saturation;
		}
	}	m_ToneMapping;

	struct	SVignetting
	{
		CFloat3	m_Color;
		float	m_ColorIntensity;
		float	m_DesaturationIntensity;
		float	m_Roundness;
		float	m_Smoothness;

		SVignetting() : m_Color(0.f), m_ColorIntensity(0.1f), m_DesaturationIntensity(0.1f), m_Roundness(1.f), m_Smoothness(1.f) {}

		bool	operator == (const SVignetting &oth) const
		{
			return	m_Color == oth.m_Color &&
					m_ColorIntensity == oth.m_ColorIntensity &&
					m_DesaturationIntensity == oth.m_DesaturationIntensity &&
					m_Roundness == oth.m_Roundness &&
					m_Smoothness == oth.m_Smoothness;
		}
	}	m_Vignetting;

	struct	SColorRemap
	{
		bool		m_Enable = true;
		bool		m_ForceSRGBToLinear = false;
		CString		m_RemapTexturePath;

		bool	operator == (const SColorRemap &oth) const
		{
			return	m_Enable == oth.m_Enable &&
					m_ForceSRGBToLinear == oth.m_ForceSRGBToLinear &&
					m_RemapTexturePath == oth.m_RemapTexturePath;
		}
	}	m_ColorRemap;

	struct	SFXAA
	{
		bool	m_Enable;
		bool	m_LumaInAlpha;

		SFXAA() : m_Enable(false), m_LumaInAlpha(true) {}

		bool	operator == (const SFXAA &oth) const
		{
			return	m_Enable == oth.m_Enable &&
					m_LumaInAlpha == oth.m_LumaInAlpha;
		}
	}	m_FXAA;

	struct	SOverdraw
	{
		u32			m_OverdrawUpperRange = 50;
		CString		m_OverdrawTexturePath;

		bool	operator == (const SOverdraw &oth) const
		{
			return	m_OverdrawUpperRange == oth.m_OverdrawUpperRange &&
				m_OverdrawTexturePath == oth.m_OverdrawTexturePath;
		}
	}	m_Overdraw;

	bool	m_Dithering = false;

	bool	operator == (const SParticleSceneOptions &oth) const
	{
		return	m_Distortion == oth.m_Distortion &&
				m_Bloom == oth.m_Bloom &&
				m_ToneMapping == oth.m_ToneMapping &&
				m_Vignetting == oth.m_Vignetting &&
				m_FXAA == oth.m_FXAA &&
				m_Dithering == oth.m_Dithering;
	}
};

//----------------------------------------------------------------------------

struct	SShadowOptions
{
	bool		m_EnableShadows = true;
	bool		m_EnableDebugShadows = false;
	float		m_ShadowBias = 0.005f;
	bool		m_EnableVarianceShadows = true;
	float		m_ShadowVarianceExponent = 10.0f;
	CUint2		m_CascadeShadowResolution = CUint2(1024);
	CFloat4		m_CascadeShadowSceneRangeRatios = CFloat4(0.15f, 0.2f, 0.25f, 0.4f);
	CFloat4		m_CascadeShadowMinDistances = CFloat4(5, 10, 20, 40);

	bool		operator == (const SShadowOptions &oth) const
	{
		return 	m_EnableShadows == oth.m_EnableShadows &&
				m_EnableDebugShadows == oth.m_EnableDebugShadows &&
				m_ShadowBias == oth.m_ShadowBias &&
				m_EnableVarianceShadows == oth.m_EnableVarianceShadows &&
				m_ShadowVarianceExponent == oth.m_ShadowVarianceExponent &&
				m_CascadeShadowResolution == oth.m_CascadeShadowResolution &&
				m_CascadeShadowSceneRangeRatios == oth.m_CascadeShadowSceneRangeRatios &&
				m_CascadeShadowMinDistances == oth.m_CascadeShadowMinDistances;
	}
};

//----------------------------------------------------------------------------

struct	SBackdropsData
{
	class	CLight
	{
	public:
		CLight() { }

		void		SetAmbient(const CFloat3 &color, float intensity = 1.0f);
		void		SetDirectional(const CFloat3 &direction, const CFloat3 &color, float intensity = 1.0f);
		void		SetSpot(const CFloat3 &position, const CFloat3 &direction, float angle, float coneFalloff, const CFloat3 &color, float intensity = 1.0f);
		void		SetPoint(const CFloat3 &position, const CFloat3 &color, float intensity = 1.0f);

		SLightRenderPass::ELightType	Type() const { return m_Type; }
		const CFloat3					&Position() const { return m_Position; }
		const CFloat3					&Direction() const { return m_Direction; }
		float							Angle() const { return m_Angle; }
		float							ConeFalloff() const { return m_ConeFalloff; }
		const CFloat3					&Color() const { return m_Color; }
		float							Intensity() const { return m_Intensity; }

	private:
		SLightRenderPass::ELightType	m_Type = SLightRenderPass::Ambient;

		CFloat3			m_Position = CFloat3::ZERO;		// Not used for directional and ambient
		CFloat3			m_Direction = CFloat3::ZERO;	// Not used for point and ambient
		float			m_Angle = 0.0f;					// Only for spotlights
		float			m_ConeFalloff = 0.0f;			// Only for spotlights

		CFloat3			m_Color = CFloat3::ZERO;
		float			m_Intensity = 0.0f;
	};

	bool								m_ShowGrid = false;
	u32									m_BackdropGridVersion = 0;	// This is incremented each time the backdrops actually change. Avoids having to make inefficient and ineffective compares
	float								m_GridSize = 1;
	u32									m_GridSubdivisions = 1;
	u32									m_GridSubSubdivisions = 1;
	CFloat4								m_GridColor = CFloat4(0.9f, 0.9f, 0.9f, 1.0f);
	CFloat4								m_GridSubColor = CFloat4(0.2f, 0.2f, 0.2f, 1.0f);
	CFloat4x4							m_GridTransforms = CFloat4x4::IDENTITY;

	bool								m_ShowMesh = false;
	bool								m_FollowInstances = false;
	bool								m_CastShadows = false;
	CString								m_MeshPath;
	CGuid								m_MeshLOD = 0;
	CString								m_MeshDiffusePath;
	PCImage								m_MeshDiffuseData;	// can be null, in which case will be loaded using 'm_MeshDiffusePath'
	CString								m_MeshNormalPath;
	PCImage								m_MeshNormalData;
	CString								m_MeshRoughMetalPath;
	PCImage								m_MeshRoughMetalData;
	float								m_MeshRoughness = 1.0f;
	float								m_MeshMetalness = 0.0f;
	u32									m_MeshVertexColorsMode = 0;	// 0: no vertex color, 1: color, 2: alpha
	u32									m_MeshVertexColorsSet = 0;
	CFloat4x4							m_MeshBackdropTransforms = CFloat4x4::IDENTITY;
	TSemiDynamicArray<u32, 4>			m_MeshFilteredSubmeshes;

	TArray<CFloat4x4>					m_FXInstancesTransforms;
	TArray<SSkinnedMeshData>			m_FXInstancesSkinnedDatas;

	TArray<CLight>						m_Lights;

	CString								m_EnvironmentMapPath;
	float								m_EnvironmentMapIntensity = 1.0f;
	CFloat3								m_EnvironmentMapColor = CFloat3::ONE;
	float								m_EnvironmentMapRotation = 0.0f;

	CFloat3								m_BackgroundColorTop = CFloat3::ZERO;
	CFloat3								m_BackgroundColorBottom = CFloat3::ZERO;
	bool								m_BackgroundUsesEnvironmentMap = false;
	bool								m_EnvironmentMapAffectsAmbient = false;
	float								m_EnvironmentMapBlur = 0.0f;

	// Temp scaffolding: Shadow config, this should ideally be per-light, for now only have a single one
	SShadowOptions						m_ShadowOptions;

	SBackdropsData() { }

	bool	IsGridSameGeometry(const SBackdropsData &oth) const
	{
		return	m_GridSize == oth.m_GridSize &&
				m_GridSubdivisions == oth.m_GridSubdivisions &&
				m_GridSubSubdivisions == oth.m_GridSubSubdivisions &&
				m_GridTransforms == oth.m_GridTransforms;
	}
};

//----------------------------------------------------------------------------

class	CRHIParticleSceneRenderHelper
{
public:
	enum	EInitRenderPasses
	{
		InitRP_GBuffer			= (1U << 0U),			// Do not render opaque geometry
		InitRP_Distortion		= (1U << 1U),			// Do not render the distortion
		InitRP_Bloom			= (1U << 2U),			// Do not render the bloom
		InitRP_ToneMapping		= (1U << 3U),			// Do not render the tone-mapping
		InitRP_ColorRemap		= (1U << 4U),			// Do not render the color remap
		InitRP_FXAA				= (1U << 5U),			// Do not render the FXAA
		InitRP_Debug			= (1U << 6U),			// Do not render the debug render passes
		__InitRP_Count			= 7U,
		InitRP_All				= ~((~0U) << __InitRP_Count),
		InitRP_AllExceptBloom	= InitRP_All ^ InitRP_Bloom,
		InitRP_None				= 0,
	};

	enum	ERenderTargetDebug
	{
		RenderTargetDebug_NoDebug = 0,
		RenderTargetDebug_Diffuse,
		RenderTargetDebug_Depth,
		RenderTargetDebug_Normal,
		RenderTargetDebug_NormalUnpacked,
		RenderTargetDebug_Roughness,
		RenderTargetDebug_Metalness,
		RenderTargetDebug_LightAccum,
		RenderTargetDebug_Distortion,
		RenderTargetDebug_PostMerge,
		RenderTargetDebug_ShadowCascades,
		__MaxRenderTargets,
	};

	CRHIParticleSceneRenderHelper();
	virtual ~CRHIParticleSceneRenderHelper();

	// Init and update:
	bool						Init(	const RHI::PApiManager	&apiManager,
										CShaderLoader			*shaderLoader,
										CResourceManager		*resourceManager,
										u32						initRP = InitRP_All);
	bool						Resize(TMemoryView<const RHI::PRenderTarget> finalRts);

	bool						SetupPostFX_Distortion(const SParticleSceneOptions::SDistortion &config, bool firstInit);
	bool						SetupPostFX_Bloom(const SParticleSceneOptions::SBloom &config, bool firstInit, bool swapChainChanged);
	bool						SetupPostFX_ToneMapping(const SParticleSceneOptions::SToneMapping &config, const SParticleSceneOptions::SVignetting &configVignetting, bool dithering, bool firstInit, bool precomputeLuma = true);
	bool						SetupPostFX_ColorRemap(const SParticleSceneOptions::SColorRemap &config,  bool firstInit);
	bool						SetupPostFX_FXAA(const SParticleSceneOptions::SFXAA &config, bool firstInit, bool lumaInAlpha = true);
	bool						SetupPostFX_Overdraw(const SParticleSceneOptions::SOverdraw &config, bool firstInit);
	bool						SetupShadows();

	void						EnablePostFX(bool enabled) { m_EnablePostFX = enabled; }
	bool						PostFXEnabled() const { return m_EnablePostFX; }
	bool						PostFXEnabled_Distortion() const { return m_EnablePostFX && m_EnableDistortion; }
	bool						PostFXEnabled_Bloom() const { return m_EnablePostFX && m_EnableBloom; }
	bool						PostFXEnabled_ToneMapping() const { return m_EnablePostFX && m_EnableToneMapping; }
	bool						PostFXEnabled_ColorRemap() const { return m_EnablePostFX && m_EnableColorRemap; }
	bool						PostFXEnabled_FXAA() const { return m_EnablePostFX && m_EnableFXAA; }

	void						EnableBrushBackground(bool enabled) { m_EnableBrushBackground = enabled; }

	void						SetDeferredMergingMinAlpha(float minAlpha) { m_DeferredMergingMinAlpha = minAlpha; }

	bool						SetSceneInfo(const SSceneInfoData &sceneInfoData, ECoordinateFrame coordinateFrame);
	bool						SetBackdropInfo(const SBackdropsData &backdropData, ECoordinateFrame coordinateFrame);
	const SBackdropsData		&GetBackdropsInfo() const { return m_BackdropsData; }
	const RHI::PConstantSet		&GetSceneInfoConstantSet() const { return m_SceneInfoConstantSet; }

	void						SetCurrentPackResourceManager(CResourceManager *resourceManager);

	void						SetBackGroundColor(const CFloat3 &top, const CFloat3 &bottom);
	void						SetBackGroundTexture(const RHI::PTexture &background);
	CGBuffer					&GetDeferredSetup();

	RHI::PFrameBuffer			GetFinalFrameBuffers(u32 index) { return m_FinalFrameBuffers[index]; }

	PKSample::CEnvironmentMap	&GetEnvironmentMap() { return m_EnvironmentMap; }
	PKSample::SBackdropsData	&GetBackDropsData() { return m_BackdropsData; }

	// Render:
	bool						RenderScene(ERenderTargetDebug		renderTargetDebug,
											const SRHIDrawOutputs	&drawOutputs,
											u32						finalRtIdx);

	virtual bool				DrawOnDebugBuffer(const RHI::PCommandBuffer &cmdBuff) { (void)cmdBuff; return true; }
	virtual bool				DrawOnFinalBuffer(const RHI::PCommandBuffer &cmdBuff) { (void)cmdBuff; return true; }
	virtual bool				GenerateGUI(const SRHIDrawOutputs &drawOutputs) { (void)drawOutputs; return true; }

	void						BindMiscLinesOpaque(const TMemoryView<const CFloat3> &positions, const TMemoryView<const CFloat4> &colors);
	void						BindMiscLinesAdditive(const TMemoryView<const CFloat3> &positions, const TMemoryView<const CFloat4> &colors);
//	void						BindMiscTrisOpaque(const TMemoryView<const CFloat3> &positions, const TMemoryView<const CFloat4> &colors, const TMemoryView<const u32> &indices);
//	void						BindMiscTrisAdditive(const TMemoryView<const CFloat3> &positions, const TMemoryView<const CFloat4> &colors, const TMemoryView<const u32> &indices);

	const SSamplableRenderTarget	&GBufferRT(SPassDescription::EGBufferRT renderTargetType) const { PK_ASSERT(renderTargetType < SPassDescription::__GBufferRTCount); return m_GBufferRTs[static_cast<u32>(renderTargetType)]; }
	const CGuid						GBufferRTIdx(SPassDescription::EGBufferRT renderTargetType) const { PK_ASSERT(renderTargetType < SPassDescription::__GBufferRTCount); return m_GBufferRTsIdx[static_cast<u32>(renderTargetType)]; }
	const RHI::PGpuBuffer			&SceneInfoBuffer() const { return m_SceneInfoConstantBuffer; }

	void						ClearMiscGeom();

protected:
	//Debug Draw Functions
	virtual void				_RenderMeshBackdropDebug(const RHI::PCommandBuffer &cmdBuff);
	virtual bool				_CullParticleDraw(bool debug, PKSample::ESampleLibGraphicResources_RenderPass renderPass, const PKSample::SRHIDrawCall &drawCall, bool validRenderState);
	virtual RHI::PRenderState	_GetDebugRenderState(	PKSample::ESampleLibGraphicResources_RenderPass renderPass,
														const PKSample::SRHIDrawCall &drawCall,
														PKSample::EShaderOptions shaderOptions,
														PKSample::ESampleLibGraphicResources_RenderPass *cacheRenderPass);
	virtual bool				_CreateFinalDebugRenderStates(	TMemoryView<const RHI::SRenderTargetDesc> frameBufferLayout,
																RHI::PRenderPass renderPass,
																CGuid mergeSubPassIdx, CGuid finalSubPassIdx);
	virtual bool				_BindDebugVertexBuffer(const PKSample::SRHIDrawCall &drawCall, const RHI::PCommandBuffer &cmdBuff);
	virtual bool				_BindDebugConstantSets(const RHI::PRenderState &renderState, const PKSample::SRHIDrawCall &drawCall, TStaticCountedArray<RHI::PConstantSet, 0x10> &constantSets);

	// Render the draw-calls:
	void			_RenderParticles(	bool debugMode,
										CRenderPassArray renderPass,
										const TArray<SRHIDrawCall> &drawCalls,
										const RHI::PCommandBuffer &cmdBuff,
										const RHI::PConstantSet &sceneInfo = null);
	void			_RenderGridBackdrop(const RHI::PCommandBuffer &cmdBuff);
	void			_RenderMeshBackdrop(const RHI::PCommandBuffer &cmdBuff);
	void			_RenderEditorMisc(const RHI::PCommandBuffer &cmdBuff);
	void			_RenderBackground(const RHI::PCommandBuffer &cmdBuff, bool clearOnly = false);

	// Update Shadow BBox:
	void			_UpdateShadowsBBox(CDirectionalShadows &shadow, const TArray<SRHIDrawCall> &drawCalls);

	RHI::PRenderTarget		_GetRenderTarget(ERenderTargetDebug target);

	RHI::PApiManager				m_ApiManager;
	CShaderLoader					*m_ShaderLoader;

	// Resource manager for Editor resources only
	CResourceManager				*m_ResourceManager;
	CResourceManager				*m_CurrentPackResourceManager;

	// Scene info constant set:
	RHI::PGpuBuffer					m_SceneInfoConstantBuffer;
	RHI::SConstantSetLayout			m_SceneInfoConstantSetLayout;
	RHI::PConstantSet				m_SceneInfoConstantSet;

	bool							m_EnableParticleRender;
	bool							m_EnableBackdropRender;
	bool							m_EnableOverdrawRender;
	bool							m_EnableBrushBackground;
	bool							m_EnablePostFX;

	float							m_DeferredMergingMinAlpha = 1.0f;

	// Default single sampler constant set layout to sample render targets:
	RHI::SConstantSetLayout				m_DefaultSamplerConstLayout;
	RHI::PConstantSampler				m_DefaultSampler;
	RHI::PConstantSampler				m_DefaultSamplerNearest;

	// Render passes:
	TStaticArray<SSamplableRenderTarget, SPassDescription::__GBufferRTCount>	m_GBufferRTs;
	TStaticArray<CGuid, SPassDescription::__GBufferRTCount>						m_GBufferRTsIdx;

	TArray<RHI::SFrameBufferClearValue>	m_BeforeBloomClearValues;
	RHI::PFrameBuffer					m_BeforeBloomFrameBuffer;
	RHI::PRenderPass					m_BeforeBloomRenderPass;
	// Opaque GBuffer:
	CGBuffer							m_GBuffer;
	// Distortion:
	bool								m_EnableDistortion; // distortion is inside the m_DeferredSetup
	CPostFxDistortion					m_Distortion;
	// Bloom:
	bool								m_EnableBloom;
	CPostFxBloom						m_Bloom;
	EGaussianBlurCombination			m_BlurTap;
	// Tone mapping:
	bool								m_EnableToneMapping;
	CPostFxToneMapping					m_ToneMapping;
	// Color remap:
	bool								m_EnableColorRemap;
	CPostFxColorRemap					m_ColorRemap;
	// FXAA:
	bool								m_EnableFXAA;
	CPostFxFXAA							m_FXAA;

	float								m_OverdrawScaleFactor = 1.0f / 50;

	struct	SBrushInfo
	{
		CFloat4		m_TopColor;
		CFloat4		m_BottomColor;
		CFloat4		m_CameraPosition;
		CFloat4x4	m_UserToRHY;
		CFloat4x4	m_InvViewProj;
		CFloat4		m_EnvironmentMapColor;
		float		m_EnvironmentMapMipLvl;
		u32			m_EnvironmentMapVisible;
		float		m_EnvironmentMapRotation;
	};

	RHI::PRenderState					m_BrushBackgroundRenderState; // Bush background (in the GBuffer)
	RHI::PGpuBuffer						m_BrushInfoData;
	RHI::PConstantSet					m_BrushInfoConstSet;
	// -> Pass 2: Bloom (multi-pass)

	enum	EFinalFrameBuffer
	{
		kMaxSwapChainSize = 3,
	};

	// -> Pass 3: Final and blit in screen
	TArray<RHI::SFrameBufferClearValue>										m_FinalClearValues;
	RHI::PRenderPass														m_FinalRenderPass;
	TStaticCountedArray<RHI::PFrameBuffer, kMaxSwapChainSize>				m_FinalFrameBuffers;
	TStaticCountedArray<RHI::PRenderTarget, kMaxSwapChainSize>				m_FinalRenderTargets;

protected:
	RHI::PRenderState				m_OverdrawHeatmapRenderState;		// Overdraw
	RHI::PConstantSet				m_OverdrawConstantSet;				// Overdraw
	RHI::PRenderState				m_CopyColorRenderState;				// copy

	RHI::PRenderState				m_CopyBackgroundColorRenderState;	// Can be used instead of brush

	RHI::PRenderState				m_CopyDepthRenderState;				// Debug copy
	RHI::PRenderState				m_CopyNormalRenderState;			// Debug copy
	RHI::PRenderState				m_CopyNormalUnpackedRenderState;	// Debug copy
	RHI::PRenderState				m_CopySpecularRenderState;			// Debug copy
	RHI::PRenderState				m_CopyAlphaRenderState;				// Debug copy
	RHI::PRenderState				m_CopyMulAddRenderState;			// Debug copy

	SSamplableRenderTarget			m_BeforeDebugOutputRt;
	CGuid							m_BeforeDebugOutputRtIdx;

	CGuid							m_ParticleDebugSubpassIdx;

	CUint2							m_ViewportSize;

	SSceneInfoData					m_SceneInfoData;			// Current scene info
	ECoordinateFrame				m_CoordinateFrame;

	// Backdrop data:
	SBackdropsData					m_BackdropsData;

	SSamplableRenderTarget			m_BasicRenderingRT;			// Render target used when there is no GBuffer enabled
	CGuid							m_BasicRenderingRTIdx;		// Will be zero
	CGuid							m_BasicRenderingSubPassIdx;

	RHI::PTexture					m_BackgroundTexture;
	RHI::PConstantSet				m_BackgroundTextureConstantSet;

	PKSample::CEnvironmentMap		m_EnvironmentMap;

	RHI::PConstantSet				m_BRDFLUTConstantSet;

	RHI::PTexture					m_DummyWhite;
	RHI::PConstantSet				m_DummyWhiteConstantSet;
	RHI::PTexture					m_DummyBlack;
	RHI::PConstantSet				m_DummyBlackConstantSet;
	RHI::PTexture					m_DummyNormal;
	RHI::PTexture					m_HeatmapTexture;

	CConstantAtlas					m_DummyAtlas;

	CConstantNoiseTexture			m_NoiseTexture;

	// Grid:
	RHI::PGpuBuffer					m_GridVertices;
	RHI::PGpuBuffer					m_GridIndices;
	u32								m_GridIdxCount;
	u32								m_GridSubdivIdxCount;

	RHI::PRenderState				m_GridRenderState;

	// Mesh:
	SMesh							m_MeshBackdrop;
	RHI::PGpuBuffer					m_MeshTransformBuffer;
	TArray<u32>						m_MeshBackdropFilteredSubmeshes;

	RHI::SConstantSetLayout			m_MeshConstSetLayout;
	RHI::PRenderState				m_MeshRenderState;
	RHI::PRenderState				m_MeshRenderStateVertexColor;

	// Misc:
	struct	SLinePointsColorBuffer
	{
		RHI::PGpuBuffer				m_LinesPointsBuffer;
		RHI::PGpuBuffer				m_LinesColorBuffer;
	};
	RHI::PRenderState				m_LinesRenderStateAdditive;
	RHI::PRenderState				m_LinesRenderStateOpaque;
	SLinePointsColorBuffer			m_LinesPointsColorBufferAdditive;
	SLinePointsColorBuffer			m_LinesPointsColorBufferOpaque;

	TMemoryView<const CFloat3>		m_MiscLinesOpaque_Position;
	TMemoryView<const CFloat4>		m_MiscLinesOpaque_Color;
	TMemoryView<const CFloat3>		m_MiscLinesAdditive_Position;
	TMemoryView<const CFloat4>		m_MiscLinesAdditive_Color;

	void							_DrawDebugLines(const RHI::PCommandBuffer &cmdBuff, const TMemoryView<const CFloat3> &positions, const TMemoryView<const CFloat4> &colors, SLinePointsColorBuffer &buffer, RHI::PRenderState &renderState);

	// Lights:
	SLightRenderPass				m_Lights;
	SShadowOptions					m_ShadowOptions;

	// Gizmo rendering:
	CGizmoDrawer					m_GizmoDrawer;

	// Render passes initialized:
	u32								m_InitializedRP;

private:
	// Update buffer for mesh backdrop infos
	void	_UpdateMeshBackdropInfos();

	// Update the lights constant sets:
	bool	_UpdateBackdropLights(const SBackdropsData &backdrop);
	// Draw the lights (full screen quad):
	void	_RenderBackdropLights(const RHI::PCommandBuffer &cmdBuff, const SLightRenderPass &lightInfo, const RHI::PRenderState &renderState);

	// Create the graphic resources:
	bool	_CreateFinalRenderStates(	const TMemoryView<const RHI::SRenderTargetDesc> frameBufferLayout,
										const RHI::PRenderPass &renderPass,
										u32 subPassIdx,
										const SSamplableRenderTarget &prevPassOut);
	bool	_CreateOpaqueBackdropRenderStates(	TMemoryView<const RHI::SRenderTargetDesc> frameBufferLayout,
												const RHI::PRenderPass &renderPass,
												u32 subPassIdx);
	bool	_CreateTransparentBackdropRenderStates(	TMemoryView<const RHI::SRenderTargetDesc> frameBufferLayout,
													const RHI::PRenderPass &renderPass,
													u32 subPassIdx);

	bool	_CreateCopyRenderState(	RHI::PRenderState &renderState,
									ECopyCombination combination,
									const TMemoryView<const RHI::SRenderTargetDesc> &frameBufferLayout,
									const RHI::PRenderPass &renderPass,
									u32 subPassIdx);
	bool	_CreateGridGeometry(float size, u32 subdiv, u32 secondSubdiv, const CFloat4x4 &transforms, ECoordinateFrame coordinateFrame);
	//Add render pass to the graphic resource manager:
	bool	_AddRenderPassesToGraphicResourceManagerIFN();

	void	_FillGBufferInOut(CGBuffer::SInOutRenderTargets &inOut);
	void	_FillDistortionInOut(CPostFxDistortion::SInOutRenderTargets &inOut);
	void	_FillBloomInOut(CPostFxBloom::SInOutRenderTargets &inOut);
	void	_FillTonemappingInOut(CPostFxToneMapping::SInOutRenderTargets &inOut);
	void	_FillColorRemapInOut(CPostFxColorRemap::SInOutRenderTargets &inOut);
	void	_FillDebugInOut(SSamplableRenderTarget &prevPassOut, CGuid &prevPassOutIdx);
	void	_FillFXAAInOut(CPostFxFXAA::SInOutRenderTargets &inOut);

	bool	_IsLastRenderPass(EInitRenderPasses renderPass) const;
	bool	_EndRenderPass(EInitRenderPasses renderPass, const RHI::PCommandBuffer &cmdBuff) const;

private:
	typedef FastDelegate<bool(const RHI::PCommandBuffer &cmdBuff)>	CbPostRenderOpaque;

	CbPostRenderOpaque				m_PostRenderOpaque;

public:
	CbPostRenderOpaque				&GetPostOpaque() { return m_PostRenderOpaque; }
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
