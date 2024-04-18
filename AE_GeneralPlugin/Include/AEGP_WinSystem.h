//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once
#include "AEGP_Define.h"
#include "AEGP_System.h"

#include <pk_toolkit/include/pk_toolkit_version.h>

__AEGP_PK_BEGIN

//----------------------------------------------------------------------------

#if defined(PK_WINDOWS)

class	CWinSystemHelper
{
private:
	CWinSystemHelper() = delete;
	~CWinSystemHelper() = delete;
public:
	static CString						GetLastErrorAsString();
	static CString						GetLastErrorAsString(TArray<DWORD> &ignore);
	static CString						_GetWindowsInstallDir();
	static TArray<SEditorExecutable>	_FindInstalledVersions(const CString &baseSearchPath);

	static SEditorExecutable			GetMatchingEditor(const SEngineVersion &version);
};

#endif

//----------------------------------------------------------------------------

__AEGP_PK_END
