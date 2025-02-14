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

#include <PK-SampleLib/PKSample.h>
#include <pk_rhi/include/Enums.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

class	CGizmo;
class	CProfilerRenderer;

enum	ContextApi
{
	Context_Sdl,
	Context_Offscreen,
	Context_Orbis,
	Context_Durango,
	Context_Glfw,
	Context_Gdk,
	Context_UNKNOWN2,
	Context_NX,
};

//----------------------------------------------------------------------------

class	CAbstractController
{
public:
	virtual ~CAbstractController() {}

	enum	EControllerButton
	{
		ButtonStart,
		// directional pad (Up/Down-Left/Right)
		ButtonDPadUp,
		ButtonDPadDown,
		ButtonDPadLeft,
		ButtonDPadRight,
		// Gameplay pad (Y/A-X/B) (Triangle/Cross-Square/Circle)
		ButtonGPadUp,
		ButtonGPadDown,
		ButtonGPadLeft,
		ButtonGPadRight,
		// Button/Trigger
		ButtonL1,
		ButtonL2,
		ButtonL3,
		ButtonR1,
		ButtonR2,
		ButtonR3,
		__MaxButtons
	};

	virtual bool	IsButtonPressed(EControllerButton button, bool onEventOnly) = 0;
	virtual bool	IsButtonReleased(EControllerButton button, bool onEventOnly) = 0;

	enum	EControllerAxis
	{
		AxisLeftX,
		AxisLeftY,
		AxisRightX,
		AxisRightY,
		__MaxAxis
	};

	virtual float	GetNormalizedAxis(EControllerAxis axis) = 0;

protected:
	static float		FilterDeadZone(float value);
	static const float	kDeadZone;
};

//----------------------------------------------------------------------------


class	CAbstractWindowContext : public CRefCountedObject
{
public:
	CAbstractWindowContext(ContextApi ctxType)
	:	m_Controller(null)
	,	m_Context(ctxType)
	,	m_Profiler(null)
	{}

	~CAbstractWindowContext() {}

	virtual bool				InitImgui(const RHI::PApiManager &manager) = 0;
	virtual bool				Init(RHI::EGraphicalApi api, const CString &title, bool allowHighDPI = false, const CUint2 &windowSize = CUint2(800, 600)) = 0;
	virtual bool				Destroy() = 0;
	virtual bool				ProcessEvents(); // Process the window events

	virtual CBool3				GetMouseClick() const = 0;
	virtual CInt2				GetMousePosition() const = 0;

	virtual void				RegisterGizmo(CGizmo *gizmo) = 0;
	virtual void				RegisterProfiler(CProfilerRenderer *profiler);

	virtual bool				HasWindowChanged() = 0; // returns true if the swap chain needs to be re-created

	virtual CUint2				GetWindowSize() const = 0;
	// Implem those in case of high DPI:
	virtual CUint2				GetDrawableSize() const { return GetWindowSize(); }
	virtual float				GetPixelRatio() const { return 1.0f; }

	virtual bool				WindowHidden() const { return false; }

	ContextApi					GetContextApi() const { return m_Context;  }

	CAbstractController			*m_Controller;

private:
	ContextApi					m_Context;

protected:
	CProfilerRenderer			*m_Profiler;

	typedef	CAbstractWindowContext	Super;
};
PK_DECLARE_REFPTRCLASS(AbstractWindowContext);

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
