//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL.
// https://popcornfx.com/popcornfx-community-license/
//----------------------------------------------------------------------------
#pragma once

#ifndef	__FX_AEGP_MAIN_H__
#define	__FX_AEGP_MAIN_H__

#include <AEConfig.h>

#include <entry.h>
#include <AE_GeneralPlug.h>
#include <AE_GeneralPlugPanels.h>
#include <A.h>
#include <SPSuites.h>
#include <AE_Macros.h>

#ifdef AE_OS_WIN
	typedef unsigned short PixelType;
	#include <Windows.h>
#endif

//----------------------------------------------------------------------------
// This entry point is exported through the PiPL (.r file)
extern "C" DllExport AEGP_PluginInitFuncPrototype EntryPointFunc;

//----------------------------------------------------------------------------

#endif

