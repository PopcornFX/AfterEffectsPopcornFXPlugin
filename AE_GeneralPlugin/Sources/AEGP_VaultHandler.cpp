//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"

#include "AEGP_VaultHandler.h"

#include "AEGP_FileWatcher.h"
#include "AEGP_World.h"
#include "AEGP_AEPKConversion.h"
#include "AEGP_Log.h"

#include "AEGP_AssetBaker.h"

#if defined(PK_WINDOWS)
#	include <Shlobj.h>
#endif

#include <pk_kernel/include/kr_log.h>
#include <pk_kernel/include/kr_log_listeners.h>
#include <pk_kernel/include/kr_log_listeners_file.h>

#include <pk_geometrics/include/ge_mesh.h>
#include <pk_geometrics/include/ge_mesh_resource.h>
#include <pk_particles_toolbox/include/pt_mesh_deformers_skin.h>
#include <pk_geometrics/include/ge_mesh_kdtree.h>
#include <pk_geometrics/include/ge_mesh_sampler_accel.h>
#include <pk_geometrics/include/ge_mesh_projection.h>
#include <pk_geometrics/include/ge_mesh_plugins.h>

__AEGP_PK_BEGIN
//----------------------------------------------------------------------------

const char	*CVaultHandler::k_VaultFolderMainName = "Persistant Studios/AfterEffects/Vault";
const char	*CVaultHandler::k_VaultFolderAssetsName = "Assets";
const char	*CVaultHandler::k_VaultFolderCacheName = "Cache";
const char	*CVaultHandler::k_VaultFolderLogsName = "Logs";

//----------------------------------------------------------------------------

CVaultHandler::CVaultHandler()
{
}

//----------------------------------------------------------------------------

CVaultHandler::~CVaultHandler()
{
}

//----------------------------------------------------------------------------

bool	CVaultHandler::InitializeIFN()
{
	if (m_Initialized == true)
		return true;
	m_Initialized = true;

	m_InternalPack = File::DefaultFileSystem()->MountPack(CPopcornFXWorld::Instance().GetInternalPackPath());

	if (!PK_VERIFY(_SetupVault()))
		return false;

	CString logPath = m_VaultPathLogs / "popcorn.htm";

	m_AELogFileListener = PK_NEW(CLogListenerFile(logPath.Data(), "popcorn-engine logfile");
	CLog::AddGlobalListener(m_AELogFileListener));

#if		0
	m_FileWatcher = PK_NEW(CFileWatcher());
	if (!PK_VERIFY(m_FileWatcher != null))
		return false;
	m_FileWatcher->SetNotifierAdd(&CVaultHandler::FileAdded);
	m_FileWatcher->SetNotifierRemove(&CVaultHandler::FileRemoved);
	m_FileWatcher->SetNotifierModify(&CVaultHandler::FileChanged);
	m_FileWatcher->SetNotifierRename(&CVaultHandler::FileRenamed);

	bool	watchPackSucceed = m_FileWatcher->SetWatchPack("");

	if (!watchPackSucceed)
		return false;
#endif
	return false;
}

//----------------------------------------------------------------------------

bool	CVaultHandler::ShutdownIFN()
{
	if (m_Initialized == false)
		return true;
	if (m_AELogFileListener != null)
	{
		CLog::RemoveGlobalListener(m_AELogFileListener);
		m_AELogFileListener = null;
	}

	return true;
}

//----------------------------------------------------------------------------

void	CVaultHandler::FileAdded(const CString &path)
{
	(void)path;
}

//----------------------------------------------------------------------------

void	CVaultHandler::FileRemoved(const CString &path)
{
	(void)path;
}

//----------------------------------------------------------------------------

void	CVaultHandler::FileChanged(const CString &path)
{
	(void)path;
}

//----------------------------------------------------------------------------

void	CVaultHandler::FileChangedRelativePath(const CString &path)
{
	(void)path;
}

//----------------------------------------------------------------------------

void	CVaultHandler::FileRenamed(const CString &oldPath, const CString &newPath)
{
	(void)oldPath;
	(void)newPath;
}

//----------------------------------------------------------------------------

bool	CVaultHandler::IsBakedAssetLatestVersion(const CString &srcPath, const CString &dstPath)
{
	IFileSystem	*fs = File::DefaultFileSystem();

	if (!fs->Exists(dstPath, true))
		return false;

	SFileTimes	srcTimes, dstTimes;

	fs->Timestamps(srcPath, srcTimes, true);
	fs->Timestamps(dstPath, dstTimes, true);

	if (srcTimes.m_LastWriteTime > dstTimes.m_LastWriteTime)
		return false;
	return true;
}

//----------------------------------------------------------------------------

bool	CVaultHandler::LoadEffectIntoVault(const CString &srcPackPath, CString &effectPath, const CString &pkprojPath, bool &refresh)
{
	IFileSystem	*fs = File::DefaultFileSystem();
	//Check if pack exist in cache

	PFilePack	dstPack = GetVaultPackFromPath(srcPackPath);
	CString		dstPackPath = dstPack->Path();
	CString		bakedName = CFilePath::StripExtension(effectPath) + ".pkb";
	CString		sourceName = CFilePath::StripExtension(effectPath) + ".pkfx";
	CString		sourcePath = srcPackPath / sourceName;

	if (!fs->Exists(dstPackPath, true))
	{
		if (!fs->CreateDirectoryChainIFN(dstPackPath, true))
			return false;
	}
	if (!refresh)
	{
		if (IsBakedAssetLatestVersion(sourcePath, dstPackPath / bakedName))
			return true;
	}

	TArray<CString>	effectsPath;

	CEffectBaker	baker;

	baker.Initialize(srcPackPath, dstPackPath, pkprojPath);

	CString rootDir = baker.GetSourcePackRootPath();
	if (!rootDir.Empty() && sourceName.StartsWith(rootDir))
	{
		sourceName = sourceName.Extract(rootDir.Length() + 1, sourceName.Length());
		effectPath = sourceName;
	}
	effectsPath.PushBack(sourceName);

	baker.ReimportAssets(effectsPath);
	baker.ClearBakedPaths();

	while (baker.PopFileChanges() != 0)
	{
	}
	baker.Clear();
	refresh = true;
	return true;
}

//----------------------------------------------------------------------------

CString		CVaultHandler::ImportResource(const CString resourcePath)
{
	IFileSystem		*fs = File::DefaultFileSystem();
	CString			filename = CFilePath::ExtractFilename(resourcePath);
	CString			sourcePath = resourcePath;
	CString			targetPath = m_VaultPathAssets / filename;

	if (IsBakedAssetLatestVersion(resourcePath, targetPath))
		return targetPath;
	CFilePath::Purify(sourcePath);
	if (!PK_VERIFY(fs->FileCopy(sourcePath, targetPath, true)))
		return null;
	return targetPath;
}

//----------------------------------------------------------------------------

CString		CVaultHandler::BakeVectorField(const CString resourcePath, const CString targetPath, const SResourceBakeConfig &config)
{
	(void)config;
	IFileSystem				*fs = File::DefaultFileSystem();
	CFilePackPath			filePackPath = CFilePackPath::FromPhysicalPath(resourcePath, fs);

	PFilePack				srcPack = null;
	if (filePackPath.Empty())
	{
		srcPack = fs->MountPack(CFilePath::StripFilename(resourcePath));
		filePackPath = CFilePackPath::FromPhysicalPath(resourcePath, fs);
	}
	if (!filePackPath.Empty())
	{
		TArray<CString>	effectsPath;

		effectsPath.PushBack(CFilePath::ExtractFilename(resourcePath));

		CEffectBaker	baker;

		baker.Initialize(filePackPath.Pack()->Path(), m_VaultPathAssets, "");
		baker.ReimportAssets(effectsPath, false);
		baker.ClearBakedPaths();

		while (baker.PopFileChanges() != 0)
		{
		}
		baker.Clear();
	}
	if (srcPack != null)
		fs->UnmountPack(srcPack.Get());
	return targetPath;
}

//----------------------------------------------------------------------------

CString		CVaultHandler::BakeMesh(const CString resourcePath, const CString targetPath, const SResourceBakeConfig &config)
{
	IFileSystem				*fs = File::DefaultFileSystem();
	CFilePackPath			filePackPath = CFilePackPath::FromPhysicalPath(resourcePath, fs);
	bool					unload = false;
	CString					targetExtension = ".pkmm";

	PFilePack	srcPack = null;
	if (filePackPath.Empty())
	{
		srcPack = fs->MountPack(CFilePath::StripFilename(resourcePath));
		filePackPath = CFilePackPath::FromPhysicalPath(resourcePath, fs);
		unload = true;
	}
	if (!filePackPath.Empty())
	{
		class	CMeshCodecMessageStreamBaker : public CMeshCodecMessageStream
		{
			virtual void	NotifyProgressMessage(const CString &msg) override		// called from time to time to tell what's being loaded
			{
				(void)msg;
#ifdef	PK_DEBUG
					printf("    Mesh Importer log: %s\n", msg.Data());	// only display these in debug
#endif
			};
			virtual void	NotifyError(const CString &msg) override { printf("    Mesh Importer ERROR: %s\n", msg.Data()); }
			virtual void	NotifyWarning(const CString &msg) override { printf("    Mesh Importer WARNING: %s\n", msg.Data()); }
		};
		CMessageStream					outBakeReport;
		CMeshCodecMessageStreamBaker	notifier;
		SMeshImportSettings				importSettings;

		importSettings.m_ImportGeometry = true;

		importSettings.m_ImportSkeleton = true;
		if (config.m_IsAnimTrack)
		{
			importSettings.m_ImportAnimation = true;
			importSettings.m_ImportSkeleton = false;
			targetExtension = ".pkan";
		}
		if (config.m_IsSkeletalAnim)
		{
			importSettings.m_ImportAnimation = true;
			importSettings.m_ImportSkeleton = true;
			targetExtension = ".pksa";
		}
		importSettings.m_WriteAccelSampling = true;
		importSettings.m_WriteAccelUV2PC = true;
		importSettings.m_WriteKdTree = true;
		importSettings.m_Positions = SVStreamCode::Type_F32;
		importSettings.m_Normals = SVStreamCode::Type_F32;
		importSettings.m_Tangents = SVStreamCode::Type_F32;
		importSettings.m_Texcoords = SVStreamCode::Type_F32;

		importSettings.m_RemapToZero = true;

		importSettings.m_ResourcePath = filePackPath.Path();
		importSettings.m_ResourceManager = Resource::DefaultManager();

		PMeshImportOut	outFbxImport = CResourceMesh::ImportFromFile(notifier, outBakeReport, &importSettings);

		if (outFbxImport != null)
		{
			if (!outFbxImport->Write(fs, targetPath + ".pkmm", targetPath + ".pkan", targetPath + ".pksa", outBakeReport))
				CLog::Log(PK_ERROR, "The fbx import failed");
		}
	}
	if (unload)
		fs->UnmountPack(srcPack.Get());
	return targetPath + targetExtension;
}

//----------------------------------------------------------------------------

CString		CVaultHandler::CopyResource(const CString resourcePath)
{
	IFileSystem		*fs = File::DefaultFileSystem();
	CString			filename = CFilePath::ExtractFilename(resourcePath);
	CString			extension = CFilePath::ExtractExtension(filename);
	CString			targetPath = m_VaultPathAssets / filename;;
	CString			sourcePath = resourcePath;


	if (IsBakedAssetLatestVersion(resourcePath, targetPath))
		return targetPath;
	CFilePath::Purify(sourcePath);
	if (!PK_VERIFY(fs->FileCopy(sourcePath, targetPath, true)))
		return null;
	return targetPath;
}

//----------------------------------------------------------------------------

CString		CVaultHandler::BakeResource(const CString resourcePath, const SResourceBakeConfig &config)
{
	IFileSystem		*fs = File::DefaultFileSystem();
	CString			filename = CFilePath::ExtractFilename(resourcePath);
	CString			extension = CFilePath::ExtractExtension(filename);
	CString			sourcePath = resourcePath;

	CFilePath::StripExtensionInPlace(filename);

	CString			targetPath = m_VaultPathAssets / filename;

	if (!config.m_StraightCopy && extension.Compare("fbx", CaseInsensitive))
	{
		CString		targetExt = ".pkmm";
		if (config.m_IsAnimTrack)
			targetExt = ".pkan";
		if (config.m_IsSkeletalAnim)
			targetExt = ".pksa";
		if (IsBakedAssetLatestVersion(resourcePath, targetPath + targetExt))
			return targetPath + targetExt;

		return BakeMesh(resourcePath, targetPath, config);
	}
	if (!config.m_StraightCopy && extension.Compare("fga", CaseInsensitive))
	{
		targetPath += ".pkvf";
		if (IsBakedAssetLatestVersion(resourcePath, targetPath))
			return targetPath;

		return BakeVectorField(resourcePath, targetPath, config);
	}
	else //Wildcard
	{
		targetPath += "." + extension;

		if (IsBakedAssetLatestVersion(resourcePath, targetPath))
			return targetPath;
		CFilePath::Purify(sourcePath);
		if (!PK_VERIFY(fs->FileCopy(sourcePath, targetPath, true)))
			return null;
	}
	return targetPath;
}

//----------------------------------------------------------------------------

PFilePack	CVaultHandler::GetVaultPackFromPath(CString path)
{
	PK_ASSERT(path != null);
	PK_ASSERT(!path.Empty());

	IFileSystem	*fs = File::DefaultFileSystem();

	CString packName = CFilePath::ExtractFilename(path);

	u32		pathHash = (u32)std::hash<std::string>{}(path.Data());
	CString vaultPath = m_VaultPathCache / packName + CString::Format("_%u",  pathHash);

	if (fs->Exists(vaultPath, true))
	{
		return fs->MountPack(vaultPath);
	}

	fs->CreateDirectoryChainIFN(vaultPath, true);

	return fs->MountPack(vaultPath);
}

//----------------------------------------------------------------------------

bool	CVaultHandler::_SetupVault()
{
	CString		localPathString;
#if defined(PK_WINDOWS)

	aechar_t	*userFolder = null;
	HRESULT		result = 0;

	result = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, (PWSTR*)&userFolder);
	if (FAILED(result))
		return false;
	WCharToCString(userFolder, &localPathString);
	CoTaskMemFree(static_cast<void*>(userFolder));

#elif defined(PK_MACOSX)
	const char	*homeFolder = getenv("HOME");
	if (!PK_VERIFY(homeFolder != null))
		return false;
	localPathString = CString(homeFolder) / "Library" / "Application Support";
#endif
	CFilePath::Purify(localPathString);

	IFileSystem	*fs = File::DefaultFileSystem();

	m_VaultPathRoot = localPathString + "/"+ k_VaultFolderMainName;
	m_VaultPathAssets = m_VaultPathRoot + "/" + k_VaultFolderAssetsName;
	m_VaultPathCache = m_VaultPathRoot + "/" + k_VaultFolderCacheName;
	m_VaultPathLogs = m_VaultPathRoot + "/" + k_VaultFolderLogsName;

	if (!fs->CreateDirectoryChainIFN(m_VaultPathRoot, true) ||
		!fs->CreateDirectoryChainIFN(m_VaultPathAssets, true) ||
		!fs->CreateDirectoryChainIFN(m_VaultPathCache, true) ||
		!fs->CreateDirectoryChainIFN(m_VaultPathLogs, true))
		return false;

	class CPKFolderWalker : public CFileDirectoryWalker
	{
	public:
		TArray<CString> m_Folders;

		CPKFolderWalker(const CString &rootDir)
			: CFileDirectoryWalker(rootDir, IgnoreVirtualFS)
		{
		}

		virtual bool	DirectoryNotifier(const CFilePack *, const char *fullPath, u32 ) override
		{
			m_Folders.PushBack(fullPath);
			return false;
		}

	};

	CPKFolderWalker walker(m_VaultPathCache);

	walker.Walk();

	for (u32 i = 0; i < walker.m_Folders.Count(); ++i)
	{
		fs->MountPack(walker.m_Folders[i]);
	}

	fs->MountPack(m_VaultPathAssets);
	return true;
}

//----------------------------------------------------------------------------

__AEGP_PK_END
