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

#if	(PK_BUILD_WITH_METAL_SUPPORT != 0)

#include "pk_rhi/include/metal/MetalApiManager.h"
#include "ApiContext/Metal/MetalContext.h"

__PK_SAMPLE_API_BEGIN

namespace	MetalFactory
{
	PApiContext			CreateContext()
	{
		return PK_NEW(CMetalContext);
	}

	RHI::PApiManager	CreateApiManager()
	{
		return PK_NEW(RHI::CMetalApiManager);
	}
}

__PK_SAMPLE_API_END

#endif // PK_BUILD_WITH_METAL_SUPPORT
