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

#include <ApiContextConfig.h>

#include "AbstractGraphicScene.h"
#include "WindowContext/AWindowContext.h"
#include "WindowContext/SdlContext/SdlContext.h"
#include "WindowContext/OffscreenContext/OffscreenContext.h"
#include "ApiContext/IApiContext.h"
#include "ApiContext/Metal/MetalContextFactory.h"
#include <pk_rhi/include/AllInterfaces.h>
#include <pk_rhi/include/null/NullApiManager.h>

// Just in case the ApiManager does not include the BasicContext
#include <pk_rhi/include/interfaces/SApiContext.h>

#include "ImguiRhiImplem.h"

#if	(PK_BUILD_WITH_OGL_SUPPORT != 0)
#include "ApiContext/OpenGL/GLContext.h"
#include <pk_rhi/include/opengl/OpenGLApiManager.h>
#endif

#if (PK_BUILD_WITH_VULKAN_SUPPORT != 0)
#	include "ApiContext/Vulkan/VulkanContext.h"
#	include <pk_rhi/include/vulkan/VulkanApiManager.h>
#endif

#if (PK_BUILD_WITH_D3D12_SUPPORT != 0)
#	include "ApiContext/D3D/D3D12Context.h"
#	include <pk_rhi/include/D3D12/D3D12ApiManager.h>
#endif

#if (PK_BUILD_WITH_D3D11_SUPPORT != 0)
#	include "ApiContext/D3D/D3D11Context.h"
#	include <pk_rhi/include/D3D11/D3D11ApiManager.h>
#endif

#if (PK_BUILD_WITH_ORBIS_SUPPORT != 0)
#	include "WindowContext/OrbisContext/OrbisApplicationContext.h"
#	include "ApiContext/Orbis/OrbisGraphicContext.h"
#	include <pk_rhi/include/orbis/OrbisApiManager.h>
#endif

#if (PK_BUILD_WITH_UNKNOWN2_SUPPORT != 0)
#	include "WindowContext/UNKNOWN2Context/UNKNOWN2ApplicationContext.h"
#	include "ApiContext/UNKNOWN2/UNKNOWN2GraphicContext.h"
#	include <pk_rhi/include/UNKNOWN2/UNKNOWN2ApiManager.h>
#endif

#if defined(PK_DURANGO)
#	include "WindowContext/DurangoContext/DurangoApplicationContext.h"
#	include "ApiContext/Durango/D3D11DurangoGraphicContext.h"
#	include "ApiContext/Durango/D3D12DurangoGraphicContext.h"
#endif

#if defined(PK_SCARLETT)
	// TODO: Change this, Scarlett should NOT include Durango files.
	// Prevents clean SDK separation when packaging SDKs and excluding platform files
	// that are not validated by the vendor for the current licensee
#	include "ApiContext/Durango/D3D12DurangoGraphicContext.h"
#endif

#if defined(PK_GGP)
#	include "WindowContext/GgpContext/GgpContext.h"
#endif

#if defined(PK_GDK)
#	include "WindowContext/GdkContext/GdkContext.h"
#endif

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

CAbstractGraphicScene::CAbstractGraphicScene()
:	m_UserInitCalled(false)
,	m_TotalTime(0)
,	m_Dt(0)
,	m_FrameIdx(0)
,	m_UseDirectDraw(false)
,	m_RenderOffscreen(false)
#if	PKSAMPLE_HAS_PROFILER_RENDERER
,	m_ProfilerRenderer(null)
#endif
{
}

//----------------------------------------------------------------------------

CAbstractGraphicScene::~CAbstractGraphicScene()
{
	m_ApiContext = null;
	m_WindowContext = null;
}

//----------------------------------------------------------------------------

bool	CAbstractGraphicScene::Init(RHI::EGraphicalApi api, bool forceDirectDraw/* = false*/, const CString &windowTitle /*= CString::EmptyString*/, bool renderOffscreen/* = false*/)
{
#if	(PK_BUILD_WITH_SDL != 0)
	if (renderOffscreen)
		m_WindowContext = PK_NEW(COffscreenContext);
	else
		m_WindowContext = PK_NEW(CSdlContext);
#elif defined(PK_DURANGO) && !defined(PK_GDK)
	m_WindowContext = PK_NEW(CDurangoApplicationContext);
#elif defined(PK_GDK)
	m_WindowContext = PK_NEW(CGdkWindowContext);
#elif defined(PK_GGP)
	m_WindowContext = PK_NEW(CGgpContext);
#endif
	if (api == RHI::GApi_Null)
	{
		m_ApiContext = null;
		m_ApiManager = PK_NEW(RHI::CNullApiManager);
	}
#if (PK_BUILD_WITH_VULKAN_SUPPORT != 0)
	else if (api == RHI::GApi_Vulkan)
	{
		m_ApiContext = PK_NEW(CVulkanContext);
		m_ApiManager = PK_NEW(RHI::CVulkanApiManager);
	}
#endif
#if (PK_BUILD_WITH_METAL_SUPPORT != 0)
	else if (api == RHI::GApi_Metal)
	{
		// We use a factory for metal because we cannot include the metal headers in cpp files (only in objc .mm files)
		m_ApiContext = MetalFactory::CreateContext();
		m_ApiManager = MetalFactory::CreateApiManager();
	}
#endif
#if (PK_BUILD_WITH_OGL_SUPPORT != 0)
	else if (api == RHI::GApi_OES)
	{
		m_ApiContext = PK_NEW(CGLContext(CGLContext::GL_OpenGLES));
		m_ApiManager = PK_NEW(RHI::COpenGLApiManager);
	}
	else if (api == RHI::GApi_OpenGL)
	{
		m_ApiContext = PK_NEW(CGLContext(CGLContext::GL_OpenGL));
		m_ApiManager = PK_NEW(RHI::COpenGLApiManager);
	}
#endif
#if	(PK_BUILD_WITH_ORBIS_SUPPORT != 0)
	else if (api == RHI::GApi_Orbis)
	{
		m_WindowContext = PK_NEW(COrbisApplicationContext);
		m_ApiContext = PK_NEW(COrbisGraphicContext);
		m_ApiManager = PK_NEW(RHI::COrbisApiManager);
	}
#endif
#if	(PK_BUILD_WITH_UNKNOWN2_SUPPORT != 0)
	else if (api == RHI::GApi_UNKNOWN2)
	{
		m_WindowContext = PK_NEW(CUNKNOWN2ApplicationContext);
		m_ApiContext = PK_NEW(CUNKNOWN2GraphicContext);
		m_ApiManager = PK_NEW(RHI::CUNKNOWN2ApiManager);
	}
#endif
#if	(PK_BUILD_WITH_D3D12_SUPPORT != 0)
	else if (api == RHI::GApi_D3D12)
	{
#	if defined(PK_DURANGO) || defined(PK_SCARLETT)
		m_ApiContext = PK_NEW(CD3D12DurangoGraphicContext);
#	else
		m_ApiContext = PK_NEW(CD3D12Context);
#	endif // defined(PK_DURANGO)
		m_ApiManager = PK_NEW(RHI::CD3D12ApiManager);
	}
#endif // (PK_BUILD_WITH_D3D12_SUPPORT != 0)
#if	(PK_BUILD_WITH_D3D11_SUPPORT != 0)
	else if (api == RHI::GApi_D3D11)
	{
# if		defined(PK_DURANGO)
		m_ApiContext = PK_NEW(CD3D11DurangoGraphicContext);
# else
		m_ApiContext = PK_NEW(CD3D11Context);
# endif
		m_ApiManager = PK_NEW(RHI::CD3D11ApiManager);
	}
#endif // (PK_BUILD_WITH_D3D11_SUPPORT != 0)
	else
	{
		PK_ASSERT_NOT_REACHED();
	}
	// Check allocs
	if (m_WindowContext == null || m_ApiManager == null)
		return false;

	// Init window context
	if (!m_WindowContext->Init(api, windowTitle))
	{
		CLog::Log(PK_ERROR, "m_WindowContext->Init failed");
		return false;
	}
	// Init IMGUI
	if (!renderOffscreen && !m_WindowContext->InitImgui(m_ApiManager))
	{
		CLog::Log(PK_ERROR, "m_WindowContext->InitImgui failed");
		return false;
	}

	m_UseDirectDraw = m_ApiManager->ApiDesc().m_SupportImmediateCommandBuffers &&
					(forceDirectDraw || !m_ApiManager->ApiDesc().m_SupportDeferredCommandBuffers);

	m_RenderOffscreen = renderOffscreen;

	if (m_ApiContext == null)
	{
		CLog::Log(PK_ERROR, "m_ApiContext is null");
		return false;
	}
#if	defined(PK_DEBUG)
	if (!m_ApiContext->InitRenderApiContext(true, m_WindowContext))
#else
	if (!m_ApiContext->InitRenderApiContext(false, m_WindowContext))
#endif
	{
		CLog::Log(PK_ERROR, "m_ApiContext->InitRenderApiContext failed");
		return false;
	}
	if (!m_ApiManager->InitApi(m_ApiContext->GetRenderApiContext()))
	{
		CLog::Log(PK_ERROR, "m_ApiManager->InitApi failed");
		return false;
	}
	m_SwapChainRenderTargets = m_ApiContext->GetCurrentSwapChain();
	if (m_SwapChainRenderTargets.Empty())
	{
		CLog::Log(PK_ERROR, "m_SwapChainRenderTargets is empty");
		return false;
	}

#if	PKSAMPLE_HAS_PROFILER_RENDERER
	m_ProfilerRenderer = PK_NEW(CProfilerRenderer);
	if (m_ProfilerRenderer == null)
		return false;
	m_WindowContext->RegisterProfiler(m_ProfilerRenderer);
#endif
	// We process the events in the case where the window is resized on the first frame:
	// we must have at least one end frame after the UserInit() and after the SwapChainSizeChanged()
	m_WindowContext->ProcessEvents();

	m_UserInitCalled = true;
	return UserInit();
}

//----------------------------------------------------------------------------

bool	CAbstractGraphicScene::Run()
{
	bool	exitSample = false;

	if (!CreateRenderTargets(false) ||
		!CreateRenderPasses() ||
		!CreateRenderStates() ||
		!CreateFrameBuffers(false))
		return false;

	if (!m_UseDirectDraw)
	{
		for (u32 i = 0; i < m_SwapChainRenderTargets.Count(); ++i)
		{
			if (!CreateCommandBuffers(false, i))
				return false;
		}
	}

	if (!LateUserInit())
		return false;

	// Main loop
	m_FrameTimer.Start();

	while (!exitSample)
	{
#if	PKSAMPLE_HAS_PROFILER_RENDERER
		m_ProfilerRenderer->ProfilerNextFrameTick();
		{
			PK_NAMEDSCOPEDPROFILE("Profiler Overhead");
			CUint2 			drawSize = m_SwapChainRenderTargets.First()->GetSize();
			float 			pxlRatio = 1.0f;

			if (drawSize != m_WindowContext->GetWindowSize())
			{
				drawSize = m_WindowContext->GetWindowSize();
				pxlRatio = m_WindowContext->GetPixelRatio();
			}
			m_ProfilerRenderer->GenerateProfilerRenderData(	m_Dt,
															CIRect(CInt2(0),
															CInt2(drawSize)),
															m_WindowContext->GetMousePosition(),
															pxlRatio);
			m_ProfilerRenderer->LockAndCopyProfilerData(m_ProfilerData);
		}
#endif

		PK_NAMEDSCOPEDPROFILE_C("MainLoop", CFloat3(0.2f, 0.2f, 0.2f));

		m_Dt = m_FrameTimer.Restart();

		m_TotalTime += m_Dt;
		++m_FrameIdx;

		exitSample = !m_WindowContext->ProcessEvents();

		if (m_WindowContext->HasWindowChanged())
		{
			// Recreate the swap chain
			m_ApiContext->RecreateSwapChain(m_WindowContext->GetDrawableSize());
			// Notify the api manager that the swap chain was changed
			m_ApiManager->SwapChainChanged(0);
			m_SwapChainRenderTargets = m_ApiContext->GetCurrentSwapChain();
			if (m_SwapChainRenderTargets.Empty())
				return false;

			if (!CreateRenderTargets(true) ||
				!CreateFrameBuffers(true))
				return false;

			if (!m_UseDirectDraw)
			{
				for (u32 i = 0; i < m_SwapChainRenderTargets.Count(); ++i)
				{
					if (!CreateCommandBuffers(false, i))
						return false;
				}
			}
		}

		if (m_WindowContext->WindowHidden())
		{
			continue;
		}

		CGuid	swapChainIdx;
		{
			PK_NAMEDSCOPEDPROFILE_C("ApiContext::BeginFrame", CFloat3(0, 0, 0.5f));
			// Only waits for the presentation
			swapChainIdx = m_ApiContext->BeginFrame();
			if (!swapChainIdx.Valid())
			{
				CLog::Log(PK_ERROR, "m_ApiContext->BeginFrame() failure");
				return false;
			}
		}
		{
			PK_NAMEDSCOPEDPROFILE_C("ApiManager::BeginFrame", CFloat3(0.3f, 0.3f, 1));
			m_ApiManager->BeginFrame(swapChainIdx);
		}
		{
			PK_NAMEDSCOPEDPROFILE("DrawHUD");
			if (!m_RenderOffscreen)
				DrawHUD();
		}
		{
			PK_NAMEDSCOPEDPROFILE_C("Update", CFloat3(1, 0, 1));
			if (!Update())
				return false;
		}
		{
			if (m_UseDirectDraw)
			{
				PK_NAMEDSCOPEDPROFILE_C("CreateImmediateCommandBuffers", CFloat3(0, 1, 0));
				if (!CreateCommandBuffers(true, swapChainIdx))
					return false;
			}
			else if (!SubmitCommandBuffers(swapChainIdx))
			{
				return false;
			}
		}
		{
			PK_NAMEDSCOPEDPROFILE_C("PostRender", CFloat3(0.5f, 0, 0.5f));
			if (!PostRender())
				return false;
		}
		void	*renderFinished;
		{
			PK_NAMEDSCOPEDPROFILE_C("ApiManager::EndFrame()", CFloat3(1, 0.3f, 0.3f));
			renderFinished = m_ApiManager->EndFrame();
		}
		{
			PK_NAMEDSCOPEDPROFILE_C("ApiContext::EndFrame()", CFloat3(0.5f, 0, 0));
			if (!m_ApiContext->EndFrame(renderFinished))
			{
				CLog::Log(PK_ERROR, "m_ApiContext->EndFrame() failure");
				return false;
			}
		}

		// TODO: Rethink this info display
#if 0//(PK_BUILD_WITH_GPU_PROFILING_SUPPORT != 0)
		RHI::PMemoryUsage	usedMemory = m_ApiContext->GetRenderApiContext()->m_UsedMemory;
		const u64			totalMemory = usedMemory->GetUsedMemory();
		if (fmodf(m_TotalTime, 10.f) <= m_Dt)
		{
			CLog::Log(	PK_INFO,
						"GPU Memory used:\n\tBuffers \t%lld\n\tTextures\t%lld\n\tRT\t\t\t%lld\n\tTotal\t\t%lld",
						usedMemory->GetUsedBuffers(), usedMemory->GetUsedTextures(), usedMemory->GetUsedRenderTargets(), totalMemory);
		}
#endif
	}

	return true;
}

//----------------------------------------------------------------------------

bool	CAbstractGraphicScene::CreateCommandBuffers(bool immediateMode, u32 swapImgIdx)
{
	RHI::PCommandBuffer	cmdBuff;
	if (immediateMode)
	{
		cmdBuff = m_ApiManager->GetCurrentImmediateCommandBuffer();
	}
	else
	{
		cmdBuff = m_ApiManager->CreateCommandBuffer(RHI::SRHIResourceInfos("Abstract Graphics Scene Command Buffer"));
		if (m_CommandBuffers.Count() != m_SwapChainRenderTargets.Count() &&
			!m_CommandBuffers.Resize(m_SwapChainRenderTargets.Count()))
			return false;
		m_CommandBuffers[swapImgIdx] = cmdBuff;
	}

	if (cmdBuff != null)
		FillCommandBuffer(cmdBuff, swapImgIdx);

	return cmdBuff != null;
}

//----------------------------------------------------------------------------

bool	CAbstractGraphicScene::SubmitCommandBuffers(u32 swapImgIdx)
{
	return m_ApiManager->SubmitCommandBufferDirect(m_CommandBuffers[swapImgIdx]);
}

//----------------------------------------------------------------------------

bool	CAbstractGraphicScene::Quit()
{
	bool	success = true;

	if (m_ApiContext != null)
		success &= m_ApiContext->WaitAllRenderFinished();

	m_GpuBuffers.Clear();
	m_CommandBuffers.Clear();
	if (m_ApiManager != null)
		success &= m_ApiManager->DestroyApi();

	// Only call user quit when all async api operations are finished & when all used resources references are released.
	if (m_UserInitCalled)
	{
		success &= UserQuit();
		m_UserInitCalled = false;
	}

	m_GpuBuffers.Clear();
	m_CommandBuffers.Clear();
	if (m_WindowContext != null)
		m_WindowContext->Destroy();

#if	PKSAMPLE_HAS_PROFILER_RENDERER
	if (m_ProfilerRenderer != null)
		PK_SAFE_DELETE(m_ProfilerRenderer);
#endif
	success &= m_ShaderLoader.Release();
	return success;
}

//----------------------------------------------------------------------------

bool	CAbstractGraphicScene::UserQuit()
{
	return true;
}

//----------------------------------------------------------------------------

RHI::EGraphicalApi	CAbstractGraphicScene::GetGraphicApiName() const
{
	PK_ASSERT(m_ApiContext != null);
	PK_ASSERT(m_ApiContext->GetRenderApiContext() != null);
	return m_ApiContext->GetRenderApiContext()->m_Api;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
