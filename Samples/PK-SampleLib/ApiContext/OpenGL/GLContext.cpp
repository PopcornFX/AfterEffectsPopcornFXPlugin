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

#if (PK_BUILD_WITH_OGL_SUPPORT != 0)

#include "GLContext.h"
#include "WindowContext/SdlContext/SdlContext.h"

#include "pk_rhi/include/Enums.h"
#include "pk_rhi/include/opengl/OpenGLRenderTarget.h"
#include "pk_rhi/include/opengl/OpenGLTexture.h"
#include "pk_rhi/include/opengl/OpenGLPopcornEnumConversion.h"
#include "pk_rhi/include/opengl/OpenGLShaderModule.h"
#include "pk_rhi/include/opengl/OpenGLShaderProgram.h"

#include <stdlib.h>
#include <assert.h>

#if (PK_BUILD_WITH_OPENGL_WGL != 0)
#	include "ApiContext/OpenGL/WGLContext.h"
#elif (PK_BUILD_WITH_OPENGL_EGL != 0)
#	include "ApiContext/OpenGL/EGLContext.h"
#elif (PK_BUILD_WITH_OPENGL_GLX != 0)
#	include "ApiContext/OpenGL/GLXContext.h"
#elif (PK_BUILD_WITH_OPENGL_NSGL != 0)
#	include "ApiContext/OpenGL/NSGLContext.h"
#endif

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

#if (PK_BUILD_WITH_SDL != 0)
PK_DECLARE_REFPTRCLASS(SdlContext);
#endif

CGLContext::CGLContext(GLApi requestApi)
:	m_TargetApi(requestApi)
,	m_SRGBCapable(true)
, 	m_WindowSize(0)
, 	m_PixelRatio(1.0f)
{
#if (PK_BUILD_WITH_OPENGL_WGL != 0)
	m_Interface = PK_NEW(CWGLContext);
#elif (PK_BUILD_WITH_OPENGL_EGL != 0)
	m_Interface = PK_NEW(CEGLContext);
#elif (PK_BUILD_WITH_OPENGL_GLX != 0)
	m_Interface = PK_NEW(CGLXContext);
#elif (PK_BUILD_WITH_OPENGL_NSGL != 0)
	m_Interface = PK_NEW(CNSGLContext);
#else
	m_Interface = null;
#endif
}

//----------------------------------------------------------------------------

CGLContext::~CGLContext()
{
	glDeleteVertexArrays(1, &m_VAOID);
	PK_DELETE(m_Interface);
}

//----------------------------------------------------------------------------

bool	CGLContext::InitRenderApiContext(bool debug, PAbstractWindowContext windowApi)
{
	(void)debug;
	if (m_Interface == null)
		return false;
	if (windowApi->GetContextApi() == PKSample::Context_Sdl)
	{
#if (PK_BUILD_WITH_SDL != 0)
		PSdlContext		sdlWindowApi =  static_cast<CSdlContext*>(windowApi.Get());
		if (!m_Interface->GetContextFromSDLWindow(sdlWindowApi->SdlGetWindow(), m_TargetApi))
#endif
			return false;
	}
	else if (windowApi->GetContextApi() == PKSample::Context_Offscreen)
	{
		CLog::Log(PK_ERROR, "We do not support offscreen context for OpenGL. Please try with other graphics API.");
		return false;
	}
	else
	{
		return false;
	}
#if (PK_BUILD_WITH_SDL != 0)
	m_ApiData.m_Api = m_TargetApi == GL_OpenGLES ? RHI::GApi_OES : RHI::GApi_OpenGL;
	glewExperimental = GL_TRUE;
	if (!PK_VERIFY_MESSAGE(glewInit() == GLEW_OK, "Glew initialization failed"))
		return false;
	// Get GL Version
	{
		int		glVersion[2] = { -1,-1 };
		glGetIntegerv(GL_MAJOR_VERSION, &glVersion[0]);
		glGetIntegerv(GL_MINOR_VERSION, &glVersion[1]);
		PK_ASSERT_MESSAGE(glVersion[1] < 10, "Error when getting opengl minor version");
		m_ApiData.m_Version = glVersion[0] + glVersion[1] * 0.1f;
	}

	m_WindowSize = windowApi->GetWindowSize();
	m_PixelRatio = windowApi->GetPixelRatio();

	m_ApiData.m_SwapChainsRenderTarget.PushBack(null);
	if (!m_Interface->ChoosePixelFormatSRGB(m_TargetApi, m_SRGBCapable) ||
		!CreateSwapChain(m_WindowSize, m_SRGBCapable, m_RenderTexture, m_ApiData.m_SwapChainsRenderTarget[0], m_FrameBuffer) ||
		!CreateFlipShader(m_Program, m_SamplerLocation))
		return false;
	glGenVertexArrays(1, &m_VAOID);

	// HINTS, check no needed
	m_Interface->SwapInterval(0);
	return true;
#endif // (PK_BUILD_WITH_SDL != 0)
}

//----------------------------------------------------------------------------

bool	CGLContext::WaitAllRenderFinished()
{
	glFinish();
	return true;
}

//----------------------------------------------------------------------------

CGuid	CGLContext::BeginFrame()
{
	return 0;
}

//----------------------------------------------------------------------------

bool	CGLContext::EndFrame(void *renderToWait)
{
	(void)renderToWait;
	CGLContext::EndFrame(m_VAOID, m_ApiData.m_SwapChainsRenderTarget[0], m_RenderTexture, m_Program, m_SamplerLocation, m_WindowSize * m_PixelRatio);

	return m_Interface->SwapBuffers();
}

//----------------------------------------------------------------------------

RHI::SApiContext	*CGLContext::GetRenderApiContext()
{
	return &m_ApiData;
}

//----------------------------------------------------------------------------

bool	CGLContext::RecreateSwapChain(const CUint2 &ctxSize)
{
	PK_ASSERT(m_ApiData.m_SwapChainsRenderTarget.Count() == 1);
	if (!PK_VERIFY(!m_ApiData.m_SwapChainsRenderTarget.Empty()))
		return false;
	PK_ASSERT(m_Interface != null);
	m_WindowSize = ctxSize / m_PixelRatio;
	m_Interface->OnSwapchainUpdate();
	return CreateSwapChain(ctxSize / m_PixelRatio, m_SRGBCapable, m_RenderTexture, m_ApiData.m_SwapChainsRenderTarget[0], m_FrameBuffer);
}

//----------------------------------------------------------------------------

TMemoryView<const RHI::PRenderTarget>	CGLContext::GetCurrentSwapChain()
{
	return TMemoryView<const RHI::PRenderTarget>(TMemoryView<const RHI::POpenGLRenderTarget>(&m_ApiData.m_SwapChainsRenderTarget[0], 1));
}

//----------------------------------------------------------------------------

bool	CGLContext::CreateSwapChain(const CUint2 &size,
									bool scRGBCapable,
									RHI::POpenGLTexture &scRenderTexture,
									RHI::POpenGLRenderTarget &scRenderTarget,
									CGuid &scFrameBuffer)
{
	if (scRGBCapable)
		glEnable(GL_FRAMEBUFFER_SRGB);

	scRenderTexture = PK_NEW(RHI::COpenGLTexture(RHI::SRHIResourceInfos("GLContext Swap Chain")));
	if (scRenderTexture == null)
		return false;

	const RHI::EPixelFormat	format = scRGBCapable ? RHI::FormatSrgb8BGRA : RHI::FormatUnorm8BGRA;
	const CUint3			size3 = size.xy1();

	scRenderTexture->OpenGLInitTexture(format, RHI::OpenGLConversion::PopcornToOpenGLPixelFormat(format), RHI::Texture2D, size3, 1, RHI::SampleCount1);

	scRenderTarget = PK_NEW(RHI::COpenGLRenderTarget(RHI::SRHIResourceInfos("GLContext Render Target")));
	if (scRenderTarget == null)
		return false;

	scRenderTarget->OpenGLSetRenderTexture(scRenderTexture, size3, format, RHI::SampleCount1);

	if (scFrameBuffer.Valid())
		glDeleteFramebuffers(1, &scFrameBuffer.Get());

	GLuint	frameBuffer = 0;
	glGenFramebuffers(1, &frameBuffer);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, frameBuffer);
	scRenderTarget->OpenGLAttachToFrameBuffer(GL_COLOR_ATTACHMENT0, GL_READ_FRAMEBUFFER);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	scFrameBuffer = frameBuffer;
	return true;
}

//----------------------------------------------------------------------------

bool	CGLContext::CreateFlipShader(RHI::POpenGLShaderProgram &blitShaderProgram, s32 &samplerLocation)
{
	const char		*kVertexCode =
#if		defined(PK_ANDROID)
		"#version 310 es\n"
#else
		"#version 330\n"
#endif
		"#extension GL_ARB_separate_shader_objects : enable\n"
		"layout(location = 0) out vec2 texCoords;"
		"vec2 positions[3] = vec2[]("
		"vec2(-1., -1.),"
		"vec2( 3., -1.),"
		"vec2(-1.,  3.)"
		");"
		"vec2 uvs[3] = vec2[]("
		"vec2(0., 1.),"
		"vec2(2., 1.),"
		"vec2(0., -1.)"
		");"
		"void main()"
		"{"
		"gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);"
		"texCoords = uvs[gl_VertexID];"
		"}";
	const char		*kFragCode =
#if		defined(PK_ANDROID)
		"#version 310 es\n"
#else
		"#version 330\n"
#endif
		"#extension GL_ARB_separate_shader_objects : enable\n"
		"precision highp float;\n"
		"layout(location = 0) in vec2 texCoords;"
		"/*layout(binding = 1)*/ uniform sampler2D texSampler;"
		"layout(location = 0) out vec4 outColor;"
		"void main()"
		"{"
		"outColor = texture(texSampler, texCoords);"
		"}";

	RHI::POpenGLShaderModule	vertex = PK_NEW(RHI::COpenGLShaderModule(RHI::SRHIResourceInfos("PK-RHI Flip vertex shader")));
	RHI::POpenGLShaderModule	frag = PK_NEW(RHI::COpenGLShaderModule(RHI::SRHIResourceInfos("PK-RHI Flip fragment shader")));

	blitShaderProgram = PK_NEW(RHI::COpenGLShaderProgram(RHI::SRHIResourceInfos("PK-RHI Flip shader program")));

	if (vertex == null || frag == null || blitShaderProgram == null)
		return false;

	if (!vertex->CompileFromCode(kVertexCode, static_cast<u32>(strlen(kVertexCode)), RHI::VertexShaderStage) ||
		!frag->CompileFromCode(kFragCode, static_cast<u32>(strlen(kFragCode)), RHI::FragmentShaderStage) ||
		!blitShaderProgram->CreateFromShaderModules(vertex, null, frag))
		return false;

	samplerLocation = blitShaderProgram->OpenGLGetUniformLocation("texSampler");

	return true;
}

//----------------------------------------------------------------------------

void	CGLContext::EndFrame(u32 vaoID, const RHI::POpenGLRenderTarget &fbo, const RHI::POpenGLTexture &renderTexture, const RHI::POpenGLShaderProgram &flipProgram, s32 samplerLocation, CUint2 backBufferSize)
{
	(void)fbo;
	glBindVertexArray(vaoID);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	GLenum		drawBuff = GL_BACK;
	glDrawBuffers(1, &drawBuff);

	glEnable(GL_SCISSOR_TEST);
	glScissor(0, 0, backBufferSize.x(), backBufferSize.y());
	glViewport(0, 0, backBufferSize.x(), backBufferSize.y());
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_BLEND);

	flipProgram->OpenGLUseProgram();

	glBindSampler(0, 0);

	glActiveTexture(GL_TEXTURE0 + samplerLocation);
	renderTexture->OpenGLBindTexture();

	glDrawArrays(GL_TRIANGLES, 0, 3);
}

//----------------------------------------------------------------------------

void	CGLContext::CopyBackbufferToTexture(const RHI::POpenGLTexture &texture, const CUint2 &size)
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glReadBuffer(GL_BACK);
	glActiveTexture(GL_TEXTURE0);
	texture->OpenGLBindTexture();
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, size.x(), size.y());
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

#endif // (PK_BUILD_WITH_OGL_SUPPORT != 0)
