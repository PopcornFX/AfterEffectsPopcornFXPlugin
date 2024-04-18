//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#include <A.h>

//----------------------------------------------------------------------------

#if (defined(MACOSX) || defined(__APPLE__) || defined(__apple__) || defined(macosx) || defined(MACOS_X)) && !defined(PK_MACOSX)
#	define	PK_MACOSX
#elif (defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__win32__)) && !defined(PK_WINDOWS)
#	define	PK_WINDOWS
#endif

//----------------------------------------------------------------------------

typedef unsigned char		u_char;
typedef unsigned short		u_short;
typedef unsigned short		u_int16;
typedef unsigned long		u_long;
typedef short int			int16;
typedef float				fpshort;

//----------------------------------------------------------------------------

#define	__AAEPK_BEGIN		namespace AAePk {
#define	__AAEPK_END			}

__AAEPK_BEGIN

//----------------------------------------------------------------------------

#define PK_SCALE_DOWN			1

#define TEST_COLOR_PROFILE		1

#define PF_TABLE_BITS	12

#define PF_DEEP_COLOR_AWARE 1	// make sure we get 16bpc pixels;

#define A_INTERNAL_TEST_ONE 0

inline bool ae_verify(bool condition)
{
#if	defined(PK_WINDOWS)
	// Only breaks on windows for now
	// should add a raise(SIGTRAP) for macos
	if (condition == false)
		__debugbreak();
#endif
	return condition;
}

#ifndef AE_STRINGIFY
#	define	AE_STRINGIFY(s)		__AE_STRINGIFY(s)
#	define	__AE_STRINGIFY(s)	# s				// don't directly use this one
#endif // !PK_STRINGIFY

#define	AE_MESSAGE(__msg)		__pragma(message(__msg))
#define	AE_TODO(__msg)			AE_MESSAGE(__FILE__ "(" AE_STRINGIFY(__LINE__) ") /!\\/!\\/!\\/!\\ TODO /!\\/!\\/!\\/!\\ " __msg)

#define AE_VERIFY(__condition)	ae_verify(__condition)

// sub (shared) context
#ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#define WGL_CONTEXT_FLAGS_ARB 0x2094
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x0002
#endif

//----------------------------------------------------------------------------

__AAEPK_END

#include <pk_version_base.h>

/* Versioning information */
#define	AEPOPCORNFX_MAJOR_VERSION	PK_VERSION_MAJOR
#define	AEPOPCORNFX_MINOR_VERSION	PK_VERSION_MINOR
#define	AEPOPCORNFX_BUG_VERSION		PK_VERSION_PATCH
#define	AEPOPCORNFX_STAGE_VERSION	PF_Stage_DEVELOP
#include "PopcornFX_Define_Version.h"

typedef	char16_t		aechar_t;

//----------------------------------------------------------------------------
