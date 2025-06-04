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
#include "WindowContext/AWindowContext.h"


#if	defined(PK_NX)
#include <nn/hid.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

class	CNXController : public CAbstractController
{
public:
	bool					Init();

	void					Process();

	bool					IsButtonPressed(EControllerButton button, bool onEventOnly) override;
	bool					IsButtonReleased(EControllerButton button, bool onEventOnly) override;

	float					GetNormalizedAxis(EControllerAxis axis) override;


private:
	int						GetDebugButton(EControllerButton button);

	nn::hid::NpadIdType			m_PadIDs[9] = { nn::hid::NpadId::Handheld, nn::hid::NpadId::No1, nn::hid::NpadId::No2, nn::hid::NpadId::No3, nn::hid::NpadId::No4, nn::hid::NpadId::No5, nn::hid::NpadId::No6, nn::hid::NpadId::No7, nn::hid::NpadId::No8};
	u32							g_CurPadIndex;
	u32							g_LastPadIndex;
	nn::hid::DebugPadState		g_DebugPadState[2];
	nn::hid::NpadHandheldState	g_NpadHandheldState[2];
#ifndef PKUNKNOWN3
	nn::hid::NpadJoyDualState	g_NpadJoyDualState[2];
#endif
	nn::hid::NpadFullKeyState	g_NpadFullKeyState[2];
	float						m_AxisData[__MaxAxis];

	static const u32		s_DebugButtonValues[__MaxButtons];
	static const u32		s_NPadButtonValues[__MaxButtons];
};

//----------------------------------------------------------------------------

class	CNXApplicationContext : public CAbstractWindowContext
{
public:
	CNXApplicationContext();

	virtual bool				InitImgui(const RHI::PApiManager &manager) override final;
	virtual bool				Init(RHI::EGraphicalApi api, const CString &title, bool allowHighDPI = false, const CUint2 &windowSize = CUint2(1920, 1080)) override; // Opens the context for the specified graphical Api
	virtual bool				Destroy() override;
	virtual bool				ProcessEvents() override;

	virtual CBool3				GetMouseClick() const override;
	virtual CInt2				GetMousePosition() const override;

	virtual void				RegisterGizmo(CGizmo *gizmo) override;

	virtual bool				HasWindowChanged() override; // returns true if the swap chain needs to be re-created (window resizing for example)

	virtual CUint2				GetWindowSize() const override;
	virtual CUint2				GetDrawableSize() const override;
	virtual float				GetPixelRatio() const override;


private:
	CUint2					m_WindowSize;
	CNXController			m_NXController;
};
PK_DECLARE_REFPTRCLASS(NXApplicationContext);

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

#endif	// defined(PK_NX)
