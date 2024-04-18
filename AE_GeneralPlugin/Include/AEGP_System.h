//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once
#include "AEGP_Define.h"

#include <pk_version.h>
#include <pk_kernel/include/kr_engine_version.h>

__AEGP_PK_BEGIN

//----------------------------------------------------------------------------

struct	SEditorExecutable
{
	CString			m_BinaryPathUninstall;
	CString			m_BinaryPath;
	SEngineVersion	m_Version;

	SEditorExecutable() {}
	SEditorExecutable(const CString &path, const CString &uninstallerPath, const SEngineVersion &version)
		: m_BinaryPathUninstall(uninstallerPath)
		, m_BinaryPath(path)
		, m_Version(version)
	{

	}

	bool	operator < (const SEditorExecutable &other) const { return m_Version < other.m_Version; }
};

//----------------------------------------------------------------------------

class	CSystemHelper
{
public: // Get Hardware ID
	static const CString	GetUniqueHardwareID();
	static const CString	GetUniqueHardwareIDForHuman();

	static bool				LaunchEditorAsPopup();

private:
	static u16			*_ComputeSystemUniqueId();

	static void			_Smear(u16 *id);
	static void			_Unsmear(u16 *id);

	static u16			_GetMacHash();

	static u16			_GetCPUHash();
	static const char	*_GetMachineName();

private:
	CSystemHelper() = delete;
	~CSystemHelper() = delete;
};

//----------------------------------------------------------------------------

__AEGP_PK_END
