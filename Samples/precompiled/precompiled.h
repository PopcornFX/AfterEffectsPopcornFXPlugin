//----------------------------------------------------------------------------
// This program is the property of Persistant Studios SARL.
//
// You may not redistribute it and/or modify it under any conditions
// without written permission from Persistant Studios SARL, unless
// otherwise stated in the latest Persistant Studios Code License.
//
// See the Persistant Studios Code License for further details.
//----------------------------------------------------------------------------

#undef	PK_LIBRARY_NAME
#undef	PV_MODULE_NAME
#undef	PV_MODULE_SYM
#define	PK_LIBRARY_NAME		"PK-Sample"
#define	PV_MODULE_NAME		"PKSample"
#define	PV_MODULE_SYM		PKSample

#include <pkapi/include/pk_precompiled_default.h>

PK_LOG_MODULE_DEFINE();

#if	defined(PK_LINUX) || defined(PK_ORBIS) || defined(PK_UNKNOWN2) || defined(PK_NX) || defined(PK_MACOSX) || defined(PK_IOS)
#	include <stdio.h>
#endif //	defined(PK_LINUX) || defined(PK_ORBIS) || defined(PK_UNKNOWN2) || defined(PK_NX) || defined(PK_MACOSX) || defined(PK_IOS)

#if	defined(PK_ANDROID) || defined(PK_NX) || defined(PK_IOS)
#	include <stdlib.h>
#endif //	defined(PK_ANDROID) || defined(PK_NX) || defined(PK_IOS)

// Define which api we are going to use by default
#if	defined(PK_ORBIS)
#	define		DEFAULT_API				GApi_Orbis
#elif	defined(PK_UNKNOWN2)
#	define		DEFAULT_API				GApi_UNKNOWN2
#elif defined(PK_DURANGO)
#	if	defined(PK_GDK)
#		define DEFAULT_API				GApi_D3D12
#	else
#		define DEFAULT_API				GApi_D3D11
#	endif
#elif defined(PK_SCARLETT)
#	define DEFAULT_API					GApi_D3D12
#elif defined(PK_WINDOWS)
//#	define		DEFAULT_API				GApi_Null
//#	define		DEFAULT_API				GApi_Vulkan
//#	define		DEFAULT_API				GApi_OpenGL
//#	define		DEFAULT_API				GApi_D3D12
#	define		DEFAULT_API				GApi_D3D11
#elif defined(PK_MACOSX)
//#	define		DEFAULT_API				GApi_Vulkan
//#	define		DEFAULT_API				GApi_OpenGL
#	define		DEFAULT_API				GApi_Metal
#elif defined(PK_GGP)
#	define		DEFAULT_API				GApi_Vulkan
#else
//#	define		DEFAULT_API				GApi_Vulkan
#	define		DEFAULT_API				GApi_OpenGL
#endif
