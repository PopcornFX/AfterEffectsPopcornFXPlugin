#pragma once

#ifndef __AE_PRECOMPILED_H__
#define __AE_PRECOMPILED_H__

#undef	PV_MODULE_NAME
#undef	PV_MODULE_SYM
#define	PV_MODULE_NAME		"AEPlugin"
#define	PV_MODULE_SYM		AEPlugin

#include <pkapi/include/pk_precompiled_default.h>

PK_LOG_MODULE_DEFINE();

#include <PopcornFX_Define.h>



#ifdef PK_NULL_AS_VARIABLE
using PopcornFX::null;
#endif

#if defined(PK_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#endif //__AE_PRECOMPILED_H__
