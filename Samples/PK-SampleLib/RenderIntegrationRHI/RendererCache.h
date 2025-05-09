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
#include "PK-SampleLib/ShaderLoader.h"

#include <pk_render_helpers/include/frame_collector/rh_particle_render_data_factory.h>

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

	bool			operator == (const CRendererCacheInstance_UpdateThread &oth) const
	{
		return	m_RendererCacheInstanceId == oth.m_RendererCacheInstanceId &&
				m_CastShadows == oth.m_CastShadows &&
				m_IsHighlighted == oth.m_IsHighlighted &&
				m_IsHidden == oth.m_IsHidden;
	}

	// Editor specific features (but that can be plugged in any other RHI integration)
	bool			CastShadows() const { return m_CastShadows; }
	bool			IsHighlighted() const { return m_IsHighlighted; }
	void			SetHighlighted(bool isHighlighted) { m_IsHighlighted = isHighlighted; }
	bool			IsHidden() const { return m_IsHidden; }
	void			SetHidden(bool isHidden) { m_IsHidden = isHidden; }

private:
	void			_OnMeshReloaded(CResourceMesh *mesh);

	CRenderPassArray							m_RenderPasses;
	bool										m_LastResolveSucceed = true;

	CRendererCacheInstanceManager::CResourceId	m_RendererCacheInstanceId;
	TResourcePtr<CResourceMesh>					m_MeshResource;

	bool	m_CastShadows = false;
	bool	m_IsHighlighted = false;
	bool	m_IsHidden = false;

};
PK_DECLARE_REFPTRCLASS(RendererCacheInstance_UpdateThread);

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
