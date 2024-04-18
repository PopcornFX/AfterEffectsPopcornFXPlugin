//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef	__FX_COPYPIXELS_H__
#define	__FX_COPYPIXELS_H__

#include "AEGP_Define.h"

#include <AE_Effect.h>

namespace PopcornFX
{
	PK_FORWARD_DECLARE(RefCountedMemoryBuffer);
}

//----------------------------------------------------------------------------

__AEGP_PK_BEGIN

struct	SCopyPixel {
	PRefCountedMemoryBuffer		m_BufferPtr;
	PF_EffectWorld				*m_InputWorld;
	double						m_Gamma;

	bool						m_IsAlphaOverride;
	double						m_AlphaOverrideValue;
};

//----------------------------------------------------------------------------

PF_Err	CopyPixelIn32(void *refcon, A_long x, A_long y, PF_Pixel32 *inP, PF_Pixel32 *);
PF_Err	CopyPixelIn16(void *refcon, A_long x, A_long y, PF_Pixel16 *inP, PF_Pixel16 *);
PF_Err	CopyPixelIn8(void *refcon, A_long x, A_long y, PF_Pixel8 *inP, PF_Pixel8 *);

PF_Err	CopyPixelOut32(void *refcon, A_long x, A_long y, PF_Pixel32 *, PF_Pixel32 *outP);
PF_Err	CopyPixelOut16(void *refcon, A_long x, A_long y, PF_Pixel16 *, PF_Pixel16 *outP);
PF_Err	CopyPixelOut8(void *refcon, A_long x, A_long y, PF_Pixel8 *, PF_Pixel8 *outP);

//----------------------------------------------------------------------------

__AEGP_PK_END

#endif
