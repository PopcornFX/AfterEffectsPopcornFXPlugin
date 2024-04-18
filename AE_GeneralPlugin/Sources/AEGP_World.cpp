//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"

#include "AEGP_World.h"

#include "AEGP_PopcornFXPlugins.h"
#include "AEGP_Scene.h"
#include "AEGP_AEPKConversion.h"

#include "AEGP_AssetBaker.h"
#include "AEGP_Log.h"

//Suite
#include <PopcornFX_Suite.h>
#include <PopcornFX_Define.h>

#include <pk_version.h>
#include <pk_kernel/include/kr_plugins.h>
#include <pk_kernel/include/kr_static_config_flags.h>
#include <pk_kernel/include/kr_init.h>
#include <pk_kernel/include/kr_log.h>
#include <pk_kernel/include/kr_log_listeners.h>
#include <pk_kernel/include/kr_log_listeners_file.h>
#include <pk_kernel/include/kr_assert_internals.h>

//StartUp Runtime
#include <pk_base_object/include/hb_init.h>
#include <pk_engine_utils/include/eu_init.h>
#include <pk_compiler/include/cp_init.h>
#include <pk_imaging/include/im_init.h>
#include <pk_geometrics/include/ge_init.h>
#include <pk_geometrics/include/ge_matrix_tools.h>
#include <pk_particles/include/ps_init.h>
#include <pk_particles_toolbox/include/pt_init.h>
#include <pk_kernel/include/kr_resources.h>
#include <pk_kernel/include/kr_log_listeners_file.h>
#include <pk_kernel/include/kr_refptr.h>

#include <pk_maths/include/pk_maths_random.h>		// for PRNG
#include <pk_maths/include/pk_maths_transforms.h>	// for CPlane in CParticleSceneBasic

#include <pk_render_helpers/include/rh_init.h>
#include <pk_kernel/include/kr_thread_pool_default.h>

//RHI
#include <pk_rhi/include/Enums.h>
#include <pk_rhi/include/RHIInit.h>
#include <pk_rhi/include/Startup.h>
//Sample
#include <PK-SampleLib/PKSampleInit.h>
#include <PK-SampleLib/SampleUtils.h>

#include <pk_rhi/include/FwdInterfaces.h>

//AE
#include <AE_GeneralPlug.h>
#include <SuiteHelper.h>

#include "AEGP_LayerHolder.h"
#include "AEGP_UpdateAEState.h"

#include "Panels/AEGP_PanelQT.h"

#include "AEGP_System.h"

#include <vector>
#include <algorithm>

#include <codecvt>

//----------------------------------------------------------------------------

PK_LOG_MODULE_DECLARE();

template <>
const A_char*	SuiteTraits<AEGP_PanelSuite1>::i_name = kAEGPPanelSuite;
template <>
const int32_t	SuiteTraits<AEGP_PanelSuite1>::i_version = kAEGPPanelSuiteVersion1;

template <>
const A_char*	SuiteTraits<PFAppSuite4>::i_name = kPFAppSuite;
template <>
const int32_t	SuiteTraits<PFAppSuite4>::i_version = kPFAppSuiteVersion4;

//----------------------------------------------------------------------------

__AEGP_PK_BEGIN

//TODO OS Specific code
static PAAERenderContext		s_AAEThreadRenderContexts = null;
static PKSample::CShaderLoader	*s_ShaderLoader = null;

//----------------------------------------------------------------------------

const char	*SAEPreferenciesKeys::GetGraphicsApiAsCharPtr(EApiValue value)
{
	return kApiNames[value];

}

//----------------------------------------------------------------------------

const char	*SAEPreferenciesKeys::GetGraphicsApiAsCharPtr(RHI::EGraphicalApi value)
{
	return GetGraphicsApiAsCharPtr(RHIApiToAEApi(value));
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------

HBO_CLASS_DEFINITION_BEGIN(CAEPProjectProperties)
	.HBO_FIELD_DEFINITION(ProjectName)
	.HBO_FIELD_DEFINITION(LayerProperties)
HBO_CLASS_DEFINITION_END

CAEPProjectProperties::CAEPProjectProperties()
	: HBO_CONSTRUCT(CAEPProjectProperties)
{
}

//----------------------------------------------------------------------------

CAEPProjectProperties::~CAEPProjectProperties()
{
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------

CPopcornFXWorld				*CPopcornFXWorld::m_Instance = null;

CPopcornFXWorld::CPopcornFXWorld()
	: m_Initialized(false)
	, m_AAETreadID(0)
	, m_GraphicsApi(RHI::GApi_Null)
	, m_AELogFileListener(null)
{
}

//----------------------------------------------------------------------------

CPopcornFXWorld::~CPopcornFXWorld()
{
	if (s_AAEThreadRenderContexts != null)
	{
		s_AAEThreadRenderContexts->Destroy();
		s_AAEThreadRenderContexts = null;
	}
	PK_SAFE_DELETE(s_ShaderLoader);
}

//----------------------------------------------------------------------------

CPopcornFXWorld	&CPopcornFXWorld::Instance()
{
	if (CPopcornFXWorld::m_Instance == null)
	{
		m_Instance = new CPopcornFXWorld();
	}
	return *m_Instance;
}

//----------------------------------------------------------------------------

static Threads::PAbstractPool		_CreateCustomThreadPool()
{
	bool						success = true;
	CThreadManager::EPriority	workersPriority = CThreadManager::Priority_High;
	PWorkerThreadPool			pool = PK_NEW(CWorkerThreadPool);

	if (PK_VERIFY(pool != null))
	{
			// Let the OS shedule our workers
			// leave 2 core for main thread and render thread
			u32		processorCount = PKMin(PKMax(CPU::Caps().ProcessAffinity().NumBitsSet(), (u32)4) - (u32)3, (u32)12);

			CPopcornFXWorld::Instance().SetWorkerCount(processorCount);
			success = pool->AddFullAffinityWorkers(processorCount, CPU::Caps().ProcessAffinity(), workersPriority);

		if (!success)
			return null;

		pool->StartWorkers();
	}
	return pool;
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::Setup(SPBasicSuite *pica_basicP, AEGP_PluginID id)
{
	PK_SCOPEDLOCK(m_Lock);
	m_AEGPID = id;
	m_Suites = pica_basicP;

	AEGP_SuiteHandler				suites(m_Suites);
	A_Err							err = A_Err_NONE;
	// AE SETUP
	{
		AEGP_InstalledEffectKey		key = AEGP_InstalledEffectKey_NONE;
		u32							effectInstalled;
		A_char						effectName[AEGP_MAX_EFFECT_MATCH_NAME_SIZE] = { '\0' };
		u32							pluginsFound = 0;

		suites.EffectSuite4()->AEGP_GetNumInstalledEffects((A_long*)&effectInstalled);

		for (u32 i = 0; i < effectInstalled; ++i)
		{
			suites.EffectSuite4()->AEGP_GetNextInstalledEffect(key, &key);
			suites.EffectSuite4()->AEGP_GetEffectMatchName(key, effectName);

			if (strcmp(effectName, "ADBE PopcornFX Emitter") == 0)
			{
				m_PKInstalledPluginKeys[EPKChildPlugins::EMITTER] = key;
				++pluginsFound;
			}
			else if (strcmp(effectName, "ADBE PopcornFX Attribute") == 0)
			{
				m_PKInstalledPluginKeys[EPKChildPlugins::ATTRIBUTE] = key;
				++pluginsFound;
			}
			else if (strcmp(effectName, "ADBE PopcornFX Sampler") == 0)
			{
				m_PKInstalledPluginKeys[EPKChildPlugins::SAMPLER] = key;
				++pluginsFound;
			}

			if (pluginsFound == EPKChildPlugins::_PLUGIN_COUNT)
				break;
		}
		PK_RELEASE_ASSERT(pluginsFound == EPKChildPlugins::_PLUGIN_COUNT);
	}

	//POPCORN FX INIT
	SDllVersion			engineVersion;
	CPKKernel::Config	configKernel;

	configKernel.m_CreateThreadPool = &_CreateCustomThreadPool;

	if (engineVersion.Major != PK_VERSION_MAJOR || engineVersion.Minor != PK_VERSION_MINOR)
	{
		PK_ASSERT_NOT_REACHED();
	}

	if (!CPKKernel::Startup(engineVersion, configKernel) ||
		!CPKBaseObject::Startup(engineVersion, CPKBaseObject::Config()) ||
		!CPKEngineUtils::Startup(engineVersion, CPKEngineUtils::Config()) ||
		!CPKCompiler::Startup(engineVersion, CPKCompiler::Config()) ||
		!CPKGeometrics::Startup(engineVersion, CPKGeometrics::Config()) ||
		!CPKImaging::Startup(engineVersion, CPKImaging::Config()) ||
		!CPKParticles::Startup(engineVersion, CPKParticles::Config()) ||
		!ParticleToolbox::Startup() ||
		!CPKRHI::Startup(engineVersion, CPKRHI::Config()) ||
		!CPKRenderHelpers::Startup(engineVersion, CPKRenderHelpers::Config()) ||
		!CPKSample::Startup(engineVersion, CPKSample::Config()) ||				// Only necessary if your engine links/relies on PKSample
		!PK_VERIFY(Kernel::CheckStaticConfigFlags(Kernel::g_BaseStaticConfig, SKernelConfigFlags())))
	{
		return false;
	}

	PK_LOG_MODULE_INIT_START;

	CAEPProjectProperties::RegisterHandler();
	CLayerProperty::RegisterHandler();
	CGraphicOverride::RegisterHandler();
	CEditorAssetEffect::RegisterHandler();

	PK_LOG_MODULE_INIT_END;

	CAELog::SetPKLogState(true);

#if 0
		_GetAEPath(AEGP_GetPathTypes_PLUGIN, m_PluginPath); < --Not implemented by AE
#endif
	if (!PK_VERIFY(_GetAEPath(AEGP_GetPathTypes_USER_PLUGIN, m_UserPluginPath)) ||
		!PK_VERIFY(_GetAEPath(AEGP_GetPathTypes_ALLUSER_PLUGIN, m_AllUserPluginPath)) ||
		!PK_VERIFY(_GetAEPath(AEGP_GetPathTypes_APP, m_AEPath)))
		return false;

	CFilePath::Purify(m_AEPath);

	CCoordinateFrame::SetupFrame(ECoordinateFrame::Frame_User1, EAbsoluteAxis::Axis_Right, EAbsoluteAxis::Axis_Down, EAbsoluteAxis::Axis_Forward);
	CCoordinateFrame::SetGlobalFrame(ECoordinateFrame::Frame_User1);

	if (!PK_VERIFY(PopcornRegisterPlugins(EPlugin_Default | EPlugin_MeshCodecFBX) == true))
		return CAELog::TryLogErrorWindows("Could not load PopcornFX plugins");

	AEGP_PersistentBlobH			blobH = NULL;
	AEGP_PersistentDataSuite4		*persistentDataSuite = suites.PersistentDataSuite4();

	if (persistentDataSuite != null)
	{
		err |= persistentDataSuite->AEGP_GetApplicationBlob(AEGP_PersistentType_MACHINE_SPECIFIC, &blobH);
		if (err == A_Err_NONE && blobH != null)
		{
			u32		targetApi;
#if defined(PK_WINDOWS)
			A_long	defaultApi = EApiValue::D3D11;
#elif defined(PK_MACOSX)
			A_long	defaultApi = EApiValue::Metal;
#endif
			err |= persistentDataSuite->AEGP_GetLong(blobH,
				SAEPreferenciesKeys::kSection,
				SAEPreferenciesKeys::kApi,
				defaultApi,
				(A_long*)&targetApi);
			if (err == A_Err_NONE)
			{
				m_GraphicsApi = AEApiToRHIApi(static_cast<EApiValue>(targetApi));
			}
		}
	}
	if (!PK_VERIFY(err == A_Err_NONE))
		return false;

	CPanelBaseGUI	*panel = CPanelBaseGUI::GetInstance();
	if (!PK_VERIFY(panel->InitializeIFN()))
		return false;

	return true;
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::FetchAEProject()
{
	AEGP_SuiteHandler				suites(m_Suites);
	A_Err							err = A_Err_NONE;
	A_long							numProj = 0;

	err |= suites.ProjSuite6()->AEGP_GetNumProjects(&numProj);

	if (numProj > 0)
	{
		AEGP_ProjectH	projPH;

		err |= suites.ProjSuite6()->AEGP_GetProjectByIndex(0, &projPH);

		if (err == A_Err_NONE && projPH != null)
		{
			AEGP_MemHandle		handle;
			aechar_t			*retrievedPath;
			CString				path = "";

			err |= suites.ProjSuite6()->AEGP_GetProjectPath(projPH, &handle);
			err |= suites.MemorySuite1()->AEGP_LockMemHandle(handle, reinterpret_cast<void **>(&retrievedPath));

			WCharToCString(retrievedPath, &path);

			err |= suites.MemorySuite1()->AEGP_UnlockMemHandle(handle);
			err |= suites.MemorySuite1()->AEGP_FreeMemHandle(handle);
			if (path != null)
			{
				m_AEProjectFilename = CFilePath::ExtractFilename(path.Data());
				CFilePath::StripExtensionInPlace(m_AEProjectFilename);
				path = CFilePath::StripFilename(path.Data());
				CFilePath::Purify(path);
				bool	newPath = false;
				
				//Create log file in AEP directory
				if (m_AEProjectPath != path)
				{
					newPath = true;
					m_AEProjectPath = path;

					CString	logPath = path / "popcorn.htm";
					
					if (m_AELogFileListener != null)
					{
						CLog::RemoveGlobalListener(m_AELogFileListener);
						m_AELogFileListener = null;
					}
					m_AELogFileListener = PK_NEW(CLogListenerFile(path.Data(), "popcorn-engine logfile");
					CLog::AddGlobalListener(m_AELogFileListener));
				}
				//CreateINF Project conf file or reload it if AEP changed
				if (m_ProjectConfFile == null || newPath)
				{
					IFileSystem			*fileSystem = File::DefaultFileSystem();

					if (m_ProjectPack != null)
						fileSystem->UnmountPack(m_ProjectPack.Get());
					if (m_ProjectConfFile != null)
					{
						m_ProjectConfFile->Unload();
						for (auto *layers : m_Layers)
							layers->m_LayerProperty = null;
					}

					CString				projectPropFilename = m_AEProjectFilename + ".pkbo";

					m_ProjectPack = fileSystem->MountPack(path);
					m_ProjectConfFile = HBO::g_Context->LoadFile_AndCreateIFN_Pure(projectPropFilename.View(), false);

				}
				if (m_ProjectConfFile != null)
				{
					PAEPProjectProperties projectProp = m_ProjectConfFile->FindFirstOf<CAEPProjectProperties>();

					if (projectProp != null)
					{
						if (!projectProp->ProjectName().Compare(m_AEProjectFilename))//project change: unload file and let the next frame recreate it
						{
							m_ProjectConfFile->Unload();
							m_ProjectConfFile = null;
						}
					}
					else //or a new project
					{
						projectProp = HBO::g_Context->NewObject<CAEPProjectProperties>(m_ProjectConfFile.Get());
						projectProp->SetProjectName(m_AEProjectFilename);

						WriteProjectFileModification();
					}
					m_ProjectProperty = projectProp;
				}
			}
		}
	}
	if (!PK_VERIFY(err == A_Err_NONE))
		return false;
	return true;
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::_GetAEPath(AEGP_GetPathTypes type, CString &dst)
{
	AEGP_MemHandle		handle;
	A_Err				err = A_Err_NONE;
	aechar_t			*retrievedPath;
	AEGP_SuiteHandler	suites(m_Suites);

	err = suites.UtilitySuite6()->AEGP_GetPluginPaths(m_AEGPID, type, &handle);
	if (!PK_VERIFY(err == A_Err_NONE))
		return false;
	suites.MemorySuite1()->AEGP_LockMemHandle(handle, reinterpret_cast<void**>(&retrievedPath));

	dst.Clear();
	WCharToCString(retrievedPath, &dst);

	err |= suites.MemorySuite1()->AEGP_UnlockMemHandle(handle);
	err |= suites.MemorySuite1()->AEGP_FreeMemHandle(handle);

	if (!PK_VERIFY(err == A_Err_NONE))
		return false;
	return true;
}

//----------------------------------------------------------------------------

bool CPopcornFXWorld::SetCommandHandle(AEGP_Command &command, const char *name)
{
	m_CommandName = name;
	m_Command = command;
	return true;
}

//----------------------------------------------------------------------------

AEGP_PluginID	CPopcornFXWorld::GetPluginID()
{
	return m_AEGPID;
}

//----------------------------------------------------------------------------

SPBasicSuite	*CPopcornFXWorld::GetAESuites()
{
	return m_Suites;
}

//----------------------------------------------------------------------------

RHI::EGraphicalApi	CPopcornFXWorld::GetRenderApi()
{
	PK_ASSERT(m_GraphicsApi != RHI::GApi_Null);
	return m_GraphicsApi;
}

//----------------------------------------------------------------------------

void	CPopcornFXWorld::SetRenderApi(EApiValue AEGraphicsApi)
{
	A_Err							err = A_Err_NONE;
	AEGP_SuiteHandler				suites(m_Suites);
	AEGP_PersistentBlobH			blobH = NULL;
	AEGP_PersistentDataSuite4		*persistentDataSuite = suites.PersistentDataSuite4();

	if (persistentDataSuite != null)
	{
		err |= persistentDataSuite->AEGP_GetApplicationBlob(AEGP_PersistentType_MACHINE_SPECIFIC, &blobH);
		if (err == A_Err_NONE && blobH != null)
		{
			err |= persistentDataSuite->AEGP_SetLong(blobH,
				SAEPreferenciesKeys::kSection,
				SAEPreferenciesKeys::kApi,
				(A_long)AEGraphicsApi);
			PK_ASSERT(err == A_Err_NONE);
		}
	}
	CAELog::TryLogInfoWindows("Graphics Api changed, restart After Effects to apply change.");
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::InitializeIFN(SAAEIOData &AAEData)
{
	(void)AAEData;
	FetchAEProject();
	if (m_Initialized == true)
		return true;
	PK_SCOPEDLOCK(m_Lock);

	m_ClassName = "PKPluginInterface";

	m_VaultHandler.InitializeIFN();
	m_Initialized = true;
	for (u32 i = 0; i < __Effect_Parameters_Count; ++i)
		CAEUpdater::s_EmitterIndexes[i] = i;
	for (u32 i = 0; i < __Attribute_Parameters_Count; ++i)
		CAEUpdater::s_AttributeIndexes[i] = i;
	for (u32 i = 0; i < __AttributeSamplerType_Parameters_Count; ++i)
		CAEUpdater::s_SamplerIndexes[i] = i;
	return true;
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::ShutdownIFN()
{
	bool				result = true;

	m_VaultHandler.ShutdownIFN();
	{
		PK_SCOPEDLOCK(m_Lock);
		if (m_Panel)
		{
			m_Panel->DestroyInstance();
			m_Panel = null;
		}

		for (s32 j = m_Layers.Count() - 1; j >= 0; --j)
		{
			SLayerHolder	&layer = *m_Layers[j];
	
			result = layer.Clear(m_Suites);
			PK_ASSERT(result);
			PK_DELETE(m_Layers[j]);
		}
		m_Layers.Clear();
		HBO::g_Context->UnloadAllFiles();

		CAAEScene::ShutdownPopcornFX();
		m_Suites = null;
		if (m_AAETreadID.Count() != 0)
		{
			for (u32 i = 0; i < m_AAETreadID.Count(); ++i)
			{
				PopcornFX::CThreadManager::UnsafeUnregisterUserThread(m_AAETreadID[i]);
			}
			m_AAETreadID.Clear();
		}
		if (m_AELogFileListener != null)
		{
			CLog::RemoveGlobalListener(m_AELogFileListener);
			m_AELogFileListener = null;
		}
		IFileSystem			*fileSystem = File::DefaultFileSystem();
		if (PK_VERIFY(fileSystem != null))
			fileSystem->UnmountAllPacks();
		m_Initialized = false;
	}
	delete m_Instance;

	PopcornUnregisterPlugins();
	CAELog::SetPKLogState(false);

	PK_LOG_MODULE_RELEASE_START;

	CAEPProjectProperties::UnregisterHandler();
	CLayerProperty::UnregisterHandler();
	CGraphicOverride::UnregisterHandler();
	CEditorAssetEffect::UnregisterHandler();

	PK_LOG_MODULE_RELEASE_END;

	CPKSample::Shutdown();
	CPKRenderHelpers::Shutdown();
	CPKRHI::Shutdown();
	ParticleToolbox::Shutdown();
	CPKParticles::Shutdown();
	CPKImaging::Shutdown();
	CPKGeometrics::Shutdown();
	CPKCompiler::Shutdown();
	CPKEngineUtils::Shutdown();
	CPKBaseObject::Shutdown();
	CPKKernel::Shutdown();
	return result;
}

//----------------------------------------------------------------------------

void CPopcornFXWorld::SetProfilingState(bool state)
{
#if	(KR_PROFILER_ENABLED != 0)
	Profiler::CProfiler	*profiler = Profiler::MainEngineProfiler();
	if (profiler == null)
		return;
	profiler->GrabCallstacks(false);
	profiler->Activate(state);
	profiler->Reset();

	if (!state)
	{
		CString path = m_VaultHandler.VaultPathRoot() / "profilerReport.pkpr";
		CDynamicMemoryStream stream = CDynamicMemoryStream();
		Profiler::CProfilerReport	report;
		profiler->BuildReport(&report);
		Profiler::WriteProfileReport(report, stream);
		FILE	*f = fopen(path.Data(), "wb");
		if (f != null)
		{
			u32		size = 0;
			u8		*buffer = stream.ExportDataAndClose(size);
			fwrite(buffer, size, 1, f);
			fclose(f);
			PK_FREE(buffer);
		}
		else
			CLog::Log(PK_ERROR, "Failed to write profile report to %s", path.Data());
		stream.Close();
	}
#endif
}

//----------------------------------------------------------------------------

void	CPopcornFXWorld::SetParametersIndexes(const int *indexes, EPKChildPlugins plugin)
{
	if (plugin == EPKChildPlugins::EMITTER)
	{
		for (u32 i = 0; i < __Effect_Parameters_Count; ++i)
		{
			CAEUpdater::s_EmitterIndexes[i] = indexes[i];
			PK_ASSERT(CAEUpdater::s_EmitterIndexes[i] >= 0);
		}
	}
	else if (plugin == EPKChildPlugins::ATTRIBUTE)
	{
		for (u32 i = 0; i < __Attribute_Parameters_Count; ++i)
		{
			CAEUpdater::s_AttributeIndexes[i] = indexes[i];
			PK_ASSERT(CAEUpdater::s_AttributeIndexes[i] >= 0);
		}
	}
	else if (plugin == EPKChildPlugins::SAMPLER)
	{
		for (u32 i = 0; i < __AttributeSamplerType_Parameters_Count; ++i)
		{
			CAEUpdater::s_SamplerIndexes[i] = indexes[i];
			PK_ASSERT(CAEUpdater::s_SamplerIndexes[i] >= 0);
		}
	}
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::SetDefaultLayerPosition(SAAEIOData& AAEData, AEGP_LayerH layer)
{
	if (layer == null)
		return false;

	AEGP_SuiteHandler			suites(m_Suites);
	A_Err						result = A_Err_NONE;
	AEGP_StreamRefH				stream = null;
	A_Time						time;

	time.value = AAEData.m_InData->current_time;
	time.scale = AAEData.m_InData->time_scale;

	result |= suites.StreamSuite2()->AEGP_GetNewLayerStream(m_AEGPID, layer, AEGP_LayerStream_POSITION, &stream);
	if (result != A_Err_NONE || stream == null)
		return false;

	AEGP_StreamValue	value;

	result |= suites.StreamSuite2()->AEGP_GetNewStreamValue(m_AEGPID, stream, AEGP_LTimeMode_LayerTime, &time, false, &value);
	if (result != A_Err_NONE)
		return false;
	value.val.three_d.x = AAEData.m_InData->width / 2.0f;
	value.val.three_d.y = AAEData.m_InData->height / 2.0f;

	result |= suites.StreamSuite2()->AEGP_SetStreamValue(m_AEGPID, stream, &value);
	if (result != A_Err_NONE)
		return false;
	return true;
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::MoveEffectIntoCurrentView(SAAEIOData &AAEData, SEmitterDesc *descriptor)
{
	SLayerHolder		*layer = GetLayerForSEmitterDesc(descriptor);

	if (layer == null && layer->m_SpawnedEmitter.m_Desc == null)
		return false;

	CAABB		bounds;
	CFloat4x4	view;
	CFloat4		pos;
	float		zoom;
	A_Time		AETime;
	
	AETime.scale = AAEData.m_InData->time_scale;
	AETime.value = AAEData.m_InData->current_time;
	if (!layer->m_Scene->GetEmitterBounds(bounds) ||
		!CAEUpdater::GetCameraViewMatrixAtTime(layer, view, pos, AETime, zoom))
		return false;
	

	CFloat3 forward = view.Inverse().StrippedZAxis().Normalized();

	float	extends = bounds.Extent().HighestComponent();
	CFloat3 newPos = pos.xyz() + forward * (extends * 2.0f);

	AEGP_SuiteHandler			suites(m_Suites);
	A_Err						result = A_Err_NONE;
	AEGP_StreamRefH				streamPos = null;
	AEGP_EffectRefH				effectRef = null;

	result = suites.PFInterfaceSuite1()->AEGP_GetNewEffectForEffect(m_AEGPID, AAEData.m_InData->effect_ref, &effectRef);
	if (result != A_Err_NONE || effectRef == null)
		return false;

	result |= suites.StreamSuite5()->AEGP_GetNewEffectStreamByIndex(m_AEGPID, effectRef, CAEUpdater::s_EmitterIndexes[Effect_Parameters_Position], &streamPos);
	PK_ASSERT(result == A_Err_NONE);
	if (result != A_Err_NONE || streamPos == null)
	{
		return false;
	}
	AEGP_StreamValue2	value;
	result |= suites.StreamSuite5()->AEGP_GetNewStreamValue(m_AEGPID, streamPos, AEGP_LTimeMode_LayerTime, &AETime, false, &value);
	if (result != A_Err_NONE)
	{
		result |= suites.EffectSuite4()->AEGP_DisposeEffect(effectRef);
		return false;
	}
	value.val.three_d.x = newPos.x();
	value.val.three_d.y = newPos.y();
	value.val.three_d.z = newPos.z();
	result |= suites.StreamSuite5()->AEGP_SetStreamValue(m_AEGPID, streamPos, &value);
	if (result != A_Err_NONE)
	{
		result |= suites.EffectSuite4()->AEGP_DisposeEffect(effectRef);
		return false;
	}
	result |= suites.StreamSuite5()->AEGP_DisposeStreamValue(&value);
	result |= suites.StreamSuite5()->AEGP_DisposeStream(streamPos);

	return true;
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::IdleUpdate()
{
	if (m_Initialized == false)
		return false;

	PK_SCOPEDPROFILE();

	AEGP_SuiteHandler			suites(m_Suites);
	AEGP_EffectSuite4			*effectSuite = suites.EffectSuite4();
	A_Err						result = A_Err_NONE;
	bool						updateEmitterPanel = false;

	if (effectSuite == null)
		return false;

	CAELog::ClearIOData();
	FetchAEProject();

	CString	compositionName;

	GetMostRecentCompName(compositionName);
	if (m_MostRecentCompName != compositionName)
	{
		if (m_Panel)
			m_Panel->UpdateScenesModel();
		m_MostRecentCompName = compositionName;
	}
	for (s32 l = m_Layers.Count() - 1; l >= 0; --l)
	{
		SLayerHolder	&layer = *m_Layers[l];

		if (!layer.m_LayerLock.TryLock())
			continue;

		A_long	count = 0;
		u32		emitterCount = 0;

		if (layer.m_SpawnedEmitter.m_Desc != null)
		{
			result = effectSuite->AEGP_GetLayerNumEffects(layer.m_EffectLayer, &count);
			if (result != A_Err_NONE)
			{
				CLog::Log(PK_INFO, "AEGP_GetLayerNumEffects failed");
				layer.m_LayerLock.Unlock();
				continue;
			}

			//Start the fuckery to handle Layer deletion
			AEGP_CompH			compH;
			AEGP_LayerIDVal		layerID;
			AEGP_LayerH			layerFromComp;
			result = suites.LayerSuite5()->AEGP_GetLayerParentComp(layer.m_EffectLayer, &compH);
			if (result != A_Err_NONE)
			{
				CLog::Log(PK_INFO, "AEGP_GetLayerNumEffects failed");
				layer.m_LayerLock.Unlock();
				continue;
			}
			result = suites.LayerSuite7()->AEGP_GetLayerID(layer.m_EffectLayer, &layerID);
			result = suites.LayerSuite7()->AEGP_GetLayerFromLayerID(compH, layerID, &layerFromComp);
			if (result != A_Err_NONE) //Layer is deleted but still on the undo stack
			{
				updateEmitterPanel = layer.m_Deleted != true ? true : updateEmitterPanel;
				layer.m_Deleted = true;
			}
			else
			{
				updateEmitterPanel = layer.m_Deleted != false ? true : updateEmitterPanel;
				layer.m_Deleted = false;
			}

			for (A_long j = count - 1; j >= 0; --j)
			{
				AEGP_EffectRefH				effectRef = null;
				AEGP_InstalledEffectKey		installedKey;

				result = suites.EffectSuite4()->AEGP_GetLayerEffectByIndex(m_AEGPID, layer.m_EffectLayer, j, &effectRef);
				result = suites.EffectSuite4()->AEGP_GetInstalledKeyFromLayerEffect(effectRef, &installedKey);

				if (!PK_VERIFY(result == A_Err_NONE))
				{
					CLog::Log(PK_INFO, "AEGP_GetLayerEffectByIndex failed");
					layer.m_LayerLock.Unlock();
					continue;
				}
				if (installedKey == m_PKInstalledPluginKeys[EPKChildPlugins::ATTRIBUTE])
				{
				}
				else if (installedKey == m_PKInstalledPluginKeys[EPKChildPlugins::EMITTER])
				{
					emitterCount += 1;
					if (layer.m_ForceRender)
					{
						layer.m_ForceRender = false;
						if (!InvalidateEmitterRender(&layer, effectRef))
						{
							layer.m_LayerLock.Unlock();
							return false;
						}
					}
				}
				result = suites.EffectSuite4()->AEGP_DisposeEffect(effectRef);
			}
			if (emitterCount == 0)
			{
				layer.Clear(m_Suites);
				m_Layers.RemoveElement(&layer);
				layer.m_LayerLock.Unlock();
				PK_DELETE(&layer);
				updateEmitterPanel = true;
				continue;
			}
		}
		if (_ExecClearAttributes(&layer) != 0) //Clear attribute did work, give time to AE to reorder internals.
		{
			layer.m_LayerLock.Unlock();
			continue;
		}

		if (_ExecSPendingEmitters(&layer) < 0)
		{
			updateEmitterPanel = true;
			layer.m_LayerLock.Unlock();
			continue;
		}

		if (_ExecSPendingAttributes(&layer) < 0)
		{
			layer.m_LayerLock.Unlock();
			continue;
		}

		layer.m_LayerLock.Unlock();
	}
	{
		PK_SCOPEDLOCK(m_UIEventLock);
		for (u32 i = 0; i < m_UIEvents.Count(); ++i)
		{
			SUIEvent	*event = m_UIEvents[i];

			event->Exec();
			PK_DELETE(event);
		}
		m_UIEvents.Clear();
	}
	if (m_Panel != null)
	{
		if (updateEmitterPanel)
			m_Panel->UpdateScenesModel();
	}
#if defined(PK_MACOSX)
	if (m_Panel != null)
		m_Panel->IdleUpdate();
#endif
	if (!PK_VERIFY(result == A_Err_NONE))
		return false;
	return true;
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::HandleNewEmitterEvent(AAePk::SAAEIOData &AAEData, SEmitterDesc *desc)
{
	A_Err						result = A_Err_NONE;
	AEGP_SuiteHandler			suites(m_Suites);
	AEGP_LayerH					layerH;
	A_long						dstID = 0, existingID;
	s32							existingLayerID = -1;

	FetchAEProject();
	PK_SCOPEDLOCK(m_Lock);
	result |= suites.PFInterfaceSuite1()->AEGP_GetEffectLayer(AAEData.m_InData->effect_ref, &layerH);
	result |= suites.LayerSuite8()->AEGP_GetLayerID(layerH, &dstID);

	if (!PK_VERIFY(result == A_Err_NONE))
		return false;

	for (u32 i = 0; i < m_Layers.Count(); ++i)
	{
		result |= suites.LayerSuite8()->AEGP_GetLayerID(m_Layers[i]->m_EffectLayer, &existingID);
		if (dstID == existingID)
		{
			existingLayerID = i;
			break;
		}
	}
	SLayerHolder		*layerHolder = null;

	if (existingLayerID >= 0)
	{
		layerHolder = m_Layers[existingLayerID];
		layerHolder->m_Scene->Quit();
		layerHolder->m_Scene = null;
	}
	else
	{
		layerHolder = PK_NEW(SLayerHolder);
		if (!PK_VERIFY(m_Layers.PushBack(layerHolder).Valid()))
			return false;
		existingLayerID = m_Layers.Count() - 1;
	}
	if (!PK_VERIFY(layerHolder != null))
		return false;

	layerHolder->m_EffectLayer = layerH;
	layerHolder->ID = existingLayerID;

	if (!SetLayerName(layerHolder))
		return false;
	if (!SetLayerCompName(layerHolder))
		return false;
	CreateLayerPropertyIFP(layerHolder);

	layerHolder->m_Scene = PK_NEW(CAAEScene);
	if (!PK_VERIFY(layerHolder->m_Scene != null))
		return false;
	layerHolder->m_Scene->SetLayerHolder(layerHolder);
	layerHolder->m_ViewMatrix = CFloat4x4::IDENTITY;
	layerHolder->m_CameraPos = CFloat4::ZERO;
	layerHolder->m_CurrentTime = AAEData.m_InData->current_time;
	layerHolder->m_TimeScale = AAEData.m_InData->time_scale;
	layerHolder->m_TimeStep = AAEData.m_InData->local_time_step;

	if (layerHolder->m_Scene->Init(AAEData) == false)
		return false;
	layerHolder->m_Scene->SetEffectDescriptor(desc);
	if (layerHolder->m_SpawnedEmitter.m_Desc != null)
	{
		layerHolder->m_SpawnedEmitter.m_EffectHandle = AAEData.m_InData->effect_ref;
		layerHolder->m_SpawnedEmitter.m_Desc = desc;
	}
	else
	{
		if (!layerHolder->m_SPendingEmitters.PushBack(SPendingEmitter{ AAEData.m_InData->effect_ref, desc }).Valid())
			return false;
	}
	if (!PK_VERIFY(result == A_Err_NONE))
		return false;
	return true;
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::HandleDeleteEmitterEvent(AAePk::SAAEIOData &AAEData, SEmitterDesc *desc)
{
	(void)AAEData;

	SLayerHolder	*layer = GetLayerForSEmitterDesc(desc);

	if (layer)
	{
		layer->Clear(m_Suites);

		u32 idx = m_Layers.IndexOf(layer);
		m_Layers.Remove(idx);

		PK_DELETE(layer);
		if (m_Panel != null)
			m_Panel->UpdateScenesModel();
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::HandleNewAttributeEvent(PF_ProgPtr &effectRef, SAttributeDesc *desc, bool asyncMerge /*= true*/, SLayerHolder *layer /*=null*/)
{
	(void)asyncMerge;
	AEGP_SuiteHandler			suites(m_Suites);

	PK_SCOPEDLOCK(m_Lock);

	if (layer == null)
	{
		PK_ASSERT(false);
		return false;
	}
	if (layer != null)
	{
		CStringId			id = GetAttributeID(desc);
		if (layer->m_SpawnedAttributes.Contains(id))
		{
			SPendingAttribute *attr = layer->m_SpawnedAttributes[id];
			attr->m_ParentEffectPtr = effectRef;

			if (attr->m_Desc != null)
			{
				*(attr->m_Desc) = *desc;
				delete (desc);
			}
			else
			{
				attr->m_Desc = desc;
			}
		}
		else //Not yet spawned in AE, must do an async merge due to SDK limitations.
		{
			if (!PK_VERIFY(layer->m_SPendingAttributes.PushBack(SPendingAttribute(effectRef, desc, null)).Valid()))
				return false;
		}
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::HandleNewAttributeSamplerEvent(PF_ProgPtr &effectRef, SAttributeSamplerDesc *desc, bool asyncMerge, SLayerHolder *layer)
{
	(void)asyncMerge;
	AEGP_SuiteHandler			suites(m_Suites);

	PK_SCOPEDLOCK(m_Lock);

	if (layer == null)
	{
		PK_ASSERT(false);
		return false;
	}

	if (layer != null)
	{
		CStringId			id = GetAttributeID(desc);
		if (layer->m_SpawnedAttributesSampler.Contains(id))
		{
			SPendingAttribute *attr = layer->m_SpawnedAttributesSampler[id];
			attr->m_ParentEffectPtr = effectRef;
			if (attr->m_Desc != null)
			{
				SAttributeSamplerDesc *descriptor = static_cast<SAttributeSamplerDesc*>(attr->m_Desc);
				if (descriptor->m_Descriptor != null)
				{
					delete(descriptor->m_Descriptor);
					descriptor->m_Descriptor = null;
					attr->m_Desc->m_IsDeleted = true;
				}
				*(static_cast<SAttributeSamplerDesc*>(attr->m_Desc)) = *(static_cast<SAttributeSamplerDesc*>(desc));
				delete (desc);
			}
			else
			{
				attr->m_Desc = desc;
			}
		}
		else //Not yet spawned in AE, must do an async merge due to SDK limitations.
		{
			if (!PK_VERIFY(layer->m_SPendingAttributes.PushBack(SPendingAttribute(effectRef, desc, null)).Valid()))
				return false;
		}
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::HandleNewAttributes(TArray<SAttributeBaseDesc*> &attributes, PF_ProgPtr &effectRef, SLayerHolder *layer, bool asyncMerge)
{
	for (u32 i = 0; i < attributes.Count(); ++i)
	{
		attributes[i]->m_Order = attributes.Count() - i - 1;
		if (attributes[i]->m_IsAttribute == true)
			HandleNewAttributeEvent(effectRef, static_cast<SAttributeDesc*>(attributes[i]), asyncMerge, layer);
		else
			HandleNewAttributeSamplerEvent(effectRef, static_cast<SAttributeSamplerDesc*>(attributes[i]), asyncMerge, layer);
	
	}
	return true;
}

//----------------------------------------------------------------------------

SLayerHolder	*CPopcornFXWorld::GetLayerForSEmitterDesc(SEmitterDesc *desc)
{
	return GetLayerForSEmitterDescID(CStringId(desc->m_UUID.c_str()));
}

//----------------------------------------------------------------------------

SLayerHolder	*CPopcornFXWorld::GetLayerForSEmitterDescID(CStringId id)
{
	for (u32 i = 0; i < m_Layers.Count(); ++i)
	{
		SLayerHolder	&layer = *m_Layers[i];
		if (layer.m_SpawnedEmitter.m_Desc && CStringId(layer.m_SpawnedEmitter.m_Desc->m_UUID.c_str()) == id)
		{
			return &layer;
		}
	}
	return null;
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::CheckEmitterValidity(AAePk::SAAEIOData &AAEData, AAePk::SEmitterDesc *descriptor)
{
	(void)AAEData;
	SLayerHolder		*layer = GetLayerForSEmitterDesc(descriptor);

	if (layer == null)
		return false;
	return true;
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::UpdateScene(SAAEIOData &AAEData, SEmitterDesc *desc)
{
	PK_SCOPEDPROFILE();
	if (!InitializeIFN(AAEData))
		return false;
	if (m_Layers.Count() == 0)
		return true;
	if (!PopcornFX::CCurrentThread::IsRegistered())
	{
		if (!m_AAETreadID.PushBack(PopcornFX::CCurrentThread::RegisterUserThread()).Valid())
			return false;
	}

	SLayerHolder		*layer = GetLayerForSEmitterDesc(desc);
	AEGP_SuiteHandler	suites(m_Suites);
	PF_Err				result = A_Err_NONE;
	A_Time				AETime;

	if (layer == null)
		return true;

	PK_SCOPEDLOCK(layer->m_LayerLock);

	layer->m_CurrentTime = AAEData.m_InData->current_time;
	layer->m_TimeScale = AAEData.m_InData->time_scale;
	//Sync Params with current frame
	result |= suites.PFInterfaceSuite1()->AEGP_ConvertEffectToCompTime(AAEData.m_InData->effect_ref, layer->m_CurrentTime, layer->m_TimeScale, &AETime);

	AEGP_LayerH			cameraLayer = null;

	result |= suites.PFInterfaceSuite1()->AEGP_GetEffectCamera(AAEData.m_InData->effect_ref, &AETime, &cameraLayer);
	layer->m_CameraLayer = cameraLayer;
	if (desc->m_Name.empty())
		return true;

	A_long				dstID = 0;
	result |= suites.LayerSuite8()->AEGP_GetLayerID(layer->m_EffectLayer, &dstID);

#if 1 // Fix for copy paste break everything...

	if (desc->m_LayerID != dstID)
	{
		AEGP_LayerH	layerH = null;
		result |= suites.PFInterfaceSuite1()->AEGP_GetEffectLayer(AAEData.m_InData->effect_ref, &layerH);
		if (!PK_VERIFY(result == A_Err_NONE))
			return false;

		if (layerH != null)
		{
			desc->m_LayerID = dstID;
			layer->m_EffectLayer = layerH;

			A_long	count = 0;
			result |= suites.EffectSuite4()->AEGP_GetLayerNumEffects(layer->m_EffectLayer, &count);

			for (A_long j = count - 1; j >= 0; --j)
			{
				bool						disposeEffect = true;
				AEGP_EffectRefH				effectRef = null;
				AEGP_InstalledEffectKey		installedKey;

				result |= suites.EffectSuite4()->AEGP_GetLayerEffectByIndex(m_AEGPID, layer->m_EffectLayer, j, &effectRef);
				result |= suites.EffectSuite4()->AEGP_GetInstalledKeyFromLayerEffect(effectRef, &installedKey);

				if (!PK_VERIFY(result == A_Err_NONE))
				{
					return false;
				}
				if (installedKey == m_PKInstalledPluginKeys[EPKChildPlugins::ATTRIBUTE])
				{
					CStringId id = GetAttributeID(effectRef);
					if (layer->m_SpawnedAttributes.Contains(id))
					{
						SPendingAttribute *attribute = layer->m_SpawnedAttributes[id];

						result |= suites.EffectSuite4()->AEGP_DisposeEffect(attribute->m_AttributeEffectRef);
						attribute->m_AttributeEffectRef = effectRef;
						disposeEffect = false;
					}
				}
				else if (installedKey == m_PKInstalledPluginKeys[EPKChildPlugins::SAMPLER])
				{
					CStringId id = GetAttributeSamplerID(effectRef);
					if (layer->m_SpawnedAttributesSampler.Contains(id))
					{
						SPendingAttribute *sampler = layer->m_SpawnedAttributesSampler[id];

						result |= suites.EffectSuite4()->AEGP_DisposeEffect(sampler->m_AttributeEffectRef);
						sampler->m_AttributeEffectRef = effectRef;
						disposeEffect = false;
					}
				}
				else if (installedKey == m_PKInstalledPluginKeys[EPKChildPlugins::EMITTER])
				{
					if (m_Panel)
					{
						m_Panel->UpdateScenesModel();
					}
				}
				if (disposeEffect)
					result |= suites.EffectSuite4()->AEGP_DisposeEffect(effectRef);
			}
		}
	}
#endif
	layer->m_SpawnedEmitter.m_EffectHandle = AAEData.m_InData->effect_ref;

#if TEST_COLOR_PROFILE & 0
	AEGP_CompH			compH;
	AEGP_ColorProfileP	colorProfile;
	A_FpShort			gamma;

	result |= suites.LayerSuite5()->AEGP_GetLayerParentComp(layer->m_EffectLayer, &compH);
	result |= suites.ColorSettingsSuite2()->AEGP_GetNewWorkingSpaceColorProfile(m_AEGPID, compH, &colorProfile);
	result |= suites.ColorSettingsSuite2()->AEGP_GetColorProfileApproximateGamma(colorProfile, &gamma);

	//AEGP_MemHandle	iccProfile = null;
	//result |= suites.ColorSettingsSuite2()->AEGP_GetNewICCProfileFromColorProfile(m_AEGPID, colorProfile, &iccProfile);
	//result |= suites.MemorySuite1()->AEGP_FreeMemHandle(iccProfile);
	result |= suites.ColorSettingsSuite2()->AEGP_DisposeColorProfile(colorProfile);

#endif
	PAAEScene	scene = layer->m_Scene;

	scene->SetLayerHolder(layer);
#if defined (PK_SCALE_DOWN)
	layer->m_ScaleFactor = desc->m_ScaleFactor;
#endif
	scene->UpdateLight(layer);
	scene->UpdateBackdrop(layer, desc);
	{
		PK_SCOPEDLOCK(GetRenderLock());
		scene->Update(AAEData);
		if (AAEData.m_ReturnCode == A_Err_NONE)
		{
			scene->Render(AAEData);
		}
	}
	if (!PK_VERIFY(result == A_Err_NONE))
		return false;
	return true;
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::UpdateEmitter(SAAEIOData &AAEData, SEmitterDesc *desc)
{
	(void)AAEData;
	(void)desc;

	return true;
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::InvalidateEmitterRender(SLayerHolder *layer , AEGP_EffectRefH effectRef)
{
	if (layer == null)
		return false;

	A_Err								result = A_Err_NONE;

	AEGP_SuiteHandler					suites(m_Suites);
	AEGP_StreamRefH						streamRef = null;

	A_Time								AETime;

	AETime.scale = layer->m_TimeScale;
	AETime.value = 0;

	result |= suites.StreamSuite2()->AEGP_GetNewEffectStreamByIndex(m_AEGPID, effectRef, CAEUpdater::s_EmitterIndexes[Effect_Parameters_Refresh_Render], &streamRef);
	if (!PK_VERIFY(result == A_Err_NONE))
		return false;

	AEGP_StreamValue value;

	result |= suites.StreamSuite2()->AEGP_GetNewStreamValue(m_AEGPID, streamRef, AEGP_LTimeMode_LayerTime, &AETime, TRUE, &value);
	if (!PK_VERIFY(result == A_Err_NONE))
		return false;

	value.val.one_d = (int)(value.val.one_d + 1) % 100;

	result |= suites.StreamSuite2()->AEGP_SetStreamValue(m_AEGPID, streamRef, &value);
	result |= suites.StreamSuite2()->AEGP_DisposeStream(streamRef);
	return true;
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::InvalidateEmitterRender(SAAEIOData &AAEData, SEmitterDesc *desc)
{
	A_Err								result = A_Err_NONE;
	AEGP_SuiteHandler					suites(m_Suites);
	AEGP_EffectRefH						effectRef = null;

	result = suites.PFInterfaceSuite1()->AEGP_GetNewEffectForEffect(m_AEGPID, AAEData.m_InData->effect_ref, &effectRef);
	if (!PK_VERIFY(result == A_Err_NONE))
		return false;
	bool ret = InvalidateEmitterRender(GetLayerForSEmitterDesc(desc) , effectRef);

	result = suites.EffectSuite4()->AEGP_DisposeEffect(effectRef);
	if (!PK_VERIFY(result == A_Err_NONE))
		return false;
	return ret;
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::LaunchEditorAsPopup(AAePk::SAAEIOData &AAEData, SEmitterDesc *desc)
{
	(void)AAEData;
	(void)desc;
	return CSystemHelper::LaunchEditorAsPopup();
}

//----------------------------------------------------------------------------

A_Err CPopcornFXWorld::DeathHook(AEGP_GlobalRefcon pluginRefCon, AEGP_DeathRefcon refCon)
{
	(void)refCon;
	(void)pluginRefCon;

	CPopcornFXWorld					&instance = AEGPPk::CPopcornFXWorld::Instance();
	const A_u_char					*commandName = (const A_u_char*)"PopcornFX";
	A_Err							result = PF_Err_NONE;
	SuiteHelper<AEGP_PanelSuite1>	panelSuite(instance.m_Suites);

	result |= panelSuite->AEGP_UnRegisterCreatePanelHook(commandName);

	instance.ShutdownIFN();

	if (!PK_VERIFY(result == A_Err_NONE))
		return result;
	return result;
}

//----------------------------------------------------------------------------

A_Err	CPopcornFXWorld::IdleHook(AEGP_GlobalRefcon pluginRefCon, AEGP_IdleRefcon refCon, A_long *maxSleep)
{
	(void)maxSleep;
	(void)refCon;
	(void)pluginRefCon;

	A_Err					err = A_Err_NONE;
	CPopcornFXWorld			&instance = AEGPPk::CPopcornFXWorld::Instance();

	instance.IdleUpdate();
	return err;
}

//----------------------------------------------------------------------------

void	CPopcornFXWorld::CreatePanel(AEGP_PlatformViewRef container, AEGP_PanelH panelH, AEGP_PanelFunctions1 *outFunctionTable, AEGP_PanelRefcon *outRefcon)
{
	(void)container;
	(void)panelH;
	(void)outFunctionTable;
	(void)outRefcon;
	
	CPanelBaseGUI	*panel = CPanelBaseGUI::GetInstance();

	panel->CreatePanel(m_Suites, panelH, container, outFunctionTable);
	*outRefcon = reinterpret_cast<AEGP_PanelRefcon>(panel);
}

//----------------------------------------------------------------------------

void	CPopcornFXWorld::Command(	AEGP_Command		command,
									AEGP_HookPriority	hookPriority,
									A_Boolean			alreadyHandled,
									A_Boolean			*handledP)
{
	(void)handledP;
	(void)alreadyHandled;
	(void)hookPriority;

	if (command == m_Command)
	{
		SuiteHelper<AEGP_PanelSuite1>		panelSuites(m_Suites);

		A_Boolean	shown = false;
		A_Boolean	topLevel = false;

		panelSuites->AEGP_IsShown((const A_u_char*)m_CommandName.Data(), &shown, &topLevel);
		if (!shown || !topLevel)
			panelSuites->AEGP_ToggleVisibility((const A_u_char*)m_CommandName.Data());
		*handledP = true;
	}
}

//----------------------------------------------------------------------------

void	CPopcornFXWorld::UpdateMenu(AEGP_WindowType activeWindow)
{
	(void)activeWindow;

	SuiteHelper<AEGP_PanelSuite1>		panelSuites(m_Suites);
	AEGP_SuiteHandler					suites(m_Suites);

	suites.CommandSuite1()->AEGP_EnableCommand(m_Command);

	A_Boolean	thumbIsShown = false;
	A_Boolean	panelIsFront = false;

	panelSuites->AEGP_IsShown((const A_u_char*)m_CommandName.Data(), &thumbIsShown, &panelIsFront);
	suites.CommandSuite1()->AEGP_CheckMarkMenuCommand(m_Command, thumbIsShown && panelIsFront);
}

//----------------------------------------------------------------------------

void	CPopcornFXWorld::ClearAttributesAndSamplers(SLayerHolder *layer)
{
	PK_ASSERT(layer != null);

	AEGP_SuiteHandler	suites(m_Suites);

	for (auto it = layer->m_SpawnedAttributes.Begin(); it != layer->m_SpawnedAttributes.End(); ++it)
	{
		layer->m_DeletedAttributes.Insert(it.Key(), *it);
		it->m_AttributeEffectRef = null;
	}
	layer->m_SpawnedAttributes.Clear();
	for (auto it = layer->m_SpawnedAttributesSampler.Begin(); it != layer->m_SpawnedAttributesSampler.End(); ++it)
	{
		layer->m_DeletedAttributesSampler.Insert(it.Key(), *it);
		it->m_AttributeEffectRef = null;
	}
	layer->m_SpawnedAttributesSampler.Clear();
}

//----------------------------------------------------------------------------

CStringId	CPopcornFXWorld::GetAEEffectID(AEGP_EffectRefH &effect, s32 paramIdx)
{
	A_Err					result = A_Err_NONE;
	AEGP_SuiteHandler		suites(m_Suites);
	AEGP_StreamRefH			streamRef = null;
	AEGP_MemHandle			nameHandle;

	result |= suites.StreamSuite2()->AEGP_GetNewEffectStreamByIndex(m_AEGPID, effect, paramIdx, &streamRef);
	if (!PK_VERIFY(result == A_Err_NONE))
		return CStringId::Null;

	result |= suites.StreamSuite5()->AEGP_GetStreamName(m_AEGPID, streamRef, false, &nameHandle);
	if (!PK_VERIFY(result == A_Err_NONE))
		return CStringId::Null;
	result |= suites.StreamSuite2()->AEGP_DisposeStream(streamRef);
	if (!PK_VERIFY(result == A_Err_NONE))
		return CStringId::Null;
	streamRef = null;

	aechar_t	*wname;
	CString		name;

	result |= suites.MemorySuite1()->AEGP_LockMemHandle(nameHandle, reinterpret_cast<void**>(&wname));
	if (!PK_VERIFY(result == A_Err_NONE))
		return CStringId::Null;
	WCharToCString(wname, &name);

	result |= suites.MemorySuite1()->AEGP_UnlockMemHandle(nameHandle);
	result |= suites.MemorySuite1()->AEGP_FreeMemHandle(nameHandle);

	if (!PK_VERIFY(result == A_Err_NONE))
		return CStringId::Null;
	if (!PK_VERIFY(name.Length() != 0))
		return CStringId::Null;
	return CStringId(name.Data());
}

//----------------------------------------------------------------------------

CStringId	CPopcornFXWorld::GetAttributeID(AEGP_EffectRefH &effect)
{
	return GetAEEffectID(effect, Attribute_Parameters_Infernal_Name);
}

//----------------------------------------------------------------------------

CStringId	CPopcornFXWorld::GetAttributeSamplerID(AEGP_EffectRefH &effect)
{
	return GetAEEffectID(effect, AttributeSamplerType_Parameters_Infernal_Name);
}

//----------------------------------------------------------------------------

CStringId	CPopcornFXWorld::GetAttributeID(SAttributeBaseDesc *desc)
{
	return CStringId(desc->GetAttributePKKey().data());
}

//----------------------------------------------------------------------------

s32		CPopcornFXWorld::_ExecSPendingEmitters(SLayerHolder *layer)
{
	PK_SCOPEDPROFILE();
	s32									emittersCount = 0;
	A_Err								result = A_Err_NONE;
	AEGP_SuiteHandler					suites(m_Suites);

	for (s32 i = layer->m_SPendingEmitters.Count() - 1; i >= 0; --i)
	{
		A_long				count;
		SPendingEmitter		&emitter = layer->m_SPendingEmitters[i];
		AEGP_LayerFlags		flags = AEGP_LayerFlag_NONE;

		result |= suites.LayerSuite8()->AEGP_GetLayerFlags(layer->m_EffectLayer, &flags);
		if ((flags & AEGP_LayerFlag_LOCKED) != AEGP_LayerFlag_NONE)
			continue;

		result |= suites.EffectSuite4()->AEGP_GetLayerNumEffects(layer->m_EffectLayer, &count);

		for (A_long j = count - 1; j >= 0; --j)
		{
			AEGP_EffectRefH				effectRef = null;
			AEGP_InstalledEffectKey		installedKey;
			PAAEScene					scene = layer->m_Scene;
			A_Time						fake_timeT = { 0, 100 };
			bool						releaseEffectHandle = true;

			result |= suites.EffectSuite4()->AEGP_GetLayerEffectByIndex(m_AEGPID, layer->m_EffectLayer, j, &effectRef);
			result |= suites.EffectSuite4()->AEGP_GetInstalledKeyFromLayerEffect(effectRef, &installedKey);

			if (installedKey == m_PKInstalledPluginKeys[EPKChildPlugins::EMITTER])
			{
				if (_SetupAutoRender(effectRef) == false)
					return -1;
				
				layer->m_SpawnedEmitter = emitter;
				scene->SetEmitterTeleport();
				scene->SetEmitterPosition(AAEToPK(emitter.m_Desc->m_Position), emitter.m_Desc->m_TransformType);
				scene->SetEmitterRotation(AngleAAEToPK(emitter.m_Desc->m_Rotation));

				if (!emitter.m_Desc->m_PathSource.empty())
					SetDestinationPackFromPath(*layer, CString(emitter.m_Desc->m_PathSource.c_str()));

				bool	forceRefresh = false;
				if (!emitter.m_Desc->m_PathSource.empty())
				{
					CString	sourcePackPath(emitter.m_Desc->m_PathSource.data());
					if (!CFilePath::IsAbsolute(emitter.m_Desc->m_PathSource.data()))
						sourcePackPath = m_AEProjectPath / sourcePackPath;

					CFilePath::Purify(sourcePackPath);
					CProjectSettingsFinder	walker(sourcePackPath);
					walker.Walk();

					CString effectName = emitter.m_Desc->m_Name.data();
					if (!m_VaultHandler.LoadEffectIntoVault(sourcePackPath, effectName, walker.ProjectSettingsPath(), forceRefresh))
						return false;
					emitter.m_Desc->m_Name = effectName.Data();
				}
				scene->RefreshAssetList();
				scene->SetLayerHolder(layer);
				scene->SetPack(layer->m_BakedPack, forceRefresh);
				scene->SetSelectedEffect(CString(emitter.m_Desc->m_Name.c_str()), forceRefresh);
				result |= suites.EffectSuite4()->AEGP_EffectCallGeneric(m_AEGPID, effectRef, &fake_timeT, PF_Cmd_COMPLETELY_GENERAL, emitter.m_Desc);

				SGetEmitterInfos infos;

				result |= suites.EffectSuite4()->AEGP_EffectCallGeneric(m_AEGPID, effectRef, &fake_timeT, PF_Cmd_COMPLETELY_GENERAL, &infos);

				CLog::Log(PK_INFO, "%s", infos.m_PathSource);
			}
			if (releaseEffectHandle)
				result |= suites.EffectSuite4()->AEGP_DisposeEffect(effectRef);
		}
		++emittersCount;
	}
	layer->m_SPendingEmitters.Clear();

	if (!PK_VERIFY(result == A_Err_NONE))
		return -1;
	return emittersCount;
}

//----------------------------------------------------------------------------

s32	CPopcornFXWorld::_ExecClearAttributes(SLayerHolder *layer)
{
	PK_SCOPEDPROFILE();
	s32					count = 0;
	A_Err				result = A_Err_NONE;
	AEGP_SuiteHandler	suites(m_Suites);
	A_long				effectCount = 0;

	PK_ASSERT(layer->m_EffectLayer != null);

	result |= suites.EffectSuite4()->AEGP_GetLayerNumEffects(layer->m_EffectLayer, &effectCount);

	for (A_long j = effectCount - 1; j >= 0; --j)
	{
		AEGP_EffectRefH				effectRef = null;
		AEGP_InstalledEffectKey		installedKey;

		result |= suites.EffectSuite4()->AEGP_GetLayerEffectByIndex(m_AEGPID, layer->m_EffectLayer, j, &effectRef);
		result |= suites.EffectSuite4()->AEGP_GetInstalledKeyFromLayerEffect(effectRef, &installedKey);

		if (installedKey == m_PKInstalledPluginKeys[EPKChildPlugins::ATTRIBUTE])
		{
			CStringId	id = GetAttributeID(effectRef);

			if (layer->m_DeletedAttributes.Contains(id))
			{
				SPendingAttribute	*attr = layer->m_DeletedAttributes[id];

				if (!_ExecDeleteAttribute(attr, effectRef))
					result |= A_Err_GENERIC;
				++count;
			}
			else if (layer->m_SpawnedAttributes.Contains(id))
			{
				SPendingAttribute	*attr = layer->m_SpawnedAttributes[id];

				if (attr->m_Deleted)
				{
					if (!_ExecDeleteAttribute(attr, effectRef))
						result |= A_Err_GENERIC;
					layer->m_SpawnedAttributes.Remove(id);
				}
			}
		}
		else if (installedKey == m_PKInstalledPluginKeys[EPKChildPlugins::SAMPLER])
		{
			CStringId	id = GetAttributeSamplerID(effectRef);

			if (layer->m_DeletedAttributesSampler.Contains(id))
			{
				SPendingAttribute	*attr = layer->m_DeletedAttributesSampler[id];

				if (!_ExecDeleteAttributeSampler(attr, effectRef))
					result |= A_Err_GENERIC;
				++count;
			}
			else if (layer->m_SpawnedAttributesSampler.Contains(id))
			{
				SPendingAttribute	*attr = layer->m_SpawnedAttributesSampler[id];

				if (attr->m_Deleted)
				{
					if (!_ExecDeleteAttributeSampler(attr, effectRef))
						result |= A_Err_GENERIC;
					layer->m_SpawnedAttributesSampler.Remove(id);
				}
			}
		}
		result |= suites.EffectSuite4()->AEGP_DisposeEffect(effectRef);
	}
	layer->m_DeletedAttributes.Clear();
	layer->m_DeletedAttributesSampler.Clear();

	if (!PK_VERIFY(result == A_Err_NONE))
		return -1;
	return count;
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::_ExecDeleteAttribute(SPendingAttribute *attribute, AEGP_EffectRefH &effectRef)
{
	A_Err				result = A_Err_NONE;
	AEGP_SuiteHandler	suites(m_Suites);

	if (attribute->m_Desc)
		attribute->m_Desc->m_IsDeleted = true;

	if (attribute->m_AttributeEffectRef != null)
	{
		A_Time				fake_timeT = { 0, 100 };
		result |= suites.EffectSuite4()->AEGP_EffectCallGeneric(m_AEGPID, attribute->m_AttributeEffectRef, &fake_timeT, PF_Cmd_COMPLETELY_GENERAL, attribute->m_Desc);
		result |= suites.EffectSuite4()->AEGP_DisposeEffect(attribute->m_AttributeEffectRef);
		attribute->m_AttributeEffectRef = null;
	}
	result |= suites.EffectSuite4()->AEGP_DeleteLayerEffect(effectRef);

	if (!PK_VERIFY(result == A_Err_NONE))
		return false;
	return result == A_Err_NONE;
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::_ExecDeleteAttributeSampler(SPendingAttribute *attribute, AEGP_EffectRefH &effectRef)
{
	A_Err				result = A_Err_NONE;
	AEGP_SuiteHandler	suites(m_Suites);

	if (attribute->m_Desc)
		attribute->m_Desc->m_IsDeleted = true;

	if (attribute->m_AttributeEffectRef != null)
	{
		A_Time	fake_timeT = { 0, 100 };
		result |= suites.EffectSuite4()->AEGP_EffectCallGeneric(m_AEGPID, attribute->m_AttributeEffectRef, &fake_timeT, PF_Cmd_COMPLETELY_GENERAL, attribute->m_Desc);
		result |= suites.EffectSuite4()->AEGP_DisposeEffect(attribute->m_AttributeEffectRef);
		attribute->m_AttributeEffectRef = null;
		PK_SAFE_DELETE(attribute->m_PKDesc);
	}
	result |= suites.EffectSuite4()->AEGP_DeleteLayerEffect(effectRef);

	if (!PK_VERIFY(result == A_Err_NONE))
		return false;
	return result == A_Err_NONE;
}

//----------------------------------------------------------------------------

s32			CPopcornFXWorld::_ExecSPendingAttributes(SLayerHolder *layer)
{
	PK_SCOPEDPROFILE();
	A_Err					result = A_Err_NONE;
	AEGP_SuiteHandler		suites(m_Suites);
	A_Time					fake_timeT = { 0, 100 };
	u32						spawned = 0;

	PK_ASSERT(layer != null);

	for (s32 i = layer->m_SPendingAttributes.Count() - 1; i >= 0 ; --i)
	{
		SPendingAttribute	&attribute = layer->m_SPendingAttributes[i];
		CStringId			id = GetAttributeID(layer->m_SPendingAttributes[i].m_Desc);
		A_long				count;
		bool				allreadySpawned = false;

		result |= suites.EffectSuite4()->AEGP_GetLayerNumEffects(layer->m_EffectLayer, &count);

		for (A_long j = count - 1; j >= 0; --j)
		{
			AEGP_EffectRefH				attributeRef = null;
			AEGP_InstalledEffectKey		installedKey;
			PAAEScene					scene = layer->m_Scene;

			result |= suites.EffectSuite4()->AEGP_GetLayerEffectByIndex(m_AEGPID, layer->m_EffectLayer, j, &attributeRef);
			result |= suites.EffectSuite4()->AEGP_GetInstalledKeyFromLayerEffect(attributeRef, &installedKey);

			if (installedKey == m_PKInstalledPluginKeys[EPKChildPlugins::ATTRIBUTE])
			{
				CStringId	spawnedID = GetAttributeID(attributeRef);

				if (spawnedID.ToString() == "AttributeName") //Default value, leave time to after effects to get its thing together.
				{
					result |= suites.EffectSuite4()->AEGP_DisposeEffect(attributeRef);
					return 0;
				}
				if (spawnedID == id)
				{
					attribute.m_AttributeEffectRef = attributeRef;

					if (layer->m_SpawnedAttributes.Contains(id))
					{
						if (layer->m_SpawnedAttributes[id]->m_AttributeEffectRef)
							result |= suites.EffectSuite4()->AEGP_DisposeEffect(attributeRef);
						if (layer->m_SpawnedAttributes[id]->m_Desc)
							delete layer->m_SpawnedAttributes[id]->m_Desc;
						layer->m_SpawnedAttributes[id]->m_Desc = attribute.m_Desc;
					}
					else
						layer->m_SpawnedAttributes.Insert(id, attribute);
					layer->m_SPendingAttributes[i].m_AttributeEffectRef = null;
					PK_ASSERT(attribute.m_Desc != null);

					result |= suites.EffectSuite4()->AEGP_EffectCallGeneric(m_AEGPID, layer->m_SpawnedAttributes[id]->m_AttributeEffectRef, &fake_timeT, PF_Cmd_COMPLETELY_GENERAL, attribute.m_Desc);

					layer->m_SPendingAttributes.Remove(i);
					allreadySpawned = true;
					break;
				}
			}
			else if (installedKey == m_PKInstalledPluginKeys[EPKChildPlugins::SAMPLER])
			{
				CStringId spawnedID = GetAttributeSamplerID(attributeRef);

				if (spawnedID.ToString() == "AttributeSamplerName") //Default value, leave time to after effects to get it's thing together.
				{
					result |= suites.EffectSuite4()->AEGP_DisposeEffect(attributeRef);
					return 0;
				}
				if (spawnedID == id)
				{
					attribute.m_AttributeEffectRef = attributeRef;

					if (layer->m_SpawnedAttributesSampler.Contains(id))
					{
						if (layer->m_SpawnedAttributesSampler[id]->m_AttributeEffectRef)
							result |= suites.EffectSuite4()->AEGP_DisposeEffect(attributeRef);
						if (layer->m_SpawnedAttributesSampler[id]->m_Desc)
							delete layer->m_SpawnedAttributesSampler[id]->m_Desc;
						layer->m_SpawnedAttributesSampler[id]->m_Desc = attribute.m_Desc;
					}
					else
						layer->m_SpawnedAttributesSampler.Insert(id, attribute);
					layer->m_SPendingAttributes[i].m_AttributeEffectRef = null;

					PK_ASSERT(attribute.m_Desc != null);

					result |= suites.EffectSuite4()->AEGP_EffectCallGeneric(m_AEGPID, layer->m_SpawnedAttributesSampler[id]->m_AttributeEffectRef, &fake_timeT, PF_Cmd_COMPLETELY_GENERAL, attribute.m_Desc);
					allreadySpawned = true;
					layer->m_SPendingAttributes.Remove(i);
					break;
				}
			}
			result |= suites.EffectSuite4()->AEGP_DisposeEffect(attributeRef);
		}
		if (allreadySpawned)
			continue;


		if ((attribute.m_Desc->m_IsAttribute && layer->m_SpawnedAttributes.Contains(id)) ||
			(!attribute.m_Desc->m_IsAttribute && layer->m_SpawnedAttributesSampler.Contains(id)))
			continue;

		AEGP_EffectRefH		effectRef = null;

		AEGP_LayerFlags		flags = AEGP_LayerFlag_NONE;
		result |= suites.LayerSuite8()->AEGP_GetLayerFlags(layer->m_EffectLayer, &flags);

		if (attribute.m_Desc->m_IsAttribute)
			result |= suites.EffectSuite4()->AEGP_ApplyEffect(m_AEGPID, layer->m_EffectLayer, m_PKInstalledPluginKeys[EPKChildPlugins::ATTRIBUTE], &effectRef);
		else
			result |= suites.EffectSuite4()->AEGP_ApplyEffect(m_AEGPID, layer->m_EffectLayer, m_PKInstalledPluginKeys[EPKChildPlugins::SAMPLER], &effectRef);
		if (result != A_Err_NONE || effectRef == null)
			return count;
		result |= suites.EffectSuite4()->AEGP_ReorderEffect(effectRef, layer->m_SPendingAttributes[i].m_Desc->m_Order);
		result |= suites.EffectSuite4()->AEGP_EffectCallGeneric(m_AEGPID, effectRef, &fake_timeT, PF_Cmd_COMPLETELY_GENERAL, layer->m_SPendingAttributes[i].m_Desc);
		attribute.m_AttributeEffectRef = effectRef;

		if (attribute.m_Desc->m_IsAttribute)
			layer->m_SpawnedAttributes.Insert(id, attribute);
		else
			layer->m_SpawnedAttributesSampler.Insert(id, attribute);
		attribute.m_AttributeEffectRef = null;
		spawned += 1;
	}
#if _DEBUG
	for (u32 i = 0; i < layer->m_SPendingAttributes.Count(); ++i)
	{
		PK_ASSERT(!layer->m_SPendingAttributes[i].m_AttributeEffectRef);
	}
#endif
	layer->m_SPendingAttributes.Clear();

	if (!PK_VERIFY(result == A_Err_NONE))
		return -1;
	return spawned;
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::_SetupAutoRender(AEGP_EffectRefH &effect)
{
	A_Err								result = A_Err_NONE;
	AEGP_SuiteHandler					suites(m_Suites);
	AEGP_StreamRefH						streamRef = null;
	AEGP_AddKeyframesInfoH				KeyFrameOperationHandle;
	A_Time								start, end;
	A_long								outIndex;
	AEGP_StreamValue2					keyValue;

	result |= suites.StreamSuite2()->AEGP_GetNewEffectStreamByIndex(m_AEGPID, effect, CAEUpdater::s_EmitterIndexes[Effect_Parameters_Infernal_Autorender], &streamRef);
	if (!PK_VERIFY(result == A_Err_NONE))
		return false;
	start.value = 0;
	start.scale = 1;
	PK_TODO("Handle Max time correctly");
	end.value = 1000;
	end.scale = 1;

	result |= suites.KeyframeSuite4()->AEGP_StartAddKeyframes(streamRef, &KeyFrameOperationHandle);
	if (!PK_VERIFY(result == A_Err_NONE))
		return false;
	if (KeyFrameOperationHandle)
	{
		//Start
		result |= suites.KeyframeSuite4()->AEGP_AddKeyframes(KeyFrameOperationHandle, AEGP_LTimeMode_CompTime, &start, &outIndex);
		//End
		result |= suites.KeyframeSuite4()->AEGP_AddKeyframes(KeyFrameOperationHandle, AEGP_LTimeMode_CompTime, &end, &outIndex);
		result |= suites.KeyframeSuite4()->AEGP_EndAddKeyframes(true, KeyFrameOperationHandle);
	}
	keyValue.streamH = streamRef;
	keyValue.val.one_d = 0;
	result |= suites.KeyframeSuite4()->AEGP_SetKeyframeValue(streamRef, 0, &keyValue);
	keyValue.val.one_d = 1000;
	result |= suites.KeyframeSuite4()->AEGP_SetKeyframeValue(streamRef, 1, &keyValue);
	result |= suites.StreamSuite2()->AEGP_DisposeStream(streamRef);
	streamRef = null;
	if (!PK_VERIFY(result == A_Err_NONE))
		return false;
	return true;
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::SetDestinationPackFromPath(SLayerHolder &layer, const CString &packPath)
{
	if (packPath.Empty())
		return false;
	CString absolutePath = packPath;
	if (!CFilePath::IsAbsolute(packPath))
	{
		absolutePath = m_AEProjectPath / absolutePath;
		CFilePath::Purify(absolutePath);
	}

	layer.m_BakedPack = m_VaultHandler.GetVaultPackFromPath(absolutePath);
	PK_ASSERT(layer.m_BakedPack != null);
	return true;
}

//----------------------------------------------------------------------------

CString		CPopcornFXWorld::GetInternalPackPath()
{
	return GetResourcesPath() + "PopcornFXInternals";
}

//----------------------------------------------------------------------------

CString		CPopcornFXWorld::GetResourcesPath()
{
#if defined(PK_WINDOWS)
	return GetPluginInstallationPath();
#elif defined(PK_MACOSX)
	return GetPluginInstallationPath() + "AE_GeneralPlugin.plugin/Contents/Resources/";
#endif
}

//----------------------------------------------------------------------------

CString CPopcornFXWorld::GetPluginVersion() const
{
	CString	version = PK_VERSION_CURRENT_STRING;
	return version;
}

//----------------------------------------------------------------------------

AEGP_InstalledEffectKey		CPopcornFXWorld::GetPluginEffectKey(EPKChildPlugins type) const
{
	return m_PKInstalledPluginKeys[type];
}

//----------------------------------------------------------------------------

CString	CPopcornFXWorld::GetPluginInstallationPath()
{
	return m_AEPath / "Plug-ins/PopcornFX/";
}

//----------------------------------------------------------------------------

void	CPopcornFXWorld::RefreshAssetList()
{
	for (u32 i = 0; i < m_Layers.Count(); ++i)
	{
		PAAEScene	scene = m_Layers[i]->m_Scene;
		scene->RefreshAssetList();
	}
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::SetSelectedEffectFromPath(SEmitterDesc *desc, CString path, bool forceReload)
{
	CFilePath::Purify(path);

	//Doesn't work with absolute path
	//if (!CFilePath::IsValidPath(path))
	//	return false;

	CString	root = CFilePath::StripFilename(path);
	CString	pkprojPath = "";

	if (!CFilePath::IsAbsolute(root))
	{
		root = m_AEProjectPath / root;
		CFilePath::Purify(root);
	}
	if (!CFilePath::IsAbsolute(path))
	{
		path = m_AEProjectPath / path;
		CFilePath::Purify(path);
	}
		
	while (root != "")
	{
		CProjectSettingsFinder walker(root);

		walker.Walk();

		if (!walker.ProjectSettingsPath().Empty())
		{
			pkprojPath = walker.ProjectSettingsPath();

			root = CFilePath::StripFilename(pkprojPath);
			CLog::Log(PK_INFO, "Pkproj found");
			break;
		}
		CFilePath::StripFilenameInPlace(root);
	}
	if (!root.Empty())
	{
		CString effectPath;

		effectPath = CFilePath::Relativize(root.Data(), path.Data());

		TArray<SLayerHolder*>	unload;
		bool					needUnload = true;
		for (u32 i = 0; i < m_Layers.Count(); ++i)
		{
			if (m_Layers[i]->m_SpawnedEmitter.m_Desc == null)
				continue;
			if (effectPath.Compare(m_Layers[i]->m_SpawnedEmitter.m_Desc->m_Name.c_str(), CaseInsensitive) == true)
			{
				if (!PK_VERIFY(unload.PushBack(m_Layers[i]).Valid()))
					return false;
				PK_SCOPEDLOCK(m_Layers[i]->m_LayerLock);
				needUnload = m_Layers[i]->m_Scene->ResetEffect(needUnload);
			}
		}
		PK_ASSERT(effectPath != null);
		bool	needRefresh = forceReload;
		if (!m_VaultHandler.LoadEffectIntoVault(root, effectPath, pkprojPath, needRefresh))
			return false;

		CString	pathDest = (m_VaultHandler.VaultPathCache() / CFilePath::ExtractFilename(root)).Data();

		for (u32 i = 0; i < unload.Count(); ++i)
		{
			SLayerHolder	*layer = unload[i];

			if (layer != null)
			{
				CString relativeSrcPath = CFilePath::Relativize(m_AEProjectPath.Data(), root.Data());

				desc->m_PathSource = !relativeSrcPath.Empty() ? relativeSrcPath.Data() : root.Data();
				desc->m_ReloadEffect = forceReload;
				SetSelectedEffect(layer, effectPath);
			}
		}
		SLayerHolder	*layer = GetLayerForSEmitterDesc(desc);
		if (layer != null && !unload.Contains(layer))
		{
			if (layer != null)
			{
				CString relativeSrcPath = CFilePath::Relativize(m_AEProjectPath.Data(), root.Data());

				desc->m_PathSource = !relativeSrcPath.Empty() ? relativeSrcPath.Data() : root.Data();
				desc->m_ReloadEffect = forceReload;
				SetSelectedEffect(layer, effectPath);
			}
		}
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::SetSelectedEffectAsync(SLayerHolder *targetLayer, CString &name)
{
	PK_ASSERT(targetLayer != null);

	PK_SCOPEDLOCK(m_UIEventLock);

	SUIEventString *event = PK_NEW(SUIEventString);

	event->m_TargetLayer = targetLayer;
	event->m_Cb = FastDelegate<bool(SLayerHolder *, CString &)>(this, &CPopcornFXWorld::SetSelectedEffect);
	event->m_Data = name;

	return m_UIEvents.PushBack(event).Valid();
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::SetEffectDefaultTransform(SLayerHolder *layer, const CFloat3 &position, const CFloat3 &rotation)
{
	(void)rotation;
	if (layer == null)
		return false;

	AEGP_SuiteHandler			suites(m_Suites);
	A_Err						result = A_Err_NONE;
	A_Time						time;

	time.value = 0;
	time.scale = layer->m_TimeScale;

	A_long						effectCount = 0;
	result |= suites.EffectSuite4()->AEGP_GetLayerNumEffects(layer->m_EffectLayer, &effectCount);

	for (A_long j = effectCount - 1; j >= 0; --j)
	{
		AEGP_EffectRefH				effectRef = null;
		AEGP_InstalledEffectKey		installedKey;

		result |= suites.EffectSuite4()->AEGP_GetLayerEffectByIndex(m_AEGPID, layer->m_EffectLayer, j, &effectRef);
		result |= suites.EffectSuite4()->AEGP_GetInstalledKeyFromLayerEffect(effectRef, &installedKey);

		if (installedKey == GetPluginEffectKey(EPKChildPlugins::EMITTER))
		{
			CFloat4x4			viewMatrix;
			CFloat4				cameraPos;
			float				cameraZoom = 0.f;

			CAEUpdater::GetCameraViewMatrixAtTime(layer, viewMatrix, cameraPos, time, cameraZoom);

			CFloat3 forward = viewMatrix.Inverse().StrippedZAxis().Normalized();
			
			CFloat3 emitterPos = cameraPos.xyz() + forward * position.Length();

			AEGP_StreamRefH				streamPos = null;
			result |= suites.StreamSuite5()->AEGP_GetNewEffectStreamByIndex(m_AEGPID, effectRef, CAEUpdater::s_EmitterIndexes[Effect_Parameters_Position], &streamPos);
			PK_ASSERT(result == A_Err_NONE);
			if (result != A_Err_NONE || streamPos == null)
			{
				result |= suites.EffectSuite4()->AEGP_DisposeEffect(effectRef);
				return false;
			}
			AEGP_StreamValue2	value;
			result |= suites.StreamSuite5()->AEGP_GetNewStreamValue(m_AEGPID, streamPos, AEGP_LTimeMode_LayerTime, &time, false, &value);
			if (result != A_Err_NONE)
			{
				result |= suites.EffectSuite4()->AEGP_DisposeEffect(effectRef);
				return false;
			}
			value.val.three_d.x = emitterPos.x();
			value.val.three_d.y = emitterPos.y();
			value.val.three_d.z = emitterPos.z();
			result |= suites.StreamSuite5()->AEGP_SetStreamValue(m_AEGPID, streamPos, &value);
			if (result != A_Err_NONE)
			{
				result |= suites.EffectSuite4()->AEGP_DisposeEffect(effectRef);
				return false;
			}
			result |= suites.StreamSuite5()->AEGP_DisposeStreamValue(&value);
			result |= suites.StreamSuite5()->AEGP_DisposeStream(streamPos);
		}
		result |= suites.EffectSuite4()->AEGP_DisposeEffect(effectRef);
		
	}
	return true;
}


//----------------------------------------------------------------------------

bool	CPopcornFXWorld::SetBackdropMeshDefaultTransform(SLayerHolder *layer)
{
	if (layer == null)
		return false;

	AEGP_SuiteHandler			suites(m_Suites);
	A_Err						result = A_Err_NONE;
	A_Time						time;

	time.value = 0;
	time.scale = layer->m_TimeScale;

	A_long						effectCount = 0;
	result |= suites.EffectSuite4()->AEGP_GetLayerNumEffects(layer->m_EffectLayer, &effectCount);

	for (A_long j = effectCount - 1; j >= 0; --j)
	{
		AEGP_EffectRefH				effectRef = null;
		AEGP_InstalledEffectKey		installedKey;

		result |= suites.EffectSuite4()->AEGP_GetLayerEffectByIndex(m_AEGPID, layer->m_EffectLayer, j, &effectRef);
		result |= suites.EffectSuite4()->AEGP_GetInstalledKeyFromLayerEffect(effectRef, &installedKey);

		if (installedKey == GetPluginEffectKey(EPKChildPlugins::EMITTER))
		{
			A_FloatPoint3 emitterPos = layer->m_SpawnedEmitter.m_Desc->m_Position;

			AEGP_StreamRefH				streamPos = null;
			result |= suites.StreamSuite5()->AEGP_GetNewEffectStreamByIndex(m_AEGPID, effectRef, CAEUpdater::s_EmitterIndexes[Effect_Parameters_BackdropMesh_Position], &streamPos);
			PK_ASSERT(result == A_Err_NONE);
			if (result != A_Err_NONE || streamPos == null)
			{
				result |= suites.EffectSuite4()->AEGP_DisposeEffect(effectRef);
				return false;
			}
			AEGP_StreamValue2	value;
			result |= suites.StreamSuite5()->AEGP_GetNewStreamValue(m_AEGPID, streamPos, AEGP_LTimeMode_LayerTime, &time, false, &value);
			if (result != A_Err_NONE)
			{
				result |= suites.EffectSuite4()->AEGP_DisposeEffect(effectRef);
				return false;
			}
			value.val.three_d.x = emitterPos.x;
			value.val.three_d.y = emitterPos.y;
			value.val.three_d.z = emitterPos.z;
			result |= suites.StreamSuite5()->AEGP_SetStreamValue(m_AEGPID, streamPos, &value);
			if (result != A_Err_NONE)
			{
				result |= suites.EffectSuite4()->AEGP_DisposeEffect(effectRef);
				return false;
			}
			result |= suites.StreamSuite5()->AEGP_DisposeStreamValue(&value);
			result |= suites.StreamSuite5()->AEGP_DisposeStream(streamPos);
		}
		result |= suites.EffectSuite4()->AEGP_DisposeEffect(effectRef);

	}
	return true;
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::SetSelectedEffect(SLayerHolder *layer, CString &fileName)
{
	if (layer != null)
	{
		PK_SCOPEDLOCK(layer->m_LayerLock);
		SEmitterDesc	*desc = layer->m_SpawnedEmitter.m_Desc;
		CString			name = CFilePath::StripExtension(fileName);
		if (desc)
		{
			bool	setTransform = false;
			CString oldName = CFilePath::StripExtension(CString(desc->m_Name.c_str()));
			if (!name.Compare(oldName))
			{
				setTransform = true;
				ClearAttributesAndSamplers(layer);
				if (layer->m_LayerProperty != null)
				{
					CLayerProperty::_TypeOfRendererProperties	overrides = layer->m_LayerProperty->RendererProperties();

					overrides.Clear();
					layer->m_LayerProperty->SetRendererProperties(overrides);
				}
			}

			SetDestinationPackFromPath(*layer, layer->m_SpawnedEmitter.m_Desc->m_PathSource.data());
			if (!layer->m_Scene->SetPack(layer->m_BakedPack, desc->m_ReloadEffect))
				return false;

			//Set Default Effect position according to the Editor
			{
				//Load Editor information into the effect
				if (setTransform)
				{
					IFileSystem	*fileSystem = File::DefaultFileSystem();
					CString		pkboPath = layer->m_BakedPack->Path() / name + ".pkbo";
					u32			rawFileSize = 0;
					u8			*rawFileBuffer = fileSystem->Bufferize(pkboPath, &rawFileSize, true);
					if (rawFileBuffer != null)
					{
						CConstMemoryStream	memoryFileView(rawFileBuffer, rawFileSize);

						PBaseObjectFile		pkboFile = layer->m_Scene->GetContext()->LoadFileFromStream(memoryFileView, pkboPath);
						PEditorAssetEffect	editorProp = pkboFile->FindFirstOf<CEditorAssetEffect>();

						CFloat3				emitterDefaultPosition = -editorProp->StartCameraPosition();
						CFloat3				emitterDefaultOrientation = -editorProp->StartCameraOrientation();

						SetEffectDefaultTransform(layer, emitterDefaultPosition, emitterDefaultOrientation);
					}
				}
			}


			if (!layer->m_Scene->SetSelectedEffect(fileName, desc->m_ReloadEffect))
				return false;

			desc->m_Name = fileName.Data();
			desc->m_Update = true;
			return true;
		}
	}
	return true;
}

//----------------------------------------------------------------------------

// - OpenGL resources are restricted per thread, mimicking the OGL driver
// - The filter will eliminate all TLS (Thread Local Storage) at PF_Cmd_GLOBAL_SETDOWN
PAAERenderContext		CPopcornFXWorld::GetCurrentRenderContext()
{
	// Lazy init:
	if (s_AAEThreadRenderContexts == null)
	{
		s_AAEThreadRenderContexts = PK_NEW(CAAERenderContext);
		s_ShaderLoader = PK_NEW(PKSample::CShaderLoader);

		if (!PK_VERIFY(s_AAEThreadRenderContexts != null && s_ShaderLoader != null))
			return null;

		s_AAEThreadRenderContexts->SetShaderLoader(s_ShaderLoader);

		if (!PK_VERIFY(s_AAEThreadRenderContexts->InitializeIFN(GetRenderApi(), CString::Format("PopcornFXRendering"))))
			return null;
	}
	return s_AAEThreadRenderContexts;
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::SetPanelInstance(CPanelBaseGUI *panel)
{
	m_Panel = panel;
	return true;
}

//----------------------------------------------------------------------------

A_Err	CPopcornFXWorld::CreatePanelHook(	AEGP_GlobalRefcon		pluginRefconP,
											AEGP_CreatePanelRefcon	refconP,
											AEGP_PlatformViewRef	container,
											AEGP_PanelH				panelH,
											AEGP_PanelFunctions1	*outFunctionTable,
											AEGP_PanelRefcon		*outRefcon)
{
	(void)pluginRefconP;
	(void)refconP;

	CPopcornFXWorld			&instance = AEGPPk::CPopcornFXWorld::Instance();

	instance.CreatePanel(container, panelH, outFunctionTable, outRefcon);
	return A_Err_NONE;
}

//----------------------------------------------------------------------------

A_Err	CPopcornFXWorld::CommandHook(	AEGP_GlobalRefcon plugin_refconP,
										AEGP_CommandRefcon refconP,
										AEGP_Command command,
										AEGP_HookPriority hook_priority,
										A_Boolean already_handledB,
										A_Boolean *handledPB)
{
	(void)refconP;
	(void)plugin_refconP;

	CPopcornFXWorld			&instance = AEGPPk::CPopcornFXWorld::Instance();

	instance.Command(command, hook_priority, already_handledB, handledPB);
	return A_Err_NONE;
}

//----------------------------------------------------------------------------

A_Err	CPopcornFXWorld::UpdateMenuHook(AEGP_GlobalRefcon pluginRefconP,
										AEGP_UpdateMenuRefcon refconP,
										AEGP_WindowType activeWindow)
{
	(void)refconP;
	(void)pluginRefconP;

	CPopcornFXWorld			&instance = AEGPPk::CPopcornFXWorld::Instance();

	instance.UpdateMenu(activeWindow);
	return A_Err_NONE;
}

//----------------------------------------------------------------------------

void	CPopcornFXWorld::OnEndSetupScene()
{
	if (m_Panel)
	{
		m_Panel->UpdateScenesModel();
	}
}

const PBaseObjectFile &CPopcornFXWorld::GetProjectConfFile()
{
	return m_ProjectConfFile;
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::GetMostRecentCompName(CString &compName)
{
	AEGP_SuiteHandler	suites(m_Suites);
	AEGP_CompH			compH = null;
	AEGP_ItemH			itemH = null;
	AEGP_MemHandle		memH = null;
	aechar_t			*compositionNameAE = null;
	CString				compositionName = "";
	PF_Err				result = A_Err_NONE;
	result |= suites.CompSuite11()->AEGP_GetMostRecentlyUsedComp(&compH);
	if (result != A_Err_NONE)
		return false;
	if (compH != null)
	{
		result |= suites.CompSuite11()->AEGP_GetItemFromComp(compH, &itemH);
		if (result != A_Err_NONE)
			return false;
		result |= suites.ItemSuite9()->AEGP_GetItemName(m_AEGPID, itemH, &memH);

		result |= suites.MemorySuite1()->AEGP_LockMemHandle(memH, reinterpret_cast<void **>(&compositionNameAE));

		WCharToCString(compositionNameAE, &compositionName);

		result |= suites.MemorySuite1()->AEGP_UnlockMemHandle(memH);
		result |= suites.MemorySuite1()->AEGP_FreeMemHandle(memH);

		compName = compositionName;
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::SetLayerName(SLayerHolder *layer)
{
	A_Err				result = A_Err_NONE;
	AEGP_SuiteHandler	suites(m_Suites);
	AEGP_ItemH			itemH = null;
	AEGP_MemHandle		memH = null;
	aechar_t			*layerNameAE = null;
	CString				layerName = "";

	result |= suites.LayerSuite8()->AEGP_GetLayerSourceItem(layer->m_EffectLayer, &itemH);
	if (result != A_Err_NONE)
		return false;

	result |= suites.ItemSuite9()->AEGP_GetItemName(m_AEGPID, itemH, &memH);
	if (result != A_Err_NONE)
		return false;

	result |= suites.MemorySuite1()->AEGP_LockMemHandle(memH, reinterpret_cast<void **>(&layerNameAE));
	if (result != A_Err_NONE)
		return false;

	WCharToCString(layerNameAE, &layerName);

	result |= suites.MemorySuite1()->AEGP_UnlockMemHandle(memH);
	if (result != A_Err_NONE)
		return false;
	result |= suites.MemorySuite1()->AEGP_FreeMemHandle(memH);
	if (result != A_Err_NONE)
		return false;

	layer->m_LayerName = layerName;

	return true;
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::SetLayerCompName(SLayerHolder *layer)
{
	A_Err				result = A_Err_NONE;
	AEGP_SuiteHandler	suites(m_Suites);
	AEGP_CompH			compH = null;
	AEGP_ItemH			itemH = null;
	AEGP_MemHandle		memH = null;
	aechar_t			*compositionNameAE = null;
	CString				compositionName = "";
	result |= suites.LayerSuite5()->AEGP_GetLayerParentComp(layer->m_EffectLayer, &compH);
	if (result != A_Err_NONE)
		return false;
	result |= suites.CompSuite11()->AEGP_GetItemFromComp(compH, &itemH);
	if (result != A_Err_NONE)
		return false;
	result |= suites.ItemSuite9()->AEGP_GetItemName(m_AEGPID, itemH, &memH);
	if (result != A_Err_NONE)
		return false;
	result |= suites.MemorySuite1()->AEGP_LockMemHandle(memH, reinterpret_cast<void **>(&compositionNameAE));
	if (result != A_Err_NONE)
		return false;
	WCharToCString(compositionNameAE, &compositionName);

	result |= suites.MemorySuite1()->AEGP_UnlockMemHandle(memH);
	if (result != A_Err_NONE)
		return false;
	result |= suites.MemorySuite1()->AEGP_FreeMemHandle(memH);
	if (result != A_Err_NONE)
		return false;
	layer->m_CompositionName = compositionName;
	if (m_MostRecentCompName != compositionName)
	{
		if (m_Panel)
			m_Panel->UpdateScenesModel();
		m_MostRecentCompName = compositionName;
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::CreateLayerPropertyIFP(SLayerHolder *layer)
{
	if (m_ProjectProperty == null)
		return false;
	
	// Go through all objects in file:
	for (const auto &obj : m_ProjectProperty->LayerProperties())
	{
		if (obj->ID() == layer->ID &&
			obj->CompName() == layer->m_CompositionName)
			layer->m_LayerProperty = obj.Get();

	}
	if (layer->m_LayerProperty == null)
	{
		CAEPProjectProperties::_TypeOfLayerProperties	LayerProps = m_ProjectProperty->LayerProperties();
		PLayerProperty									prop = m_ProjectProperty->File()->Context()->NewObject<CLayerProperty>(m_ProjectConfFile.Get());

		if (!LayerProps.PushBack(prop).Valid())
			return false;
		layer->m_LayerProperty = prop;

		prop->SetCompName(layer->m_CompositionName); // Useless ?
		prop->SetID(layer->ID);

		m_ProjectProperty->SetLayerProperties(LayerProps);
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CPopcornFXWorld::SetResourceOverride(CStringId layerID, u32 rdrID, u32 propID, const CString &value)
{
	IFileSystem			*fileSystem = File::DefaultFileSystem();
	SLayerHolder		*layerHolder = GetLayerForSEmitterDescID(layerID);

	if (layerHolder == null || layerHolder->m_LayerProperty == null)
		return false;

	PK_SCOPEDLOCK(layerHolder->m_LayerLock);
	CLayerProperty::_TypeOfRendererProperties	overrides = layerHolder->m_LayerProperty->RendererProperties();
	CGraphicOverride							*existingOverride = null;
	bool										updated = false;
	for (const auto &entry : overrides)
	{
		if (entry->RendererID() == rdrID && entry->PropertyID() == propID)
		{
			existingOverride = entry.Get();
			if (value.Empty())
			{
				overrides.RemoveElementFromRawPointerInArray_AndKeepOrder(&entry);
				layerHolder->m_LayerProperty->SetRendererProperties(overrides);
				updated = true;
			}
			break;
		}
	}

	if (!value.Empty())
	{
		AEGPPk::SResourceBakeConfig	bakeConfig;
		bakeConfig.m_StraightCopy = true;
		CString virtualPath = fileSystem->PhysicalToVirtual(value);
		if (virtualPath == null) //Not in loaded packs, bake into resources folder
		{
			virtualPath = fileSystem->PhysicalToVirtual(GetVaultHandler().BakeResource(value, bakeConfig));
		}
		if (existingOverride != null)
		{
			if (!existingOverride->Value().Compare(virtualPath))
			{
				existingOverride->SetValue(virtualPath);
				updated = true;
			}
		}
		else
		{
			PGraphicOverride	prop = m_ProjectProperty->File()->Context()->NewObject<CGraphicOverride>(m_ProjectConfFile.Get());

			prop->SetRendererID(rdrID);
			prop->SetPropertyID(propID);
			prop->SetValue(virtualPath);

			if (!overrides.PushBack(prop).Valid())
				return false;

			layerHolder->m_LayerProperty->SetRendererProperties(overrides);
			updated = true;
		}
	}
	if (updated)
	{
		layerHolder->m_ForceRender = true;
		WriteProjectFileModification();
		layerHolder->m_Scene->SetupScene(true, true);
	
	}
	return true;
}

//----------------------------------------------------------------------------

bool CPopcornFXWorld::WriteProjectFileModification()
{
	if (m_ProjectConfFile == null)
		return false;
	IFileSystem	*fs = File::DefaultFileSystem();

	if (fs->Exists(m_AEProjectPath, true))
	{
		if (!PK_VERIFY(HBO::g_Context->WriteFile(m_ProjectConfFile.Get(), m_ProjectConfFile->Path())))
			return false;
	}
	return true;
}

//----------------------------------------------------------------------------
__AEGP_PK_END
