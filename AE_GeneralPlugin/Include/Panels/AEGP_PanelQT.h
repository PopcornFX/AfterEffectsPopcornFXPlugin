//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef	__FX_PANELQT_H__
#define	__FX_PANELQT_H__

#include "AEGP_Define.h"

#include <AE_GeneralPlugPanels.h>
#include <SuiteHelper.h>

#include <pk_kernel/include/kr_refptr.h>
#include <pk_kernel/include/kr_thread_pool.h>

#include <QApplication>
#include <QMainWindow>

//----------------------------------------------------------------------------

struct SPBasicSuite;

__AEGP_PK_BEGIN

class CGraphicalResourcesTreeModel;

//----------------------------------------------------------------------------

class	QPanelAppSignalSink : public QObject
{
	Q_OBJECT
public:
	QPanelAppSignalSink(QApplication *app, QObject *parent = null);

	Q_SIGNAL void	OnWindowHandlerChanged(WId wid);
	Q_SIGNAL void	OnWindowSizeChanged(const QRect &rect);
	Q_SIGNAL void	OnRenderersChanged();
	Q_SIGNAL void	OnExit();

	void	DoExit();

private:
	QApplication	*m_App = null;
};

//----------------------------------------------------------------------------

class	QPanel : public QObject
{
	Q_OBJECT
public:
	QPanel(QPanelAppSignalSink* appSignal, QApplication *application, QObject *parent = null);
	~QPanel();

private:
	void	_CreateWindowContent();

	void	_SetWindow(WId wid);
	void	_SetGeometry(const QRect &windowRect);
	void	_UpdateModel();


	CGraphicalResourcesTreeModel	*m_TreeModel = null;

	QPanelAppSignalSink				*m_AppSignal = null;
	QApplication					*m_App = null;

	QWindow							*m_Window = null;
	QWidget							*m_Widget = null;

	WId								m_PendingWindowHandle = 0;
};

//----------------------------------------------------------------------------

class CPanelApp
{
public:
	CPanelApp();
	~CPanelApp();

	bool	Startup();
	void	LaunchApp();
	void	Shutdown();

	void	OnWindowHandlerChanged(WId wid);
	void	OnWindowSizeChanged(const QRect &rect);
	void	OnRenderersChanged();
	void	OnExit();

private:
	QApplication					*m_QApp = null;
#if defined(PK_MACOSX)
	QEventLoop						*m_EventLoop = null;
#endif
	QPanelAppSignalSink				*m_AppSignalSink = null;

	QPanel							*m_Panel = null;
};

//----------------------------------------------------------------------------

#if !defined(PK_MACOSX)
class CAsynchronousJob_QtThread : public CAsynchronousJob
{
public:
	CAsynchronousJob_QtThread();
	~CAsynchronousJob_QtThread();
	virtual void		_OnRefptrDestruct() override {}


protected:
	virtual void		_VirtualLaunch(Threads::SThreadContext &) override { ImmediateExecute(); }

public:
	void				ImmediateExecute();

	CPanelApp			&App() { return m_App; }

	// Waited for by the main thread:
	Threads::CEvent				m_Initialized;
	Threads::CEvent				m_Exited;

private:
	CPanelApp					m_App;
};
PK_DECLARE_REFPTRCLASS(AsynchronousJob_QtThread);
#endif

//----------------------------------------------------------------------------

class CPanelBaseGUI
{
	static CPanelBaseGUI			*m_Instance;

	CPanelBaseGUI();
	virtual		~CPanelBaseGUI();

public:
	static CPanelBaseGUI	*GetInstance();
	static bool				DestroyInstance();


	bool		InitializeIFN();
#if defined(PK_MACOSX)
	void		IdleUpdate();
#endif
	bool		CreatePanel(SPBasicSuite *spbP, AEGP_PanelH panelH, AEGP_PlatformViewRef platformWindowRef, AEGP_PanelFunctions1 *outFunctionTable);
	void		SetGeometry(const QRect &rect);
	void		UpdateScenesModel();

protected:
	//Base AE Commands
	void	operator=(const CPanelBaseGUI&) = delete;
	CPanelBaseGUI(const CPanelBaseGUI&) = delete;

	AEGP_PanelH				m_PanelHandle;

	virtual void 			GetSnapSizes(A_LPoint *snapSizes, A_long *numSizesP);
	virtual void			PopulateFlyout(AEGP_FlyoutMenuItem *itemsP, A_long *in_out_numItemsP);
	virtual void			DoFlyoutCommand(AEGP_FlyoutMenuCmdID commandID);

private:

	static A_Err			_GetSnapSizes(AEGP_PanelRefcon refcon, A_LPoint *snapSizes, A_long *numSizesP);
	static A_Err			_PopulateFlyout(AEGP_PanelRefcon refcon, AEGP_FlyoutMenuItem *itemsP, A_long *in_out_numItemsP);
	static A_Err			_DoFlyoutCommand(AEGP_PanelRefcon refcon, AEGP_FlyoutMenuCmdID commandID);

	SPBasicSuite					*m_BasicSuite = null;

#if !defined(PK_MACOSX)
	PAsynchronousJob_QtThread		m_Task;
#else
	CPanelApp						m_App;
#endif

	bool							m_Initialized = false;

#if defined(PK_WINDOWS)
public:

	static LRESULT CALLBACK	StaticOSWindowWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT					OSWindowWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
	typedef LRESULT(CALLBACK* WindowProc)(HWND	hwnd, UINT	message, WPARAM	wParam, LPARAM	lParam);

	Threads::CCriticalSection		m_HandleLock;
	HWND							m_WindowHandle;
	WindowProc						m_WindowProc;

	void	_SetWindowHandle(HWND hwnd);
#endif
};

//----------------------------------------------------------------------------

__AEGP_PK_END

#endif //!__FX_PANELQT_H__
