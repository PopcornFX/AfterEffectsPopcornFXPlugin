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

#if	(PK_BUILD_WITH_METAL_SUPPORT != 0)

__PK_RHI_API_BEGIN
PK_FORWARD_DECLARE_INTERFACE(ApiManager);
__PK_RHI_API_END

__PK_SAMPLE_API_BEGIN

PK_FORWARD_DECLARE_INTERFACE(ApiContext);

namespace	MetalFactory
{
	PApiContext				CreateContext();
	RHI::PApiManager		CreateApiManager();
}

__PK_SAMPLE_API_END

#endif // PK_BUILD_WITH_METAL_SUPPORT
