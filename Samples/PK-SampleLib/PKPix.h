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

#include "PKSample.h"
#include <pk_kernel/include/kr_profiler.h>

#if	(KR_PROFILER_ENABLED != 0)
#if defined(PK_BUILD_WITH_D3D12_SUPPORT) && (PK_BUILD_WITH_D3D12_SUPPORT != 0)

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

bool	PKPixBeginEvent(void *arg, const Profiler::SNodeDescriptor *nodeDescriptor, const Profiler::SGPUProfileEventContext *ctx);

void	PKPixEndEvent(void *arg, const Profiler::SNodeDescriptor *nodeDescriptor,  const Profiler::SGPUProfileEventContext *ctx);

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

#endif //(PK_BUILD_WITH_D3D12_SUPPORT != 0)
#endif //(KR_PROFILER_ENABLED != 0)
