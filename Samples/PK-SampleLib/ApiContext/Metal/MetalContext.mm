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

#include "MetalContext.h"
#include "ApiContextConfig.h"

#if (PK_BUILD_WITH_METAL_SUPPORT != 0) && defined(PK_MACOSX)

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <SDL.h>
#include <SDL_syswm.h>

#include <WindowContext/SdlContext/SdlContext.h>
#include <pk_rhi/include/metal/MetalApiManager.h>
#include <pk_rhi/include/metal/MetalRenderTarget.h>
#include <pk_rhi/include/metal/MetalPopcornEnumConversion.h>

#include "MetalFinalBlitShader.h"

@implementation PKMetalView

+ (Class)layerClass
{
	return NSClassFromString(@"CAMetalLayer");
}

- (BOOL)wantsUpdateLayer
{
	return YES;
}

- (CALayer*)makeBackingLayer
{
	return [self.class.layerClass layer];
}

- (instancetype)initWithFrame:(NSRect)frame
						highDPI:(BOOL)highDPI
{
	if ((self = [super initWithFrame:frame]))
	{
		self.highDPI = highDPI;
		self.wantsLayer = YES;
		self.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
		[self updateDrawableSize];
	}
	return self;
}

- (void)updateDrawableSize
{
	CAMetalLayer *metalLayer = (CAMetalLayer *)self.layer;
	CGSize size = self.bounds.size;
	CGSize backingSize = size;

	if (self.highDPI)
	{
		backingSize = [self convertSizeToBacking:size];
	}

	metalLayer.contentsScale = backingSize.height / size.height;
	metalLayer.drawableSize = backingSize;
}

- (void)resizeWithOldSuperviewSize:(NSSize)oldSize
{
	[super resizeWithOldSuperviewSize:oldSize];
	[self updateDrawableSize];
}

@end

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

#if (PK_BUILD_WITH_SDL != 0)
PK_DECLARE_REFPTRCLASS(SdlContext);
#endif

//----------------------------------------------------------------------------

CMetalContext::CMetalContext()
:	m_CurrentView(null)
{
}

//----------------------------------------------------------------------------

CMetalContext::~CMetalContext()
{
	DestroyFinalBlitData(m_FinalBlit);
}

//----------------------------------------------------------------------------

bool	CMetalContext::InitRenderApiContext(bool debug, PAbstractWindowContext windowApi)
{
	(void)debug;

	// Create the device and the queue:
	id<MTLDevice>			device = MTLCreateSystemDefaultDevice();

	if (!PK_VERIFY(device != null))
		return false;

	id<MTLCommandQueue>	queue = [device newCommandQueue];

	if (!PK_VERIFY(queue != null))
		return false;

	m_ApiData.m_Api = RHI::GApi_Metal;
	// Fill the api data with the metal data:
	m_ApiData.m_Device = device;
	m_ApiData.m_Queue = queue;
	m_ApiData.m_SwapChainCount = 1;

	if (!PK_VERIFY(m_ApiData.m_SwapChains.Resize(1)))
		return false;

#if (PK_BUILD_WITH_SDL != 0)
	if (windowApi->GetContextApi() == PKSample::Context_Sdl)
	{
		// Get the window native info:
		PSdlContext		sdlWindowApi = static_cast<CSdlContext*>(windowApi.Get());
		SDL_SysWMinfo	info;
		SDL_VERSION(&info.version);
		SDL_GetWindowWMInfo(sdlWindowApi->SdlGetWindow(), &info);

		// Attach the metal layer to the sdl window:
		NSView 				*view = info.info.cocoa.window.contentView;

		if (!PK_VERIFY(MetalCreateView(view, m_CurrentView, sdlWindowApi->HasHighDPI())))
			return false;

		if (!PK_VERIFY(RecreateSwapChain(windowApi->GetDrawableSize())))
			return false;

		m_ApiData.m_SwapChains.First().m_Layer = (CAMetalLayer*)m_CurrentView.layer;
	}
	else if (windowApi->GetContextApi() == PKSample::Context_Offscreen)
#else
	if (windowApi->GetContextApi() == PKSample::Context_Offscreen)
#endif
	{
		if (!PK_VERIFY(RecreateSwapChain(windowApi->GetDrawableSize())))
			return false;

		m_ApiData.m_SwapChains.First().m_RenderTarget = m_SwapChainRT;
	}
	else
	{
		// Does not implement any other way to get window handle
		return false;
	}

	if (!PK_VERIFY(CreateFinalBlitData(device, m_FinalBlit)))
		return false;

	return true;
}

//----------------------------------------------------------------------------

bool	CMetalContext::WaitAllRenderFinished()
{
	return true;
}

//----------------------------------------------------------------------------

CGuid	CMetalContext::BeginFrame()
{
	return 0;
}

//----------------------------------------------------------------------------

bool	CMetalContext::EndFrame(void *renderToWait)
{
	RHI::SWaitAllSwapChains 	*syncInfo = reinterpret_cast<RHI::SWaitAllSwapChains*>(renderToWait);
	bool isOffScreen = (m_CurrentView == null);

	return EndFrame(m_ApiData.m_Queue, m_ApiData.m_SwapChains, m_FinalBlit, syncInfo, isOffScreen);
}

//----------------------------------------------------------------------------

RHI::SApiContext	*CMetalContext::GetRenderApiContext()
{
	return &m_ApiData;
}

//----------------------------------------------------------------------------

bool	CMetalContext::RecreateSwapChain(const CUint2 &ctxSize)
{
	PK_ASSERT(m_ApiData.m_SwapChains.Count() == 1);
	if (!PK_VERIFY(!m_ApiData.m_SwapChains.Empty()))
		return false;

	// offscreen rendering does not have a view instance
	if (m_CurrentView != null && !PK_VERIFY(MetalSetupLayer(m_ApiData.m_Device, m_CurrentView, ctxSize)))
		return false;

	m_SwapChainRT = MetalCreateRenderTarget(m_ApiData.m_Device, ctxSize);

	if (m_SwapChainRT == null)
		return false;

	m_ApiData.m_SwapChains.First().m_RenderTarget = m_SwapChainRT;
	return true;
}

//----------------------------------------------------------------------------

TMemoryView<const RHI::PRenderTarget>	CMetalContext::GetCurrentSwapChain()
{
	return TMemoryView<const RHI::PRenderTarget>(m_SwapChainRT);
}

//----------------------------------------------------------------------------

bool 	CMetalContext::CreateFinalBlitData(id<MTLDevice> device, SFinalBlit &finalBlit, RHI::EPixelFormat pxlFormat)
{
	// Create the shader functions:
	NSString 			*vertexFunc = [[NSString alloc] initWithUTF8String:"vert_main"];
	NSString 			*fragFunc = [[NSString alloc] initWithUTF8String:"frag_main"];
	NSError				*errorData = null;
	dispatch_data_t 	binData = dispatch_data_create(MetalFinalBlitShader_metallib, MetalFinalBlitShader_metallib_len, dispatch_get_main_queue(), DISPATCH_DATA_DESTRUCTOR_DEFAULT);

	finalBlit.m_Library = [device newLibraryWithData:binData error:&errorData];

	if (!PK_VERIFY(errorData == null))
		return false;

	finalBlit.m_VertexFunc = [finalBlit.m_Library newFunctionWithName:vertexFunc];
	finalBlit.m_FragmentFunc = [finalBlit.m_Library newFunctionWithName:fragFunc];

	if (!PK_VERIFY(finalBlit.m_VertexFunc != null && finalBlit.m_FragmentFunc != null))
		return false;

	// Create the render pass descriptor for the final blit:
	finalBlit.m_RenderPassDesc = [MTLRenderPassDescriptor renderPassDescriptor];
	finalBlit.m_RenderPassDesc.colorAttachments[0].loadAction  = MTLLoadActionDontCare;
	finalBlit.m_RenderPassDesc.colorAttachments[0].storeAction = MTLStoreActionStore;

	// Create the pipeline state:
	MTLRenderPipelineDescriptor *pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];

	if (!PK_VERIFY(pipelineStateDescriptor != nil))
		return false;

	pipelineStateDescriptor.label = @"Final blit pipeline state";
	pipelineStateDescriptor.supportIndirectCommandBuffers = NO;
	pipelineStateDescriptor.vertexFunction = finalBlit.m_VertexFunc;
	pipelineStateDescriptor.fragmentFunction = finalBlit.m_FragmentFunc;
	pipelineStateDescriptor.colorAttachments[0].pixelFormat = RHI::MetalConversion::PopcornToMetalPixelFormat(pxlFormat);

	finalBlit.m_PipelineState = [device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&errorData];

	if (!PK_VERIFY(errorData == null))
		return false;

	[finalBlit.m_RenderPassDesc retain];
	[finalBlit.m_Library retain];
	[finalBlit.m_VertexFunc retain];
	[finalBlit.m_FragmentFunc retain];
	[finalBlit.m_PipelineState retain];
	return true;
}

//----------------------------------------------------------------------------

void 	CMetalContext::DestroyFinalBlitData(SFinalBlit &finalBlit)
{
	[finalBlit.m_RenderPassDesc autorelease];
	[finalBlit.m_Library autorelease];
	[finalBlit.m_VertexFunc autorelease];
	[finalBlit.m_FragmentFunc autorelease];
	[finalBlit.m_PipelineState autorelease];
}

//----------------------------------------------------------------------------

bool 	CMetalContext::EndFrame(id<MTLCommandQueue> queue,
								const TMemoryView<RHI::SMetalBasicContext::SMetalSwapChain> &swapChains,
								const SFinalBlit &finalBlit,
								RHI::SWaitAllSwapChains *syncInfo,
								bool isOffScreen,
								const RHI::PRenderTarget renderTarget)
{
	PK_FOREACH(curSwap, swapChains)
	{
		id<MTLTexture> 			mtlRt = CastMetal(curSwap->m_RenderTarget)->MetalGetTexture();

		// We create a command buffer for the final blit:
		id<MTLCommandBuffer>	cmdBuff = [queue commandBuffer];
		id<CAMetalDrawable> 	drawable;

		if (curSwap->m_Layer != null)
		{
			drawable = [curSwap->m_Layer nextDrawable];
			finalBlit.m_RenderPassDesc.colorAttachments[0].texture = drawable.texture;
		}
		else
		{
			PK_ASSERT(renderTarget != null);
			finalBlit.m_RenderPassDesc.colorAttachments[0].texture = CastMetal(renderTarget)->MetalGetTexture();
		}

		id<MTLRenderCommandEncoder> encoder = [cmdBuff renderCommandEncoderWithDescriptor:finalBlit.m_RenderPassDesc];

		[encoder setRenderPipelineState:finalBlit.m_PipelineState];

		[encoder setFragmentTexture:mtlRt atIndex:0];

		[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];

		[encoder endEncoding];

		if (curSwap->m_Layer != null && !isOffScreen)
		{
			[cmdBuff presentDrawable:drawable];
		}
		else
		{
			id<MTLBlitCommandEncoder> blitEncoder = [cmdBuff blitCommandEncoder];
			[blitEncoder synchronizeTexture:CastMetal(renderTarget)->MetalGetTexture() slice:0 level:0];
			[blitEncoder endEncoding];
		}

		// Notify the sync info that the present is finished:
		[cmdBuff addCompletedHandler:^(id<MTLCommandBuffer> commandBuffer)
		{
			(void)commandBuffer;
			PK_SCOPEDLOCK(syncInfo->m_SwapChainDoneCountLock);
			++syncInfo->m_SwapChainDoneCount;
			if (syncInfo->m_SwapChainDoneCount == syncInfo->m_SwapChainToWait)
				syncInfo->m_CondVar.NotifyAll();
		}];

		[cmdBuff commit];
	}
	return true;
}

//----------------------------------------------------------------------------

bool 	CMetalContext::MetalCreateView(NSView *currentView, PKMetalView *&pkView, bool highDPI)
{
	MetalRemoveView(pkView);
	pkView = [[PKMetalView alloc] initWithFrame:currentView.frame highDPI:highDPI];

	if (!PK_VERIFY(pkView != nil))
		return false;

	[pkView retain];
	[currentView addSubview:pkView];
	return true;
}

//----------------------------------------------------------------------------

void 	CMetalContext::MetalRemoveView(PKMetalView *&pkView)
{
	if (pkView != null)
	{
		[pkView removeFromSuperview];
		[pkView autorelease];
	}
}

//----------------------------------------------------------------------------

bool 	CMetalContext::MetalSetupLayer(	id<MTLDevice> device,
										PKMetalView *&pkView,
										const CUint2 &ctxSize,
										RHI::EPixelFormat pxlFormat)
{
	CAMetalLayer 		*metalLayer = (CAMetalLayer*)pkView.layer;

	if (!PK_VERIFY(metalLayer != null))
		return false;

	metalLayer.device = device;
	metalLayer.pixelFormat = RHI::MetalConversion::PopcornToMetalPixelFormat(pxlFormat);
	metalLayer.framebufferOnly = true;
	metalLayer.drawableSize = NSMakeSize(ctxSize.x(), ctxSize.y());
	return true;
}

//----------------------------------------------------------------------------

RHI::PMetalRenderTarget		CMetalContext::MetalCreateRenderTarget(	id<MTLDevice> device,
																	const CUint2 &ctxSize,
																	RHI::EPixelFormat pxlFormat,
																	bool cpuAccessible)
{
	RHI::PMetalRenderTarget	mtlRt = PK_NEW(RHI::CMetalRenderTarget(RHI::SRHIResourceInfos("Metal Context Render Target"), null));

	if (!PK_VERIFY(mtlRt != null))
		return null;
	if (!PK_VERIFY(mtlRt->MetalCreateRenderTarget(	ctxSize,
													pxlFormat,
													true,
													RHI::SampleCount1,
													device,
													cpuAccessible)))
		return null;
	return mtlRt;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

#endif
