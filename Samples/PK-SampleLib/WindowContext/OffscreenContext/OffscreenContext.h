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

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

// This class is mainly for PopcornFX internal purposes. It performs as a
// minimum windows context that is only necessary for offscreen rendering.
class	COffscreenContext : public CAbstractWindowContext
{
public:
	COffscreenContext();
	~COffscreenContext() {};

	virtual bool				InitImgui(const RHI::PApiManager &manager) override;
	virtual bool				Init(RHI::EGraphicalApi api, const CString &title, bool allowHighDPI = false, const CUint2 &windowSize = CUint2(1920, 1080)) override; // Opens the context for the specified graphical Api
	virtual bool				Destroy() override { return true; };
	virtual bool				ProcessEvents() override;

	virtual CBool3				GetMouseClick() const override { return CBool3(false); };
	virtual CInt2				GetMousePosition() const override { return CInt2::ZERO; };

	virtual void				RegisterGizmo(CGizmo *gizmo) override { (void)gizmo; };

	virtual bool				HasWindowChanged() override { return false; };

	virtual CUint2				GetWindowSize() const override { return m_WindowSize; }
	virtual CUint2				GetDrawableSize() const override { return m_WindowSize * m_PixelRatio; }
	virtual float				GetPixelRatio() const override { return m_PixelRatio; }

	virtual bool				WindowHidden() const override { return false; };

private:
	// Currently used API
	RHI::EGraphicalApi			m_UsedApi;

	CUint2 						m_WindowSize;
	float 						m_PixelRatio; // For high-dpi screens
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
