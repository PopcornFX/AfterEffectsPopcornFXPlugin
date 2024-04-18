//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"

#include "AEGP_PackExplorer.h"

__AEGP_PK_BEGIN

//----------------------------------------------------------------------------

CPackExplorer::CPackExplorer(const CString &pack, IFileSystem *fileSystem)
	: CFileDirectoryWalker(pack, IgnoreVirtualFS, fileSystem)
	, m_Pack(pack)
{
}

//----------------------------------------------------------------------------

CPackExplorer::~CPackExplorer()
{
	m_Pack = null;
}

//----------------------------------------------------------------------------

void	CPackExplorer::Explore()
{
	if (!m_Pack.Empty())
	{
		Walk();
	}
}

//----------------------------------------------------------------------------

bool	CPackExplorer::DirectoryNotifier(const CFilePack *pack, const char *fullPath, u32 directoryFirstCharPos)
{
	(void)directoryFirstCharPos;
	(void)pack;

	const CString	filename = CFilePath::ExtractFilename(fullPath);

	// go full recursive unless a hidden directory somehow ended up in the pack
	return filename[0] != '.';
}

//----------------------------------------------------------------------------

void	CPackExplorer::FileNotifier(const CFilePack *pack, const char *fullPath, u32 fileFirstCharPos)
{
	(void)fileFirstCharPos;
	(void)pack;

	const char	*extension = CFilePath::ExtractExtension(fullPath);

	// add the effect to the list
	if (extension != null && !strcasecmp(extension, "pkfx"))
	{
		const CString	effectPath = CFilePath::Relativize(m_Pack.Data(), fullPath);

		m_EffectPaths.PushBack(effectPath);
	}
}

//----------------------------------------------------------------------------

CBakedPackExplorer::CBakedPackExplorer(const CString &pack, IFileSystem *fileSystem)
	: CFileDirectoryWalker(pack, IgnoreVirtualFS, fileSystem)
	, m_Pack(pack)
{
}

//----------------------------------------------------------------------------

CBakedPackExplorer::~CBakedPackExplorer()
{
	m_Pack = null;
}

//----------------------------------------------------------------------------

void	CBakedPackExplorer::Explore()
{
	if (!m_Pack.Empty())
	{
		Walk();
	}
}

//----------------------------------------------------------------------------

bool	CBakedPackExplorer::DirectoryNotifier(const CFilePack *pack, const char *fullPath, u32 directoryFirstCharPos)
{
	(void)directoryFirstCharPos;
	(void)pack;

	const CString	filename = CFilePath::ExtractFilename(fullPath);

	// go full recursive unless a hidden directory somehow ended up in the pack
	return filename[0] != '.';
}

//----------------------------------------------------------------------------

void	CBakedPackExplorer::FileNotifier(const CFilePack *pack, const char *fullPath, u32 fileFirstCharPos)
{
	(void)fileFirstCharPos;
	(void)pack;

	const char	*extension = CFilePath::ExtractExtension(fullPath);

	// add the effect to the list
	if (extension != null && !strcasecmp(extension, "pkb"))
	{
		const CString	effectPath = CFilePath::Relativize(m_Pack.Data(), fullPath);

		m_EffectPaths.PushBack(effectPath);
	}
}

//----------------------------------------------------------------------------

__AEGP_PK_END
