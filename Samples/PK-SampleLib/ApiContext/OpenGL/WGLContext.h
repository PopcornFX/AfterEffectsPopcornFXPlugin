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

#include "ApiContextConfig.h"

#if (PK_BUILD_WITH_OPENGL_WGL != 0)

#include "ApiContext/OpenGL/GLContext.h"
#include "WindowContext/AWindowContext.h"

struct	SDL_Window;

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

struct	SWGLPlatformContext;

//----------------------------------------------------------------------------

class	CWGLContext : public IGLContext
{
public:
	CWGLContext();
	~CWGLContext();

#if	(PK_BUILD_WITH_SDL != 0)
	virtual bool				GetContextFromSDLWindow(SDL_Window *window, CGLContext::GLApi targetApi) override;
#endif
	virtual bool				ChoosePixelFormatSRGB(CGLContext::GLApi targetApi, bool &sRGBCapable) override;

	virtual void				SwapInterval(s32 interval) override;
	virtual bool				SwapBuffers() override;

private:
	bool						GetContextWithAttribs(CGLContext::GLApi targetApi);

	SWGLPlatformContext			*m_Context;
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

#endif // (PK_BUILD_WITH_OPENGL_WGL != 0)
