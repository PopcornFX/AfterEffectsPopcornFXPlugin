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

#include <pk_kernel/include/kr_string_id.h>
#include <pk_rhi/include/FwdInterfaces.h>

__PK_API_BEGIN
namespace HBO { class CContext; }
struct	SLinkGPUContext;
__PK_API_END

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

namespace	SimInterfaces
{
	bool	Bind_ConstantBuffer(const RHI::PGpuBuffer &buffer, const SLinkGPUContext &context);
	bool	Bind_Texture(const RHI::PTexture &texture, const SLinkGPUContext &context);
	bool	Bind_Sampler(CStringId mangledName, const SLinkGPUContext &context);

	//----------------------------------------------------------------------------

	// SI_GBuffer_ProjectToPosition(), SI_GBuffer_ProjectToNormal(), ..
	bool	BindGBufferSamplingSimInterfaces(const CString &coreLibPath, HBO::CContext *context = null);
	void	UnbindGBufferSamplingSimInterfaces(const CString &coreLibPath);
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
