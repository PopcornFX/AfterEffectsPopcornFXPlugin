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

#include <pk_toolkit/include/pk_toolkit_version.h>
#include <pk_kernel/include/kr_static_config_flags.h>

#include "PK-SampleLib/PKSampleInit.h"
#include "PK-SampleLib/ImguiRhiImplem.h"
#include "PK-SampleLib/RenderIntegrationRHI/RendererCache.h"
#include "pk_render_helpers/include/render_features/rh_features_basic.h"

PK_LOG_MODULE_DECLARE();

//----------------------------------------------------------------------------

#ifndef PK_API_LIB
#if	defined(PK_WINAPI)

BOOL APIENTRY	DllMain(HINSTANCE hLibModule, DWORD callOrigin, LPVOID reserved)
{
	PopcornFX::CPKSample::RegisterLibraryEntryPoint(hLibModule);
	::DisableThreadLibraryCalls(hLibModule);
	return TRUE;
}

#else

int	_init(void)
{
	return (0);
}

int	_fini(void)
{
	return (0);
}

#endif
#endif

__PK_API_BEGIN
//----------------------------------------------------------------------------

bool	CPKSampleBase::m_Active = false;

//----------------------------------------------------------------------------

bool	CPKSampleBase::InternalStartup(const Config &)
{
	PK_LOG_MODULE_INIT_START;

	PKSample::CRHIMaterialShaders::RegisterHandler();
	PKSample::CRHIRenderingFeature::RegisterHandler();
	PKSample::CRHIRenderingSettings::RegisterHandler();

	PKSample::SConstantAtlasKey::SetupConstantSetLayout();
	PKSample::SConstantNoiseTextureKey::SetupConstantSetLayout();
	PKSample::SConstantDrawRequests::SetupConstantSetLayouts();

	PKSample::STextureKey::SetupDefaultResource();
	PKSample::SGeometryKey::SetupDefaultResource();

	PK_LOG_MODULE_INIT_END;
	m_Active = true;
	return true;
}

//----------------------------------------------------------------------------

bool	CPKSampleBase::InternalShutdown()
{
	m_Active = false;
	PK_LOG_MODULE_RELEASE_START;

	PKSample::CRendererCacheInstance_UpdateThread::RenderThread_DestroyAllResources();

	PKSample::SConstantAtlasKey::ClearConstantSetLayoutIFN();
	PKSample::SConstantNoiseTextureKey::ClearConstantSetLayoutIFN();
	PKSample::SConstantDrawRequests::ClearConstantSetLayoutsIFN();

	PKSample::ImGuiPkRHI::QuitIFN();

	PKSample::CRHIRenderingSettings::UnregisterHandler();
	PKSample::CRHIRenderingFeature::UnregisterHandler();
	PKSample::CRHIMaterialShaders::UnregisterHandler();

	PK_LOG_MODULE_RELEASE_END;
	return true;
}

//----------------------------------------------------------------------------
__PK_API_END
