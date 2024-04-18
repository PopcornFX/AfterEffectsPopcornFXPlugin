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
#include "PK-SampleLib/ApiContext/IApiContext.h"
#include "PK-SampleLib/WindowContext/AWindowContext.h"

#include <pk_rhi/include/metal/MetalBasicContext.h>

#if (PK_BUILD_WITH_METAL_SUPPORT != 0) && defined(PK_MACOSX)

//----------------------------------------------------------------------------

@interface PKMetalView : NSView

- (instancetype)initWithFrame:(NSRect)frame
                      highDPI:(BOOL)highDPI;

@property (nonatomic) BOOL highDPI;

@end

//----------------------------------------------------------------------------

__PK_RHI_API_BEGIN
	struct SWaitAllSwapChains;
	PK_FORWARD_DECLARE(MetalRenderTarget);
__PK_RHI_API_END

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

class	CMetalContext : public IApiContext
{
public:
	CMetalContext();
	~CMetalContext();

	virtual bool									InitRenderApiContext(bool debug, PAbstractWindowContext windowApi) override;
	virtual bool									WaitAllRenderFinished() override;
	virtual CGuid									BeginFrame() override;
	virtual bool									EndFrame(void *renderToWait) override;
	virtual RHI::SApiContext						*GetRenderApiContext() override;
	virtual bool									RecreateSwapChain(const CUint2 &ctxSize) override;
	virtual TMemoryView<const RHI::PRenderTarget>	GetCurrentSwapChain() override;

	// Create final flip render state / render pass:
	struct	SFinalBlit
	{
		MTLRenderPassDescriptor 	*m_RenderPassDesc;
		id<MTLLibrary>				m_Library;
		id<MTLFunction> 			m_VertexFunc;
		id<MTLFunction> 			m_FragmentFunc;
		id<MTLRenderPipelineState>	m_PipelineState;
	};

	static bool 									CreateFinalBlitData(id<MTLDevice> device,
																		SFinalBlit &finalBlit,
																		RHI::EPixelFormat pxlFormat = RHI::FormatSrgb8BGRA);
	static void 									DestroyFinalBlitData(SFinalBlit &finalBlit);

	static bool 									EndFrame(	id<MTLCommandQueue> queue,
																const TMemoryView<RHI::SMetalBasicContext::SMetalSwapChain> &swapChains,
																const SFinalBlit &finalBlit,
																RHI::SWaitAllSwapChains *syncInfo,
																bool isOffScreen,
																const RHI::PRenderTarget renderTarget = null);
	static bool 									MetalCreateView(NSView *currentView, PKMetalView *&pkView, bool highDPI);
	static void 									MetalRemoveView(PKMetalView *&pkView);
	static bool 									MetalSetupLayer(id<MTLDevice> device,
																	PKMetalView *&pkView,
																	const CUint2 &ctxSize,
																	RHI::EPixelFormat pxlFormat = RHI::FormatSrgb8BGRA);
	static RHI::PMetalRenderTarget					MetalCreateRenderTarget(id<MTLDevice> device,
																			const CUint2 &ctxSize,
																			RHI::EPixelFormat pxlFormat = RHI::FormatSrgb8BGRA,
																			bool cpuAccessible = false);

private:
	// Cannot directly have the SMetalBasicContext as we cannot include it in a cpp file
	// the goal is that most include should be able to be included in cpp files...
	RHI::SMetalBasicContext		m_ApiData;
	RHI::PMetalRenderTarget 	m_SwapChainRT;

	SFinalBlit					m_FinalBlit;

	PKMetalView					*m_CurrentView;
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

#endif // (PK_BUILD_WITH_METAL_SUPPORT != 0)

