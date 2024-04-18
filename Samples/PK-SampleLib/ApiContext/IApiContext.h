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

#include <PK-SampleLib/ApiContextConfig.h>

#include <PK-SampleLib/PKSample.h>
#include <pk_rhi/include/RHI.h>

__PK_RHI_API_BEGIN
struct SApiContext;
__PK_RHI_API_END

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

PK_FORWARD_DECLARE(AbstractWindowContext);

//----------------------------------------------------------------------------

class	IApiContext : public CRefCountedObject
{
public:
	IApiContext() { }
	virtual ~IApiContext() { }

	virtual bool									InitRenderApiContext(bool debug, PAbstractWindowContext windowApi) = 0;
	virtual bool									WaitAllRenderFinished() = 0;
	virtual CGuid									BeginFrame() = 0;
	virtual bool									EndFrame(void *renderToWait) = 0;
	virtual RHI::SApiContext						*GetRenderApiContext() = 0;
	virtual bool									RecreateSwapChain(const CUint2 &ctxSize) = 0;
	virtual TMemoryView<const RHI::PRenderTarget>	GetCurrentSwapChain() = 0;
};
PK_DECLARE_REFPTRINTERFACE(ApiContext);

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
