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

#if (PK_BUILD_WITH_OPENGL_NSGL != 0)

// The new MacOS versions declare OpenGL context as deprecated, this define just remove the compilation warnings:
#define 	GL_SILENCE_DEPRECATION

#include "NSGLContext.h"

#if	(PK_BUILD_WITH_SDL != 0)
#include "WindowContext/SdlContext/SdlContext.h"
#include <SDL.h>
#include <SDL_syswm.h>
#endif

#include <GL/glew.h>

#import <Cocoa/Cocoa.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

#if	(PK_BUILD_WITH_SDL != 0)
PK_DECLARE_REFPTRCLASS(SdlContext);
#endif

//----------------------------------------------------------------------------

struct	SNSGLPlatformContext
{
	NSOpenGLContext		*m_GLContext;
	NSWindow			*m_NSWindow;

	SNSGLPlatformContext() : m_GLContext(null), m_NSWindow(null) {}
};

//----------------------------------------------------------------------------

CNSGLContext::CNSGLContext()
{
	m_Context = PK_NEW(SNSGLPlatformContext);
}

//----------------------------------------------------------------------------

CNSGLContext::~CNSGLContext()
{
	PK_SAFE_DELETE(m_Context);
}

//----------------------------------------------------------------------------

void	CNSGLContext::SwapInterval(s32 interval)
{
	(void)interval;
}

//----------------------------------------------------------------------------

bool	CNSGLContext::SwapBuffers()
{
	glFlush();
	[m_Context->m_GLContext flushBuffer];
	return true;
}

//----------------------------------------------------------------------------

static NSOpenGLPixelFormatAttribute	glAttributes[] =
{
	NSOpenGLPFAAccelerated,
	NSOpenGLPFADoubleBuffer,
	NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion4_1Core,
	NSOpenGLPFAColorSize, 24,
	NSOpenGLPFAAlphaSize, 8,
	0
};

//----------------------------------------------------------------------------

#if	(PK_BUILD_WITH_SDL != 0)
bool	CNSGLContext::GetContextFromSDLWindow(SDL_Window *window, CGLContext::GLApi targetApi)
{
	(void)targetApi;
	if (m_Context == null)
		return false;

	SDL_SysWMinfo	info;
	SDL_VERSION(&info.version);
	SDL_GetWindowWMInfo(window, &info);
	m_Context->m_NSWindow = info.info.cocoa.window;

	NSOpenGLPixelFormat	*pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:glAttributes];
	if (!PK_VERIFY_MESSAGE(pixelFormat != nil, "NSOpenGLPixelFormat init failed"))
		return false;
	m_Context->m_GLContext = [[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:nil];
	if (!PK_VERIFY_MESSAGE(m_Context->m_GLContext != nil, "NSOpenGLContext init failed"))
		return false;
	[m_Context->m_GLContext setView:[m_Context->m_NSWindow contentView]];
	[m_Context->m_GLContext makeCurrentContext];
	return true;
}
#endif

//----------------------------------------------------------------------------

bool	CNSGLContext::ChoosePixelFormatSRGB(CGLContext::GLApi targetApi, bool &sRGBCapable)
{
	(void)targetApi;
	sRGBCapable = true;
	return true;
}

//----------------------------------------------------------------------------

void	CNSGLContext::OnSwapchainUpdate()
{
	[m_Context->m_GLContext update];
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

#endif // (PK_BUILD_WITH_OPENGL_NSGL != 0)
