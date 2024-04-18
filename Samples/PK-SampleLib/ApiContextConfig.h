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

//----------------------------------------------------------------------------
// Raw config, must not depend on PopcornFX defines

#if !defined(PK_BUILD_WITH_ORBIS_SUPPORT)
#	define	PK_BUILD_WITH_ORBIS_SUPPORT		0
#endif
#if !defined(PK_BUILD_WITH_VULKAN_SUPPORT)
#	define	PK_BUILD_WITH_VULKAN_SUPPORT	0
#endif
#if !defined(PK_BUILD_WITH_OGL_SUPPORT)
#	define	PK_BUILD_WITH_OGL_SUPPORT		0
#endif
#if !defined(PK_BUILD_WITH_D3D11_SUPPORT)
#	define	PK_BUILD_WITH_D3D11_SUPPORT		0
#endif
#if !defined(PK_BUILD_WITH_D3D12_SUPPORT)
#	define	PK_BUILD_WITH_D3D12_SUPPORT		0
#endif
#if !defined(PK_BUILD_WITH_METAL_SUPPORT)
#	define	PK_BUILD_WITH_METAL_SUPPORT		0
#endif
#if !defined(PK_BUILD_WITH_UNKNOWN2_SUPPORT)
#	define	PK_BUILD_WITH_UNKNOWN2_SUPPORT		0
#endif

//----------------------------------------------------------------------------

#if	(PK_BUILD_WITH_OGL_SUPPORT != 0)
#	ifndef PK_BUILD_WITH_OPENGL_WGL
#		if defined(_WIN32)
#			define PK_BUILD_WITH_OPENGL_WGL		1
#		else
#			define PK_BUILD_WITH_OPENGL_WGL		0
#		endif
#	endif

#	ifndef PK_BUILD_WITH_OPENGL_EGL
#		if defined(GLEW_EGL)
#			define PK_BUILD_WITH_OPENGL_EGL		1
#		else
#			define PK_BUILD_WITH_OPENGL_EGL		0
#		endif
#	endif

#	ifndef PK_BUILD_WITH_OPENGL_GLX
#		if (PK_BUILD_WITH_OPENGL_EGL == 0) && (defined(LINUX) || defined(_LINUX) || defined(__LINUX__) || defined(__linux__))
#			define PK_BUILD_WITH_OPENGL_GLX		1
#		else
#			define PK_BUILD_WITH_OPENGL_GLX		0
#		endif
#	endif

#	ifndef PK_BUILD_WITH_OPENGL_NSGL
#		if (defined(MACOSX) || defined(__APPLE__) || defined(__apple__) || defined(macosx) || defined(MACOS_X))
#			define PK_BUILD_WITH_OPENGL_NSGL	1
#		else
#			define PK_BUILD_WITH_OPENGL_NSGL	0
#		endif
#	endif

#else
#			define PK_BUILD_WITH_OPENGL_WGL		0
#			define PK_BUILD_WITH_OPENGL_EGL		0
#			define PK_BUILD_WITH_OPENGL_GLX		0
#			define PK_BUILD_WITH_OPENGL_NSGL	0
#endif

//----------------------------------------------------------------------------

#if !defined(PK_BUILD_WITH_PSSL_GENERATOR)
#	define	PK_BUILD_WITH_PSSL_GENERATOR		0
#endif

//----------------------------------------------------------------------------

#if	!defined(PK_BUILD_WITH_SDL)
#	error PK_BUILD_WITH_SDL should be defined
#endif

//----------------------------------------------------------------------------
