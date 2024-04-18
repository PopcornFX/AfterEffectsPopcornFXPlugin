//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"
#include "AEGP_AssetBaker.h"

#include "AEGP_World.h"

#include "AEGP_PopcornFXPlugins.h"

#include "AEGP_PackExplorer.h"
#include "AEGP_FileWatcher.h"
#include "pk_kernel/include/kr_log_listeners_file.h"

//Baking
#include "pk_imaging/include/im_resource.h"
#include "pk_geometrics/include/ge_mesh_resource_handler.h"
#include "pk_geometrics/include/ge_rectangle_list.h"
#include "pk_particles/include/ps_font_metrics_resource.h"
#include "pk_particles/include/ps_vectorfield_resource.h"
#include "pk_base_object/include/hbo_context.h"

#include "PK-AssetBakerLib/AssetBaker_Oven.h"
#include "PK-AssetBakerLib/AssetBaker_Oven_HBO.h"
#include "PK-AssetBakerLib/AssetBaker_Oven_Mesh.h"
#include "PK-AssetBakerLib/AssetBaker_Oven_VectorField.h"
#include "PK-AssetBakerLib/AssetBaker_Oven_Texture.h"
#include "PK-AssetBakerLib/AssetBaker_Oven_StraightCopy.h"

#include "AEGP_Log.h"

__AEGP_PK_BEGIN

//----------------------------------------------------------------------------

static const CStringView	kPopcornProjectExtension = "pkproj";

//----------------------------------------------------------------------------

HBO_CLASS_DEFINITION_BEGIN(CEditorAssetEffect)
.HBO_FIELD_DEFINITION(StartCameraPosition)
[
	HBO::Properties::DefaultValue(CFloat3(0.0f, 0.0f, 5.0f)),
	HBO::Properties::AlwaysSerialize(),
	HBO::Properties::Caracs(HBO::FieldCaracs::Caracs_3DCoordinateDistance),
	HBO::Properties::Description("Initial position of the camera when opening the effect")
]
.HBO_FIELD_DEFINITION(StartCameraOrientation)
[
	HBO::Properties::DefaultValue(CFloat3::ZERO),
	HBO::Properties::AlwaysSerialize(),
	HBO::Properties::Caracs(HBO::Caracs_3DCoordinate),
	HBO::Properties::Description("Initial euler orientation of the camera when opening the effect")
]
HBO_CLASS_DEFINITION_END

//----------------------------------------------------------------------------

CEditorAssetEffect::CEditorAssetEffect()
	:HBO_CONSTRUCT(CEditorAssetEffect)
{

}

//----------------------------------------------------------------------------

CEditorAssetEffect::~CEditorAssetEffect()
{

}

//----------------------------------------------------------------------------

void CEditorAssetEffect::CopyFrom(CEditorAssetEffect *other)
{
	SetStartCameraPosition(other->StartCameraPosition());
	SetStartCameraOrientation(other->StartCameraOrientation());
}

//----------------------------------------------------------------------------

CProjectSettingsFinder::CProjectSettingsFinder(const CString &rootDir, IFileSystem *controller)
	: CFileDirectoryWalker(rootDir, IgnoreVirtualFS, controller)
{
	SetFilePathValidator(ProjectSettingsPathValidator);
}

//----------------------------------------------------------------------------

void	CProjectSettingsFinder::FileNotifier(const CFilePack *pack, const char *fullPath, u32 fileFirstCharPos)
{
	(void)pack;
	(void)fileFirstCharPos;

	PK_ASSERT(CFilePath::IsPure(fullPath));
	PK_ASSERT(kPopcornProjectExtension == CFilePath::ExtractExtension(fullPath));

	if (!PK_VERIFY(m_ProjectSettingsPath == null))
	{
		CLog::Log(PK_ERROR, "Multiple pkproj found in RootDir (\"%s\").", fullPath);
	}
	else
	{
		m_ProjectSettingsPath = fullPath;
	}
}

//----------------------------------------------------------------------------

bool	CProjectSettingsFinder::DirectoryNotifier(const CFilePack *pack, const char *fullPath, u32 directoryFirstCharPos)
{
	(void)pack;
	(void)fullPath;
	(void)directoryFirstCharPos;
	return true;	// Recursive search
}

//----------------------------------------------------------------------------

const CString	&CProjectSettingsFinder::ProjectSettingsPath() const
{
	return m_ProjectSettingsPath;
}

//----------------------------------------------------------------------------

bool	CProjectSettingsFinder::ProjectSettingsPathValidator(const char *filePath)
{
	return kPopcornProjectExtension == CFilePath::ExtractExtension(filePath);
}

//----------------------------------------------------------------------------
//							SBakeContext
//----------------------------------------------------------------------------

SBakeContext::SBakeContext()
	: m_BakeResourceMeshHandler(null)
	, m_BakeResourceImageHandler(null)
	, m_BakeResourceRectangleListHandler(null)
	, m_BakeResourceFontMetricsHandler(null)
	, m_BakeResourceVectorFieldHandler(null)
	, m_BakeFSController(null)
	, m_BakeResourceManager(null)
	, m_Initialized(false)
{
}

//----------------------------------------------------------------------------

SBakeContext::~SBakeContext()
{
	if (!m_Initialized)
		return;

	CCookeryLogger::Shutdown();

	if (m_BakeResourceManager != null)
	{
		PK_ASSERT(m_BakeResourceMeshHandler != null);
		PK_ASSERT(m_BakeResourceImageHandler != null);
		PK_ASSERT(m_BakeResourceVectorFieldHandler != null);
		PK_ASSERT(m_BakeResourceFontMetricsHandler != null);
		PK_ASSERT(m_BakeResourceRectangleListHandler != null);

		m_BakeResourceManager->UnregisterHandler<PopcornFX::CResourceMesh>(m_BakeResourceMeshHandler);
		m_BakeResourceManager->UnregisterHandler<PopcornFX::CImage>(m_BakeResourceImageHandler);
		m_BakeResourceManager->UnregisterHandler<PopcornFX::CRectangleList>(m_BakeResourceRectangleListHandler);
		m_BakeResourceManager->UnregisterHandler<PopcornFX::CFontMetrics>(m_BakeResourceFontMetricsHandler);
		m_BakeResourceManager->UnregisterHandler<PopcornFX::CVectorField>(m_BakeResourceVectorFieldHandler);
	}
	PK_SAFE_DELETE(m_BakeResourceMeshHandler);
	PK_SAFE_DELETE(m_BakeResourceImageHandler);
	PK_SAFE_DELETE(m_BakeResourceVectorFieldHandler);
	PK_SAFE_DELETE(m_BakeResourceFontMetricsHandler);
	PK_SAFE_DELETE(m_BakeResourceRectangleListHandler);
	PK_SAFE_DELETE(m_BakeContext);
	PK_SAFE_DELETE(m_BakeFSController);
	PK_SAFE_DELETE(m_BakeResourceManager);

	// unregister the oven's HBO bake-config classes:
	COvenBakeConfig_Audio::UnregisterHandler();
	COvenBakeConfig_StraightCopy::UnregisterHandler();
	COvenBakeConfig_Particle::UnregisterHandler();
	COvenBakeConfig_ParticleCompiler::UnregisterHandler();
	COvenBakeConfig_VectorField::UnregisterHandler();
	COvenBakeConfig_TextureAtlas::UnregisterHandler();
	COvenBakeConfig_Texture::UnregisterHandler();
	COvenBakeConfig_Mesh::UnregisterHandler();
	COvenBakeConfig_HBO::UnregisterHandler();
	COvenBakeConfig_Base::UnregisterHandler();
}

//----------------------------------------------------------------------------

bool	SBakeContext::Init()
{
	PK_ASSERT(m_BakeResourceMeshHandler == null);
	PK_ASSERT(m_BakeResourceImageHandler == null);
	PK_ASSERT(m_BakeResourceVectorFieldHandler == null);
	PK_ASSERT(m_BakeResourceFontMetricsHandler == null);
	PK_ASSERT(m_BakeResourceRectangleListHandler == null);
	PK_ASSERT(m_BakeFSController == null);
	PK_ASSERT(m_BakeResourceManager == null);

	// We do not constant fold the images because we do not want to link with all the image codecs:
	m_BakeResourceImageHandler = PK_NEW(CResourceHandlerDummy);
	m_BakeResourceVectorFieldHandler = PK_NEW(CResourceHandlerDummy);
	// Keep this updated with all PopcornFX resource types
	m_BakeResourceMeshHandler = PK_NEW(PopcornFX::CResourceHandlerMesh);
	m_BakeResourceRectangleListHandler = PK_NEW(PopcornFX::CResourceHandlerRectangleList);
	m_BakeResourceFontMetricsHandler = PK_NEW(PopcornFX::CResourceHandlerFontMetrics);

	if (!PK_VERIFY(m_BakeResourceMeshHandler != null) ||
		!PK_VERIFY(m_BakeResourceImageHandler != null) ||
		!PK_VERIFY(m_BakeResourceRectangleListHandler != null) ||
		!PK_VERIFY(m_BakeResourceFontMetricsHandler != null) ||
		!PK_VERIFY(m_BakeResourceVectorFieldHandler != null))
		return false;

	m_BakeFSController = File::NewInternalFileSystem();
	if (!PK_VERIFY(m_BakeFSController != null))
		return false;

	m_BakeResourceManager = PK_NEW(PopcornFX::CResourceManager(m_BakeFSController));
	if (!PK_VERIFY(m_BakeResourceManager != null))
		return false;
	m_BakeResourceManager->RegisterHandler<PopcornFX::CResourceMesh>(m_BakeResourceMeshHandler);
	m_BakeResourceManager->RegisterHandler<PopcornFX::CImage>(m_BakeResourceImageHandler);
	m_BakeResourceManager->RegisterHandler<PopcornFX::CRectangleList>(m_BakeResourceRectangleListHandler);
	m_BakeResourceManager->RegisterHandler<PopcornFX::CFontMetrics>(m_BakeResourceFontMetricsHandler);
	m_BakeResourceManager->RegisterHandler<PopcornFX::CVectorField>(m_BakeResourceVectorFieldHandler);

	m_BakeContext = PK_NEW(PopcornFX::HBO::CContext(m_BakeResourceManager));
	if (!PK_VERIFY(m_BakeContext != null))
		return false;

	// register the oven's HBO bake-config classes:
	COvenBakeConfig_Base::RegisterHandler();
	COvenBakeConfig_HBO::RegisterHandler();
	COvenBakeConfig_Mesh::RegisterHandler();
	COvenBakeConfig_Texture::RegisterHandler();
	COvenBakeConfig_TextureAtlas::RegisterHandler();
	COvenBakeConfig_VectorField::RegisterHandler();
	COvenBakeConfig_ParticleCompiler::RegisterHandler();
	COvenBakeConfig_Particle::RegisterHandler();
	COvenBakeConfig_StraightCopy::RegisterHandler();
	COvenBakeConfig_Audio::RegisterHandler();

	const CVaultHandler		&vault = CPopcornFXWorld::Instance().GetVaultHandler();

	const CString			vaultlogs = vault.VaultPathLog();

	if (!PK_VERIFY(CCookeryLogger::Startup(vaultlogs / "AssetBakerLogs", true)))
		return false;

	m_Initialized = true;
	return true;
}

//----------------------------------------------------------------------------

bool	SBakeContext::_RemapPath(CString &path)
{
	CString		extension = CFilePath::ExtractExtension(path.Data());

	if (extension.Compare("fbx", CaseInsensitive))
		path = CFilePath::StripExtension(path) + ".pkmm";
	if (extension.Compare("fga", CaseInsensitive))
		path = CFilePath::StripExtension(path) + ".pkvf";
	if (extension.Compare("pkfx", CaseInsensitive))
		path = CFilePath::StripExtension(path) + ".pkb";

	return true;
}

CString	SBakeContext::_RemapFX(const CString &path)
{
	CString		extension = CFilePath::ExtractExtension(path.Data());

	if (extension.Compare("pkfx", CaseInsensitive))
		return CFilePath::StripExtension(path) + ".pkb";
	return null;
}

//----------------------------------------------------------------------------

CEffectBaker::CEffectBaker()
	: m_Initialized(false)
	, m_SrcPack(null)
	, m_DstPack(null)
{
	m_BakeContext.Init();
}

//----------------------------------------------------------------------------

CEffectBaker::~CEffectBaker()
{
	Clear();
	m_Initialized = false;
}

//----------------------------------------------------------------------------

void			CEffectBaker::FileAdded(const CString &path)
{
	if (path.EndsWith(".pkfx") == true || path.EndsWith(".pkri"))
	{
		SAssetChange	effect;

		if (IsChangeRegistered(path, EAssetChangesType::Add) == true)
			return;
		CString		cleanPath = path.Extract(m_SrcPackPath.SlashAppended().Length(), path.Length());
		CFilePath::Purify(cleanPath);
		effect.m_EffectPath = cleanPath;
		effect.m_Type = EAssetChangesType::Add;

		PK_SCOPEDLOCK(m_Lock);
		m_ToBake.PushBack(effect);
	}
}

//----------------------------------------------------------------------------

void	CEffectBaker::FileRemoved(const CString &path)
{
	(void)path;
#if 0 //User isn't supposed to remove content of the vault during play ?
	if (path.EndsWith(".pkfx") == true || path.EndsWith(".pkri"))
	{
		SAssetChange	effect;
	
		if (IsChangeRegistered(path, EAssetChangesType::Remove) == true)
			return;
		CString		cleanPath = path.Extract(m_SrcPackPath.SlashAppended().Length(), path.Length());
		CFilePath::Purify(cleanPath);
		effect.m_EffectPath = cleanPath;
		effect.m_Type = EAssetChangesType::Remove;
	
		PK_SCOPEDLOCK(m_Lock);
	}
#endif
}

//----------------------------------------------------------------------------

void	CEffectBaker::FileChanged(const CString &path)
{
	if (path.EndsWith(".pkfx") == true || path.EndsWith(".pkri"))
	{
		SAssetChange	effect;

		if (IsChangeRegistered(path, EAssetChangesType::Update) == true)
			return;
		CString		cleanPath = path.Extract(m_SrcPackPath.SlashAppended().Length(), path.Length());
		CFilePath::Purify(cleanPath);

		CEffectBaker::FileChangedRelativePath(cleanPath);
	}
}

//----------------------------------------------------------------------------

void	CEffectBaker::FileChangedRelativePath(const CString &path)
{
	SAssetChange	effect;

	effect.m_EffectPath = path;
	effect.m_Type = EAssetChangesType::Update;

	PK_SCOPEDLOCK(m_Lock);
	m_ToBake.PushBack(effect);
}

//----------------------------------------------------------------------------

void	CEffectBaker::FileRenamed(const CString &oldPath, const CString &newPath)
{
	if (newPath.EndsWith(".pkfx") == true)
	{
		SAssetChange	effect;

		if (IsChangeRegistered(newPath, EAssetChangesType::Rename) == true)
			return;

		CString		cleanPath = newPath.Extract(m_SrcPackPath.SlashAppended().Length(), newPath.Length());
		CString		cleanOldPath = oldPath.Extract(m_SrcPackPath.SlashAppended().Length(), oldPath.Length());

		CFilePath::Purify(cleanPath);
		CFilePath::Purify(cleanOldPath);

		effect.m_EffectPath = cleanPath;
		effect.m_EffectPathOld = cleanOldPath;
		effect.m_Type = EAssetChangesType::Rename;

		PK_SCOPEDLOCK(m_Lock);
		m_ToBake.PushBack(effect);
	}
}

//----------------------------------------------------------------------------

void	CEffectBaker::Initialize(const CString &srcPack, const CString &dstPack, const CString &pkprojPath)
{
	m_RootDir = "";
	if (!pkprojPath.Empty())
		LoadProjectSettings(pkprojPath);

	m_SrcPackPath = srcPack / m_RootDir;
	m_DstPackPath = dstPack;

	if (m_SrcPackPath == m_DstPackPath)
	{
		CAELog::TryLogErrorWindows("Bake Error: Trying to bake into the source pack, aborting");
		return;
	}
	CLog::Log(PK_INFO, "Setting up cookery with source pack: \"%s\" and destination path: \"%s\"", m_SrcPackPath.Data(), m_DstPackPath.Data());
	m_BakeContext.m_BakeFSController->UnmountAllPacks();

	m_SrcPack = m_BakeContext.m_BakeFSController->MountPack(m_SrcPackPath);

	// Initialize the cookery:
	if (!m_Initialized)
	{
		Mem::Reinit(m_Cookery);

		m_Cookery.SetHBOContext(m_BakeContext.m_BakeContext);
		if (!m_Cookery.TurnOn())
		{
			CLog::Log(PK_WARN, "Couldn't initialize the cookery, TurnOn Failed.");
			return;
		}

		const CGuid	ovenIdHBO = m_Cookery.RegisterOven(PK_NEW(COven_HBO));
		const CGuid	ovenIdMesh = m_Cookery.RegisterOven(PK_NEW(COven_Mesh));
		const CGuid	ovenIdTexture = m_Cookery.RegisterOven(PK_NEW(COven_Texture));
		const CGuid	ovenIdTextureAtlas = m_Cookery.RegisterOven(PK_NEW(COven_TextureAtlas));
		const CGuid	ovenIdVectorField = m_Cookery.RegisterOven(PK_NEW(COven_VectorField));

		COven_Particle	*ovenParticle = PK_NEW(COven_Particle);
		ovenParticle->SetExternalPathRemapper(FastDelegate<bool(CString &)>(SBakeContext::_RemapPath));
		const CGuid	ovenIdParticle = m_Cookery.RegisterOven(ovenParticle);

		const CGuid	ovenIdStraightCopy = m_Cookery.RegisterOven(PK_NEW(COven_StraightCopy));
		const CGuid	ovenIdAudio = m_Cookery.RegisterOven(PK_NEW(COven_Audio));

		if (!ovenIdHBO.Valid() || !ovenIdMesh.Valid() || !ovenIdTexture.Valid() || !ovenIdTextureAtlas.Valid() ||
			!ovenIdVectorField.Valid() || !ovenIdParticle.Valid() || !ovenIdStraightCopy.Valid() ||
			!ovenIdAudio.Valid())
		{
			CLog::Log(PK_WARN, "Couldn't initialize the cookery, RegisterOven Failed.");
			return;
		}

		m_Cookery.MapOven("pkri", ovenIdStraightCopy);		// Editor Material
		m_Cookery.MapOven("pkma", ovenIdStraightCopy);		// Editor Material
		// map all known extensions to the appropriate oven:
		m_Cookery.MapOven("fbx", ovenIdMesh);				// FBX mesh
		m_Cookery.MapOven("pkmm", ovenIdMesh);				// PopcornFX multi-mesh
		m_Cookery.MapOven("dds", ovenIdTexture);			// dds image
		m_Cookery.MapOven("png", ovenIdTexture);			// png image
		m_Cookery.MapOven("jpg", ovenIdTexture);			// jpg image
		m_Cookery.MapOven("jpeg", ovenIdTexture);			// jpg image
		m_Cookery.MapOven("tga", ovenIdTexture);			// tga image
		m_Cookery.MapOven("tif", ovenIdTexture);			// tiff image
		m_Cookery.MapOven("tiff", ovenIdTexture);			// tiff image
		m_Cookery.MapOven("pkm", ovenIdTexture);			// pkm image
		m_Cookery.MapOven("pvr", ovenIdTexture);			// pvrtc image
		//m_Cookery.MapOven("exr", ovenIdTexture);			// exr image ------------- Collide with FBX
		m_Cookery.MapOven("hdr", ovenIdTexture);			// hdr image
		m_Cookery.MapOven("txt", ovenIdStraightCopy);		// misc
		m_Cookery.MapOven("fga", ovenIdVectorField);		// FGA vector-field
		m_Cookery.MapOven("pkfm", ovenIdStraightCopy);		// PopcornFX font
		m_Cookery.MapOven("pkvf", ovenIdStraightCopy);		// PopcornFX vector-field
		m_Cookery.MapOven("pkat", ovenIdTextureAtlas);		// PopcornFX atlas definition
		m_Cookery.MapOven("pksc", ovenIdStraightCopy);		// PopcornFX simulation cache
		m_Cookery.MapOven("pkbo", ovenIdHBO);				// PopcornFX base object
		m_Cookery.MapOven("pkan", ovenIdHBO);				// PopcornFX Animation
		m_Cookery.MapOven("pksa", ovenIdHBO);				// PopcornFX Skeletal Animation
		m_Cookery.MapOven("mp3", ovenIdAudio);				// mp3 sound
		m_Cookery.MapOven("wav", ovenIdAudio);				// wav sound
		m_Cookery.MapOven("ogg", ovenIdAudio);				// ogg sound
		m_Cookery.MapOven("pkfx", ovenIdParticle, FastDelegate<CString(const CString &)>(SBakeContext::_RemapFX));			// PopcornFX Effect

		m_Cookery.AddOvenFlags(PopcornFX::COven::Flags_BakeMemoryVersion);
		AEGPPk::CPopcornFXWorld	&world = AEGPPk::CPopcornFXWorld::Instance();
		CString					installPath = world.GetPluginInstallationPath();
		CString					SrcPackPath = srcPack.Replace('/', '\\');

		PopcornFX::PBaseObjectFile	configFile = m_Cookery.m_BaseConfigFile;
		PK_FOREACH(it, configFile->ObjectList())
		{
			PopcornFX::COvenBakeConfig_Particle	*config = PopcornFX::HBO::Cast<PopcornFX::COvenBakeConfig_Particle>(*it);
			if (config != null)
			{
				config->SetCompile(true);
				config->SetSourceConfig(Bake_NoSource);
				config->SetRemoveEditorNodes(true);
				config->SetBakeMode(PopcornFX::COvenBakeConfig_HBO::Bake_SaveAsBinary);

				COvenBakeConfig_Particle::_TypeOfBackendCompilers	backendCompilers;

#if (PK_COMPILER_BUILD_COMPILER_D3D12 != 0)
				if (world.GetRenderApi() == RHI::GApi_D3D12)
				{
					//BakeBytecodeCommandLine_Windows = "\"{DXC}\" -T cs_6_0 -E main -WX -Ges -O3 -nologo -all_resources_bound -Fo \"$(TargetPath)\" \"$(InputPath)\"";
					const CString	cmdLine_D3D12 = "\"" + installPath + "fxc.exe\"" + " -T cs_5_1 -E main -WX -Ges -O3 -all_resources_bound -Fo \"$(TargetPath)\" \"$(InputPath)\"";
					POvenBakeConfig_ParticleCompiler	backendCompiler = m_Cookery.HBOContext()->NewObject<COvenBakeConfig_ParticleCompiler>(configFile.Get());
					if (!PK_VERIFY(backendCompiler != null) ||
						!PK_VERIFY(backendCompilers.PushBack(backendCompiler).Valid()))
						return;
					backendCompiler->SetTarget(BackendTarget_D3D12);
					backendCompiler->SetBakeBytecodeCommandLine_Windows(cmdLine_D3D12);
				}
#endif
#if (PK_COMPILER_BUILD_COMPILER_D3D11 != 0)
				if (world.GetRenderApi() == RHI::GApi_D3D11)
				{
					const CString	cmdLine_D3D11 = "\"" + installPath + "fxc.exe\"" + " -T cs_5_0 -E main -WX -Ges -O3 -Fo \"$(TargetPath)\" \"$(InputPath)\"";
					POvenBakeConfig_ParticleCompiler	backendCompiler = m_Cookery.HBOContext()->NewObject<COvenBakeConfig_ParticleCompiler>(configFile.Get());
					if (!PK_VERIFY(backendCompiler != null) ||
						!PK_VERIFY(backendCompilers.PushBack(backendCompiler).Valid()))
						return;
					backendCompiler->SetTarget(BackendTarget_D3D11);
					backendCompiler->SetBakeBytecodeCommandLine_Windows(cmdLine_D3D11);
				}
#endif
				config->SetBackendCompilers(backendCompilers);

				config->SetCompilerSwitches("--determinism");
				config->SetCommandLine_Windows(	"\"" + installPath + "PK-ShaderTool_r.exe\" -v -k false -O \"$(TargetPath)\"" + " -P \"" + SrcPackPath + "\" " +
												"-api d3d "		+ "-c \"\\\"" + installPath + "fxc.exe\\\" -T ##ShortStage##_5_0 ##InputPath## -Fo ##OutputPath## -nologo -O3\" \"$(InputPath)\"");
				config->SetCommandLine_MacOsX(	"\"" + installPath + "AE_GeneralPlugin.plugin/Contents/MacOs/PK-ShaderTool_r\" -v -k false -O \"$(TargetPath)\"" + " -P \"" + SrcPackPath + "\" " +
												"-api metal -c \"xcrun -sdk macosx metal -mmacosx-version-min=10.14 -std=macos-metal2.1 ##InputPath## -o ##OutputPath##\" \"$(InputPath)\"");
				continue;
			}
			PopcornFX::COvenBakeConfig_Base	*configBase = PopcornFX::HBO::Cast<PopcornFX::COvenBakeConfig_Base>(*it);
			if (configBase != null)
				continue;
		}
		m_Initialized = true;
	}
	const PopcornFX::SBakeTarget	defaultTarget("AfterEffect_Generic", m_DstPackPath);

	m_Cookery.m_DstPackPaths.PushBack(defaultTarget);

}

//----------------------------------------------------------------------------

void	CEffectBaker::Clear()
{
	PK_SCOPEDLOCK(m_Lock);

	m_BakeContext.m_BakeFSController->UnmountAllPacks();
}

//----------------------------------------------------------------------------

void	CEffectBaker::Lock()
{
	m_Lock.Lock();
}

//----------------------------------------------------------------------------

void	CEffectBaker::Unlock()
{
	m_Lock.Unlock();
}

//----------------------------------------------------------------------------

void	CEffectBaker::CancelAllFileChanges()
{
	m_ToBake.Clear();
}

//----------------------------------------------------------------------------

int		CEffectBaker::PopFileChanges()
{
	if (m_ToBake.Count() != 0)
	{
		SAssetChange effect = m_ToBake.Pop();

		BakeAssetOrAddToRetryStack(effect);
	}
	return m_ToBake.Count();
}

//----------------------------------------------------------------------------

bool	CEffectBaker::IsChangeRegistered(const CString &path, EAssetChangesType type)
{
	for (u32 i = 0; i < m_ToBake.Count(); ++i)
	{
		if (m_ToBake[i].m_Type == type && path.Compare(m_ToBake[i].m_EffectPath))
			return true;
	}
	return false;
}

//----------------------------------------------------------------------------

void	CEffectBaker::ReimportAssets(TArray<CString> &paths, bool importPkri /*=true*/)
{
	for (u32 i = 0; i < paths.Count(); ++i)
	{
		FileChangedRelativePath(paths[i]);
	}
	if (importPkri)
		FileChangedRelativePath(m_LibraryDir + "/PopcornFXCore/Materials/Interface/Editor.pkri");
}

//----------------------------------------------------------------------------

void	CEffectBaker::ReimportAllAssets(bool refresh)
{
	(void)refresh;

	CPackExplorer				packExplorer(m_SrcPack->Path(), m_BakeContext.m_BakeFSController);
	packExplorer.Explore();

	TMemoryView<const CString>	path = packExplorer.EffectPaths();
	
	for (u32 i = 0; i < path.Count(); ++i)
	{
		FileChangedRelativePath(path[i]);
	}
}

//----------------------------------------------------------------------------

void	CEffectBaker::LoadProjectSettings(const CString &pkprojPath)
{
	CString			projectSettingsFilePath;
	HBO::CContext	context;
	if (pkprojPath.Empty())
	{
		CProjectSettingsFinder	finder(m_SrcPack->Path(), m_BakeContext.m_BakeFSController);

		finder.Walk();

		projectSettingsFilePath = finder.ProjectSettingsPath();
	}
	else
		projectSettingsFilePath = pkprojPath;

	if (projectSettingsFilePath.Empty() ||
		!m_BakeContext.m_BakeFSController->Exists(projectSettingsFilePath, true))
	{
		CLog::Log(PK_ERROR, "Could not find Project Settings in pack \"%s\"", m_SrcPack->Path().Data());
		return;
	}

	const CString			fileBuffer = m_BakeContext.m_BakeFSController->BufferizeToString(projectSettingsFilePath, true);
	if (fileBuffer == null)
	{
		CLog::Log(PK_ERROR, "Failed to read project settings file at \"%s\"", projectSettingsFilePath.Data());
		return;
	}

	CConstMemoryStream		stream(fileBuffer.Data(), fileBuffer.Length());
	PProjectSettings		projectSettings = CProjectSettings::LoadFromStream(stream, &context);
	if (projectSettings == null)
	{
		CLog::Log(PK_ERROR, "Failed to load project settings file at \"%s\"", projectSettingsFilePath.Data());
		return;
	}

	PProjectSettingsGeneral	general = projectSettings->General();
	m_RootDir = general->RootDir();
	m_LibraryDir = general->LibraryDir();
	m_EditorCacheDir = general->EditorCacheDir();
	m_PresetsDir = general->PresetsDir();
}

//----------------------------------------------------------------------------

void	CEffectBaker::GetAllAssetPath()
{
	CPackExplorer					packExplorer(m_SrcPack->Path(), m_BakeContext.m_BakeFSController);
	SDirectoryValidator				directoryValidator(m_LibraryDir, m_EditorCacheDir, m_PresetsDir);
	CPackExplorer::PathValidator	directoryPathValidator = CPackExplorer::PathValidator(&(directoryValidator), &SDirectoryValidator::cmp);

	packExplorer.SetDirectoryPathValidator(directoryPathValidator);
	packExplorer.Explore();

	TMemoryView<const CString>		paths = packExplorer.EffectPaths();
	TArray<const char*>				pathChar(paths.Count());
	for (u32 i = 0; i < paths.Count(); ++i)
	{
		pathChar[i] = paths[i].Data();
	}
}

//----------------------------------------------------------------------------

bool	CEffectBaker::BakeAssetOrAddToRetryStack(SAssetChange &assetInfo)
{
	CLog::Log(PK_INFO, "Baking asset '%s' and its dependencies...", assetInfo.m_EffectPath.Data());
	if (!BakeAsset(assetInfo.m_EffectPath))
	{
		CString errorLog = "Bake Error: " + assetInfo.m_EffectPath + " failed to bake";
		CAELog::TryLogErrorWindows(errorLog);
		return false;
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CEffectBaker::LoadAndBrowseEffect(const CString &path)
{
	PBaseObjectFile		file = m_BakeContext.m_BakeContext->LoadFile(path);

	if (!PK_VERIFY(file != null))
	{
		CLog::Log(PK_ERROR, "Could not load the effect file for '%s'", path.Data());
		return false;
	}

	PEditorAssetEffect		editorAssetEffect = file->FindFirstOf<CEditorAssetEffect>();
	if (!PK_VERIFY(editorAssetEffect != null))
	{
		CLog::Log(PK_ERROR, "Could not find the CEditorAssetEffect object in file for '%s'", path.Data());
		return false;
	}
	CString				pkboPath = CFilePath::StripExtension(path) + ".pkbo";
	IFileSystem			*fileSystem = File::DefaultFileSystem();
	PFilePack			dstPack = fileSystem->MountPack(m_DstPackPath);

	fileSystem->SetForcedFileCreationPack(dstPack);
	PBaseObjectFile		pkboFile = HBO::g_Context->LoadFile_AndCreateIFN_Pure(pkboPath.View(), false);


	PEditorAssetEffect	editorProp = pkboFile->FindFirstOf<CEditorAssetEffect>();
	if (editorProp == null)
	{
		editorProp = HBO::g_Context->NewObject<CEditorAssetEffect>(pkboFile.Get());
	}
	editorProp->CopyFrom(editorAssetEffect.Get());
		
	if (!PK_VERIFY(HBO::g_Context->WriteFile(pkboFile.Get(), pkboFile->Path())))
	{
		return false;
	}
	fileSystem->SetForcedFileCreationPack(null);
	editorAssetEffect = null;
	file->Unload();
	return true;
}

//----------------------------------------------------------------------------

bool	CEffectBaker::BakeAsset(const CString &path, bool bakeDependencies)
{
	if (m_BakedPaths.Contains(path))
		return true;

	m_BakedPaths.PushBack(path);

	if (bakeDependencies)
	{
		TArray<CString>	dependencies;

		m_Cookery.GetAssetDependencies(path, dependencies);
		for (u32 i = 0; i < dependencies.Count(); i++)
		{
			const CString		&dependency = (dependencies[i]);
			if (dependency == null ||
				dependency.EndsWith(".pkfx") ||
				dependency.EndsWith(".pkbo"))
				continue;
			if (BakeAsset(dependency, true) == false)
			{
				CLog::Log(PK_ERROR, "Asset Dependency of '%s' failed baking:	'%s'", path.Data(), dependency.Data());
				return false;
			}
		}
	}
	PopcornFX::CMessageStream	bakerErrors;
	if (!m_Cookery.BakeAsset(path, m_Cookery.m_BaseConfigFile, bakerErrors))
	{
		CLog::Log(PK_ERROR, "Couldn't bake effect '%s':", path.Data());
		bakerErrors.Log();
		return false;
	}
	if (path.EndsWith(".pkfx"))
	{
		LoadAndBrowseEffect(path);
	}
	return true;
}

//----------------------------------------------------------------------------

__AEGP_PK_END
