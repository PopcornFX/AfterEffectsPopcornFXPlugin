//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef __AEGP_VAULHANDLER_H__
#define __AEGP_VAULHANDLER_H__

#include <AEConfig.h>

#include "AEGP_Define.h"
#include "AEGP_FileWatcher.h"

#include <pk_kernel/include/kr_log_listeners_file.h>
#include <pk_kernel/include/kr_string.h>

//----------------------------------------------------------------------------

namespace AAePk
{
	struct SAttributeSamplerDesc;
}

__AEGP_PK_BEGIN

PK_DECLARE_REFPTRCLASS(LogListenerFile);

//----------------------------------------------------------------------------

struct SResourceBakeConfig
{
	bool	m_StraightCopy = false;
	bool	m_IsAnimTrack  = false;
	bool	m_IsSkeletalAnim = false;
};

//----------------------------------------------------------------------------

class CVaultHandler
{
public:
	CVaultHandler();
	~CVaultHandler();

	bool					InitializeIFN();
	bool					ShutdownIFN();
	//func ptr callback for the file watcher
	static void				FileAdded(const CString &path);
	static void				FileRemoved(const CString &path);
	static void				FileChanged(const CString &path);
	static void				FileChangedRelativePath(const CString &path);
	static void				FileRenamed(const CString &oldPath, const CString &newPath);

	bool					IsBakedAssetLatestVersion(const CString &srcPath, const CString &dstPath);
	bool					LoadEffectIntoVault(const CString &packPath, CString &effectPath, const CString &pkprojPath, bool &refresh);
	CString					ImportResource(const CString resourcePath);

	CString					CopyResource(const CString resourcePath);

	CString					BakeVectorField(const CString resourcePath, const CString targetPath, const SResourceBakeConfig &config);
	CString					BakeMesh(const CString resourcePath, const CString targetPath, const SResourceBakeConfig &config);
	CString					BakeResource(const CString resourcePath, const SResourceBakeConfig &config);

	PFilePack				GetVaultPackFromPath(CString path);

	const CString			VaultPathRoot() const { return m_VaultPathRoot; };
	const CString			VaultPathAssets() const { return m_VaultPathAssets; };
	const CString			VaultPathCache() const { return m_VaultPathCache; };
	const CString			VaultPathLog() const { return m_VaultPathLogs; };
	PFilePack				InternalPack() const { return m_InternalPack; };
private:

	bool					_SetupVault();
private:

	bool						m_Initialized = false;

	PFileWatcher				m_FileWatcher = null;

	PLogListenerFile			m_AELogFileListener = null;

	PFilePack					m_InternalPack = null;

	CString						m_VaultPathRoot = "";
	CString						m_VaultPathAssets = "";
	CString						m_VaultPathCache = "";
	CString						m_VaultPathLogs = "";

	static const char			*k_VaultFolderMainName;
	static const char			*k_VaultFolderAssetsName;
	static const char			*k_VaultFolderCacheName;
	static const char			*k_VaultFolderLogsName;
};

//----------------------------------------------------------------------------

__AEGP_PK_END

#endif
