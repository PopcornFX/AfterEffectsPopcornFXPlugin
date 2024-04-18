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

#include <popcornfx.h>

#include <pkapi/include/pk_precompiled_default.h>

// Note (Alex) : fix me, render helper import badly d3d11.h everywhere because of missing abstraction for specific storage
//  -> see rh_gpu_batch.inl and ps_stream_to_render.h
#if defined(PK_PARTICLES_UPDATER_USE_D3D11) && (PK_PARTICLES_UPDATER_USE_D3D11 != 0)
#	include <pk_rhi/include/D3D11/D3D11RHI.h>
#endif

#define __PK_SAMPLE_API_BEGIN		namespace	PKSample {
#define __PK_SAMPLE_API_END		}

#if defined(PK_DESKTOP_TOOLS)
#	define	PK_SAMPLE_LIB_HAS_SHADER_GENERATOR	1
#else
#	define	PK_SAMPLE_LIB_HAS_SHADER_GENERATOR	0
#endif

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

using namespace PK_NAMESPACE;

// Todo(Hugo/Alex/Thomas): Definitely a bad place for this.
const char	kDefaultShadersFolder[] = "Shaders";

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
