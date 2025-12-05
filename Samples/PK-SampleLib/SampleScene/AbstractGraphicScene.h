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
#include "PK-SampleLib/ShaderLoader.h"
#include "PK-SampleLib/ProfilerRenderer.h"	// for PKSAMPLE_HAS_PROFILER_RENDERER. TODO: DO NOT define configuration macros in general headers, forces code to #include all those headers even if it just cares about #ifdefing stuff out. Make configuration headers like in the runtime !

#include "pk_rhi/include/interfaces/IFrameBuffer.h"
#include "pk_rhi/include/interfaces/ICommandBuffer.h"
#include "pk_rhi/include/interfaces/IRenderTarget.h"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

PK_FORWARD_DECLARE(AbstractWindowContext);
PK_FORWARD_DECLARE_INTERFACE(ApiContext);

//----------------------------------------------------------------------------

class	CAbstractGraphicScene
{
public:
	CAbstractGraphicScene();
	virtual ~CAbstractGraphicScene();

	virtual bool		Init(RHI::EGraphicalApi api, bool forceDirectDraw = false, const CString &windowTitle = CString::EmptyString, bool renderOffscreen = false, const CUint2 &windowSize = CUint2(1920, 1080));
	virtual bool		Run();
	virtual bool		Quit();

protected:
	virtual bool		UserInit() = 0;
	virtual bool		LateUserInit() { return true; }
	virtual bool		UserQuit();

	virtual bool		CreateRenderTargets(bool recreateSwapChain) = 0;
	virtual bool		CreateRenderPasses() { return true; }
	virtual bool		CreateRenderStates() { return true; }
	virtual bool		CreateFrameBuffers(bool recreateSwapChain) = 0;

	// Create a single buffer, can be override
	virtual bool		CreateCommandBuffers(bool immediateMode, u32 swapImgIdx);
	virtual void		FillCommandBuffer(const RHI::PCommandBuffer &cmdBuff, u32 swapImgIdx) = 0;

	// Called for deferred command buffers
	virtual bool		SubmitCommandBuffers(u32 swapImgIdx);

	virtual void		DrawHUD() { };
	virtual bool		Update() = 0;
	virtual bool		PostRender() = 0;

	RHI::EGraphicalApi	GetGraphicApiName() const;

	bool									m_UserInitCalled;

	// the window context handles the window and the inputs
	PAbstractWindowContext					m_WindowContext;
	// The Api manager is used to draw
	RHI::PApiManager						m_ApiManager;
	// The Api context handles the initialization of the Api
	PApiContext								m_ApiContext;

	// Bunch of command buffer when using defered command buffers
	TArray<RHI::PCommandBuffer>				m_CommandBuffers;

	TMemoryView<const RHI::PRenderTarget>	m_SwapChainRenderTargets;

	CShaderLoader							m_ShaderLoader;

	CTimer									m_FrameTimer;
	float									m_TotalTime;
	float									m_Dt;
	u32										m_FrameIdx;

	bool									m_UseDirectDraw;
	bool									m_RenderOffscreen;

#if	(PKSAMPLE_HAS_PROFILER_RENDERER == 1)
	CProfilerRenderer						*m_ProfilerRenderer;
	CProfilerRenderer::SProfilerRenderData	m_ProfilerData;
#endif
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
