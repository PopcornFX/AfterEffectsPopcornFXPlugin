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

#include "PK-SampleLib/PKSample.h"
#include "PK-SampleLib/SampleUtils.h"
#include "PK-SampleLib/ShaderGenerator/ParticleShaderGenerator.h"

#include <pk_render_helpers/include/resource_manager/rh_resource_manager.h>

#include <pk_rhi/include/AllInterfaces.h>
#include <pk_rhi/include/interfaces/SApiContext.h>
#include <pk_kernel/include/kr_resources.h>
#include <pk_particles/include/Renderers/ps_renderer_base.h>
#include "PK-SampleLib/RenderIntegrationRHI/FeatureRenderingSettings.h"

__PK_API_BEGIN
namespace	HBO
{
	class	CContext;
}

PK_FORWARD_DECLARE(RectangleList);

// We use this define for the shadow depth format
// Here we use variance shadow maps, which needs 2 float components
#define IDEAL_SHADOW_DEPTH_FORMAT	RHI::FormatFloat32RG

__PK_API_END

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

// SampleLib Graphic Resources enums:
enum	ESampleLibGraphicResources_BlendMode
{
	ParticleBlend_Additive = 0,
	ParticleBlend_AdditiveNoAlpha,
	ParticleBlend_AlphaBlend,
	ParticleBlend_AlphaBlendAdditive,
	ParticleBlend_Opaque,
	ParticleBlend_Distortion,
	ParticleBlend_Multiply,
	__MaxParticleBlend
};

//----------------------------------------------------------------------------

enum	ESampleLibGraphicResources_RenderPass
{
	ParticlePass_Opaque = 0,			// GBuffer pass
	ParticlePass_Decal,					// Decal render pass
	ParticlePass_Lighting,				// Light accumulation pass
	ParticlePass_Transparent,			// Post merging pass
	ParticlePass_Distortion,			// Distortion Map
	ParticlePass_Tint,					// Tinting
	ParticlePass_TransparentPostDisto,	// Transparent particles not affected by distortion
	ParticlePass_Debug,					// Post bloom, still in HDR Buffer debug pass
	ParticlePass_Compositing,			// Compositing & UI pass
	ParticlePass_OpaqueShadow,			// Shadow map
	__MaxParticlePass
};

enum
{
	kMaxMaxSimultaneousRenderPass = 4,
};


class	CRenderPassArray : public TStaticCountedArray<ESampleLibGraphicResources_RenderPass, kMaxMaxSimultaneousRenderPass>
{
public:
	CRenderPassArray()
	{
	}

	template<u32 _Count>
	CRenderPassArray(ESampleLibGraphicResources_RenderPass(&renderPasses)[_Count])
		: TStaticCountedArray<ESampleLibGraphicResources_RenderPass, kMaxMaxSimultaneousRenderPass>(TMemoryView<const ESampleLibGraphicResources_RenderPass>(renderPasses))
	{ }

	CRenderPassArray(ESampleLibGraphicResources_RenderPass renderPass)
		: TStaticCountedArray<ESampleLibGraphicResources_RenderPass, kMaxMaxSimultaneousRenderPass>(TMemoryView<const ESampleLibGraphicResources_RenderPass>(renderPass))
	{ }
};

extern const CStringView	s_ParticleRenderPassNames[__MaxParticlePass];

//----------------------------------------------------------------------------

// Global description of all passes of the app
// Could also be passed as a RenderThread_CreateResource argument
struct	 SPassDescription
{
	enum	EGBufferRT
	{
		GBufferRT_Diffuse = 0,			// GBuffer Diffuse HDR:			0
		GBufferRT_Depth,				// GBuffer Depth:				1
		GBufferRT_Emissive,				// GBuffer Emissive HDR:		2

		__GBufferRTCount
	};

	TArray<RHI::SRenderTargetDesc>			m_FrameBufferLayout;
	RHI::PRenderPass						m_RenderPass;
	CGuid									m_SubPassIdx;

	static TArray<SPassDescription>		s_PassDescriptions;
	// Bake render state using single pass as reference
	// Pass used should be compatible with final passes - see vulkan render pass compatibility
	// - https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#renderpass-compatibility

	// Static definition of pass in pixel format, as it was designed
	static const TMemoryView<const RHI::EPixelFormat>	s_PassOutputsFormats[__MaxParticlePass];
	static const TMemoryView<const RHI::EPixelFormat>	s_GBufferDefaultFormats;
};

//----------------------------------------------------------------------------

struct	SMaterialSettings
{
	CString		m_MaterialName;
	CString		m_FragmentShaderPath;
	CString		m_VertexShaderPath;
};

//----------------------------------------------------------------------------

struct	SPrepareArg
{
	CResourceManager						*m_ResourceManager;
	PCRendererDataBase						m_Renderer;
	TArray<SToggledRenderingFeature>		m_FeaturesSettings;
	SMaterialSettings						m_MaterialSettings;
	RHI::SGPUCaps							m_GpuCaps;
	CRenderPassArray						m_RenderPasses;
	RHI::EGraphicalApi						m_ApiName;

	CString									m_ShaderFolder;

	SPrepareArg()
	:	m_ResourceManager(null)
	{
	}

	SPrepareArg(const PCRendererDataBase &renderer, HBO::CContext *context, const RHI::SGPUCaps *gpuCaps);

	bool	_HasProperty(CStringId materialProperty) const;
};

//----------------------------------------------------------------------------

struct	SCreateArg
{
	RHI::PApiManager			m_ApiManager;
	TArray<CResourceManager*>	m_ResourceManagers;

	SCreateArg(const RHI::PApiManager &apiManager = null)
	:	m_ApiManager(apiManager)
	{
		m_ResourceManagers.Clear();
	}

	SCreateArg(const RHI::PApiManager &apiManager, CResourceManager *resourceManager)
	:	m_ApiManager(apiManager)
	{
		if (m_ResourceManagers.Resize(1))
		{
			m_ResourceManagers[0] = resourceManager;
		}
	}

	SCreateArg(const RHI::PApiManager &apiManager, TArray<CResourceManager*> resourceManagers)
		: m_ApiManager(apiManager)
		, m_ResourceManagers(resourceManagers)
	{
	}
};

//----------------------------------------------------------------------------

struct	STextureKey;
struct	SConstantSamplerKey;
struct	SRenderStateKey;
struct	SShaderModuleKey;
struct	SShaderProgramKey;
struct	SGeometryKey;
struct	SConstantAtlasKey;
struct	SRendererCacheKey;
struct	SRendererCacheInstanceKey;
struct	SComputeStateKey;

PK_FORWARD_DECLARE(GeometryResource);
PK_FORWARD_DECLARE(ConstantAtlas);
PK_FORWARD_DECLARE(RendererCache);
PK_FORWARD_DECLARE(RendererCacheInstance);

#define EXEC_X_GRAPHIC_RESOURCE(__sep) \
		X_GRAPHIC_RESOURCE(Texture) __sep \
		X_GRAPHIC_RESOURCE(ConstantSampler) __sep \
		X_GRAPHIC_RESOURCE(RenderState) __sep \
		X_GRAPHIC_RESOURCE(ComputeState) __sep \
		X_GRAPHIC_RESOURCE(ShaderModule) __sep \
		X_GRAPHIC_RESOURCE(ShaderProgram) __sep \
		X_GRAPHIC_RESOURCE(Geometry) __sep \
		X_GRAPHIC_RESOURCE(ConstantAtlas) __sep \
		X_GRAPHIC_RESOURCE(RendererCache) __sep \
		X_GRAPHIC_RESOURCE(RendererCacheInstance) __sep

// Graphic resources dependencies:
//	- Renderer Cache Instance	x1
//		- Texture				xN
//		- Sampler				x1
//		- Geometry				x1
//		- Constant Atlas		x1
//		- Renderer Cache		x1
//			- Render State			xN (depending on the shader options)
//				- Shader Program		x1
//					- Shader Module			xN

//----------------------------------------------------------------------------

template<>
bool TGraphicResourceManager<PRendererCache, SRendererCacheKey, SPrepareArg, SCreateArg>::RenderThread_CreateMissingResources(const SCreateArg &args);

// We define all the resources we need to handle:
typedef		TGraphicResourceManager<RHI::PTexture, STextureKey, SPrepareArg, SCreateArg>								CTextureManager;
typedef		TGraphicResourceManager<RHI::PConstantSampler, SConstantSamplerKey, SPrepareArg, SCreateArg>				CConstantSamplerManager;
typedef		TGraphicResourceManager<RHI::PRenderState, SRenderStateKey, SPrepareArg, SCreateArg>						CRenderStateManager;
typedef		TGraphicResourceManager<RHI::PShaderModule, SShaderModuleKey, SPrepareArg, SCreateArg>						CShaderModuleManager;
typedef		TGraphicResourceManager<RHI::PShaderProgram, SShaderProgramKey, SPrepareArg, SCreateArg>					CShaderProgramManager;
typedef		TGraphicResourceManager<PGeometryResource, SGeometryKey, SPrepareArg, SCreateArg>							CGeometryManager;
typedef		TGraphicResourceManager<PConstantAtlas, SConstantAtlasKey, SPrepareArg, SCreateArg>							CConstantAtlasManager;
typedef		TGraphicResourceManager<PRendererCache, SRendererCacheKey, SPrepareArg, SCreateArg>							CRendererCacheManager;
typedef		TGraphicResourceManager<PRendererCacheInstance, SRendererCacheInstanceKey, SPrepareArg, SCreateArg>			CRendererCacheInstanceManager;
typedef		TGraphicResourceManager<RHI::PComputeState, SComputeStateKey, SPrepareArg, SCreateArg>						CComputeStateManager;

//----------------------------------------------------------------------------
// Keys contains data to identify each resource
// if necessary can be compared to retrieve existing resource
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
// Textures
//----------------------------------------------------------------------------

struct	STextureKey
{
	bool					m_LoadAsSRGB;
	CString					m_Path;

	STextureKey() : m_LoadAsSRGB(false) {}

	bool					UpdateThread_Prepare(const SPrepareArg &args);
	RHI::PTexture			RenderThread_CreateResource(const SCreateArg &args);
	void					UpdateThread_ReleaseDependencies() {}
	RHI::PTexture			RenderThread_ReloadResource(const SCreateArg &args);
	bool					operator == (const STextureKey &other) const;
	static const char		*GetResourceName() { return "Texture"; }

	static void								SetupDefaultResource();
	static CTextureManager::CResourceId		s_DefaultResourceID;
};

//----------------------------------------------------------------------------
// Constant sampler
//----------------------------------------------------------------------------

struct	SConstantSamplerKey
{
	RHI::EFilteringMode		m_MagFilter : 4;
	RHI::EFilteringMode		m_MinFilter : 4;
	RHI::EWrapMode			m_WrapU : 4;
	RHI::EWrapMode			m_WrapV : 4;
	RHI::EWrapMode			m_WrapW : 4;
	u32						m_MipmapCount : 8;

	PK_STATIC_ASSERT(RHI::SampleLinear == 0);
	PK_STATIC_ASSERT(RHI::SampleRepeat == 0);
	SConstantSamplerKey() : m_MagFilter(RHI::SampleLinear), m_MinFilter(RHI::SampleLinear), m_WrapU(RHI::SampleRepeat), m_WrapV(RHI::SampleRepeat), m_WrapW(RHI::SampleRepeat), m_MipmapCount(0) {}

	bool					UpdateThread_Prepare(const SPrepareArg &args);
	RHI::PConstantSampler	RenderThread_CreateResource(const SCreateArg &args);
	void					UpdateThread_ReleaseDependencies() {}
	bool					operator == (const SConstantSamplerKey &other) const;
	static const char		*GetResourceName() { return "ConstantSampler"; }
};

//----------------------------------------------------------------------------
// Render state
//----------------------------------------------------------------------------

struct	SRenderStateKey
{
	EShaderOptions							m_Options;
	ESampleLibGraphicResources_RenderPass	m_RenderPassIdx;

	PK_STATIC_ASSERT(Option_None == 0);
	PK_STATIC_ASSERT(ParticlePass_Opaque == 0);
	SRenderStateKey() : m_Options(Option_None), m_RenderPassIdx(__MaxParticlePass), m_NeededConstants(0) {}

	bool					UpdateThread_Prepare(const SPrepareArg &args);
	RHI::PRenderState		RenderThread_CreateResource(const SCreateArg &args);
	void					UpdateThread_ReleaseDependencies();
	bool					operator == (const SRenderStateKey &other) const;
	static const char		*GetResourceName() { return "RenderState"; }

	const RHI::SShaderBindings	&GetGeneratedShaderBindings() const { return m_ShaderBindings; }
	u32						GetNeededConstants() const { return m_NeededConstants; }

private:
	RHI::SShaderBindings					m_ShaderBindings;
	RHI::SPipelineState						m_PipelineState;
	TArray<RHI::SVertexInputBufferDesc>		m_InputVertexBuffers;
	// Dependency:
	CShaderProgramManager::CResourceId		m_ShaderProgram;
	// Needed constants:
	u32										m_NeededConstants;
	// Hash to compare render states:
	CDigestMD5								m_RenderStateHash;
};

//----------------------------------------------------------------------------
// Compute state
//----------------------------------------------------------------------------

struct	SComputeStateKey
{
	EComputeShaderType			m_Type;

	SComputeStateKey() : m_Type(ComputeType_None) {}

	bool						UpdateThread_Prepare(const SPrepareArg &args);
	RHI::PComputeState			RenderThread_CreateResource(const SCreateArg &args);
	void						UpdateThread_ReleaseDependencies();
	bool						operator == (const SComputeStateKey &other) const;
	static const char			*GetResourceName() { return "ComputeState"; }
	const RHI::SShaderBindings	&GetGeneratedShaderBindings() const { return m_ShaderBindings; }

private:
	RHI::SShaderBindings					m_ShaderBindings;
	// Dependency:
	CShaderProgramManager::CResourceId		m_ShaderProgram;
};

//----------------------------------------------------------------------------
// Shader module
//----------------------------------------------------------------------------

struct	SShaderModuleKey
{
	CString					m_Path;
	RHI::EShaderStage		m_Stage;

	PK_STATIC_ASSERT(RHI::VertexShaderStage == 0);
	SShaderModuleKey() : m_Stage(RHI::VertexShaderStage) {}

	bool					UpdateThread_Prepare(const SPrepareArg &args);
	RHI::PShaderModule		RenderThread_CreateResource(const SCreateArg &args);
	RHI::PShaderModule		RenderThread_ReloadResource(const SCreateArg &args);
	void					UpdateThread_ReleaseDependencies() {}
	bool					operator == (const SShaderModuleKey &other) const;
	static const char		*GetResourceName() { return "ShaderModule"; }
};

//----------------------------------------------------------------------------
// Shader program
//----------------------------------------------------------------------------

struct	SShaderProgramKey
{
	EShaderOptions			m_Option;
	ESampleLibGraphicResources_RenderPass	m_RenderPassIdx;
	EComputeShaderType		m_ComputeShaderType;
	RHI::SShaderBindings	m_ShaderBindings;

	PK_STATIC_ASSERT(Option_None == 0);
	PK_STATIC_ASSERT(ComputeType_None == 0);
	SShaderProgramKey() : m_Option(Option_None), m_RenderPassIdx(__MaxParticlePass), m_ComputeShaderType(ComputeType_None){}

	bool					UpdateThread_Prepare(const SPrepareArg &args);
	RHI::PShaderProgram		RenderThread_CreateResource(const SCreateArg &args);
	void					UpdateThread_ReleaseDependencies();
	bool					operator == (const SShaderProgramKey &other) const;
	static const char		*GetResourceName() { return "ShaderProgram"; }

	CShaderModuleManager::CResourceId	m_ShaderModules[RHI::ShaderStage_Count];
};

//----------------------------------------------------------------------------
// Geometry resource
//----------------------------------------------------------------------------

class	CGeometryResource : public CRefCountedObject
{
public:
	TArray<Utils::GpuBufferViews>	m_PerGeometryViews;
};
PK_DECLARE_REFPTRCLASS(GeometryResource);

//----------------------------------------------------------------------------

struct	SGeometryKey
{
	enum	EGeometryType
	{
		Geom_Mesh,
		Geom_Sphere,
		Geom_Cube,
	};

	EGeometryType			m_GeomType;
	CString					m_Path;

	SGeometryKey();
	bool					UpdateThread_Prepare(const SPrepareArg &args) { (void)args; return true; }
	PGeometryResource		RenderThread_CreateResource(const SCreateArg &args);
	void					UpdateThread_ReleaseDependencies() {}
	PGeometryResource		RenderThread_ReloadResource(const SCreateArg &args);
	bool					operator == (const SGeometryKey &other) const;
	static const char		*GetResourceName() { return "Geometry"; }

	bool					CreateGeomSphere(const RHI::PApiManager &apiManager, u32 subdivs, Utils::GpuBufferViews &toFill);
	bool					CreateGeomCube(const RHI::PApiManager &apiManager, Utils::GpuBufferViews &toFill);

	static void								SetupDefaultResource();
	static CGeometryManager::CResourceId	s_DefaultResourceID;
};

//----------------------------------------------------------------------------
// Constant atlas data
//----------------------------------------------------------------------------

class	CConstantAtlas : public CRefCountedObject
{
public:
	// Billboard geometry
	RHI::PConstantSet		m_AtlasConstSet;
	RHI::PGpuBuffer			m_AtlasBuffer;
};
PK_DECLARE_REFPTRCLASS(ConstantAtlas);

//----------------------------------------------------------------------------

struct	SConstantAtlasKey
{
	PCRectangleList			m_SourceAtlas;
	CString					m_Path;

	bool					UpdateThread_Prepare(const SPrepareArg &args) { (void)args; return m_SourceAtlas != null; }
	PConstantAtlas			RenderThread_CreateResource(const SCreateArg &args);
	void					UpdateThread_ReleaseDependencies() {}
	PConstantAtlas			RenderThread_ReloadResource(const SCreateArg &args);
	bool					operator == (const SConstantAtlasKey &other) const;
	static const char		*GetResourceName() { return "Atlas"; }

	static const RHI::SConstantSetLayout	&GetAtlasConstantSetLayout();

	static void								SetupConstantSetLayout();
	static void								ClearConstantSetLayoutIFN();
};

//----------------------------------------------------------------------------
// Constant noise data
//----------------------------------------------------------------------------

class	CConstantNoiseTexture : public CRefCountedObject
{
public:
	// Billboard geometry
	RHI::PTexture		m_NoiseTexture;
	RHI::PConstantSet	m_NoiseTextureConstantSet;
};

//----------------------------------------------------------------------------

struct	SConstantNoiseTextureKey
{
	static const char		*GetResourceName() { return "DitheringPatterns"; }

	static const RHI::SConstantSetLayout	&GetNoiseTextureConstantSetLayout();

	static void								SetupConstantSetLayout();
	static void								ClearConstantSetLayoutIFN();
};

//----------------------------------------------------------------------------
//	Draw requests constant set for GPU billboarding (vertex/geometry)
//----------------------------------------------------------------------------

struct	SConstantDrawRequests
{
	static const RHI::SConstantSetLayout	&GetConstantSetLayout(ERendererClass rendererType);
	static bool								GetConstantBufferDesc(RHI::SConstantBufferDesc &outDesc, u32 elementCount, ERendererClass rendererType);

	static void								SetupConstantSetLayouts();
	static void								ClearConstantSetLayoutsIFN();
};

//----------------------------------------------------------------------------
//	Vertex billboarding constants
//----------------------------------------------------------------------------

struct	SConstantVertexBillboarding
{
	static bool								GetPushConstantBufferDesc(RHI::SPushConstantBuffer &outDesc, bool gpuStorage);
};

struct	SVertexBillboardingConstants
{
	u32		m_IndicesOffset;
	u32		m_StreamOffsetsIndex;
};
PK_STATIC_ASSERT(sizeof(SVertexBillboardingConstants) <= sizeof(CUint4));

//----------------------------------------------------------------------------
// Helpers
//----------------------------------------------------------------------------

// - Scene info (! Any modification to that struct requires an upgrader for .vert/.frag files !):
struct	SSceneInfoData
{
	CFloat4x4		m_ViewProj;
	CFloat4x4		m_View;
	CFloat4x4		m_Proj;
	CFloat4x4		m_InvView;
	CFloat4x4		m_InvViewProj;
	CFloat4x4		m_UserToLHZ;				// Helper matrix to convert from user to LHZup coord system.
	CFloat4x4		m_LHZToUser;				// Helper matrix to convert from LHZup to user coord system.
	CFloat4x4		m_BillboardingView;
	CFloat4x4		m_PackNormalView;
	CFloat4x4		m_UnpackNormalView;
	CFloat4			m_SideVector;
	CFloat4			m_DepthVector;
	CFloat2			m_ZBufferLimits;
	CFloat2			m_ViewportSize;
	CFloat1			m_Handedness;

	SSceneInfoData()
	:	m_ViewProj(CFloat4x4::IDENTITY)
	,	m_View(CFloat4x4::IDENTITY)
	,	m_Proj(CFloat4x4::IDENTITY)
	,	m_InvView(CFloat4x4::IDENTITY)
	,	m_InvViewProj(CFloat4x4::IDENTITY)
	,	m_UserToLHZ(CFloat4x4::IDENTITY)
	,	m_LHZToUser(CFloat4x4::IDENTITY)
	,	m_BillboardingView(CFloat4x4::IDENTITY)
	,	m_PackNormalView(CFloat4x4::IDENTITY)
	,	m_UnpackNormalView(CFloat4x4::IDENTITY)
	,	m_SideVector(CFloat4::ZERO)
	,	m_DepthVector(CFloat4::ZERO)
	,	m_ZBufferLimits(0.1f, 1000.f)
	,	m_ViewportSize(CFloat2::ONE)
	,	m_Handedness(1.f)
	{}
};

//----------------------------------------------------------------------------
// Renderer Cache (Name collides with the runtime renderer cache, should be something like CRenderStates)
//----------------------------------------------------------------------------

class	CRendererCache : public CRefCountedObject
{
public:
	struct	SRenderStateFeature
	{
		EShaderOptions							m_Options;
		u32										m_NeededConstants;
		ESampleLibGraphicResources_RenderPass	m_RenderPassIdx;
		RHI::PRenderState						m_RenderState;

		RHI::SConstantSetLayout					m_GPUStorageSimDataConstantSetLayout;
		RHI::SConstantSetLayout					m_GPUStorageOffsetsConstantSetLayout;

		bool									m_Expected;

		SRenderStateFeature()
		:	m_Options(EShaderOptions::Option_None)
		,	m_RenderPassIdx(ESampleLibGraphicResources_RenderPass::__MaxParticlePass)
		,	m_Expected(false)
		{}
	};

	struct	SComputeStateFeature
	{
		RHI::PComputeState						m_ComputeState;
		EComputeShaderType						m_Type;

		bool									m_Expected;

		SComputeStateFeature()
			: m_Type(EComputeShaderType::ComputeType_None)
			, m_Expected(false)
		{}
	};

	TArray<SRenderStateFeature>		m_RenderStates;
	CRenderPassArray				m_RenderPasses;
	TArray<SComputeStateFeature>	m_ComputeStates;
	SCreateArg						m_LastCreateArgs;

	RHI::PRenderState				GetRenderState(EShaderOptions option, ESampleLibGraphicResources_RenderPass renderPass = __MaxParticlePass, u32 *neededConstants = null);
	RHI::PComputeState				GetComputeState(EComputeShaderType type);
	bool							GetGPUStorageConstantSets(EShaderOptions option, const RHI::SConstantSetLayout *&outSimDataConstantSet, const RHI::SConstantSetLayout *&outOffsetsConstantSet) const;
};
PK_DECLARE_REFPTRCLASS(RendererCache);

//----------------------------------------------------------------------------

struct	SRendererCacheKey
{
public:
	bool				UpdateThread_Prepare(const SPrepareArg &args);
	PRendererCache		RenderThread_CreateResource(const SCreateArg &args, PRendererCache cache = null);
	bool				RenderThread_IsPartiallyBuilt(const PRendererCache &cache) const;
	void				UpdateThread_ReleaseDependencies();
	bool				operator == (const SRendererCacheKey &) const;
	static const char	*GetResourceName() { return "RendererCache"; }

private:
	bool				PushRenderState(const SPrepareArg &args, EShaderOptions options);
	bool				PushComputeState(const SPrepareArg &args, EComputeShaderType options);

	// Dependencies
	struct	SRenderStateFeature
	{
		EShaderOptions							m_Options;
		ESampleLibGraphicResources_RenderPass	m_RenderPassIdx;
		CRenderStateManager::CResourceId		m_RenderState;
		u32										m_NeededConstants;

		SRenderStateFeature()
			: m_Options(EShaderOptions::Option_None)
			, m_RenderPassIdx(ESampleLibGraphicResources_RenderPass::__MaxParticlePass)
			, m_NeededConstants(0)
		{}
	};

	struct	SComputeStateFeature
	{
		CComputeStateManager::CResourceId		m_ComputeState;
		EComputeShaderType						m_Type;

		SComputeStateFeature()
			: m_Type(EComputeShaderType::ComputeType_None)
		{}
	};

	TArray<SRenderStateFeature>					m_RenderStates;
	CRenderPassArray							m_RenderPasses;
	TArray<SComputeStateFeature>				m_ComputeStates;
};

//----------------------------------------------------------------------------
// Renderer Cache Instance
//----------------------------------------------------------------------------

class	CRendererCacheInstance : public CRefCountedObject
{
public:
	RHI::PConstantSet					m_ConstSet;
	RHI::PGpuBuffer						m_ConstValues;
	TArray<RHI::PTexture>				m_ConstTextures;
	TArray<RHI::PConstantSampler>		m_ConstSamplers;

	// Mesh renderer resource
	PGeometryResource					m_AdditionalGeometry;
	PConstantAtlas						m_Atlas;

	// Billboards/Ribbons
	bool								m_HasAtlas;
	bool								m_HasRawUV0;

	// Associated cache
	PRendererCache						m_Cache;
};
PK_DECLARE_REFPTRCLASS(RendererCacheInstance);

//----------------------------------------------------------------------------

struct	SRendererCacheInstanceKey
{
public:
	bool					UpdateThread_Prepare(const SPrepareArg &args);
	PRendererCacheInstance	RenderThread_CreateResource(const SCreateArg &args);
	void					UpdateThread_ReleaseDependencies();
	bool					operator == (const SRendererCacheInstanceKey &) const;
	static const char		*GetResourceName() { return "RendererCacheInstance"; }

private:
	bool											m_HasAtlas;
	bool											m_HasRawUV0;

	RHI::SConstantSetLayout							m_ConstSetLayout;
	// Dependencies
	CRendererCacheManager::CResourceId				m_Cache;
	TArray<CTextureManager::CResourceId>			m_Textures;
	TArray<CConstantSamplerManager::CResourceId>	m_Samplers;
	CGeometryManager::CResourceId					m_Geometry;
	CConstantAtlasManager::CResourceId				m_Atlas;
	CDigestMD5										m_ContentHash;
	// Tmp data
	TArray<SRendererFeaturePropertyValue>			m_ConstantProperties;
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
