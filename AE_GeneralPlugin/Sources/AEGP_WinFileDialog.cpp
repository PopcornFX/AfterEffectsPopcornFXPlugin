//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include <ae_precompiled.h>

#if defined(PK_WINDOWS)

#include "AEGP_WinFileDialog.h"

#include <pk_kernel/include/kr_file.h>
#include <pk_kernel/include/kr_callbacks.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <windowsx.h>
#include <XInput.h>
#include <tchar.h>

#include <shobjidl.h>
#include <shlwapi.h>

#include <sstream>

#include <pk_kernel/include/kr_log.h>

#include "AEGP_World.h"

//----------------------------------------------------------------------------

class CDialogEventHandler : public IFileDialogEvents,
	public IFileDialogControlEvents
{
public:
	// IUnknown methods
	IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv)
	{
#pragma warning(disable:4838)
		static const QITAB qit[] = {
			QITABENT(CDialogEventHandler, IFileDialogEvents),
			QITABENT(CDialogEventHandler, IFileDialogControlEvents),
			{ 0 },
		};
		return QISearch(this, qit, riid, ppv);

#pragma warning(default:4838)
	}

	IFACEMETHODIMP_(ULONG) AddRef()
	{
		return InterlockedIncrement(&_cRef);
	}

	IFACEMETHODIMP_(ULONG) Release()
	{
		long cRef = InterlockedDecrement(&_cRef);
		if (!cRef)
			delete this;
		return cRef;
	}

	// IFileDialogEvents methods
	IFACEMETHODIMP		OnFileOk(IFileDialog *) { return S_OK; };
	IFACEMETHODIMP		OnFolderChange(IFileDialog *) { return S_OK; };
	IFACEMETHODIMP		OnFolderChanging(IFileDialog *, IShellItem *) { return S_OK; };
	IFACEMETHODIMP		OnHelp(IFileDialog *) { return S_OK; };
	IFACEMETHODIMP		OnSelectionChange(IFileDialog *) { return S_OK; };
	IFACEMETHODIMP		OnShareViolation(IFileDialog *, IShellItem *, FDE_SHAREVIOLATION_RESPONSE *) { return S_OK; };
	IFACEMETHODIMP		OnTypeChange(IFileDialog *) { return S_OK; };
	IFACEMETHODIMP		OnOverwrite(IFileDialog *, IShellItem *, FDE_OVERWRITE_RESPONSE *) { return S_OK; };

	// IFileDialogControlEvents methods
	IFACEMETHODIMP		OnItemSelected(IFileDialogCustomize *, DWORD, DWORD) { return S_OK; };
	IFACEMETHODIMP		OnButtonClicked(IFileDialogCustomize *, DWORD) { return S_OK; };
	IFACEMETHODIMP		OnCheckButtonToggled(IFileDialogCustomize *, DWORD, BOOL) { return S_OK; };
	IFACEMETHODIMP		OnControlActivating(IFileDialogCustomize *, DWORD) { return S_OK; };

	CDialogEventHandler() : _cRef(1) { };
private:
	~CDialogEventHandler() { };
	long _cRef;
};

//----------------------------------------------------------------------------

// Instance creation helper
HRESULT CDialogEventHandler_CreateInstance(REFIID riid, void **ppv)
{
	*ppv = NULL;
	CDialogEventHandler		*pDialogEventHandler = new (std::nothrow) CDialogEventHandler();
	HRESULT					hr = pDialogEventHandler ? S_OK : E_OUTOFMEMORY;
	if (SUCCEEDED(hr))
	{
		hr = pDialogEventHandler->QueryInterface(riid, ppv);
		pDialogEventHandler->Release();
	}
	return hr;
}

//----------------------------------------------------------------------------

HRESULT		WinBasicFileOpen(SWinFileOpenData &data)
{
	IFileDialog				*pfd = NULL;
	AEGP_SuiteHandler		suites(AEGPPk::CPopcornFXWorld::Instance().GetAESuites());
	HWND					winHandle = null;

	suites.UtilitySuite6()->AEGP_GetMainHWND(&winHandle);

	HRESULT			hr = CoCreateInstance(CLSID_FileOpenDialog,
		NULL,
		CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&pfd));
	if (SUCCEEDED(hr))
	{
		IFileDialogEvents	*pfde = NULL;
		hr = CDialogEventHandler_CreateInstance(IID_PPV_ARGS(&pfde));
		if (SUCCEEDED(hr))
		{
			DWORD	dwCookie;
			hr = pfd->Advise(pfde, &dwCookie);
			if (SUCCEEDED(hr))
			{
				DWORD	dwFlags;
				hr = pfd->GetOptions(&dwFlags);
				if (SUCCEEDED(hr))
				{
					hr = pfd->SetOptions(dwFlags | FOS_FORCEFILESYSTEM);
					if (SUCCEEDED(hr))
					{
						TArray<COMDLG_FILTERSPEC>		filters;
						for (u32 i = 0; i < data.m_Filters.Count(); i++)
							filters.PushBack(data.m_Filters[i].m_Spec);

						hr = pfd->SetFileTypes(filters.Count(), filters.RawDataPointer());
						if (SUCCEEDED(hr))
						{
							hr = pfd->SetFileTypeIndex(0);
							if (SUCCEEDED(hr))
							{
								hr = pfd->SetDefaultExtension(L"*.pkproj");
								if (SUCCEEDED(hr))
								{
									hr = pfd->Show(winHandle);
									if (SUCCEEDED(hr))
									{
										IShellItem	*psiResult;
										hr = pfd->GetResult(&psiResult);
										if (SUCCEEDED(hr))
										{
											wchar_t		*pszFilePath = NULL;
											hr = psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
											if (SUCCEEDED(hr))
											{
												char		result[1024];
												CString		stringResult;
												wcstombs(result, pszFilePath, 1024);
												stringResult = result;

												data.m_Cb(stringResult);
											}
											psiResult->Release();
										}
									}
								}
							}
						}
					}
				}
				pfd->Unadvise(dwCookie);
			}
			pfde->Release();
		}
		pfd->Release();
	}
	return hr;
}

//----------------------------------------------------------------------------

#endif
