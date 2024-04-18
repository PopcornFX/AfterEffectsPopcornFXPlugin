//----------------------------------------------------------------------------
// This program is the property of Persistant Studios SARL.
//
// You may not redistribute it and/or modify it under any conditions
// without written permission from Persistant Studios SARL, unless
// otherwise stated in the latest Persistant Studios Code License.
//
// See the Persistant Studios Code License for further details.
//----------------------------------------------------------------------------


#include "ApiContextConfig.h"

#if (PK_BUILD_WITH_OPENGL_WGL != 0)

#undef	NOGDI
#undef	NOCTLMGR
#pragma warning(push)
#pragma warning(disable : 4668) // C4668 (level 4)	'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
#include <windows.h>
#pragma warning(pop)
#include <wingdi.h>

#include "precompiled.h"
#include "WGLContext.h"

#include "WindowContext/SdlContext/SdlContext.h"
#if	(PK_BUILD_WITH_SDL != 0)
#	if defined(PK_COMPILER_CLANG_CL)
#		pragma clang diagnostic push
#		pragma clang diagnostic ignored "-Wpragma-pack"
#	endif // defined(PK_COMPILER_CLANG_CL)
#	include <SDL_syswm.h>
#	if defined(PK_COMPILER_CLANG_CL)
#		pragma clang diagnostic pop
#	endif // defined(PK_COMPILER_CLANG_CL)
#endif // (PK_BUILD_WITH_SDL != 0)

#include <GL/glew.h>
#include <GL/wglew.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

#if	(PK_BUILD_WITH_SDL != 0)
PK_DECLARE_REFPTRCLASS(SdlContext);
#endif

namespace
{
	// Function handle error from native wglMakeCurrent
	bool	_MakeCurrent(HDC dc, HGLRC glc)
	{
		bool err = (wglMakeCurrent(dc, glc) == TRUE);
		return PK_VERIFY_MESSAGE(err, "WGL make current context failed");
	}
}

//----------------------------------------------------------------------------

struct	SWGLPlatformContext
{
	HDC						m_DeviceHandle;
	HGLRC					m_GLContext;
	PIXELFORMATDESCRIPTOR	m_PixelFormatDescriptor;

	SWGLPlatformContext() : m_DeviceHandle(0), m_GLContext(0) { Mem::Clear(m_PixelFormatDescriptor); }
};

//----------------------------------------------------------------------------

CWGLContext::CWGLContext()
{
	m_Context = PK_NEW(SWGLPlatformContext);
}

//----------------------------------------------------------------------------

CWGLContext::~CWGLContext()
{
	if (m_Context->m_GLContext != null)
	{
		_MakeCurrent(NULL, NULL);
		wglDeleteContext(m_Context->m_GLContext);
	}
	PK_SAFE_DELETE(m_Context);
}

//----------------------------------------------------------------------------

void	CWGLContext::SwapInterval(s32 interval)
{
	wglSwapIntervalEXT(interval);
}

//----------------------------------------------------------------------------

bool	CWGLContext::SwapBuffers()
{
	return wglSwapLayerBuffers(m_Context->m_DeviceHandle, WGL_SWAP_MAIN_PLANE) == TRUE;
}

//----------------------------------------------------------------------------

#if	(PK_BUILD_WITH_SDL != 0)
bool	CWGLContext::GetContextFromSDLWindow(SDL_Window *window, CGLContext::GLApi targetApi)
{
	if (m_Context == null)
		return false;
	HGLRC	&glContext = m_Context->m_GLContext;
	// Get context from HDC
	{
		SDL_SysWMinfo	info;
		SDL_VERSION(&info.version);
		SDL_GetWindowWMInfo(window, &info);
		m_Context->m_DeviceHandle = info.info.win.hdc;
		glContext = wglCreateContext(m_Context->m_DeviceHandle);
		if (!PK_VERIFY_MESSAGE(glContext != null, "WGL context creation failed"))
			return false;
		if (!_MakeCurrent(m_Context->m_DeviceHandle, glContext))
			return false;
	}

	// Init glew for futur use
	if (!PK_VERIFY_MESSAGE(wglewInit() == GLEW_OK, "WGLew initialization failed"))
		return false;
	return GetContextWithAttribs(targetApi);
}
#endif

//----------------------------------------------------------------------------

namespace
{

	static const int	kAttribListGLES[] =
	{
		WGL_CONTEXT_MAJOR_VERSION_ARB,		2,
		WGL_CONTEXT_MINOR_VERSION_ARB,		0,
		WGL_CONTEXT_PROFILE_MASK_ARB,		WGL_CONTEXT_ES2_PROFILE_BIT_EXT ,
#if defined(PK_DEBUG)
		WGL_CONTEXT_FLAGS_ARB,				WGL_CONTEXT_DEBUG_BIT_ARB,
#endif
		0,
	};

	//----------------------------------------------------------------------------

	static const int	kAttribListGL[] =
	{
		WGL_CONTEXT_MAJOR_VERSION_ARB,		3,
		WGL_CONTEXT_MINOR_VERSION_ARB,		3,
#if defined(PKSAMPLE_CONTEXT_CORE_ONLY)
		WGL_CONTEXT_PROFILE_MASK_ARB,		WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
#else
		WGL_CONTEXT_PROFILE_MASK_ARB,		WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
#endif
#if defined(PK_DEBUG)
		WGL_CONTEXT_FLAGS_ARB,				WGL_CONTEXT_DEBUG_BIT_ARB | WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
#else
		WGL_CONTEXT_FLAGS_ARB,				WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
#endif
		0,
	};

	//----------------------------------------------------------------------------

	static const int	kAttribList[] =
	{
		WGL_DRAW_TO_WINDOW_ARB,				GL_TRUE,
		WGL_SUPPORT_OPENGL_ARB,				GL_TRUE,
		WGL_DOUBLE_BUFFER_ARB,				GL_TRUE,
		WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB,	GL_TRUE,
		WGL_ACCELERATION_ARB,				WGL_FULL_ACCELERATION_ARB,
		WGL_COLOR_BITS_ARB,					32,
		WGL_DEPTH_BITS_ARB,					0,
		WGL_STENCIL_BITS_ARB,				0,
		0,	//End
	};

}

//----------------------------------------------------------------------------

// Create new context with OpenGL ES profile
bool	CWGLContext::GetContextWithAttribs(CGLContext::GLApi targetApi)
{
	const int	*attribList = kAttribListGL;
	if (targetApi == CGLContext::GL_OpenGLES)
	{
		// Check first the support of extensions
		const bool	support =	wglewIsSupported("WGL_ARB_create_context") &&
								wglewIsSupported("WGL_EXT_create_context_es2_profile");
		if (!PK_VERIFY_MESSAGE(support, "WGL extensions for ES profile is not supported."))
			return false;
		attribList = kAttribListGLES;
	}

	// Create with extended creation function with attributes
	{
		HGLRC	newContext = wglCreateContextAttribsARB(m_Context->m_DeviceHandle, 0, attribList);
		if (!PK_VERIFY_MESSAGE(newContext != null, "Context creation with attribs failed"))
			return false;

		// Make current the new one, avoid error with old one at deletion
		if (!_MakeCurrent(m_Context->m_DeviceHandle, newContext))
			return false;

		// Delete old one and replace with the new context
		wglDeleteContext(m_Context->m_GLContext);
		m_Context->m_GLContext = newContext;
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CWGLContext::ChoosePixelFormatSRGB(CGLContext::GLApi targetApi, bool &sRGBCapable)
{
	int		pixelFormats[512];
	UINT	numFormats = 0;

	wglChoosePixelFormatARB(m_Context->m_DeviceHandle, kAttribList, NULL, 512, pixelFormats, &numFormats);

	for (u32 i = 0; i < numFormats; ++i)
	{
		int	attrib = WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB;
		int	result = GL_TRUE;
		if (wglGetPixelFormatAttribivARB(m_Context->m_DeviceHandle, pixelFormats[i], 0, 1, &attrib, &result) == FALSE)
			return false;

		// Added check with OpenGLES, because of a weird behavior: SRGB is not enabled when the WGL api ensure to be enabled.
		sRGBCapable = (result == GL_TRUE) && targetApi != CGLContext::GL_OpenGLES;

		DescribePixelFormat(m_Context->m_DeviceHandle, pixelFormats[i], sizeof(m_Context->m_PixelFormatDescriptor), &m_Context->m_PixelFormatDescriptor);
		if (SetPixelFormat(m_Context->m_DeviceHandle, pixelFormats[i], &m_Context->m_PixelFormatDescriptor) == TRUE)
			return true;
	}
	CLog::Log(PK_ERROR, "Could not find any suitable pixel format for the back buffer");
	return false;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

#endif // (PK_BUILD_WITH_OPENGL_WGL != 0)
