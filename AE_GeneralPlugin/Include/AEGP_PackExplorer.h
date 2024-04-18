//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef	__FX_PACK_EXPLORER_H__
#define	__FX_PACK_EXPLORER_H__

#include "AEGP_Define.h"

#include <pk_kernel/include/kr_file_directory_walker.h>

__AEGP_PK_BEGIN

//----------------------------------------------------------------------------

class	CPackExplorer : public CFileDirectoryWalker, public CRefCountedObject
{
protected:
	CString				m_Pack;
	TArray<CString>		m_EffectPaths;

	virtual bool		DirectoryNotifier(const CFilePack *pack, const char *fullPath, u32 directoryFirstCharPos) override;
	virtual void		FileNotifier(const CFilePack *pack, const char *fullPath, u32 fileFirstCharPos) override;

public:
				CPackExplorer(const CString &pack, IFileSystem *fileSystem = null);
	virtual		~CPackExplorer();

	void						Explore();	// will start scanning

	const CString				&Pack() const { return m_Pack; }
	TMemoryView<const CString>	EffectPaths() const { return m_EffectPaths; }
};

//----------------------------------------------------------------------------

class	CBakedPackExplorer : public CFileDirectoryWalker, public CRefCountedObject
{
protected:
	CString				m_Pack;
	TArray<CString>		m_EffectPaths;

	virtual bool		DirectoryNotifier(const CFilePack *pack, const char *fullPath, u32 directoryFirstCharPos) override;
	virtual void		FileNotifier(const CFilePack *pack, const char *fullPath, u32 fileFirstCharPos) override;

public:
	CBakedPackExplorer(const CString &pack, IFileSystem *fileSystem = null);
	virtual		~CBakedPackExplorer();

	void						Explore();	// will start scanning

	const CString				&Pack() const { return m_Pack; }
	TMemoryView<const CString>	EffectPaths() const { return m_EffectPaths; }
};

//----------------------------------------------------------------------------

__AEGP_PK_END

#endif	// __FX_PACK_EXPLORER_H__
