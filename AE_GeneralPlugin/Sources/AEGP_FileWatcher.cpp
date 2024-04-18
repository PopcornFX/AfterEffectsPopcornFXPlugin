//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"
#include "AEGP_FileWatcher.h"

__AEGP_PK_BEGIN
//----------------------------------------------------------------------------

bool	CFileWatcher::CreateWatcherIFN()
{
	if (m_FileWatcher == null)
	{
		m_FileWatcher = CFileSystemWatcher::NewWatcher();
		if (!PK_VERIFY(m_FileWatcher != null))
		{
			CLog::Log(PK_ERROR, "Could not create the file watcher");
			return false;
		}
	}
	return true;
}

//----------------------------------------------------------------------------

void	CFileWatcher::RemoveWatchIFN()
{
	if (m_FileWatcher != null)
		m_FileWatcher->RemoveWatch(m_PathToWatch);
}

//----------------------------------------------------------------------------

CFileWatcher::CFileWatcher()
{
}

//----------------------------------------------------------------------------

CFileWatcher::~CFileWatcher()
{
	RemoveWatchIFN();
	m_FileWatcher = null;
}

//----------------------------------------------------------------------------

void	CFileWatcher::PauseFileWatcher()
{
	RemoveWatchIFN();
}

//----------------------------------------------------------------------------

void	CFileWatcher::RestartFileWatcher()
{
	SetWatchPack(m_PathToWatch);
}

//----------------------------------------------------------------------------

bool	CFileWatcher::SetWatchPack(const CString &pathToWatch)
{
	CString	purifiedPath = pathToWatch;
	CFilePath::Purify(purifiedPath);
	purifiedPath.AppendSlash();

	RemoveWatchIFN();
	CreateWatcherIFN();

	m_PathToWatch = pathToWatch;
	m_FileWatcher->AddWatch(m_PathToWatch);
	return true;
}

//----------------------------------------------------------------------------

void	CFileWatcher::SetNotifierAdd(void(*callback)(const CString &filePath))
{
	CreateWatcherIFN();
	m_FileWatcher->m_NotifierAdd.Clear();
	m_FileWatcher->m_NotifierAdd += callback;
}

//----------------------------------------------------------------------------

void	CFileWatcher::SetNotifierRemove(void(*callback)(const CString &filePath))
{
	CreateWatcherIFN();
	m_FileWatcher->m_NotifierRemove.Clear();
	m_FileWatcher->m_NotifierRemove += callback;
}

//----------------------------------------------------------------------------

void	CFileWatcher::SetNotifierModify(void(*callback)(const CString &filePath))
{
	CreateWatcherIFN();
	m_FileWatcher->m_NotifierModify.Clear();
	m_FileWatcher->m_NotifierModify += callback;
}

//----------------------------------------------------------------------------

void	CFileWatcher::SetNotifierRename(void(*callback)(const CString &oldFilePath, const CString &newFilePath))
{
	CreateWatcherIFN();
	m_FileWatcher->m_NotifierRename.Clear();
	m_FileWatcher->m_NotifierRename += callback;
}

//----------------------------------------------------------------------------
__AEGP_PK_END
