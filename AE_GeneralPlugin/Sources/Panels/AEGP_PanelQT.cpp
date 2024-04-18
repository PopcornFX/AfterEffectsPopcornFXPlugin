//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include <ae_precompiled.h>

#include "Panels/AEGP_PanelQT.h"

#include "RenderApi/AEGP_BaseContext.h"
#include "AEGP_RenderContext.h"

#include "AEGP_World.h"
#include "AEGP_Scene.h"
#include "AEGP_PackExplorer.h"

#include "AEGP_AssetBaker.h"

#include "AEGP_Log.h"
//Suite
#include <PopcornFX_Suite.h>

//AE
#include <AE_GeneralPlug.h>
#include <SuiteHelper.h>

#include <QtWidgets>
#include <QTreeView>

#include "Panels/AEGP_GraphicalResourcesTreeModel.h"
#include "AEGP_AEPKConversion.h"

#include "AEGP_FileDialog.h"

#if defined(PK_WINDOWS)
#include <Windows.h>
#include <windowsx.h>
#endif

__AEGP_PK_BEGIN

//----------------------------------------------------------------------------

template<typename _Type>
PK_FORCEINLINE void		safe_delete(_Type * & ptr)
{
	if (ptr != null)
	{
		delete ptr;
		ptr = null;
	}
}

//----------------------------------------------------------------------------

#if defined(PK_WINDOWS)
const char	OSWndObjectProperty[] = "CPanelPopcornFX_PlatPtr";
#endif

CPanelBaseGUI	*CPanelBaseGUI::m_Instance = null;

//----------------------------------------------------------------------------

QPanelAppSignalSink::QPanelAppSignalSink(QApplication *app, QObject *parent)
	: QObject(parent)
	, m_App(app)
{
}

//----------------------------------------------------------------------------

void	QPanelAppSignalSink::DoExit()
{
	m_App->exit();
}

//----------------------------------------------------------------------------

QPanel::QPanel(QPanelAppSignalSink* appSignal, QApplication *application, QObject *parent /*= null*/)
	: QObject(parent)
	, m_AppSignal(appSignal)
	, m_App(application)
{
	m_TreeModel = new CGraphicalResourcesTreeModel();
	m_TreeModel->UpdateModel();
	
	QObject::connect(appSignal, &QPanelAppSignalSink::OnWindowHandlerChanged, this, &QPanel::_SetWindow);
	QObject::connect(appSignal, &QPanelAppSignalSink::OnWindowSizeChanged, this, &QPanel::_SetGeometry);
	QObject::connect(appSignal, &QPanelAppSignalSink::OnRenderersChanged, this, &QPanel::_UpdateModel);
}

//----------------------------------------------------------------------------

QPanel::~QPanel()
{
	safe_delete(m_TreeModel);
	safe_delete(m_Widget);
	safe_delete(m_Window);
}

//----------------------------------------------------------------------------

void	QPanel::_CreateWindowContent()
{
	QVBoxLayout	*container = new QVBoxLayout();
	container->setMargin(0);
	m_Widget->setLayout(container);

	QTabWidget	*tab = new QTabWidget();
	container->addWidget(tab);

	QWidget	*rendererTab = new QWidget();
	{
		QVBoxLayout						*layout = new QVBoxLayout();
		layout->setMargin(0);

		QTreeView						*view = new QTreeView();

		view->setItemDelegate(new CGraphicResourceDelegate());

		// this line cause the error "QObject::~QObject: Timers cannot be stopped from another thread" when the application quit.
		// this is a known issue
		// see here : https://forum.qt.io/topic/128256/timers-cannot-be-stopped-from-a-different-thread/6
		// and here : https://lists.qt-project.org/pipermail/development/2021-May/041517.html
		view->setModel(m_TreeModel);
		view->setColumnWidth(2, 35);

		QObject::connect(view, &QTreeView::clicked, m_TreeModel, [this](const QModelIndex &index)
		{
			QModelIndex	targetIndex;
			targetIndex = index;
			
			QVariant variant = this->m_TreeModel->data(targetIndex, Qt::DisplayRole);

			if (variant.canConvert<CGraphicResourceView>())
			{
				CGraphicalResourcesTreeItem		*item = this->m_TreeModel->Item(targetIndex);
				CGraphicResourceView			view = qvariant_cast<CGraphicResourceView>(variant);
				if (view.Type() == CGraphicResourceView::ViewType::ViewType_PathResource)
				{
					AEGPPk::SFileDialog	cbData;

#if !defined(PK_MACOSX)
					// Filter *.* not working on MacOS
					cbData.AddFilter("Image file (*.*)", "*.*");
#endif
					struct	SFunctor
					{
						void Function(const CString path)
						{
							if (m_Item != null)
							{
								m_Item->SetData(1, QString(path.Data()));

								AEGPPk::CPopcornFXWorld		&world = AEGPPk::CPopcornFXWorld::Instance();
								if (!world.SetResourceOverride(m_Item->GetLayerID(), m_Item->GetRendererID(), m_Item->GetID(), path))
								{
									CLog::Log(PK_ERROR, "Set resource override failed");
									return;
								}
							}
						}

						CGraphicalResourcesTreeItem*	m_Item = null;
					};

					static SFunctor	functor;

					functor.m_Item = item;
					cbData.SetCallback(FastDelegate<void(const CString)>(&functor, &SFunctor::Function));

					cbData.BasicFileOpen();
				}
			}
			else if (variant.canConvert<CGraphicResetButtonView>())
			{
				CGraphicalResourcesTreeItem		*item = this->m_TreeModel->Item(targetIndex);

				AEGPPk::CPopcornFXWorld		&world = AEGPPk::CPopcornFXWorld::Instance();
				world.SetResourceOverride(item->GetLayerID(), item->GetRendererID(), item->GetID(), "");
			}
		});

		layout->addWidget(view);

		rendererTab->setLayout(layout);
	}
	tab->addTab(rendererTab, "Renderers");

	QWidget	*settingsTab = new QWidget(tab);
	{
		QVBoxLayout *layout = new QVBoxLayout();

		QGridLayout *Glayout = new QGridLayout();
		Glayout->setColumnStretch(0, 1);
		{
			QString message = "Graphical API";
			QLabel *label = new QLabel(message);
			Glayout->addWidget(label, 0, 0);

			QComboBox *graphicCombo = new QComboBox();

			RHI::EGraphicalApi				currentApi = AEGPPk::CPopcornFXWorld::Instance().GetRenderApi();
			EApiValue						selectedAPI = RHIApiToAEApi(currentApi);
			u32								selectedAPIIndex = 0;
			for (u32 i = 0; i < PK_ARRAY_COUNT(SAEPreferenciesKeys::kSupportedAPIs); ++i)
			{
				if (selectedAPI == SAEPreferenciesKeys::kSupportedAPIs[i])
					selectedAPIIndex = i;
				const char *apiStr = SAEPreferenciesKeys::GetGraphicsApiAsCharPtr(SAEPreferenciesKeys::kSupportedAPIs[i]);
				graphicCombo->insertItem(i, apiStr);
			}
			graphicCombo->setCurrentIndex(selectedAPIIndex);
			QObject::connect(graphicCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [](int value) { AEGPPk::CPopcornFXWorld::Instance().SetRenderApi(SAEPreferenciesKeys::kSupportedAPIs[value]); });
			Glayout->addWidget(graphicCombo, 0, 1);
		}
		layout->addLayout(Glayout);

#if defined(PK_DEBUG)
		{
			QString messageButton = "Reload CSS";
			QPushButton  *button = new QPushButton(messageButton);
			QObject::connect(button, &QPushButton::released, [this]()
				{
					CPopcornFXWorld			&instance = AEGPPk::CPopcornFXWorld::Instance();
					QString	path = instance.GetPluginInstallationPath().Data() + QString("/Stylesheet.qss");
					QFile	file(path);

					if (!file.open(QFile::ReadOnly))
						return false;
					QString styleSheet = QLatin1String(file.readAll());
					m_App->setStyleSheet(styleSheet);
					return true;
				});
			layout->addWidget(button);
		}

		{
			QString messageButton = "Profile";
			QCheckBox  *button = new QCheckBox(messageButton);
			QObject::connect(button, &QCheckBox::stateChanged, [this](int state)
				{
					CPopcornFXWorld			&instance = AEGPPk::CPopcornFXWorld::Instance();

					instance.SetProfilingState(state != 0);
				});
			layout->addWidget(button);
		}
#endif

		settingsTab->setLayout(layout);
	}
	tab->addTab(settingsTab, "Settings");
}

//----------------------------------------------------------------------------

void	QPanel::_SetWindow(WId wid)
{
	m_PendingWindowHandle = wid;
}

//----------------------------------------------------------------------------

void	QPanel::_SetGeometry(const QRect &windowRect)
{
	if (m_PendingWindowHandle != 0)
	{
		safe_delete(m_Widget);
		safe_delete(m_Window);

		m_Widget = new QWidget(null, Qt::FramelessWindowHint);

		_CreateWindowContent();
#if defined(PK_WINDOWS)
		m_Widget->setProperty("_q_embedded_native_parent_handle", QVariant(m_PendingWindowHandle));
#else
		//Create internal window
		m_Widget->winId();
		m_Window = QWindow::fromWinId(m_PendingWindowHandle);
		m_Widget->windowHandle()->setParent(m_Window);
#endif
	}

#if defined(PK_MACOSX)
	(void)windowRect;
	if (m_Window != null)
	{
		// In the Qt documentation of QWindow::fromWinId this is not advised to observe state changes like this
		// Do it like this for mac anyway since it's working but this should be changed
		// https://doc.qt.io/qt-5/qwindow.html#fromWinId
		QRect	newRect = QRect(0, 0, m_Window->geometry().width(), m_Window->geometry().height());
		if (m_Widget != null && m_Widget->geometry() != newRect)
			m_Widget->setGeometry(newRect);
	}
#else
	if (m_Widget != null && m_Widget->geometry() != windowRect)
		m_Widget->setGeometry(windowRect);
#endif

	if (m_PendingWindowHandle != 0)
	{
		m_Widget->show();
		m_PendingWindowHandle = 0;
	}
}

//----------------------------------------------------------------------------

void	QPanel::_UpdateModel()
{
	if (m_TreeModel != null)
		m_TreeModel->UpdateModel();
}

//----------------------------------------------------------------------------

CPanelApp::CPanelApp()
{
}

//----------------------------------------------------------------------------

CPanelApp::~CPanelApp()
{
	PK_ASSERT(m_QApp == null);
	PK_ASSERT(m_AppSignalSink == null);
	PK_ASSERT(m_Panel == null);
}

//----------------------------------------------------------------------------

bool	CPanelApp::Startup()
{
	CPopcornFXWorld	&instance = AEGPPk::CPopcornFXWorld::Instance();

#if defined(PK_MACOSX)
	QApplication::setAttribute(Qt::AA_MacPluginApplication, true);
	CString	QTPath = instance.GetPluginInstallationPath() / "AE_GeneralPlugin.plugin/Contents/PlugIns";
#else
	CString	QTPath = instance.GetPluginInstallationPath() / "popcornfx.qt";
#endif

	CLog::Log(PK_INFO, "path: %s", QTPath.Data());

	char *argv[] = { (char *)"", (char *)"-platformpluginpath", QTPath.RawDataForWriting(), null };
	int argc = sizeof(argv) / sizeof(char*) - 1;

	m_QApp = new QApplication(argc, argv);
	if (!PK_VERIFY(m_QApp != null))
	{
		CLog::Log(PK_ERROR, "Could not initialize Qt");
		return false;
	}

#if defined(PK_MACOSX)
	m_EventLoop = new QEventLoop();
	if (!PK_VERIFY(m_EventLoop != null))
	{
		CLog::Log(PK_ERROR, "Could not initialize Qt Event Loop");
		return false;
	}
#endif

	qRegisterMetaType<WId>("WId");

	m_AppSignalSink = new QPanelAppSignalSink(m_QApp);
	if (m_AppSignalSink == null)
	{
		CLog::Log(PK_ERROR, "Could not initialize Qt signal sink");
		Shutdown();
		return false;
	}
	QObject::connect(m_AppSignalSink, &QPanelAppSignalSink::OnExit, m_AppSignalSink, &QPanelAppSignalSink::DoExit, Qt::QueuedConnection);

	if (!QResource::registerResource(QString(instance.GetResourcesPath().Data()) + "Resources.rcc"))
	{
		CLog::Log(PK_ERROR, "Could not load Resources.rcc");
		PK_ASSERT_NOT_REACHED();
	}

	QFile	stylesheetFile(QString(instance.GetResourcesPath().Data()) + "/Stylesheet.qss");
	if (!stylesheetFile.open(QFile::ReadOnly))
	{
		CLog::Log(PK_ERROR, "Could not load Stylesheet.qss");
		PK_ASSERT_NOT_REACHED();
	}

	QString styleSheet = QLatin1String(stylesheetFile.readAll());
	m_QApp->setStyleSheet(styleSheet);

	m_Panel = new QPanel(m_AppSignalSink, m_QApp);
	if (!PK_VERIFY(m_Panel != null))
	{
		CLog::Log(PK_ERROR, "Could not initialize Qt panel");
		Shutdown();
		return false;
	}
	return true;
}

//----------------------------------------------------------------------------

void	CPanelApp::LaunchApp()
{
#if defined(PK_WINDOWS)
	m_QApp->exec();
#else
	OnWindowSizeChanged(QRect(0, 0, 0, 0));
	m_EventLoop->processEvents();
#endif
}

//----------------------------------------------------------------------------

void	CPanelApp::Shutdown()
{
#if defined(PK_MACOSX)
	if (m_QApp != null)
		m_QApp->quit();
	safe_delete(m_EventLoop);
#endif
	safe_delete(m_Panel);
	safe_delete(m_AppSignalSink);
	safe_delete(m_QApp);
}

//----------------------------------------------------------------------------

void	CPanelApp::OnWindowHandlerChanged(WId wid)
{
	if (PK_VERIFY(m_AppSignalSink != null))
		Q_EMIT m_AppSignalSink->OnWindowHandlerChanged(wid);
}

//----------------------------------------------------------------------------

void	CPanelApp::OnWindowSizeChanged(const QRect &rect)
{
	if (PK_VERIFY(m_AppSignalSink != null))
		Q_EMIT m_AppSignalSink->OnWindowSizeChanged(rect);
}

//----------------------------------------------------------------------------

void	CPanelApp::OnRenderersChanged()
{
	if (PK_VERIFY(m_AppSignalSink != null))
		Q_EMIT m_AppSignalSink->OnRenderersChanged();
}

//----------------------------------------------------------------------------

void	CPanelApp::OnExit()
{
	if (PK_VERIFY(m_AppSignalSink != null))
		Q_EMIT m_AppSignalSink->OnExit();
}

//----------------------------------------------------------------------------
#if defined(PK_WINDOWS)

CAsynchronousJob_QtThread::CAsynchronousJob_QtThread()
{
}

//----------------------------------------------------------------------------

CAsynchronousJob_QtThread::~CAsynchronousJob_QtThread()
{
}

//----------------------------------------------------------------------------

void		CAsynchronousJob_QtThread::ImmediateExecute()
{
	if (!m_App.Startup())
	{
		m_Initialized.Trigger();
		m_Exited.Trigger();
		return;
	}

	m_Initialized.Trigger();

	m_App.LaunchApp();

	// VMN: Locking operation on my pc.
	//m_App.Shutdown();

	m_Exited.Trigger();
}

#endif
//----------------------------------------------------------------------------

CPanelBaseGUI::CPanelBaseGUI()
#if defined(PK_WINDOWS)
	: m_Task(null)
#endif
{
}

//----------------------------------------------------------------------------

CPanelBaseGUI::~CPanelBaseGUI()
{
#if defined(PK_WINDOWS)
	if (m_Task != null)
	{
		m_Task->App().OnExit();
		m_Task->m_Exited.Wait();
	}
#else
	m_App.Shutdown();
#endif
}

//----------------------------------------------------------------------------

CPanelBaseGUI	*CPanelBaseGUI::GetInstance()
{
	if (m_Instance == null)
	{
		m_Instance = new CPanelBaseGUI();
	}
	return m_Instance;
}

//----------------------------------------------------------------------------

bool	CPanelBaseGUI::DestroyInstance()
{
	if (m_Instance)
		delete m_Instance;
	m_Instance = null;
	return true;
}

//----------------------------------------------------------------------------

bool CPanelBaseGUI::InitializeIFN()
{
	if (!m_Initialized)
	{
		AAePk::SAAEIOData		AAEData{ PF_Cmd_ABOUT, null, null, null, null };
		CPopcornFXWorld			&instance = AEGPPk::CPopcornFXWorld::Instance();
		if (!instance.InitializeIFN(AAEData))
			return false;
		instance.SetPanelInstance(this);

#if defined(PK_WINDOWS)
		m_Task = PK_NEW(CAsynchronousJob_QtThread());
		if (!PK_VERIFY(m_Task != null))
			return false;
		m_Task->AddToPool(Scheduler::ThreadPool());
		Scheduler::ThreadPool()->KickTasks(true);
		m_Task->m_Initialized.Wait();
#else
		m_App.Startup();
#endif

		m_Initialized = true;
	}
	return true;
}

//----------------------------------------------------------------------------
#if defined(PK_MACOSX)
void CPanelBaseGUI::IdleUpdate()
{
	PK_ASSERT(m_Initialized);

	m_App.LaunchApp();
}
#endif
//----------------------------------------------------------------------------

bool CPanelBaseGUI::CreatePanel(SPBasicSuite *spbP, AEGP_PanelH panelH, AEGP_PlatformViewRef platformWindowRef, AEGP_PanelFunctions1 *outFunctionTable)
{
	PK_ASSERT(m_Initialized);

	m_BasicSuite = spbP;
	(void)panelH;
	outFunctionTable->DoFlyoutCommand = _DoFlyoutCommand;
	outFunctionTable->GetSnapSizes = _GetSnapSizes;
	outFunctionTable->PopulateFlyout = _PopulateFlyout;

	{
#if defined(PK_WINDOWS)
		m_Task->App().OnWindowHandlerChanged((WId)platformWindowRef);
#else
		m_App.OnWindowHandlerChanged((WId)platformWindowRef);
#endif

#if defined(PK_WINDOWS)
		_SetWindowHandle((HWND)platformWindowRef);
#endif
	}
	return true;
}

//----------------------------------------------------------------------------

void	CPanelBaseGUI::SetGeometry(const QRect &rect)
{
	PK_ASSERT(m_Initialized);
#if defined(PK_WINDOWS)
	m_Task->App().OnWindowSizeChanged(rect);
#else
	m_App.OnWindowSizeChanged(rect);
#endif
}

//----------------------------------------------------------------------------

void	CPanelBaseGUI::UpdateScenesModel()
{
	PK_ASSERT(m_Initialized);
#if defined(PK_WINDOWS)
	m_Task->App().OnRenderersChanged();
#else
	m_App.OnRenderersChanged();
#endif
}

//----------------------------------------------------------------------------

void	CPanelBaseGUI::GetSnapSizes(A_LPoint *snapSizes, A_long *numSizesP)
{
	snapSizes[0].x = 100;
	snapSizes[0].y = 100;
	snapSizes[1].x = 200;
	snapSizes[1].y = 400;
	*numSizesP = 2;
}

//----------------------------------------------------------------------------

void 	CPanelBaseGUI::PopulateFlyout(AEGP_FlyoutMenuItem *itemsP, A_long *in_out_numItemsP)
{
	(void)itemsP;
	(void)in_out_numItemsP;
}

//----------------------------------------------------------------------------

void	CPanelBaseGUI::DoFlyoutCommand(AEGP_FlyoutMenuCmdID commandID)
{
	(void)commandID;
}

//----------------------------------------------------------------------------

A_Err	CPanelBaseGUI::_GetSnapSizes(AEGP_PanelRefcon refcon, A_LPoint *snapSizes, A_long *numSizesP)
{
	reinterpret_cast<CPanelBaseGUI*>(refcon)->GetSnapSizes(snapSizes, numSizesP);
	return PF_Err_NONE;
}

//----------------------------------------------------------------------------

A_Err	CPanelBaseGUI::_PopulateFlyout(AEGP_PanelRefcon refcon, AEGP_FlyoutMenuItem *itemsP, A_long * in_out_numItemsP)
{
	reinterpret_cast<CPanelBaseGUI*>(refcon)->PopulateFlyout(itemsP, in_out_numItemsP);
	return PF_Err_NONE;
}

//----------------------------------------------------------------------------

A_Err	CPanelBaseGUI::_DoFlyoutCommand(AEGP_PanelRefcon refcon, AEGP_FlyoutMenuCmdID commandID)
{
	reinterpret_cast<CPanelBaseGUI*>(refcon)->DoFlyoutCommand(commandID);
	return PF_Err_NONE;
}

#if defined(PK_WINDOWS)

LRESULT CALLBACK	CPanelBaseGUI::StaticOSWindowWndProc(	HWND	hWnd,
															UINT	message,
															WPARAM	wParam,
															LPARAM	lParam)
{
	CPanelBaseGUI* platPtr = reinterpret_cast<CPanelBaseGUI*>(::GetProp(hWnd, OSWndObjectProperty));
	if (platPtr)
	{
		return platPtr->OSWindowWndProc(hWnd, message, wParam, lParam);
	}
	else
	{
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
}

//----------------------------------------------------------------------------

LRESULT CPanelBaseGUI::OSWindowWndProc(HWND hwnd, UINT message, WPARAM	wParam,	LPARAM lParam)
{
	PK_SCOPEDLOCK(m_HandleLock);

	bool			eventHandled = false;

	if (m_WindowHandle == hwnd) // Filter events that are not for our window
	{
		RECT	rect;
		GetClientRect(hwnd, &rect);

		u32		width = rect.right - rect.left;
		u32		height = rect.bottom - rect.top;

		SetGeometry(QRect(rect.left, rect.top, width, height));

		eventHandled = true;
	}

	if (m_WindowProc && !eventHandled)
		return CallWindowProc(m_WindowProc, hwnd, message, wParam, lParam);
	else
		return DefWindowProc(hwnd, message, wParam, lParam);
}

//----------------------------------------------------------------------------

void	CPanelBaseGUI::_SetWindowHandle(HWND hwnd)
{
	PK_SCOPEDLOCK(m_HandleLock);

	m_WindowHandle = hwnd;
	m_WindowProc = (WindowProc)GetWindowLongPtrA(m_WindowHandle, GWLP_WNDPROC);
	SetWindowLongPtrA(m_WindowHandle, GWLP_WNDPROC, (LONG_PTR)CPanelBaseGUI::StaticOSWindowWndProc);
	::SetProp(m_WindowHandle, OSWndObjectProperty, (HANDLE)this);
}

#endif // defined(PK_WINDOWS)

//----------------------------------------------------------------------------

__AEGP_PK_END
