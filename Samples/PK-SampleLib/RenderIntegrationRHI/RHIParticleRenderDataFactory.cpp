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

#include "RHIParticleRenderDataFactory.h"

#include "RHIBillboardingBatch_CPUsim.h"
#include "RHIBillboardingBatch_GPUsim.h"

// Custom RHI policy:
#include "RHIParticleRenderDataFactory.h"

// Material to RHI
#include "MaterialToRHI.h"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

bool	CRHIParticleRenderDataFactory::UpdateThread_Initialize(	RHI::PApiManager apiManager,
																HBO::CContext *hboContext,
																const RHI::SGPUCaps &gpuCaps,
																Drawers::EBillboardingLocation billboardingLocation)
{
	PK_SCOPEDLOCK(m_FactoryLock); // Those members are accessed by the render thread
	m_ApiManager = apiManager;
	m_ApiName = apiManager->ApiName();
	m_HBOContext = hboContext;
	m_GPUCaps = gpuCaps;
	m_BillboardingLocation = billboardingLocation;
	m_IsInitialized = true;
	return true;
}

//----------------------------------------------------------------------------

void	CRHIParticleRenderDataFactory::UpdateThread_Release()
{
	PK_SCOPEDLOCK(m_FactoryLock); // Those members are accessed by the render thread
	m_IsInitialized = false;
	m_HBOContext = null;
	m_PendingCaches.Clear();
}

//----------------------------------------------------------------------------

bool	CRHIParticleRenderDataFactory::IsInitializedWithAPI(RHI::EGraphicalApi apiName)
{
	return m_IsInitialized && apiName == m_ApiName;
}

//----------------------------------------------------------------------------

void	CRHIParticleRenderDataFactory::UpdateThread_SetBillboardingLocation(Drawers::EBillboardingLocation billboardingLocation)
{
	PK_SCOPEDLOCK(m_FactoryLock);
	m_BillboardingLocation = billboardingLocation;
}

//----------------------------------------------------------------------------

PRendererCacheBase	CRHIParticleRenderDataFactory::UpdateThread_CreateRendererCache(const PRendererDataBase &renderer, const CParticleDescriptor *particleDesc)
{
	(void)particleDesc;
	PK_ASSERT(m_IsInitialized); // Check if initialized!
	// We create the renderer cache:
	PRendererCacheInstance_UpdateThread	rendererCache = PK_NEW(CRendererCacheInstance_UpdateThread());

	if (rendererCache == null)
		return null;
	if (!rendererCache->UpdateThread_Build(renderer, m_GPUCaps, m_HBOContext, kDefaultShadersFolder, m_ApiName))
	{
		CLog::Log(PK_ERROR, "Could not create the renderer cache");
		return null;
	}

	PK_SCOPEDLOCK(m_PendingCachesLock);
	m_PendingCaches.PushBack(rendererCache);
	return rendererCache;
}

//----------------------------------------------------------------------------

bool	CRHIParticleRenderDataFactory::RenderThread_BuildPendingCaches(const RHI::PApiManager &apiManager)
{
	// This method is not really useful as all the renderer caches are calling the same static function
	// that builds all the renderer caches.

	if (!PK_VERIFY(m_IsInitialized)) // Check if initialized!
		return false;

	PK_SCOPEDLOCK(m_FactoryLock); // Any render thread function that need to access the member set by the UpdateThread_Initialize need a lock
	PK_SCOPEDLOCK(m_PendingCachesLock);

	const u32	matCount = m_PendingCaches.Count();
	bool		success = true;
	for (u32 iMat = 0; iMat < matCount; ++iMat)
	{
		PRendererCacheInstance_UpdateThread	rendererCache = m_PendingCaches[iMat];
		PK_ASSERT(rendererCache != null);

			if (!rendererCache->RenderThread_Build(apiManager, m_HBOContext))
		{
			CLog::Log(PK_ERROR, "Could not create the graphical resources for an effect created this frame");
			success = false;
		}
	}
	m_PendingCaches.Clear();
	return success;
}

//----------------------------------------------------------------------------

CRendererBatchDrawer *CRHIParticleRenderDataFactory::CreateBillboardingBatch2(ERendererClass rendererType, const PRendererCacheBase &rendererCache, bool gpuStorage)
{
	(void)rendererCache;
	if (!PK_VERIFY(m_IsInitialized)) // Check if initialized!
		return null;

	if (gpuStorage)
	{
		switch (rendererType)
		{
		case	Renderer_Billboard:
			if (m_BillboardingLocation == Drawers::BillboardingLocation_VertexShader && m_GPUCaps.m_SupportsShaderResourceViews)
				return PK_NEW(CRHIRendererBatch_BillboardGPU_VertexBB(m_ApiManager));
			else if (m_GPUCaps.m_SupportsGeometryShaders)
				return PK_NEW(CRHIRendererBatch_BillboardGPU_GeomBB(m_ApiManager));
			else
				return null;

		case	Renderer_Ribbon:
			if (m_GPUCaps.m_SupportsShaderResourceViews)
				return PK_NEW(CRHIRendererBatch_Ribbon_GPU(m_ApiManager));
			else
				return null;

		case	Renderer_Mesh:
			return PK_NEW(CRHIRendererBatch_Mesh_GPU(m_ApiManager));

		//case	Renderer_Decal:
		//	return PK_NEW(CRHIRendererBatch_Decal_GPU(m_ApiManager));

		//case	Renderer_Triangle:
		//	return PK_NEW(CRHIRendererBatch_Triangle_GPU(m_ApiManager));

		default:
			break;
		}
	}
	else
	{
		switch (rendererType)
		{
		case	Renderer_Billboard:
		{
			if (m_BillboardingLocation == Drawers::BillboardingLocation_VertexShader && m_GPUCaps.m_SupportsShaderResourceViews)
				return PK_NEW(CRHIRendererBatch_Billboard_VertexBB(m_ApiManager));
			else if (m_BillboardingLocation == Drawers::BillboardingLocation_GeometryShader && m_GPUCaps.m_SupportsGeometryShaders)
				return PK_NEW(CRHIRendererBatch_Billboard_GeomBB(m_ApiManager));
			else
				return PK_NEW(CRHIRendererBatch_Billboard_CPUBB(m_ApiManager));
		}

		case	Renderer_Ribbon:
			return PK_NEW(CRHIRendererBatch_Ribbon_CPU(m_ApiManager));

		case	Renderer_Mesh:
			return PK_NEW(CRHIRendererBatch_Mesh_CPU(m_ApiManager));

		case	Renderer_Decal:
			return PK_NEW(CRHIRendererBatch_Decal_CPU(m_ApiManager));

		case	Renderer_Triangle:
		{
			if (m_BillboardingLocation != Drawers::BillboardingLocation_CPU && m_GPUCaps.m_SupportsShaderResourceViews)
				return PK_NEW(CRHIRendererBatch_Triangle_VertexBB(m_ApiManager));
			else
				return PK_NEW(CRHIRendererBatch_Triangle_CPUBB(m_ApiManager));
		}

		case	Renderer_Light:
			return PK_NEW(CRHIRendererBatch_Light_CPU(m_ApiManager));

		default:
			break;
		}
	}

	return null;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
