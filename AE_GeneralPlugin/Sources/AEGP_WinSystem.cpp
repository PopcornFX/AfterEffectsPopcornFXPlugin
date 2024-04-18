//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include <ae_precompiled.h>

#include "AEGP_WinSystem.h"
#include "PopcornFX_Suite.h"

#include "AEGP_System.h"

#include <pk_toolkit/include/pk_toolkit_version.h>

#if defined(PK_WINDOWS)

#include <processthreadsapi.h>
#include <windows.h>
#include <tchar.h> 
#include <stdio.h>
#include <strsafe.h>
#include <io.h>
#include <fcntl.h>
#include <namedpipeapi.h>


__AEGP_PK_BEGIN

//----------------------------------------------------------------------------

CString CWinSystemHelper::GetLastErrorAsString()
{
	TArray<DWORD>	error;
	return GetLastErrorAsString(error);
}

//----------------------------------------------------------------------------

CString CWinSystemHelper::GetLastErrorAsString(TArray<DWORD> &ignore)
{
	//Get the error message ID, if any.
	DWORD errorMessageID = ::GetLastError();
	if (errorMessageID == 0 || ignore.Contains(errorMessageID))
		return CString();

	LPSTR messageBuffer = nullptr;

	//Ask Win32 to give us the string version of that message ID.
	//The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
	size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
								 NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

	//Copy the error message into a std::string.
	CString message(messageBuffer, (u32)size);

	//Free the Win32's string's buffer.
	LocalFree(messageBuffer);

	return message;
}

//----------------------------------------------------------------------------

CString	CWinSystemHelper::_GetWindowsInstallDir()
{
	CString	appFolderPath;

	aechar_t	dstPath[MAX_PATH];
	if (::ExpandEnvironmentStringsW(L"%ProgramW6432%", (LPWSTR)dstPath, PK_ARRAY_COUNT(dstPath)) != 0)
	{
		std::string dest;
		dstPath[PK_ARRAY_COUNT(dstPath) - 1] = 0;
		WCharToString(dstPath, &dest);
		appFolderPath = dest.c_str();
		appFolderPath = appFolderPath + (appFolderPath.EndsWith("/") ? "" : "/") + "Persistant Studios/";
	}
	return appFolderPath;
}

//----------------------------------------------------------------------------

TArray<SEditorExecutable>	CWinSystemHelper::_FindInstalledVersions(const CString &baseSearchPath)
{
	WIN32_FIND_DATA	ffd;
	TCHAR			szDir[MAX_PATH];
	HANDLE			hFind = INVALID_HANDLE_VALUE;
	DWORD			dwError = 0;

	// we expect 'baseSearchPath' to be the root 'Persistant Studios' install dir,
	// inside which are located PopcornFX editor executables of the form 'PopcornFX-X.Y/Binaries_Release/PK-Editor.exe'
	TArray<SEditorExecutable>	paths;

	// Prepare string for use with FindFile functions.  First, copy the
	// string to a buffer, then append '\*' to the directory name.
	StringCchCopy(szDir, MAX_PATH, baseSearchPath.Data());
	StringCchCat(szDir, MAX_PATH, TEXT("\\*"));

	// Find the first file in the directory.
	hFind = FindFirstFile(szDir, &ffd);

	if (INVALID_HANDLE_VALUE == hFind)
	{
		CString error = GetLastErrorAsString();
		CLog::Log(PK_ERROR, "%s", error.Data());
		return paths;
	}

	// List all the files in the directory with some info about them.
	do
	{
		if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			const CString	entryName = ffd.cFileName;
			if (entryName.StartsWith("PopcornFX-"))
			{
				const CString	editorVersionDir = baseSearchPath + entryName;
				const CString	editorExecPathV2 = editorVersionDir + "/bin/PK-Editor.exe";
				const CString	editorExecPathUn = editorVersionDir + "/Uninstall.exe";

				DWORD dwAttrib = GetFileAttributes(TEXT(editorExecPathV2.Data()));
				if (dwAttrib != INVALID_FILE_ATTRIBUTES)
				{
					const SDllVersion	version = PKTKGetFileVersionInfo(editorExecPathV2.Data());
					if (!paths.PushBack(SEditorExecutable(editorExecPathV2, "", SEngineVersion(version.Major, version.Minor, version.Patch, version.RevID))).Valid())
					{
						CLog::Log(PK_ERROR, "TArray Alloc Failed");
						return paths;
					}

				}
			}
		}
	} while (FindNextFile(hFind, &ffd) != 0);

	dwError = GetLastError();
	if (dwError != ERROR_NO_MORE_FILES)
	{
		CString error = GetLastErrorAsString();
		CLog::Log(PK_ERROR, "%s", error.Data());
	}

	FindClose(hFind);
	return paths;
}

//----------------------------------------------------------------------------

SEditorExecutable	CWinSystemHelper::GetMatchingEditor(const SEngineVersion &version)
{
	PK_ASSERT(version.Major() == 2);

	CGuid	bestVerPatch;
	CGuid	bestVer;

	TArray<SEditorExecutable>	execList = _FindInstalledVersions(_GetWindowsInstallDir());
	for (u32 i = 0; i < execList.Count(); i++)
	{
		const SEditorExecutable		&execVer = execList[i];

		if (execVer.m_Version.Equal_IgnoreRevID(version))
		{
			// Exact version, always the best option.
			// This assumes there's always a single version installed at the same patch,
			// and there can't be multiple versions with the same major.minor.patch version.
			// Otherwise, when there are multiple similar versions installed,
			// it'll take the first one in the list. Do we care?
			// Do we want to handle that better and actually return the one with
			// the closest revid ?
			CLog::Log(PK_INFO, "Running exact match editor : %s", execVer.m_BinaryPath.Data());
			return execVer;	// Exact version
		}
		if (execVer.m_Version.Equal_IgnorePatch(version))
		{
			// Within the same minor version, this is the best candidate
			if (!bestVerPatch.Valid())
				bestVerPatch = i;
			else
			{
				const u32	patchPrev = execList[bestVerPatch].m_Version.Patch();
				const u32	patchNew = execVer.m_Version.Patch();
				{
					if (patchNew >= patchPrev)
						bestVerPatch = i;
				}
			}
		}
		else if (execVer.m_Version >= version)
		{
			// Minor version higher than project
			if (!bestVer.Valid())
				bestVer = i;
			else
			{
				if (execVer.m_Version < execList[bestVer].m_Version)	// if it's closer to the target project version
					bestVer = i;
			}
		}
	}
	if (bestVerPatch.Valid())
		return execList[bestVerPatch];
	if (bestVer.Valid())
		return execList[bestVer];

	SEditorExecutable	ret;
#if _DEBUG
	ret.m_BinaryPath = "%PK_SDK_ROOT%\\Tools\\Poped\\build_vs2019\\PK-Editor_d.exe";
#else
	ret.m_BinaryPath = "%PK_SDK_ROOT%\\Tools\\Poped\\build_vs2019\\PK-Editor_r.exe";
#endif
	CLog::Log(PK_INFO, "Running default path dev: %s", ret.m_BinaryPath.Data());
	return ret;
}

//----------------------------------------------------------------------------

__AEGP_PK_END

#endif	// PK_WINDOWS
