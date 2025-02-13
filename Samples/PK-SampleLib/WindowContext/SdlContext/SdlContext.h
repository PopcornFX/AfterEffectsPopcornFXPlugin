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

#include <PK-SampleLib/WindowContext/AWindowContext.h>

#include <PK-SampleLib/ApiContextConfig.h>

#if (PK_BUILD_WITH_SDL != 0)

#if defined(PK_COMPILER_CLANG_CL)
#	pragma clang diagnostic push
#	pragma clang diagnostic ignored "-Wpragma-pack"
#	pragma clang diagnostic ignored "-Wmacro-redefined" // SDL_cpuinfo.h define __SSE__, __SSE2__
#endif // defined(PK_COMPILER_CLANG_CL)

#include <SDL.h>

#if defined(PK_COMPILER_CLANG_CL)
#	pragma clang diagnostic pop
#endif // defined(PK_COMPILER_CLANG_CL)

#undef main

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

class	CProfilerRenderer;

struct	SOpenGLContext
{
	SDL_GLContext	m_Context;
};

//----------------------------------------------------------------------------

class	CSdlController : public CAbstractController
{
public:
	CSdlController();
	virtual ~CSdlController();

	void	Init();
	void	UpdateUserIFN(SDL_JoystickID id);
	void	Destroy();

	void	PreProcess();
	void	Process(const SDL_Event &event);

	bool	IsButtonPressed(EControllerButton button, bool onEventOnly) override;
	bool	IsButtonReleased(EControllerButton button, bool onEventOnly) override;

	float	GetNormalizedAxis(EControllerAxis axis) override;

	SDL_JoystickID	GetJoystickID() const { return m_JoyID; }

private:
	u32					m_JoyData[2];
	float				m_AxisData[__MaxAxis];
	SDL_JoystickID		m_JoyID;
};

//----------------------------------------------------------------------------

class	CSdlContext : public CAbstractWindowContext
{
public:
	CSdlContext();
	~CSdlContext();

	virtual bool				InitImgui(const RHI::PApiManager &manager) override;
	virtual bool				Init(RHI::EGraphicalApi api, const CString &title, bool allowHighDPI = false, const CUint2 &windowSize = CUint2(1920, 1080)) override; // Opens the context for the specified graphical Api
	virtual bool				Destroy() override;
	virtual bool				ProcessEvents() override;

	virtual CBool3				GetMouseClick() const override;
	virtual CInt2				GetMousePosition() const override;

	virtual void				RegisterGizmo(CGizmo *gizmo) override;

	virtual bool				HasWindowChanged() override; // returns true if the swap chain needs to be re-created (window resizing for example)

	virtual CUint2				GetWindowSize() const override { return m_WindowSize; }
	virtual CUint2				GetDrawableSize() const override { return m_WindowSize * m_PixelRatio; }
	virtual float				GetPixelRatio() const override { return m_PixelRatio; }

	virtual bool				WindowHidden() const override;

	SDL_Window					*SdlGetWindow() const;

	void 						UpdateContextSize();

	bool 						HasHighDPI() const { return m_PixelRatio != 1.0f; }

private:
	void						CheckSDLError(int line);

	bool 						m_WindowHidden;
	// Does the context needs a new swap chain?
	bool						m_WindowChanged;
	// Currently used API
	RHI::EGraphicalApi			m_UsedApi;
	// SDL Windows
	SDL_Window					*m_Window;

	CGizmo						*m_Gizmo;

	CSdlController				m_SdlController;

	CUint2 						m_WindowSize;
	float 						m_PixelRatio; // For high-dpi screens
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

#endif	// (PK_BUILD_WITH_SDL != 0)
