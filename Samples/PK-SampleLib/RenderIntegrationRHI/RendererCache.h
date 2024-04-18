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
#include "PK-SampleLib/RenderIntegrationRHI/RHIGraphicResources.h"
#include "PK-SampleLib/RenderIntegrationRHI/FeatureRenderingSettings.h"
#include <pk_render_helpers/include/frame_collector/rh_particle_render_data_factory.h>
#include "PK-SampleLib/ShaderLoader.h"

__PK_SAMPLE_API_BEGIN

//----------------------------------------------------------------------------
//
//	Renderer Cache Instance
//
//----------------------------------------------------------------------------

class CRendererCacheInstance_UpdateThread : public CRendererCacheBase
{
public:
	CRendererCacheInstance_UpdateThread() { }
	virtual ~CRendererCacheInstance_UpdateThread();

	virtual void	UpdateThread_BuildBillboardingFlags(const PRendererDataBase &renderer) override;

	bool			UpdateThread_Build(const PCRendererDataBase &rendererData, const RHI::SGPUCaps &gpuCaps, HBO::CContext *context, const CString &shaderFolder = CString(), RHI::EGraphicalApi apiName = RHI::GApi_Null); // This is called first
	bool			RenderThread_Build(const RHI::PApiManager &apiManager, HBO::CContext *context); // Then this is called

	PCRendererCacheInstance		RenderThread_GetCacheInstance();

	static void		RenderThread_FlushAllResources();
	static void		RenderThread_ClearAllGpuResources();
	static void		RenderThread_DestroyAllResources();

	bool			operator == (const CRendererCacheInstance_UpdateThread &oth) const { return m_RendererCacheInstanceId == oth.m_RendererCacheInstanceId; }

	bool			CastShadows() const { return m_CastShadows; }

private:
	void			_OnMeshReloaded(CResourceMesh *mesh);

	CRenderPassArray							m_RenderPasses;
	bool										m_LastResolveSucceed = true;
	bool										m_CastShadows = false;
	CRendererCacheInstanceManager::CResourceId	m_RendererCacheInstanceId;
	TResourcePtr<CResourceMesh>					m_MeshResource;
};
PK_DECLARE_REFPTRCLASS(RendererCacheInstance_UpdateThread);

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
