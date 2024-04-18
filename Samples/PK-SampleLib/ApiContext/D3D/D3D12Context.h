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

#include "PK-SampleLib/ApiContextConfig.h"

#if (PK_BUILD_WITH_D3D12_SUPPORT != 0) && defined(PK_WINDOWS)

#include <pk_rhi/include/D3D12/D3D12BasicContext.h>
#include <pk_rhi/include/D3D12/D3D12Fence.h>
#include "PK-SampleLib/ApiContext/IApiContext.h"
#include "PK-SampleLib/WindowContext/AWindowContext.h"

struct	SDL_Window;

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

struct	SD3D12PlatformContext;

//----------------------------------------------------------------------------

class	CD3D12Context : public IApiContext
{
public:
	CD3D12Context();
	~CD3D12Context();

	virtual bool									InitRenderApiContext(bool debug, PAbstractWindowContext windowApi) override;
	virtual bool									WaitAllRenderFinished() override;
	virtual CGuid									BeginFrame() override;
	virtual bool									EndFrame(void *renderToWait) override;
	virtual RHI::SApiContext						*GetRenderApiContext() override { return &m_ApiData; }
	virtual bool									RecreateSwapChain(const CUint2 &ctxSize) override;
	virtual TMemoryView<const RHI::PRenderTarget>	GetCurrentSwapChain() override;

	static const u32			kFrameCount = 3;

	RHI::SD3D12BasicContext			*GetD3D12Context() { return &m_ApiData; }
	const RHI::SD3D12BasicContext	*GetD3D12Context() const { return &m_ApiData; }

	bool						InitContext(bool debug);
#if defined(PK_WINDOWS)
	bool						AddSwapChain(HWND winHandle, CUint2 winSize, TMemoryView<const RHI::PRenderTarget> *view = null);
#endif

	CGuid						BeginFrame(u32 swapchainIdx);
	bool						EndFrame(u32 swapchainIdx);

	bool						DestroySwapChain(u32 swapChainIdx);
	bool						RecreateSwapChain(u32 swapChainIdx, const CUint2 &ctxSize);

private:
	bool						LoadDynamicLibrary();

	bool						EnableDebugLayer();
	bool						CreateDevice(bool debug);
	void						AdditionalFilterForDebugLayer();
	bool						CreateCommandQueue();
	bool						PickHardwareAdapter();
	bool						CreateDescriptorAllocator();

	bool						CreateSwapChain(u32 swapChainIdx, HWND winHandle, CUint2 winSize, TMemoryView<const RHI::PRenderTarget> *view);
	bool						CreateRenderTargets(u32 swapChainIdx, CUint2 winSize, TMemoryView<const RHI::PRenderTarget> *view);
	bool						CreateOffscreenRenderTarget(const CUint2 &winSize);

	SD3D12PlatformContext		*m_Context;
	RHI::SD3D12BasicContext		m_ApiData;
};
PK_DECLARE_REFPTRCLASS(D3D12Context);

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

#endif	// (PK_BUILD_WITH_D3D12_SUPPORT != 0) && defined(PK_WINDOWS)
