#pragma once

#undef	PV_MODULE_NAME
#undef	PV_MODULE_SYM
#define	PV_MODULE_NAME		"AEPlugin"
#define	PV_MODULE_SYM		AEPlugin

#include <PopcornFX_Define.h>

#if defined(PK_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
