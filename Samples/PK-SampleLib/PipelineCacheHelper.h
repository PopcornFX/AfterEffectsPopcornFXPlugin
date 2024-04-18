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
#include <pk_rhi/include/interfaces/IApiManager.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

class	CPipelineCacheHelper
{
public:
	CPipelineCacheHelper();
	~CPipelineCacheHelper();

	void			LoadPipelineCache(const RHI::PApiManager &apiManager, const CString &filePath);
	bool			SavePipelineCache(const RHI::PApiManager &apiManager, const CString &filePath);
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
