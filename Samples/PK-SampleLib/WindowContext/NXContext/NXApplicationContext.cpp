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

#include "NXApplicationContext.h"

#if	defined(PK_NX)

#include "ProfilerRenderer.h"

//#ifdef	USE_IMGUI
#include "ImguiRhiImplem.h"
//#endif
#include <nn/settings/settings_DebugPad.h>

#define	PK_NX_PAD_HANDLE_INVALID	-1
#define	PAD_COUNT					9

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

const u32	CNXController::s_DebugButtonValues[__MaxButtons] =
{
	nn::hid::DebugPadButton::Start::Index,
	nn::hid::DebugPadButton::Up::Index,
	nn::hid::DebugPadButton::Down::Index,
	nn::hid::DebugPadButton::Left::Index,
	nn::hid::DebugPadButton::Right::Index,
	nn::hid::DebugPadButton::X::Index,
	nn::hid::DebugPadButton::B::Index,
	nn::hid::DebugPadButton::Y::Index,
	nn::hid::DebugPadButton::A::Index,
	nn::hid::DebugPadButton::L::Index,
	nn::hid::DebugPadButton::ZL::Index,
	0,
	nn::hid::DebugPadButton::R::Index,
	nn::hid::DebugPadButton::ZR::Index,
	0,
};

const u32	CNXController::s_NPadButtonValues[__MaxButtons] =
{
	nn::hid::NpadButton::Plus::Index,
	nn::hid::NpadButton::Up::Index,
	nn::hid::NpadButton::Down::Index,
	nn::hid::NpadButton::Left::Index,
	nn::hid::NpadButton::Right::Index,
	nn::hid::NpadButton::X::Index,
	nn::hid::NpadButton::B::Index,
	nn::hid::NpadButton::Y::Index,
	nn::hid::NpadButton::A::Index,
	nn::hid::NpadButton::L::Index,
	nn::hid::NpadButton::ZL::Index,
	nn::hid::NpadButton::StickL::Index,
	nn::hid::NpadButton::R::Index,
	nn::hid::NpadButton::ZR::Index,
	nn::hid::NpadButton::StickR::Index,
};

//----------------------------------------------------------------------------

bool	CNXController::Init()
{
	nn::settings::DebugPadKeyboardMap	map;
	nn::settings::GetDebugPadKeyboardMap(&map);
	map.buttonA = nn::hid::KeyboardKey::A::Index;
	map.buttonB = nn::hid::KeyboardKey::B::Index;
	map.buttonX = nn::hid::KeyboardKey::X::Index;
	map.buttonY = nn::hid::KeyboardKey::Y::Index;
	map.buttonL = nn::hid::KeyboardKey::L::Index;
	map.buttonR = nn::hid::KeyboardKey::R::Index;
	map.buttonLeft = nn::hid::KeyboardKey::LeftArrow::Index;
	map.buttonRight = nn::hid::KeyboardKey::RightArrow::Index;
	map.buttonUp = nn::hid::KeyboardKey::UpArrow::Index;
	map.buttonDown = nn::hid::KeyboardKey::DownArrow::Index;
	map.buttonStart = nn::hid::KeyboardKey::Space::Index;
	nn::settings::SetDebugPadKeyboardMap(map);
	nn::hid::InitializeDebugPad();
	nn::hid::InitializeNpad();
	nn::hid::SetSupportedNpadStyleSet(nn::hid::NpadStyleFullKey::Mask | nn::hid::NpadStyleJoyDual::Mask | nn::hid::NpadStyleHandheld::Mask);
	nn::hid::SetSupportedNpadIdType(m_PadIDs, PAD_COUNT);
	return true;
}

//----------------------------------------------------------------------------

void	CNXController::Process()
{
	g_CurPadIndex = (g_CurPadIndex + 1) % 2;
	g_LastPadIndex = 1 - g_CurPadIndex;
	nn::hid::GetDebugPadState(&g_DebugPadState[g_CurPadIndex]);

	for (int i = 0; i < PAD_COUNT; ++i)
	{
		nn::hid::NpadStyleSet style = nn::hid::GetNpadStyleSet(m_PadIDs[i]);

		if (style.Test<nn::hid::NpadStyleHandheld>())
			nn::hid::GetNpadState(&g_NpadHandheldState[g_CurPadIndex], m_PadIDs[i]);
		else if (style.Test<nn::hid::NpadStyleJoyDual>())
			nn::hid::GetNpadState(&g_NpadJoyDualState[g_CurPadIndex], m_PadIDs[i]);
		else if (style.Test<nn::hid::NpadStyleFullKey>())
			nn::hid::GetNpadState(&g_NpadFullKeyState[g_CurPadIndex], m_PadIDs[i]);
	}

	m_AxisData[0] = 0;
	m_AxisData[1] = 0;
	m_AxisData[2] = 0;
	m_AxisData[3] = 0;

	nn::hid::DebugPadState debugPadState = g_DebugPadState[g_CurPadIndex];
	if (debugPadState.analogStickL.x != 0 || debugPadState.analogStickL.y != 0 ||
		debugPadState.analogStickR.x != 0 || debugPadState.analogStickR.y != 0)
	{
		
		m_AxisData[0] = float(debugPadState.analogStickL.x) / float(TNumericTraits<s16>::Max());
		m_AxisData[1] = float(debugPadState.analogStickL.y) / float(TNumericTraits<s16>::Max());
		m_AxisData[2] = float(debugPadState.analogStickR.x) / float(TNumericTraits<s16>::Max());
		m_AxisData[3] = float(debugPadState.analogStickR.y) / float(TNumericTraits<s16>::Max());
		return;
	}

	for (int i = 0; i < PAD_COUNT; ++i)
	{
		nn::hid::NpadStyleSet		style = nn::hid::GetNpadStyleSet(m_PadIDs[i]);
		nn::hid::AnalogStickState	stickL;
		nn::hid::AnalogStickState	stickR;

		if (style.Test<nn::hid::NpadStyleHandheld>())
		{
			stickL = g_NpadHandheldState[g_CurPadIndex].analogStickL;
			stickR = g_NpadHandheldState[g_CurPadIndex].analogStickR;
		}
		else if (style.Test<nn::hid::NpadStyleJoyDual>())
		{
			stickL = g_NpadJoyDualState[g_CurPadIndex].analogStickL;
			stickR = g_NpadJoyDualState[g_CurPadIndex].analogStickR;
		}
		else if (style.Test<nn::hid::NpadStyleFullKey>())
		{
			stickL = g_NpadFullKeyState[g_CurPadIndex].analogStickL;
			stickR = g_NpadFullKeyState[g_CurPadIndex].analogStickR;
		}

		if (stickL.x != 0 || stickL.y != 0 ||
			stickR.x != 0 || stickR.y != 0)
		{

			m_AxisData[0] = float(stickL.x) / float(TNumericTraits<s16>::Max());
			m_AxisData[1] = float(stickL.y) / float(TNumericTraits<s16>::Max());
			m_AxisData[2] = float(stickR.x) / float(TNumericTraits<s16>::Max());
			m_AxisData[3] = float(stickR.y) / float(TNumericTraits<s16>::Max());

			return;
		}
	}
}

//----------------------------------------------------------------------------

bool	CNXController::IsButtonPressed(EControllerButton button, bool onEventOnly)
{
	int	cur = g_CurPadIndex;
	int	prev = g_LastPadIndex;

	// debug pad
	int	buttonIndex = s_DebugButtonValues[button];

	if (!g_DebugPadState[prev].buttons[buttonIndex] && g_DebugPadState[cur].buttons[buttonIndex])
		return true;

	// npad
  	buttonIndex = s_NPadButtonValues[button];

	for (int i = 0; i < PAD_COUNT; ++i)
	{
		nn::hid::NpadStyleSet style = nn::hid::GetNpadStyleSet(m_PadIDs[i]);

		if (style.Test<nn::hid::NpadStyleHandheld>() && !g_NpadHandheldState[prev].buttons[buttonIndex] && g_NpadHandheldState[cur].buttons[buttonIndex])
			return true;
		else if (style.Test<nn::hid::NpadStyleJoyDual>() && !g_NpadJoyDualState[prev].buttons[buttonIndex] && g_NpadJoyDualState[cur].buttons[buttonIndex])
			return true;
		else if (style.Test<nn::hid::NpadStyleFullKey>() && !g_NpadFullKeyState[prev].buttons[buttonIndex] && g_NpadFullKeyState[cur].buttons[buttonIndex])
			return true;
	}

	return false;
}

//----------------------------------------------------------------------------

bool	CNXController::IsButtonReleased(EControllerButton button, bool onEventOnly)
{
	int	cur	= g_CurPadIndex;
	int	prev = g_LastPadIndex;

	// debug pad
	int	buttonIndex = s_DebugButtonValues[button];

	if (g_DebugPadState[prev].buttons[buttonIndex] && !g_DebugPadState[cur].buttons[buttonIndex])
		return true;

	// npad
	buttonIndex = s_NPadButtonValues[button];

	for (int i = 0; i < PAD_COUNT; ++i)
	{
		nn::hid::NpadStyleSet style = nn::hid::GetNpadStyleSet(m_PadIDs[i]);

		if (style.Test<nn::hid::NpadStyleHandheld>() && g_NpadHandheldState[prev].buttons[buttonIndex] && !g_NpadHandheldState[cur].buttons[buttonIndex])
			return true;
		else if (style.Test<nn::hid::NpadStyleJoyDual>() && g_NpadJoyDualState[prev].buttons[buttonIndex] && !g_NpadJoyDualState[cur].buttons[buttonIndex])
			return true;
		else if (style.Test<nn::hid::NpadStyleFullKey>() && g_NpadFullKeyState[prev].buttons[buttonIndex] && !g_NpadFullKeyState[cur].buttons[buttonIndex])
			return true;
	}
	return false;
}

//----------------------------------------------------------------------------

float	CNXController::GetNormalizedAxis(EControllerAxis axis)
{
	if (axis >= __MaxAxis)
		return 0;
	return FilterDeadZone(m_AxisData[axis]);
}

//----------------------------------------------------------------------------
//
// CNXApplicationContext
//
//----------------------------------------------------------------------------

CNXApplicationContext::CNXApplicationContext()
:	CAbstractWindowContext(Context_NX)
{
	CAbstractWindowContext::m_Controller = &m_NXController;
}

//----------------------------------------------------------------------------

bool	CNXApplicationContext::Init(RHI::EGraphicalApi api, const CString &title, bool)
{
	(void)api; (void)title;

	return m_NXController.Init();
}

//----------------------------------------------------------------------------

bool	CNXApplicationContext::InitImgui(const RHI::PApiManager &manager)
{
	(void)manager;
	ImGuiPkRHI::SImguiInit	init;

	init.m_IsMultiThreaded = false;
	Mem::Clear(init.m_KeyMap);
	if (!ImGuiPkRHI::Init(init))
		return false;
	ImGuiPkRHI::CreateViewport();
	return true;
}

//----------------------------------------------------------------------------

bool	CNXApplicationContext::Destroy()
{
	ImGuiPkRHI::ReleaseViewport();
	PKSample::ImGuiPkRHI::QuitIFN();
	return true;
}

//----------------------------------------------------------------------------

bool	CNXApplicationContext::ProcessEvents()
{
	m_NXController.Process();

	return Super::ProcessEvents();
}

//----------------------------------------------------------------------------

CBool3	CNXApplicationContext::GetMouseClick() const
{
	return CBool3(false, false, false);
}

//----------------------------------------------------------------------------

CInt2	CNXApplicationContext::GetMousePosition() const
{
	return CInt2(0);
}

//----------------------------------------------------------------------------

void	CNXApplicationContext::RegisterGizmo(CGizmo *gizmo)
{
	(void)gizmo;
}

//----------------------------------------------------------------------------

bool	CNXApplicationContext::HasWindowChanged()
{
	return false;
}

//----------------------------------------------------------------------------

CUint2	CNXApplicationContext::GetWindowSize() const
{
	return CUint2(1920, 1080); // TODO: properly handle changing sizes
}

//----------------------------------------------------------------------------

CUint2	CNXApplicationContext::GetDrawableSize() const
{
	return CUint2(3840, 2160); // TODO: properly handle changing sizes
}

//----------------------------------------------------------------------------

float	CNXApplicationContext::GetPixelRatio() const
{
	return 2.0f;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

#endif
