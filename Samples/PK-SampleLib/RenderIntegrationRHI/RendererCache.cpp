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

#include "RendererCache.h"

#include "PK-SampleLib/ShaderGenerator/ShaderGenerator.h"

#include <pk_render_helpers/include/render_features/rh_features_basic.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------
//
//	Renderer Cache Instance
//
//----------------------------------------------------------------------------

CRendererCacheInstance_UpdateThread::~CRendererCacheInstance_UpdateThread()
{
	if (m_RendererCacheInstanceId.Valid())
	{
		CRendererCacheInstanceManager::UpdateThread_ReleaseResource(m_RendererCacheInstanceId);
	}

	if (m_MeshResource != null && !m_MeshResource->Empty())
		m_MeshResource->m_OnReloaded -= FastDelegate<void(CResourceMesh*)>(this, &CRendererCacheInstance_UpdateThread::_OnMeshReloaded);
}

//----------------------------------------------------------------------------

void	CRendererCacheInstance_UpdateThread::UpdateThread_BuildBillboardingFlags(const PRendererDataBase &renderer)
{
	(void)renderer;
	// Already resolved in UpdateThread_Build()
}

//----------------------------------------------------------------------------

bool	CRendererCacheInstance_UpdateThread::RenderThread_Build(const RHI::PApiManager &apiManager, HBO::CContext *context)
{
	// We create all the missing resources (all the dependencies are directly handled in the RenderThread_CreateResource):
	if (context != null && context->ResourceManager() != null)
		return CRendererCacheInstanceManager::RenderThread_CreateMissingResources(SCreateArg(apiManager, context->ResourceManager()));
	return false;
}

//----------------------------------------------------------------------------

bool	CRendererCacheInstance_UpdateThread::UpdateThread_Build(const PCRendererDataBase &rendererData, const RHI::SGPUCaps &gpuCaps, HBO::CContext *context, const CString &shaderFolder, RHI::EGraphicalApi apiName)
{
	PK_SCOPEDPROFILE();
	SPrepareArg	args(rendererData, context, &gpuCaps);
	args.m_ApiName = apiName;
 	args.m_ShaderFolder = shaderFolder;
	m_Flags.m_HasUV = false;
	m_Flags.m_HasNormal = false;
	m_Flags.m_HasTangent = false;
	m_Flags.m_FlipU = false;
	m_Flags.m_FlipV = false;
	m_Flags.m_RotateTexture = false;
	m_Flags.m_HasAtlasBlending = false;
	m_Flags.m_HasRibbonCorrectDeformation = false;

	m_RenderPasses = args.m_RenderPasses;

	if (rendererData->m_Declaration.IsFeatureEnabled(BasicRendererProperties::SID_Lit()))
		m_CastShadows = rendererData->m_Declaration.GetPropertyValue_B(BasicRendererProperties::SID_Lit_CastShadows(), false);
	if (!m_CastShadows && rendererData->m_Declaration.IsFeatureEnabled(BasicRendererProperties::SID_LegacyLit()))
		m_CastShadows = rendererData->m_Declaration.GetPropertyValue_B(BasicRendererProperties::SID_LegacyLit_CastShadows(), false);
	if (!m_CastShadows && rendererData->m_Declaration.IsFeatureEnabled(BasicRendererProperties::SID_LegacyLitOpaque()))
		m_CastShadows = rendererData->m_Declaration.GetPropertyValue_B(BasicRendererProperties::SID_LegacyLitOpaque_CastShadows(), false);
	if (!m_CastShadows && rendererData->m_Declaration.IsFeatureEnabled(BasicRendererProperties::SID_Opaque()))
		m_CastShadows = rendererData->m_Declaration.GetPropertyValue_B(BasicRendererProperties::SID_Opaque_CastShadows(), false);

	// TextureUvs feature:
	const SRendererFeaturePropertyValue	*textureUVs = rendererData->m_Declaration.FindProperty(BasicRendererProperties::SID_TextureUVs());
	const SRendererFeaturePropertyValue	*textureUVsFlipU = rendererData->m_Declaration.FindProperty(BasicRendererProperties::SID_TextureUVs_FlipU());
	const SRendererFeaturePropertyValue	*textureUVsFlipV = rendererData->m_Declaration.FindProperty(BasicRendererProperties::SID_TextureUVs_FlipV());
	const SRendererFeaturePropertyValue	*textureUVsRotateTexture = rendererData->m_Declaration.FindProperty(BasicRendererProperties::SID_TextureUVs_RotateTexture());
	const bool	hasTextureUVs = textureUVs != null && textureUVs->ValueB();

	if (hasTextureUVs)
	{
		PK_ASSERT(rendererData->m_RendererType == Renderer_Ribbon);
		m_Flags.m_FlipU = textureUVsFlipU != null ? textureUVsFlipU->ValueB() : false;
		m_Flags.m_FlipV = textureUVsFlipV != null ? textureUVsFlipV->ValueB() : false;
		m_Flags.m_RotateTexture = textureUVsRotateTexture != null ? textureUVsRotateTexture->ValueB() : false;
	}

	// Flip UV feature (flips V):
	const SRendererFeaturePropertyValue	*flipUV = rendererData->m_Declaration.FindProperty(BasicRendererProperties::SID_FlipUVs());
	const bool	hasFlipUV = flipUV != null && flipUV->ValueB();

	if (hasFlipUV)
	{
		PK_ASSERT(rendererData->m_RendererType == Renderer_Billboard);
		PK_ASSERT(!m_Flags.m_FlipU);
		PK_ASSERT(!m_Flags.m_FlipV);
		m_Flags.m_FlipU = false;
		m_Flags.m_FlipV = true;
	}

	// Basic Transform feature (flips U and V independently):
	const SRendererFeaturePropertyValue	*basicTransformUVs = rendererData->m_Declaration.FindProperty(BasicRendererProperties::SID_BasicTransformUVs());
	const SRendererFeaturePropertyValue	*basicTransformUVsFlipU = rendererData->m_Declaration.FindProperty(BasicRendererProperties::SID_BasicTransformUVs_FlipU());
	const SRendererFeaturePropertyValue	*basicTransformUVsFlipV = rendererData->m_Declaration.FindProperty(BasicRendererProperties::SID_BasicTransformUVs_FlipV());
	const SRendererFeaturePropertyValue	*basicTransformUVsRotateUV = rendererData->m_Declaration.FindProperty(BasicRendererProperties::SID_BasicTransformUVs_RotateUV());
	const bool	hasBasicTransform = basicTransformUVs != null && basicTransformUVs->ValueB();

	if (hasBasicTransform)
	{
		PK_ASSERT(!m_Flags.m_FlipU);
		PK_ASSERT(!m_Flags.m_FlipV);
		m_Flags.m_FlipU = basicTransformUVsFlipU != null && basicTransformUVsFlipU->ValueB();
		m_Flags.m_FlipV = basicTransformUVsFlipV != null && basicTransformUVsFlipV->ValueB();
		m_Flags.m_RotateTexture = basicTransformUVsRotateUV != null && basicTransformUVsRotateUV->ValueB();
	}

	const SRendererFeaturePropertyValue	*atlas = rendererData->m_Declaration.FindProperty(BasicRendererProperties::SID_Atlas());
	const SRendererFeaturePropertyValue	*atlasBlending = rendererData->m_Declaration.FindProperty(BasicRendererProperties::SID_Atlas_Blending());
	const bool	hasAtlas = atlas != null && atlas->ValueB();
	const bool	needsSecondUVSet = hasAtlas && (atlasBlending != null && atlasBlending->ValueI().x() >= 1);

	const SRendererFeaturePropertyValue *alphaMasks = rendererData->m_Declaration.FindProperty(BasicRendererProperties::SID_AlphaMasks());
	const SRendererFeaturePropertyValue *uvDistortions = rendererData->m_Declaration.FindProperty(BasicRendererProperties::SID_UVDistortions());

	if (hasAtlas && ((uvDistortions != null && uvDistortions->ValueB()) || (alphaMasks != null && alphaMasks->ValueB())))
		m_Flags.m_HasRawUV0 = true;

	if (needsSecondUVSet)
		m_Flags.m_HasAtlasBlending = true;

	const CStringId	strIdFeature_CorrectDeformation = BasicRendererProperties::SID_CorrectDeformation();

	for (const SToggledRenderingFeature &renderingFeature : args.m_FeaturesSettings)
	{
		if (renderingFeature.m_Settings != null)
		{
			m_Flags.m_HasUV = m_Flags.m_HasUV | renderingFeature.m_Settings->UseUV();
			m_Flags.m_HasNormal = m_Flags.m_HasNormal | renderingFeature.m_Settings->UseNormal();
			m_Flags.m_HasTangent = m_Flags.m_HasTangent | renderingFeature.m_Settings->UseTangent();
		}
		if (renderingFeature.m_FeatureName == strIdFeature_CorrectDeformation)
			m_Flags.m_HasRibbonCorrectDeformation = true;
	}

	const SRendererFeaturePropertyValue	*transparentFeature = rendererData->m_Declaration.FindProperty(BasicRendererProperties::SID_Transparent());
	const SRendererFeaturePropertyValue	*transparentType = rendererData->m_Declaration.FindProperty(BasicRendererProperties::SID_Transparent_Type());
	const SRendererFeaturePropertyValue	*disto = rendererData->m_Declaration.FindProperty(BasicRendererProperties::SID_Distortion());
	const SRendererFeaturePropertyValue	*diffuse = rendererData->m_Declaration.FindProperty(BasicRendererProperties::SID_Diffuse());

	PK_TODO("Find a way to propagate enum info into RendererDeclaration");
	m_Flags.m_NeedSort |= (transparentType != null && transparentType->ValueI().x() >= 2);
	m_Flags.m_NeedSort |= (disto != null && disto->ValueB() && diffuse != null && diffuse->ValueB()); // If we have a diffuse and a distortion, this means alpha-blend
	m_Flags.m_NeedSort |= (transparentType == null && transparentFeature != null && diffuse != null && diffuse->ValueB()); // If there is diffuse transparency
	m_Flags.m_Slicable = transparentFeature != null; // Right now, only supports non-opaque billboards/ribbons as slicable

	// FIXME: to remove. Load the mesh here to get their bbox. The mesh won't be loaded twice as we keep a refptr from the resource manager.
	if (m_MeshResource != null)
		m_MeshResource->m_OnReloaded -= FastDelegate<void(CResourceMesh*)>(this, &CRendererCacheInstance_UpdateThread::_OnMeshReloaded);
	const SRendererFeaturePropertyValue	*propMesh = rendererData->m_Declaration.FindProperty(BasicRendererProperties::SID_Mesh());
	CString								remappedPath = (propMesh != null) ? propMesh->m_ValuePath : CString();
	m_MeshResource = args.m_ResourceManager->Load<CResourceMesh>(remappedPath, false, SResourceLoadCtl(false, true));

	// reset info
	m_GlobalMeshBounds.Degenerate();
	m_SubMeshBounds.Clear();

	if (m_MeshResource != null && !m_MeshResource->Empty())
	{
		m_MeshResource->m_OnReloaded += FastDelegate<void(CResourceMesh*)>(this, &CRendererCacheInstance_UpdateThread::_OnMeshReloaded);

		m_SubMeshBounds.Resize(m_MeshResource->BatchList().Count());
		for (u32 iMesh = 0; iMesh < m_SubMeshBounds.Count(); ++iMesh)
		{
			m_SubMeshBounds[iMesh] = m_MeshResource->BatchList()[iMesh]->RawMesh()->BBox();
			m_GlobalMeshBounds.Add(m_SubMeshBounds[iMesh]);
		}
		if (!PK_VERIFY(m_PerLODMeshCount.Resize(m_MeshResource->LODCount())))
			return false;
		for (u32 lodIdx = 0; lodIdx < m_PerLODMeshCount.Count(); ++lodIdx)
			m_PerLODMeshCount[lodIdx] = m_MeshResource->BatchList(lodIdx).Count();
	}
	else
	{
		// Editor only debugging, allows to draw default mesh:
		if (!PK_VERIFY(m_PerLODMeshCount.Resize(1)))
			return false;
		m_PerLODMeshCount.First() = 1;
	}

	SRendererCacheInstanceKey	rendererCacheKey;
	m_RendererCacheInstanceId = CRendererCacheInstanceManager::UpdateThread_GetResource(rendererCacheKey, args);
	return m_RendererCacheInstanceId.Valid();
}

//----------------------------------------------------------------------------

PCRendererCacheInstance		CRendererCacheInstance_UpdateThread::RenderThread_GetCacheInstance()
{
	PCRendererCacheInstance	cacheInstance = CRendererCacheInstanceManager::RenderThread_GetResource(m_RendererCacheInstanceId);

	if (cacheInstance != null)
	{
		m_LastResolveSucceed = true;
	}
	else if (m_LastResolveSucceed)
	{
		CLog::Log(PK_WARN, "The renderer cache instance could not be resolved: it might not be created yet or its creation has failed");
		m_LastResolveSucceed = false;
	}
	return cacheInstance;
}

//----------------------------------------------------------------------------

void	CRendererCacheInstance_UpdateThread::RenderThread_FlushAllResources()
{
	// We flush all the unused resources (no dependencies, we need to do it for all the used managers):
#define	X_GRAPHIC_RESOURCE(__name)	C ## __name ## Manager::RenderThread_FlushUnusedResources()
	EXEC_X_GRAPHIC_RESOURCE(;)
#undef X_GRAPHIC_RESOURCE
}

//----------------------------------------------------------------------------

void	CRendererCacheInstance_UpdateThread::RenderThread_ClearAllGpuResources()
{
	// We flush all the unused resources (no dependencies, we need to do it for all the used managers):
#define	X_GRAPHIC_RESOURCE(__name)	C ## __name ## Manager::RenderThread_ClearGpuResources()
	EXEC_X_GRAPHIC_RESOURCE(;)
#undef X_GRAPHIC_RESOURCE
}

//----------------------------------------------------------------------------

void	CRendererCacheInstance_UpdateThread::RenderThread_DestroyAllResources()
{
	// We flush all the unused resources (no dependencies, we need to do it for all the used managers):
#define	X_GRAPHIC_RESOURCE(__name)	C ## __name ## Manager::RenderThread_Destroy()
	EXEC_X_GRAPHIC_RESOURCE(;)
#undef X_GRAPHIC_RESOURCE
}

//----------------------------------------------------------------------------

void	CRendererCacheInstance_UpdateThread::_OnMeshReloaded(CResourceMesh *mesh)
{
	(void)mesh;
	m_GlobalMeshBounds.Degenerate();
	m_SubMeshBounds.Clear();

	m_SubMeshBounds.Resize(m_MeshResource->BatchList().Count());
	for (u32 iMesh = 0; iMesh < m_SubMeshBounds.Count(); ++iMesh)
	{
		m_SubMeshBounds[iMesh] = m_MeshResource->BatchList()[iMesh]->RawMesh()->BBox();
		m_GlobalMeshBounds.Add(m_SubMeshBounds[iMesh]);
	}
	if (!PK_VERIFY(m_PerLODMeshCount.Resize(m_MeshResource->LODCount())))
		return;
	for (u32 lodIdx = 0; lodIdx < m_PerLODMeshCount.Count(); ++lodIdx)
		m_PerLODMeshCount[lodIdx] = m_MeshResource->BatchList(lodIdx).Count();
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
