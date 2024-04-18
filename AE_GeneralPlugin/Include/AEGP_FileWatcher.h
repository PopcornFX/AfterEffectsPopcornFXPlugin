//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef	__AEGP_FILE_WATCHER_H__
#define	__AEGP_FILE_WATCHER_H__

#include "AEGP_Define.h"

#include <pk_kernel/include/kr_file_watcher.h>

__AEGP_PK_BEGIN
//----------------------------------------------------------------------------

class	CFileWatcher : public CRefCountedObject
{
private:
	PFileSystemWatcher			m_FileWatcher;
	CString						m_PathToWatch;

	bool	CreateWatcherIFN();
	void	RemoveWatchIFN();

public:
	CFileWatcher();
	~CFileWatcher();

	void	PauseFileWatcher();
	void	RestartFileWatcher();

	bool	SetWatchPack(const CString &pathToWatch);
	void	SetNotifierAdd(void(*callback)(const CString &filePath));
	void	SetNotifierRemove(void(*callback)(const CString &filePath));
	void	SetNotifierModify(void(*callback)(const CString &filePath));
	void	SetNotifierRename(void(*callback)(const CString &oldFilePath, const CString &newFilePath));
};
PK_DECLARE_REFPTRCLASS(FileWatcher);

//----------------------------------------------------------------------------
__AEGP_PK_END

#endif
