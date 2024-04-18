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

#if (PK_BUILD_WITH_OPENGL_GLX != 0)

#include "GLXContext.h"

#include <stdlib.h>
#include <assert.h>

#if	(PK_BUILD_WITH_SDL != 0)
#include "WindowContext/SdlContext/SdlContext.h"
#include <SDL.h>
#include <SDL_syswm.h>
#endif

#include <GL/glew.h>
#include <GL/glxew.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

#if	(PK_BUILD_WITH_SDL != 0)
PK_DECLARE_REFPTRCLASS(SdlContext);
#endif

namespace
{
	// Function handle error from native glxMakeCurrent
	bool	_MakeCurrent(Display *dc, GLXDrawable drawable, GLXContext glc)
	{
		bool err = (glXMakeCurrent(dc, drawable, glc) == True);
		return PK_VERIFY_MESSAGE(err, "GLX make current context failed");
	}
}

//----------------------------------------------------------------------------

struct	SGLXPlatformContext
{
	Display					*m_DeviceHandle;
	GLXDrawable				m_Drawable;
	GLXContext				m_GLContext;

	SGLXPlatformContext() : m_DeviceHandle(0), m_Drawable(0), m_GLContext(0) {}
};

//----------------------------------------------------------------------------

CGLXContext::CGLXContext()
{
	m_Context = PK_NEW(SGLXPlatformContext);
}

//----------------------------------------------------------------------------

CGLXContext::~CGLXContext()
{
	if (m_Context->m_GLContext != null)
	{
		glXDestroyContext(m_Context->m_DeviceHandle, m_Context->m_GLContext);
		_MakeCurrent(m_Context->m_DeviceHandle, 0, 0);
	}
	PK_SAFE_DELETE(m_Context);
}

//----------------------------------------------------------------------------

void	CGLXContext::SwapInterval(s32 interval)
{
	(void)interval;
	glXSwapIntervalEXT(m_Context->m_DeviceHandle, m_Context->m_Drawable, 0);
}

//----------------------------------------------------------------------------

bool	CGLXContext::SwapBuffers()
{
	glXSwapBuffers(m_Context->m_DeviceHandle, m_Context->m_Drawable);
	return true;
}

//----------------------------------------------------------------------------

#if	(PK_BUILD_WITH_SDL != 0)
bool	CGLXContext::GetContextFromSDLWindow(SDL_Window *window, CGLContext::GLApi targetApi)
{
	if (m_Context == null)
		return false;
	// Get context from HDC
	{
		SDL_SysWMinfo	info;
		SDL_VERSION(&info.version);
		SDL_GetWindowWMInfo(window, &info);
		m_Context->m_DeviceHandle = info.info.x11.display;
		m_Context->m_Drawable = info.info.x11.window;

		XVisualInfo visualInfo;
		if (!PK_VERIFY_MESSAGE( XMatchVisualInfo(m_Context->m_DeviceHandle, DefaultScreen(m_Context->m_DeviceHandle), 24, TrueColor, &visualInfo) != 0 , "Match XVisualInfo failed"))
			return false;
		// Create default context
		m_Context->m_GLContext = glXCreateContext(m_Context->m_DeviceHandle, &visualInfo, NULL, True);
		// Make current
		if (!PK_VERIFY_MESSAGE(m_Context->m_GLContext != 0, "GLX context creation failed") ||
			!_MakeCurrent(m_Context->m_DeviceHandle, m_Context->m_Drawable, m_Context->m_GLContext))
			return false;
	}

	if (!PK_VERIFY_MESSAGE(glxewInit() == GLEW_OK, "GLXew initialization failed"))
		return false;
	return GetContextWithAttribs(targetApi);
}
#endif

//----------------------------------------------------------------------------

namespace
{
	static const int	kAttribListGL[] =
	{
		GLX_CONTEXT_MAJOR_VERSION_ARB,		3,
		GLX_CONTEXT_MINOR_VERSION_ARB,		3,
#if defined(PKSAMPLE_CONTEXT_CORE_ONLY)
		GLX_CONTEXT_PROFILE_MASK_ARB,		GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
#else
		GLX_CONTEXT_PROFILE_MASK_ARB,		GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
#endif
#if defined(PK_DEBUG)
		GLX_CONTEXT_FLAGS_ARB,				GLX_CONTEXT_DEBUG_BIT_ARB | GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
#else
		GLX_CONTEXT_FLAGS_ARB,				GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
#endif
		0,
	};

	//----------------------------------------------------------------------------

	static const int	kAttribListGLES[] =
	{
		GLX_CONTEXT_MAJOR_VERSION_ARB,		2,
		GLX_CONTEXT_MINOR_VERSION_ARB,		0,
		GLX_CONTEXT_PROFILE_MASK_ARB,		GLX_CONTEXT_ES2_PROFILE_BIT_EXT,
#if defined(PK_DEBUG)
		GLX_CONTEXT_FLAGS_ARB,				GLX_CONTEXT_DEBUG_BIT_ARB,
#endif
		0,
	};

	//----------------------------------------------------------------------------

	static const int	kVisualAttribs[] =
	{
		GLX_X_RENDERABLE, True,
		GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
		GLX_RENDER_TYPE, GLX_RGBA_BIT,
		GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
		GLX_RED_SIZE, 8,
		GLX_GREEN_SIZE, 8,
		GLX_BLUE_SIZE, 8,
		GLX_ALPHA_SIZE, 8,
		GLX_DEPTH_SIZE, 24,
		GLX_STENCIL_SIZE, 8,
		GLX_DOUBLEBUFFER, True,
		//GLX_SAMPLE_BUFFERS, 1,
		//GLX_SAMPLES, 4,
		None
	};
}

//----------------------------------------------------------------------------

// Create new context with OpenGL ES profile
bool	CGLXContext::GetContextWithAttribs(CGLContext::GLApi targetApi)
{
	const int	*attribList = kAttribListGL;
	if (targetApi == CGLContext::GL_OpenGLES)
	{
		// Check first the support of extensions
		const bool	support =	glxewIsSupported("GLX_ARB_create_context") &&
								glxewIsSupported("GLX_EXT_create_context_es2_profile");
		if (!PK_VERIFY_MESSAGE(support, "GLX extensions for ES profile is not supported."))
			return false;
		attribList = kAttribListGLES;
	}

	// Create with extended creation function with attributes
	{
		// Get a matching FB config
		int				fbcount;
		GLXFBConfig		*fbc = glXChooseFBConfig(m_Context->m_DeviceHandle, DefaultScreen(m_Context->m_DeviceHandle), kVisualAttribs, &fbcount);
		if (!PK_VERIFY(fbc != null))
			return false;

		GLXContext		newContext = glXCreateContextAttribsARB(m_Context->m_DeviceHandle, fbc[0], 0, True, attribList);

		XFree(fbc);
		fbc = null;

		if (!PK_VERIFY_MESSAGE(newContext != 0, "Context creation with attribs failed"))
			return false;

		// Make current the new one, avoid error with old one at deletion
		if (!_MakeCurrent(m_Context->m_DeviceHandle, m_Context->m_Drawable, m_Context->m_GLContext))
			return false;

		glXDestroyContext(m_Context->m_DeviceHandle, m_Context->m_GLContext);
		m_Context->m_GLContext = newContext;
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CGLXContext::ChoosePixelFormatSRGB(CGLContext::GLApi targetApi, bool &sRGBCapable)
{
	(void)targetApi;
	sRGBCapable = true;
	return true;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

#endif // (PK_BUILD_WITH_OPENGL_WGL != 0)
