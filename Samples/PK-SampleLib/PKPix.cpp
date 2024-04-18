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
#include "PKPix.h"

#if	(KR_PROFILER_ENABLED != 0)
#if defined(PK_BUILD_WITH_D3D12_SUPPORT) && (PK_BUILD_WITH_D3D12_SUPPORT != 0)

#include <pk_rhi/include/D3D12/D3D12CommandBuffer.h>

#if !defined(PK_RETAIL)
#	if defined(_MSC_VER) && (_MSC_VER >= 1900) && (_MSC_VER < 1910)	// >= vs2015, < vs2017
#		pragma warning(disable: 4577)	// Silence vs2015 noexcept warning
#	endif
#	define PROFILE
#	include <pix3.h>
#	undef PROFILE
#endif

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

bool	PKPixBeginEvent(void *arg, const Profiler::SNodeDescriptor *nodeDescriptor, const Profiler::SGPUProfileEventContext *ctx)
{
#if defined(USE_PIX)
	(void)arg;
	if (ctx->m_CommandList != null)
	{
		PIXBeginEvent(ctx->m_CommandList, nodeDescriptor->m_ColorBGRA8, nodeDescriptor->m_Name);
		return true;
	}
	else if (ctx->m_CommandQueue != null)
	{
		PIXBeginEvent(ctx->m_CommandQueue, nodeDescriptor->m_ColorBGRA8, nodeDescriptor->m_Name);
		return true;
	}
#else
	(void)arg; (void)nodeDescriptor; (void)ctx;
#endif
	return false;
}

//----------------------------------------------------------------------------

void	PKPixEndEvent(void *arg, const Profiler::SNodeDescriptor *nodeDescriptor, const Profiler::SGPUProfileEventContext *ctx)
{
#if defined(USE_PIX)
	(void)arg;
	(void)nodeDescriptor;
	if (ctx->m_CommandList != null)
	{
		PIXEndEvent(ctx->m_CommandList);
	}
	else if (ctx->m_CommandQueue != null)
	{
		PIXEndEvent(ctx->m_CommandQueue);
	}
#else
	(void)arg; (void)nodeDescriptor; (void)ctx;
#endif
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

#endif //(PK_BUILD_WITH_D3D12_SUPPORT != 0)
#endif //(KR_PROFILER_ENABLED != 0)
