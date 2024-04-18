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

#include "PK-SampleLib/ApiContextConfig.h"

#if (PK_BUILD_WITH_OGL_SUPPORT != 0)

#include <pk_rhi/include/opengl/OpenGLBasicContext.h>
#include "PK-SampleLib/ApiContext/IApiContext.h"
#include "PK-SampleLib/WindowContext/AWindowContext.h"

//#define PKSAMPLE_CONTEXT_CORE_ONLY

PK_FORWARD_DECLARE(OpenGLShaderProgram);

struct	SDL_Window;

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

class	IGLContext;

//----------------------------------------------------------------------------

class	CGLContext : public IApiContext
{
public:
	enum GLApi
	{
		GL_OpenGL,
		GL_OpenGLES,
	};

public:
	CGLContext(GLApi api);
	~CGLContext();

	virtual bool									InitRenderApiContext(bool debug, PAbstractWindowContext windowApi) override;
	virtual bool									WaitAllRenderFinished() override;
	virtual CGuid									BeginFrame() override;
	virtual bool									EndFrame(void *renderToWait) override;
	virtual RHI::SApiContext						*GetRenderApiContext() override;
	virtual bool									RecreateSwapChain(const CUint2 &ctxSize) override;
	virtual TMemoryView<const RHI::PRenderTarget>	GetCurrentSwapChain() override;

	static bool						CreateSwapChain(const CUint2 &size,
													bool scRGBCapable,
													RHI::POpenGLTexture &scRenderTexture,
													RHI::POpenGLRenderTarget &scRenderTarget,
													CGuid &scFrameBuffer);
	static bool						CreateFlipShader(RHI::POpenGLShaderProgram &blitShaderProgram, s32 &samplerLocation);
	static void						EndFrame(u32 vaoID, const RHI::POpenGLRenderTarget &fbo, const RHI::POpenGLTexture &renderTexture, const RHI::POpenGLShaderProgram &flipProgram, s32 samplerLocation, CUint2 backBufferSize);
	static void						CopyBackbufferToTexture(const RHI::POpenGLTexture &renderTexture, const CUint2 &size);

protected:
	// Default
	GLuint						m_VAOID;

	GLApi						m_TargetApi;
	bool						m_SRGBCapable;
	IGLContext					*m_Interface;
	CGuid						m_FrameBuffer;
	RHI::POpenGLTexture			m_RenderTexture;
	RHI::POpenGLShaderProgram	m_Program;
	s32							m_SamplerLocation;

	//This struct is the actual data that is shared between the Api context and the Api manager
	RHI::SOpenGLBasicContext	m_ApiData;

	CUint2						m_WindowSize;
	float 						m_PixelRatio;
};

//----------------------------------------------------------------------------

class	IGLContext
{
public:
	virtual~IGLContext() {}

#if	(PK_BUILD_WITH_SDL != 0)
	virtual bool				GetContextFromSDLWindow(SDL_Window *window, CGLContext::GLApi targetApi) = 0;
#endif
	virtual bool				ChoosePixelFormatSRGB(CGLContext::GLApi targetApi, bool &sRGBCapable) = 0;

	virtual void				SwapInterval(s32 interval) = 0;
	virtual bool				SwapBuffers() = 0;

	virtual void				OnSwapchainUpdate() {}
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

#endif // (PK_BUILD_WITH_OGL_SUPPORT != 0)
