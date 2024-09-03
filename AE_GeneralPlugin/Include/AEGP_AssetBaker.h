//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef	__AEGP_ASSET_BAKER_H__
#define	__AEGP_ASSET_BAKER_H__

#include "AEGP_Define.h"

#include "pk_base_object/include/hbo_object.h"
#include "pk_kernel/include/kr_threads.h"
#include "pk_kernel/include/kr_timers.h"
#include "pk_kernel/include/kr_resources.h"
#include "pk_kernel/include/kr_file_directory_walker.h"
#include <PK-AssetBakerLib/AssetBaker_Cookery.h>

//----------------------------------------------------------------------------

__PK_API_BEGIN

class	CResourceHandlerMesh;
class	CResourceHandlerImage;
class	CResourceHandlerRectangleList;
class	CResourceHandlerFontMetrics;
class	CResourceHandlerVectorField;
class	CResourceManager;
class	CCookery;
namespace	HBO {
	class	CContext;
}

__PK_API_END

__AEGP_PK_BEGIN

//----------------------------------------------------------------------------

class	CResourceHandlerDummy : public IResourceHandler
{
public:
	CResourceHandlerDummy() { }
	virtual ~CResourceHandlerDummy() { }
	virtual void	*Load(const CResourceManager *resourceManager, u32 resourceTypeID, const CString &resourcePath, bool pathNotVirtual, const SResourceLoadCtl &loadCtl, CMessageStream &loadReport, SAsyncLoadStatus *asyncLoadStatus) override
	{
		(void)resourceManager; (void)resourceTypeID; (void)resourcePath; (void)pathNotVirtual; (void)loadCtl; (void)loadReport; (void)asyncLoadStatus;
		return null;
	}
	virtual void	*Load(const CResourceManager *resourceManager, u32 resourceTypeID, const CFilePackPath &resourcePath, const SResourceLoadCtl &loadCtl, CMessageStream &loadReport, SAsyncLoadStatus *asyncLoadStatus) override
	{
		(void)resourceManager; (void)resourceTypeID; (void)resourcePath; (void)loadCtl; (void)loadReport; (void)asyncLoadStatus;
		return null;
	}

	virtual void	Unload(const CResourceManager *, u32, void *) override { }
	virtual void	AppendDependencies(const CResourceManager *, u32, void *, TArray<CString> &) const override { }
	virtual void	AppendDependencies(const CResourceManager *, u32, const CString &, bool, TArray<CString> &) const override { }
	virtual void	AppendDependencies(const CResourceManager *, u32, const CFilePackPath &, TArray<CString> &) const override { }
	virtual void	BroadcastResourceChanged(const CResourceManager *, const CFilePackPath &) override { }
};

class	CProjectSettingsFinder : public CFileDirectoryWalker
{
public:
	CProjectSettingsFinder(const CString &rootDir, IFileSystem *controller = null);

	virtual void	FileNotifier(const CFilePack *pack, const char *fullPath, u32 fileFirstCharPos) override;
	virtual bool	DirectoryNotifier(const CFilePack *pack, const char *fullPath, u32 directoryFirstCharPos) override;
	const CString	&ProjectSettingsPath() const;

private:
	static bool		ProjectSettingsPathValidator(const char *filePath);

private:
	CString		m_ProjectSettingsPath;
};

//----------------------------------------------------------------------------

class	SBakeContext
{
public:
	SBakeContext();
	~SBakeContext();

	IResourceHandler				*m_BakeResourceMeshHandler;
	IResourceHandler				*m_BakeResourceImageHandler;
	IResourceHandler				*m_BakeResourceRectangleListHandler;
	IResourceHandler				*m_BakeResourceFontMetricsHandler;
	IResourceHandler				*m_BakeResourceVectorFieldHandler;

	IFileSystem						*m_BakeFSController;
	CResourceManager				*m_BakeResourceManager;
	HBO::CContext					*m_BakeContext;

	bool							m_Initialized;

	bool			Init();

	static bool		_RemapPath(CString &path);
	static CString	_RemapFX(const CString &path);
};

//----------------------------------------------------------------------------

enum EAssetChangesType : int
{
	Undefined = 0,
	Add,
	Remove,
	Update,
	Rename
};

//----------------------------------------------------------------------------

class HBO_CLASS(CEditorAssetEffect), public CBaseObject
{
private:
	// Camera
	HBO_FIELD(CFloat3, StartCameraPosition);
	HBO_FIELD(CFloat3, StartCameraOrientation);
public:
	CEditorAssetEffect();
	virtual ~CEditorAssetEffect();

	void CopyFrom(CEditorAssetEffect *other);

	HBO_CLASS_DECLARATION();
};
PK_DECLARE_REFPTRCLASS(EditorAssetEffect);

//----------------------------------------------------------------------------

class	CEffectBaker : public CRefCountedObject
{
private:
	struct	SAssetChange
	{
		CString					m_EffectPath;
		CString					m_EffectPathOld;
		EAssetChangesType		m_Type;
	};

	struct SDirectoryValidator
	{
		const CStringView		m_LibraryDir;
		const CStringView		m_EditorCacheDir;
		const CStringView		m_TemplatesDir;

		bool cmp(const char *rawPath)
		{
			const CStringView path = CStringView::FromNullTerminatedString(rawPath);
			return !path.Contains(m_LibraryDir) && !path.Contains(m_EditorCacheDir) && !path.Contains(m_TemplatesDir);
		}

		SDirectoryValidator(const CString &library, const CString &editor, const CString &templat)
		:	m_LibraryDir(library)
		,	m_EditorCacheDir(editor)
		,	m_TemplatesDir(templat)
		{
		}
	};

public:
	CEffectBaker();
	~CEffectBaker();

	void				FileAdded(const CString &path);
	void				FileRemoved(const CString &path);
	void				FileChanged(const CString &path);
	void				FileChangedRelativePath(const CString &path);
	void				FileRenamed(const CString &oldPath, const CString &newPath);

	void				Initialize(const CString &srcPack, const CString &dstPack, const CString &pkprojPath);
	void				LoadProjectSettings(const CString &pkprojPath);
	void				Clear();

	void				Lock();
	void				Unlock();

	void				CancelAllFileChanges();
	int					PopFileChanges();
	bool				IsChangeRegistered(const CString &path, EAssetChangesType type);
	void				ReimportAssets(TArray<CString> &paths);
	void				ReimportAllAssets(bool refesh);
	void				GetAllAssetPath();
	const CString		&GetSourcePackRootPath() const { return m_RootDir; }
	bool				BakeAssetOrAddToRetryStack(SAssetChange &path);
	bool				LoadAndBrowseEffect(const CString &path);
	bool				BakeAsset(const CString &path, bool bakeDependencies = true);

	SBakeContext		&GetBakeContextData() { return m_BakeContext; }

	void				ClearBakedPaths() { m_BakedPaths.Clear(); }
private:

	bool						m_Initialized;

	TArray<SAssetChange>		m_ToBake;
	TArray<CString>				m_BakedPaths;
	CString						m_DstPackPath;
	CString						m_SrcPackPath;
	PFilePack					m_SrcPack;
	PFilePack					m_DstPack;

	SBakeContext				m_BakeContext;
	CCookery					m_Cookery;
	Threads::CCriticalSection	m_Lock;

	CString						m_RootDir;
	CString						m_LibraryDir;
	CString						m_EditorCacheDir;
	CString						m_PresetsDir;
};
PK_DECLARE_REFPTRCLASS(EffectBaker);

//----------------------------------------------------------------------------

__AEGP_PK_END

#endif
