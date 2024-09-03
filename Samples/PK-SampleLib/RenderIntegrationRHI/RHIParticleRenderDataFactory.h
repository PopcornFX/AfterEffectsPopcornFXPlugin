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
#include "PK-SampleLib/RenderIntegrationRHI/RHITypePolicy.h"
#include "PK-SampleLib/RenderIntegrationRHI/FeatureRenderingSettings.h"

#include <pk_rhi/include/interfaces/SApiContext.h>

#include <pk_render_helpers/include/frame_collector/rh_particle_render_data_factory.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

// This class handles the creation of the "Renderer-cache" and "Renderer-batch"  (aka a factory)
class	CRHIParticleRenderDataFactory
{
public:
	CRHIParticleRenderDataFactory() : m_IsInitialized(false), m_ApiName(RHI::GApi_Null), m_HBOContext(null), m_BillboardingLocation(Drawers::BillboardingLocation_CPU)  { }
	virtual ~CRHIParticleRenderDataFactory() { }

public:
	// Create the renderer cache
	virtual PRendererCacheBase			UpdateThread_CreateRendererCache(const PRendererDataBase &renderer, const CParticleDescriptor *particleDesc);

	// Create the billboarding batch for this renderer type
	CRendererBatchDrawer				*CreateBillboardingBatch2(ERendererClass rendererType, const PRendererCacheBase &rendererCache, bool gpuStorage); //new

	void								UpdateThread_CollectedForRendering(const PRendererCacheBase &rendererCache) { (void)rendererCache; /* Nothing to do */ }

public:
	virtual bool						UpdateThread_Initialize(RHI::PApiManager apiManager, HBO::CContext *hboContext, const RHI::SGPUCaps &gpuCaps, Drawers::EBillboardingLocation billboardingLocation = Drawers::BillboardingLocation_CPU);
	virtual void						UpdateThread_Release();
	bool								IsInitializedWithAPI(RHI::EGraphicalApi apiName);

	void								UpdateThread_SetBillboardingLocation(Drawers::EBillboardingLocation billboardingLocation);
	Drawers::EBillboardingLocation		UpdateThread_BillboardingLocation() const { return m_BillboardingLocation; }

	bool								RenderThread_BuildPendingCaches(const RHI::PApiManager &apiManager);

protected:
	bool					m_IsInitialized;

	RHI::PApiManager		m_ApiManager;
	RHI::EGraphicalApi		m_ApiName;
	HBO::CContext			*m_HBOContext;
	RHI::SGPUCaps			m_GPUCaps;

	Drawers::EBillboardingLocation	m_BillboardingLocation;

	Threads::CCriticalSection						m_PendingCachesLock;
	TArray<PRendererCacheInstance_UpdateThread>		m_PendingCaches;

	// We add a lock to be able to re-initialize the factory on the game thread while its running:
	Threads::CCriticalSection		m_FactoryLock;
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
