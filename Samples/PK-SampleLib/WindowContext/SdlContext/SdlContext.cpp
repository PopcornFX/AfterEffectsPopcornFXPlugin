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

#include "SdlContext.h"

#if		(PK_BUILD_WITH_SDL != 0)

#include "Gizmo.h"
#include "ProfilerRenderer.h"
#include "ImguiRhiImplem.h"

#if defined(PK_COMPILER_CLANG_CL)
#	pragma clang diagnostic push
#	pragma clang diagnostic ignored "-Wpragma-pack"
#endif // defined(PK_COMPILER_CLANG_CL)
#include <SDL_syswm.h>
#if defined(PK_COMPILER_CLANG_CL)
#	pragma clang diagnostic pop
#endif // defined(PK_COMPILER_CLANG_CL)

#define		DEFAULT_WINDOW_WIDTH	800
#define		DEFAULT_WINDOW_HEIGHT	600

#define		PK_SDL_JOY_ID_INVALID	-1

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------
//
//	CSdlController
//
//----------------------------------------------------------------------------

namespace
{

	const u32	kSdlButtonValue[] =
	{
		SDL_CONTROLLER_BUTTON_START,
		SDL_CONTROLLER_BUTTON_DPAD_UP,
		SDL_CONTROLLER_BUTTON_DPAD_DOWN,
		SDL_CONTROLLER_BUTTON_DPAD_LEFT,
		SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
		SDL_CONTROLLER_BUTTON_Y,
		SDL_CONTROLLER_BUTTON_A,
		SDL_CONTROLLER_BUTTON_X,
		SDL_CONTROLLER_BUTTON_B,
		SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
		SDL_CONTROLLER_BUTTON_MAX,
		SDL_CONTROLLER_BUTTON_LEFTSTICK,
		SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
		SDL_CONTROLLER_BUTTON_MAX,
		SDL_CONTROLLER_BUTTON_RIGHTSTICK
	};
	PK_STATIC_ASSERT(PK_ARRAY_COUNT(kSdlButtonValue) == CSdlController::__MaxButtons);

	//----------------------------------------------------------------------------

	CSdlController::EControllerButton	GetControllerButtonFromSDLCode(Uint8 sdlCode)
	{
		CSdlController::EControllerButton	button = CSdlController::ButtonStart;
		while (button < CSdlController::__MaxButtons)
		{
			if (kSdlButtonValue[button] == sdlCode)
				return button;
			button = static_cast<CAbstractController::EControllerButton>(button + 1);
		}
		return CSdlController::__MaxButtons;
	}

	//----------------------------------------------------------------------------

	const u32	kSdlAxisValue[] =
	{
		SDL_CONTROLLER_AXIS_LEFTX,
		SDL_CONTROLLER_AXIS_LEFTY,
		SDL_CONTROLLER_AXIS_RIGHTX,
		SDL_CONTROLLER_AXIS_RIGHTY
	};
	PK_STATIC_ASSERT(PK_ARRAY_COUNT(kSdlAxisValue) == CSdlController::__MaxAxis);

	//----------------------------------------------------------------------------

	CSdlController::EControllerAxis		GetControllerAxisFromSDLCode(Uint8 sdlCode)
	{
		CSdlController::EControllerAxis	axis = CSdlController::AxisLeftX;
		while (axis < CSdlController::__MaxAxis)
		{
			if (kSdlAxisValue[axis] == sdlCode)
				return axis;
			axis = static_cast<CAbstractController::EControllerAxis>(axis + 1);
		}
		return CSdlController::__MaxAxis;
	}

}

//----------------------------------------------------------------------------

CSdlController::CSdlController()
:	m_JoyID(PK_SDL_JOY_ID_INVALID)
{
	Mem::Clear(m_JoyData);
	Mem::Clear(m_AxisData);
}

//----------------------------------------------------------------------------

CSdlController::~CSdlController()
{
}

//----------------------------------------------------------------------------

void	CSdlController::Init()
{
	if (SDL_NumJoysticks() <= 0)
		return;
	SDL_GameController	*controller = SDL_GameControllerOpen(0);
	if (controller == null)
		return;
	SDL_Joystick		*joystick = SDL_GameControllerGetJoystick(controller);
	if (joystick != null)
		m_JoyID = SDL_JoystickInstanceID(joystick);
}

//----------------------------------------------------------------------------

void	CSdlController::UpdateUserIFN(SDL_JoystickID id)
{
	if (m_JoyID == PK_SDL_JOY_ID_INVALID)
	{
		SDL_GameController	*controller = SDL_GameControllerFromInstanceID(id);
		if (controller == null)
			controller = SDL_GameControllerOpen(0);
		if (controller == null)
			return;
		SDL_Joystick		*joystick = SDL_GameControllerGetJoystick(controller);
		if (joystick != null)
			m_JoyID = SDL_JoystickInstanceID(joystick);
	}
}

//----------------------------------------------------------------------------

void	CSdlController::Destroy()
{
	if (m_JoyID != PK_SDL_JOY_ID_INVALID)
	{
		SDL_GameController	*controller = SDL_GameControllerFromInstanceID(m_JoyID);
		if (controller != null)
			SDL_GameControllerClose(controller);
	}
	m_JoyID = PK_SDL_JOY_ID_INVALID;
	Mem::Clear(m_JoyData[0]);
}

//----------------------------------------------------------------------------

void	CSdlController::PreProcess()
{
	m_JoyData[1] = m_JoyData[0];
	SDL_GameControllerUpdate();
}

//----------------------------------------------------------------------------

void	CSdlController::Process(const SDL_Event &ev)
{
	if (m_JoyID == PK_SDL_JOY_ID_INVALID || ev.cbutton.which != m_JoyID)
		return;

	if (ev.type == SDL_CONTROLLERBUTTONUP || ev.type == SDL_CONTROLLERBUTTONDOWN)
	{
		EControllerButton	button = GetControllerButtonFromSDLCode(ev.cbutton.button);
		if (ev.cbutton.state == SDL_PRESSED)
			m_JoyData[0] |= (1 << button);
		else if (ev.cbutton.state == SDL_RELEASED)
			m_JoyData[0] &= ~(1 << button);
	}

	if (ev.type == SDL_CONTROLLERAXISMOTION)
	{
		if (ev.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ||
			ev.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
		{
			EControllerButton	button = (ev.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ? ButtonL2 : ButtonR2);
			if (FilterDeadZone(ev.caxis.value) > 0)
				m_JoyData[0] |= (1 << button);
			else
				m_JoyData[0] &= ~(1 << button);
		}
		else
		{
			EControllerAxis	axis = GetControllerAxisFromSDLCode(ev.caxis.axis);
			if (axis < __MaxAxis)
				m_AxisData[axis] = float(ev.caxis.value) / float(TNumericTraits<s16>::Max());
		}

	}
}

//----------------------------------------------------------------------------

bool	CSdlController::IsButtonPressed(EControllerButton button, bool onEventOnly)
{
	bool	pressed = (m_JoyData[0] & (1 << button)) != 0;
	if (onEventOnly)
		pressed &= (m_JoyData[1] & (1 << button)) == 0;
	return pressed;
}

//----------------------------------------------------------------------------

bool	CSdlController::IsButtonReleased(EControllerButton button, bool onEventOnly)
{
	bool	pressed = (m_JoyData[0] & (1 << button)) == 0;
	if (onEventOnly)
		pressed &= (m_JoyData[1] & (1 << button)) != 0;
	return pressed;
}

//----------------------------------------------------------------------------

float	CSdlController::GetNormalizedAxis(EControllerAxis axis)
{
	if (axis >= __MaxAxis)
		return 0;
	return FilterDeadZone(m_AxisData[axis]);
}

//----------------------------------------------------------------------------
//
//	CSdlContext
//
//----------------------------------------------------------------------------

CSdlContext::CSdlContext()
:	CAbstractWindowContext(Context_Sdl)
,	m_Gizmo(null)
{
	m_WindowHidden = false;
	m_WindowChanged = false;
	m_Controller = &m_SdlController;
	m_WindowSize = CUint2::ZERO;
	m_PixelRatio = 1.0f;
}

//----------------------------------------------------------------------------

CSdlContext::~CSdlContext()
{
}

//----------------------------------------------------------------------------

bool	CSdlContext::InitImgui(const RHI::PApiManager &manager)
{
	(void)manager;
	ImGuiPkRHI::SImguiInit	init;

	init.m_IsMultiThreaded = false;
	init.m_KeyMap[ImGuiKey_Tab] = SDLK_TAB;
	init.m_KeyMap[ImGuiKey_LeftArrow] = SDL_SCANCODE_LEFT;
	init.m_KeyMap[ImGuiKey_RightArrow] = SDL_SCANCODE_RIGHT;
	init.m_KeyMap[ImGuiKey_UpArrow] = SDL_SCANCODE_UP;
	init.m_KeyMap[ImGuiKey_DownArrow] = SDL_SCANCODE_DOWN;
	init.m_KeyMap[ImGuiKey_PageUp] = SDL_SCANCODE_PAGEUP;
	init.m_KeyMap[ImGuiKey_PageDown] = SDL_SCANCODE_PAGEDOWN;
	init.m_KeyMap[ImGuiKey_Home] = SDL_SCANCODE_HOME;
	init.m_KeyMap[ImGuiKey_End] = SDL_SCANCODE_END;
	init.m_KeyMap[ImGuiKey_Delete] = SDLK_DELETE;
	init.m_KeyMap[ImGuiKey_Backspace] = SDLK_BACKSPACE;
	init.m_KeyMap[ImGuiKey_Enter] = SDLK_RETURN;
	init.m_KeyMap[ImGuiKey_Escape] = SDLK_ESCAPE;
	init.m_KeyMap[ImGuiKey_A] = SDLK_a;
	init.m_KeyMap[ImGuiKey_C] = SDLK_c;
	init.m_KeyMap[ImGuiKey_V] = SDLK_v;
	init.m_KeyMap[ImGuiKey_X] = SDLK_x;
	init.m_KeyMap[ImGuiKey_Y] = SDLK_y;
	init.m_KeyMap[ImGuiKey_Z] = SDLK_z;
	if (!ImGuiPkRHI::Init(init))
		return false;
	ImGuiPkRHI::CreateViewport();
#ifdef	PK_WINDOWS
	SDL_SysWMinfo	wmInfo;

	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo(m_Window, &wmInfo);
	ImGuiPkRHI::ChangeWindowHandle(wmInfo.info.win.window);
#endif
	return true;
}

//----------------------------------------------------------------------------

bool	CSdlContext::Init(RHI::EGraphicalApi api, const CString &title, bool allowHighDPI, const CUint2 &windowSize)
{
#if	defined(PK_PRINT_SDL_VERSION)
	SDL_version compiled;
	SDL_version linked;

	SDL_VERSION(&compiled);
	SDL_GetVersion(&linked);

	CLog::Log(PK_INFO, "We compiled against SDL version %d.%d.%d", compiled.major, compiled.minor, compiled.patch);
	CLog::Log(PK_INFO, "We are linking against SDL version %d.%d.%d", linked.major, linked.minor, linked.patch);
#endif

	u32	sdlWindowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;

	// For some reason, when using OpenGL we have to enable the SDL_WINDOW_ALLOW_HIGHDPI flag...
	if (allowHighDPI || api == RHI::GApi_OpenGL || api == RHI::GApi_OES)
		sdlWindowFlags |= SDL_WINDOW_ALLOW_HIGHDPI;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0)
	{
		CLog::Log(PK_ERROR, "SDL_Init failed: %s", SDL_GetError());
		return false;
	}
	m_UsedApi = api;
	if (m_UsedApi == RHI::GApi_OpenGL || m_UsedApi == RHI::GApi_OES)
		sdlWindowFlags |= SDL_WINDOW_OPENGL;
	m_Window = SDL_CreateWindow(title.Data(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
								windowSize.x(), windowSize.y(),
								sdlWindowFlags);
	if (m_Window == null)
	{
		CLog::Log(PK_ERROR, "SDL_CreateWindow failure");
		return false;
	}
	UpdateContextSize();
	m_SdlController.Init();
	return true;
}

//----------------------------------------------------------------------------

bool	CSdlContext::HasWindowChanged()
{
	return m_WindowChanged;
}

//----------------------------------------------------------------------------

bool	CSdlContext::Destroy()
{
	ImGuiPkRHI::ReleaseViewport();
	PKSample::ImGuiPkRHI::Quit();
	SDL_DestroyWindow(m_Window);
	SDL_Quit();
	return true;
}

//----------------------------------------------------------------------------

bool	CSdlContext::ProcessEvents()
{
	SDL_Event	event;
#if	PKSAMPLE_HAS_PROFILER_RENDERER
	CUint2		windowSize = GetDrawableSize();
#endif

	m_WindowChanged = false;
	m_SdlController.PreProcess();
	while (SDL_PollEvent(&event))
	{
		// Controller event
		if (event.type == SDL_CONTROLLERDEVICEADDED || event.type == SDL_CONTROLLERDEVICEREMAPPED)
			m_SdlController.UpdateUserIFN(event.cdevice.which);
		else if (event.type == SDL_CONTROLLERDEVICEREMOVED && event.cdevice.which == m_SdlController.GetJoystickID())
			m_SdlController.Destroy();
		else if ((event.type & SDL_CONTROLLERAXISMOTION) == SDL_CONTROLLERAXISMOTION) // all controller event
			m_SdlController.Process(event);

		// Imgui event
		if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
		{
			int		key = event.key.keysym.sym & ~SDLK_SCANCODE_MASK;
			int		modifiers = 0;

			modifiers |= ((SDL_GetModState() & KMOD_SHIFT) != 0) ? ImGuiPkRHI::Modifier_Shift : 0;
			modifiers |= ((SDL_GetModState() & KMOD_CTRL) != 0) ? ImGuiPkRHI::Modifier_Ctrl : 0;
			modifiers |= ((SDL_GetModState() & KMOD_ALT) != 0) ? ImGuiPkRHI::Modifier_Alt : 0;
			modifiers |= ((SDL_GetModState() & KMOD_GUI) != 0) ? ImGuiPkRHI::Modifier_Super : 0;
			ImGuiPkRHI::KeyEvents(event.type == SDL_KEYDOWN, key, modifiers);
		}
		else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP)
		{
			int		mouseButtons = 0;

			mouseButtons |= (event.button.button == SDL_BUTTON_LEFT) ? ImGuiPkRHI::MouseButton_Left : 0;
			mouseButtons |= (event.button.button == SDL_BUTTON_MIDDLE) ? ImGuiPkRHI::MouseButton_Middle : 0;
			mouseButtons |= (event.button.button == SDL_BUTTON_RIGHT) ? ImGuiPkRHI::MouseButton_Right : 0;
			ImGuiPkRHI::MouseButtonEvents(event.type == SDL_MOUSEBUTTONDOWN, mouseButtons);
		}
		else if (event.type == SDL_TEXTINPUT)
		{
			ImGuiPkRHI::TextInput(event.text.text);
		}
		else if (event.type == SDL_MOUSEWHEEL)
		{
			ImGuiPkRHI::MouseWheel(event.wheel.y);
		}
		else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
		{
			ImGuiPkRHI::WindowLostFocus();
		}

		// Gizmo key events
		if (m_Gizmo != null)
		{
			if (event.type == SDL_KEYUP)
			{
				if (event.key.keysym.sym == SDLK_q)
					m_Gizmo->Disable();
				else if (event.key.keysym.sym == SDLK_w)
					m_Gizmo->SetMode(GizmoTranslating);
				else if (event.key.keysym.sym == SDLK_e)
					m_Gizmo->SetMode(GizmoRotating);
				else if (event.key.keysym.sym == SDLK_r)
					m_Gizmo->SetMode(GizmoScaling);
			}
			else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP)
			{
				if (event.button.button == SDL_BUTTON_LEFT)
				{
					m_Gizmo->SetMouseClicked(GetMousePosition() * GetPixelRatio(), event.type == SDL_MOUSEBUTTONDOWN);
				}
			}
		}

#if	PKSAMPLE_HAS_PROFILER_RENDERER
		// Profiler key events
		if (m_Profiler != null)
		{
			if (event.type == SDL_KEYUP)
			{
				switch (event.key.keysym.sym)
				{
				case SDLK_i: // up
					if (m_Profiler->Enabled())
						m_Profiler->Zoom(1.0f, windowSize);
					break;
				case SDLK_k: // down
					if (m_Profiler->Enabled())
						m_Profiler->Zoom(-1.0f, windowSize);
					break;
				case SDLK_l: // right
					if (m_Profiler->Enabled())
						m_Profiler->Offset(1.0f);
					break;
				case SDLK_j: // left
					if (m_Profiler->Enabled())
						m_Profiler->Offset(-1.0f);
					break;
				case SDLK_v:
					m_Profiler->Enable(!m_Profiler->Enabled());
					break;
				case SDLK_b:
					m_Profiler->SetPaused(!m_Profiler->IsPaused());
					break;
				case SDLK_u:
					if (m_Profiler->Enabled())
						m_Profiler->SetCurrentHistoryFrame(m_Profiler->CurrentHistoryFrame() + 1);
					break;
				case SDLK_o:
					if (m_Profiler->Enabled())
						m_Profiler->SetCurrentHistoryFrame(m_Profiler->CurrentHistoryFrame() - 1);
					break;
				case SDLK_y:
					if (m_Profiler->Enabled())
						m_Profiler->VerticalOffset(-1.0f);
					break;
				case SDLK_h:
					if (m_Profiler->Enabled())
						m_Profiler->VerticalOffset(1.0f);
					break;
				}
			}
		}
#endif	// PKSAMPLE_HAS_PROFILER_RENDERER

		// Window events
		if (event.type == SDL_QUIT)
		{
			return false;
		}
		if (event.type == SDL_WINDOWEVENT)
		{
			if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
				m_WindowChanged = true;
			else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
				m_WindowHidden = true;
			else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
				m_WindowHidden = false;
		}
	}

	if (m_WindowChanged)
	{
		UpdateContextSize();
	}

	ImGuiPkRHI::MouseMoved(GetMousePosition()); // IMGUI handles the pixel ratio internally
	ImGuiPkRHI::UpdateInputs();
	if (m_Gizmo != null)
	{
		m_Gizmo->SetMouseMoved(GetMousePosition() * GetPixelRatio());
	}

	return Super::ProcessEvents();
}

//----------------------------------------------------------------------------

CBool3	CSdlContext::GetMouseClick() const
{
	const u32	mstate = SDL_GetMouseState(null, null);
	return CBool3(	(mstate & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0,
					(mstate & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0,
					(mstate & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0);
}

//----------------------------------------------------------------------------

CInt2	CSdlContext::GetMousePosition() const
{
	CInt2	mousePos = CInt2::ZERO;
	SDL_GetMouseState(&mousePos.x(), &mousePos.y());
	return mousePos;
}

//----------------------------------------------------------------------------

void	CSdlContext::RegisterGizmo(CGizmo *gizmo)
{
	m_Gizmo = gizmo;
}

//----------------------------------------------------------------------------

bool	CSdlContext::WindowHidden() const
{
	return m_WindowHidden;
}

//----------------------------------------------------------------------------

SDL_Window	*CSdlContext::SdlGetWindow() const
{
	return m_Window;
}

//----------------------------------------------------------------------------

void	CSdlContext::UpdateContextSize()
{
	CInt2	size;
	SDL_GetWindowSize(m_Window, &size.x(), &size.y());
	PK_ASSERT(size.x() >= 0 && size.y() >= 0);
	m_WindowSize = CUint2(size);
	SDL_GL_GetDrawableSize(m_Window, &size.x(), &size.y());
	m_PixelRatio = static_cast<float>(size.x()) / static_cast<float>(m_WindowSize.x());
}

//----------------------------------------------------------------------------

void	CSdlContext::CheckSDLError(int line)
{
	(void)line;
#if defined(PK_DEBUG)
	const char	*error = SDL_GetError();
	if (*error != '\0')
	{
		printf("SDL Error: %s\n", error);
		if (line != -1)
			printf(" + line: %i\n", line);
		SDL_ClearError();
	}
#endif
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

#endif
