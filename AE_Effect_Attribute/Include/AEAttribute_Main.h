//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef	__FX_AEATTRIBUTE_MAIN_H__
#define	__FX_AEATTRIBUTE_MAIN_H__

#include <AEConfig.h>

#ifdef AE_OS_WIN
	typedef unsigned short PixelType;
	#include <Windows.h>
#endif

#include <entry.h>
#include <AE_Effect.h>

#include "PopcornFX_Define.h"

//----------------------------------------------------------------------------

extern "C" {
	DllExport
	PF_Err
	EffectMain(
		PF_Cmd			cmd,
		PF_InData		*in_data,
		PF_OutData		*out_data,
		PF_ParamDef		*params[],
		PF_LayerDef		*output,
		void			*extra);

}

//----------------------------------------------------------------------------

#endif

