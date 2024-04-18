//----------------------------------------------------------------------------
// This program is the property of Persistant Studios SARL.
//
// You may not redistribute it and/or modify it under any conditions
// without written permission from Persistant Studios SARL, unless
// otherwise stated in the latest Persistant Studios Code License.
//
// See the Persistant Studios Code License for further details.
//----------------------------------------------------------------------------

#include "precompiled.h"
#include "ApiContextConfig.h"

#if (PK_BUILD_WITH_OPENGL_EGL != 0)

#include "EGLContext.h"

#include <stdlib.h>
#include <assert.h>

#if	(PK_BUILD_WITH_SDL != 0)
#include "WindowContext/SdlContext/SdlContext.h"
#include <SDL.h>
#include <SDL_syswm.h>
#endif

#include <GL/glew.h>
#include <GL/eglew.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

#if	(PK_BUILD_WITH_SDL != 0)
PK_DECLARE_REFPTRCLASS(SdlContext);
#endif

namespace
{
	// Function handle error from native glxMakeCurrent
	bool	_MakeCurrent(EGLDisplay display, EGLSurface surface, EGLContext context)
	{
		bool err = (eglMakeCurrent(display, surface, surface, context) == EGL_TRUE);
		return PK_VERIFY_MESSAGE(err, "GLX make current context failed");
	}
}

//----------------------------------------------------------------------------

struct	SEGLPlatformContext
{
	EGLDisplay				m_DeviceHandle;
	EGLSurface				m_Surface;
	EGLContext				m_GLContext;

	SEGLPlatformContext() : m_DeviceHandle(0), m_Surface(0), m_GLContext(0) {}
};

//----------------------------------------------------------------------------

CEGLContext::CEGLContext()
:	m_Context(PK_NEW(SEGLPlatformContext))
{
}

//----------------------------------------------------------------------------

CEGLContext::~CEGLContext()
{
	if (m_Context->m_GLContext != null)
	{
		_MakeCurrent(m_Context->m_DeviceHandle, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		(void)PK_VERIFY_MESSAGE(eglDestroyContext(m_Context->m_DeviceHandle, m_Context->m_GLContext) == EGL_TRUE, "EGL destroy context failed");
		(void)PK_VERIFY_MESSAGE(eglTerminate(m_Context->m_DeviceHandle) == EGL_TRUE, "EGL terminate connection failed");
	}
}

//----------------------------------------------------------------------------

void	CEGLContext::SwapInterval(s32 interval)
{
	(void)PK_VERIFY_MESSAGE(eglSwapInterval(m_Context->m_DeviceHandle, interval) == EGL_TRUE, "EGL failed to set swap interval");
}

//----------------------------------------------------------------------------

bool	CEGLContext::SwapBuffers()
{
	return eglSwapBuffers(m_Context->m_DeviceHandle, m_Context->m_Surface) == EGL_TRUE;
}

//----------------------------------------------------------------------------

#if (PK_BUILD_WITH_SDL != 0)

namespace
{
	//----------------------------------------------------------------------------

	static const EGLint	kConfigAttribsListGL[] =
	{
		EGL_CONFORMANT, EGL_OPENGL_BIT,
		EGL_RED_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_BUFFER_SIZE, 32,
		EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,
		EGL_DEPTH_SIZE, 0,
		EGL_STENCIL_SIZE, 0,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_NONE,
	};

	//----------------------------------------------------------------------------

	static const EGLint	kConfigAttribsListGLES[] =
	{
		EGL_CONFORMANT, EGL_OPENGL_BIT | EGL_OPENGL_ES3_BIT,
		EGL_RED_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_BUFFER_SIZE, 32,
		EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,
		EGL_DEPTH_SIZE, 0,
		EGL_STENCIL_SIZE, 0,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_NONE,
	};

	//----------------------------------------------------------------------------

	static const EGLint	kSurfaceAttribsList[] =	// FIXME(Julien): Not const?
	{
		EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
		EGL_GL_COLORSPACE, EGL_GL_COLORSPACE_SRGB,
		EGL_NONE,
	};

	//----------------------------------------------------------------------------

	static const EGLint	kContextAttribsGL[] =
	{
		EGL_CONTEXT_MAJOR_VERSION, 3,
		EGL_CONTEXT_MINOR_VERSION, 3,
#if defined(PKSAMPLE_CONTEXT_CORE_ONLY)
		EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
#else
		EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT,
#endif
#if !defined(PK_LINUX) // not supported on all linux driver, need check on other platforms
		EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE, EGL_TRUE,
#	if defined(PK_DEBUG)
		EGL_CONTEXT_OPENGL_DEBUG, EGL_TRUE,
#	endif
#endif
		EGL_NONE
	};

	//----------------------------------------------------------------------------

	static const EGLint	kContextAttribsGLES[] =
	{
		EGL_CONTEXT_MAJOR_VERSION, 3,
		EGL_CONTEXT_MINOR_VERSION, 2,
//#if defined(PKSAMPLE_CONTEXT_CORE_ONLY)
//		EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
//#else
//		EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT,
//#endif
#if defined(PK_DEBUG) && !defined(PK_LINUX) // not supported on all linux driver, need check on other platforms
		EGL_CONTEXT_OPENGL_DEBUG, EGL_TRUE,
#endif
		EGL_NONE,
	};

}

//----------------------------------------------------------------------------

bool	CEGLContext::GetContextFromSDLWindow(SDL_Window * window, CGLContext::GLApi targetApi)
{
	if (m_Context == null)
		return false;
	// Get Display
	{
		SDL_SysWMinfo	info;
		SDL_VERSION(&info.version);
		SDL_GetWindowWMInfo(window, &info);

		EGLNativeDisplayType	nativeDisplay = EGL_DEFAULT_DISPLAY;
		EGLNativeWindowType		nativeWindow = 0;

#if defined(SDL_VIDEO_DRIVER_X11)
		if (info.subsystem == SDL_SYSWM_X11)
		{
			nativeDisplay = info.info.x11.display;
			nativeWindow = info.info.x11.window;
		}
#endif
		PFNEGLGETDISPLAYPROC ___eglGetDisplay = (PFNEGLGETDISPLAYPROC) eglGetProcAddress("eglGetDisplay");
		m_Context->m_DeviceHandle = ___eglGetDisplay(nativeDisplay);
		if (!PK_VERIFY_MESSAGE(m_Context->m_DeviceHandle != EGL_NO_DISPLAY, "EGL get display failed"))
			return false;

		if (!PK_VERIFY_MESSAGE(eglewInit(m_Context->m_DeviceHandle) == GLEW_OK, "EGLew initialization failed"))
			return false;

		EGLint	v[2];
		if (!PK_VERIFY_MESSAGE(eglInitialize(m_Context->m_DeviceHandle, &v[0], &v[1]) == EGL_TRUE, "EGL initialize failed"))
			return false;

		EGLConfig	config;
		EGLint		numConfig;
		if (!PK_VERIFY_MESSAGE(eglChooseConfig(m_Context->m_DeviceHandle, (targetApi == CGLContext::GL_OpenGLES) ? kConfigAttribsListGLES : kConfigAttribsListGL, &config, 1, &numConfig) == EGL_TRUE, "EGL choose config failed"))
			return false;

		// Create surface
		m_Context->m_Surface = eglCreateWindowSurface(m_Context->m_DeviceHandle, config, nativeWindow, kSurfaceAttribsList);

		// Explicit bind apidl
		if (!PK_VERIFY_MESSAGE(eglBindAPI(targetApi == CGLContext::GL_OpenGLES ? EGL_OPENGL_ES_API : EGL_OPENGL_API) == EGL_TRUE, "EGL bind api failed"))
			return false;

		// Create context
		m_Context->m_GLContext = eglCreateContext(m_Context->m_DeviceHandle, config, EGL_NO_CONTEXT, targetApi == CGLContext::GL_OpenGLES ? kContextAttribsGLES : kContextGLAttribsGL);
		// Make current
		if (!PK_VERIFY_MESSAGE(m_Context->m_Surface != EGL_NO_SURFACE, "EGL surface creation failed") ||
			!PK_VERIFY_MESSAGE(m_Context->m_GLContext != EGL_NO_CONTEXT, "EGL context creation failed") ||
			!_MakeCurrent(m_Context->m_DeviceHandle, m_Context->m_Surface, m_Context->m_GLContext))
			return false;
	}

	return true;
}

#endif

//----------------------------------------------------------------------------

bool	CEGLContext::ChoosePixelFormatSRGB(CGLContext::GLApi targetApi, bool &sRGBCapable)
{
	(void)targetApi; (void)&sRGBCapable;
	return true;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

#endif // (PK_BUILD_WITH_OPENGL_WGL != 0)
