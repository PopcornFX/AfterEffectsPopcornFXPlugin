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

// Default implementations of the batches:
#include <pk_render_helpers/include/batches/rh_billboard_batch.h>
#include <pk_render_helpers/include/batches/rh_ribbon_batch.h>
#include <pk_render_helpers/include/batches/rh_mesh_batch.h>
#include <pk_render_helpers/include/batches/rh_decal_batch.h>
#include <pk_render_helpers/include/batches/rh_triangle_batch.h>
#include <pk_render_helpers/include/batches/rh_light_batch.h>
#include <pk_render_helpers/include/batches/rh_sound_batch.h>

#include "RHIBillboardingBatchPolicy.h"
#include "RHIBillboardingBatchPolicy_Vertex.h"

// Custom RHI policy:
#include "RHIParticleRenderDataFactory.h"

// Material to RHI
#include "MaterialToRHI.h"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

bool	CRHIParticleRenderDataFactory::UpdateThread_Initialize(	RHI::EGraphicalApi apiName,
																HBO::CContext *hboContext,
																const RHI::SGPUCaps &gpuCaps,
																Drawers::EBillboardingLocation billboardingLocation)
{
	PK_SCOPEDLOCK(m_FactoryLock); // Those members are accessed by the render thread
	m_ApiName = apiName;
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

Drawers::EBillboardingLocation	CRHIParticleRenderDataFactory::ResolveBillboardingLocationForStorage(bool gpuStorage)
{
	PK_SCOPEDLOCK(m_FactoryLock);

	Drawers::EBillboardingLocation	bbLocation = m_BillboardingLocation;

	// Default fallback: CPU -> Geometry shader -> Vertex shader
	if (gpuStorage)
	{
		PK_ASSERT(m_GPUCaps.m_SupportsShaderResourceViews || m_GPUCaps.m_SupportsGeometryShaders);

		if (bbLocation == Drawers::BillboardingLocation_CPU)
			bbLocation = Drawers::BillboardingLocation_GeometryShader;

		// Fallback on Geometry shader billboarding
		if (bbLocation == Drawers::BillboardingLocation_VertexShader && !m_GPUCaps.m_SupportsShaderResourceViews)
			bbLocation = Drawers::BillboardingLocation_GeometryShader;
		// Fallback on CPU billboarding
		if (bbLocation == Drawers::BillboardingLocation_GeometryShader && !m_GPUCaps.m_SupportsGeometryShaders)
			bbLocation = Drawers::BillboardingLocation_VertexShader;
	}
	else
	{
		// Fallback on Geometry shader billboarding
		if (bbLocation == Drawers::BillboardingLocation_VertexShader && !m_GPUCaps.m_SupportsShaderResourceViews)
			bbLocation = Drawers::BillboardingLocation_GeometryShader;
		// Fallback on CPU billboarding
		if (bbLocation == Drawers::BillboardingLocation_GeometryShader && !m_GPUCaps.m_SupportsGeometryShaders)
			bbLocation = Drawers::BillboardingLocation_CPU;
	}
	return bbLocation;
}

//----------------------------------------------------------------------------

CRHIParticleRenderDataFactory::CBillboardingBatchInterface	*CRHIParticleRenderDataFactory::CreateBillboardingBatch(ERendererClass rendererType, const PRendererCacheBase &rendererCache, bool gpuStorage)
{
	(void)rendererCache;
	if (!PK_VERIFY(m_IsInitialized)) // Check if initialized!
		return null;

	// Default billboarding batch implementations:
	typedef	TBillboardBatch<CRHIParticleBatchTypes, CRHIBillboardingBatchPolicy>		CBillboardBillboardingBatch;
	typedef	TBillboardBatch<CRHIParticleBatchTypes, CRHIBillboardingBatchPolicy_Vertex>	CBillboardBillboardingBatch_Vertex;
	typedef	TRibbonBatch<CRHIParticleBatchTypes, CRHIBillboardingBatchPolicy>			CRibbonBillboardingBatch;
	typedef	TRibbonBatch<CRHIParticleBatchTypes, CRHIBillboardingBatchPolicy_Vertex>	CRibbonBillboardingBatch_Vertex;
	typedef	TMeshBatch<CRHIParticleBatchTypes, CRHIBillboardingBatchPolicy>				CMeshBillboardingBatch;
	typedef	TDecalBatch<CRHIParticleBatchTypes, CRHIBillboardingBatchPolicy>			CDecalBillboardingBatch;
	typedef	TTriangleBatch<CRHIParticleBatchTypes, CRHIBillboardingBatchPolicy>			CTriangleBillboardingBatch;
	typedef	TTriangleBatch<CRHIParticleBatchTypes, CRHIBillboardingBatchPolicy_Vertex>	CTriangleBillboardingBatch_Vertex;
	typedef TLightBatch<CRHIParticleBatchTypes, CRHIBillboardingBatchPolicy>			CLightBillboardingBatch;
	typedef TSoundBatch<CRHIParticleBatchTypes, CRHIBillboardingBatchPolicy>			CSoundBillboardingBatch;

	const Drawers::EBillboardingLocation	bbLocation = ResolveBillboardingLocationForStorage(gpuStorage);

	if (gpuStorage)
	{
		if (bbLocation != Drawers::BillboardingLocation_CPU)
		{
			switch (rendererType)
			{
			case	Renderer_Billboard:
			{
				switch (bbLocation)
				{
				case	Drawers::BillboardingLocation_GeometryShader:
				{
					CBillboardBillboardingBatch	*batch = PK_NEW((CBillboardBillboardingBatch));
					if (PK_VERIFY(batch != null))
						batch->SetBillboardingLocation(Drawers::BillboardingLocation_GeometryShader);
					return batch;
				}
				case	Drawers::BillboardingLocation_VertexShader:
				{
					CBillboardBillboardingBatch_Vertex	*batch = PK_NEW(CBillboardBillboardingBatch_Vertex);
					if (!PK_VERIFY(batch != null))
						return null;
					batch->SetBillboardingLocation(Drawers::BillboardingLocation_VertexShader);
					return batch;
				}
				default:
					PK_ASSERT_NOT_REACHED(); // Compute shader billboarding not implemented in RHIParticleRenderDataFactory
					break;
				}
				return null;
			}
			case	Renderer_Mesh:
			{
				CMeshBillboardingBatch	*batch = PK_NEW(CMeshBillboardingBatch);
				if (!PK_VERIFY(batch != null))
					return null;
				return batch;
			}
			case	Renderer_Ribbon:
			{
				CRibbonBillboardingBatch_Vertex	*batch = PK_NEW(CRibbonBillboardingBatch_Vertex);
				if (!PK_VERIFY(batch != null))
					return null;
				batch->SetBillboardingLocation(Drawers::BillboardingLocation_VertexShader);
				return batch;
			}
			default:
				break;
			}
		}
	}
	else
	{
		switch (rendererType)
		{
		case	Renderer_Billboard:
		{
			switch (bbLocation)
			{
			case	Drawers::BillboardingLocation_CPU:
			case	Drawers::BillboardingLocation_GeometryShader:
			{
				CBillboardBillboardingBatch	*batch = PK_NEW((CBillboardBillboardingBatch));
				if (!PK_VERIFY(batch != null))
					return null;
				batch->SetBillboardingLocation(bbLocation);
				return batch;
			}
			case	Drawers::BillboardingLocation_VertexShader:
			{
				CBillboardBillboardingBatch_Vertex	*batch = PK_NEW(CBillboardBillboardingBatch_Vertex);
				if (!PK_VERIFY(batch != null))
					return null;
				batch->SetBillboardingLocation(bbLocation);
				return batch;
			}
			default:
				PK_ASSERT_NOT_REACHED(); // Compute shader billboarding not implemented in RHIParticleRenderDataFactory
				break;
			}
			return null;
		}
		case	Renderer_Ribbon:
		{
			CRibbonBillboardingBatch	*batch = PK_NEW(CRibbonBillboardingBatch);
			return batch;
		}
		case	Renderer_Mesh:
			return PK_NEW(CMeshBillboardingBatch);
		case	Renderer_Triangle:
		{
			switch (bbLocation)
			{
			case	Drawers::BillboardingLocation_CPU:
			case	Drawers::BillboardingLocation_GeometryShader: // Falls back to BillboardingLocation_CPU
			{
				CTriangleBillboardingBatch	*batch = PK_NEW(CTriangleBillboardingBatch);
				if (!PK_VERIFY(batch != null))
					return null;
				batch->SetBillboardingLocation(Drawers::BillboardingLocation_CPU);
				return batch;
			}
			case	Drawers::BillboardingLocation_VertexShader:
			{
				CTriangleBillboardingBatch_Vertex	*batch = PK_NEW(CTriangleBillboardingBatch_Vertex);
				if (!PK_VERIFY(batch != null))
					return null;
				batch->SetBillboardingLocation(bbLocation);
				return batch;
			}
			default:
				PK_ASSERT_NOT_REACHED();
				break;
			}
			return null;
		}
		case	Renderer_Decal:
			return PK_NEW(CDecalBillboardingBatch);
		case	Renderer_Light:
			return PK_NEW(CLightBillboardingBatch);
		case	Renderer_Sound:
			return PK_NEW(CSoundBillboardingBatch);
		default:
			PK_ASSERT_NOT_REACHED();
		}
	}
	return null;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
