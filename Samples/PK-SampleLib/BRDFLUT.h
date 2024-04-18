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
#include "pk_maths/include/pk_maths_fp16.h"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

enum { BRDFLUTArraySize = 64 * 64 * 2 * 2/*16*/ };
extern const PopcornFX::f16	BRDFLUTArray[];

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
