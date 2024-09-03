//----------------------------------------------------------------------------
// This program is the property of Persistant Studios SARL.
//
// You may not redistribute it and/or modify it under any conditions
// without written permission from Persistant Studios SARL, unless
// otherwise stated in the latest Persistant Studios Code License.
//
// See the Persistant Studios Code License for further details.
//----------------------------------------------------------------------------

#include "precompiled.h"

#include "PopcornStartup.h"

#include <pk_toolkit/include/pk_toolkit_version.h>
#include <pk_kernel/include/kr_static_config_flags.h>
#include <pkapi_library.h>

#include <pk_kernel/include/kr_init.h>
#include <pk_base_object/include/hb_init.h>
#include <pk_engine_utils/include/eu_init.h>
#include <pk_compiler/include/cp_init.h>
#include <pk_imaging/include/im_init.h>
#include <pk_geometrics/include/ge_init.h>
#include <pk_particles/include/ps_init.h>
#include <pk_particles_toolbox/include/pt_init.h>
#include <pk_render_helpers/include/rh_init.h>

#include <pk_particles/include/ps_system.h>
#include <pk_kernel/include/kr_plugins.h>
#include <pk_kernel/include/kr_log_listeners.h>
#include <pk_kernel/include/kr_log_listeners_file.h>
#include <pk_kernel/include/kr_thread_pool_default.h>
#include <pk_version_base.h>

#include <pk_rhi/include/RHIInit.h>
#include <PK-SampleLib/PKSampleInit.h>

#if KR_PROFILER_ENABLED != 0
#	if defined(PK_BUILD_WITH_D3D12_SUPPORT) && (PK_BUILD_WITH_D3D12_SUPPORT != 0)
#		include <PKPix.h>
#	elif defined(PK_BUILD_WITH_UNKNOWN2_SUPPORT) && (PK_BUILD_WITH_UNKNOWN2_SUPPORT != 0)
#		include <PKRazor.h>
#	endif
#endif // KR_PROFILER_ENABLED != 0

//----------------------------------------------------------------------------

PK_LOG_MODULE_DEFINE();

#if (defined(PK_ANDROID) || defined(PK_WINAPI) || defined(PK_LINUX) || defined(PK_MACOSX) || defined(PK_IOS))
#	define USE_PLUGIN_CODEC_PKM		1
#else
#	define USE_PLUGIN_CODEC_PKM		0
#endif

#if (defined(PK_IOS) || defined(PK_WINAPI) || defined(PK_LINUX) || defined(PK_MACOSX) || defined(PK_IOS))
#	define USE_PLUGIN_CODEC_PVR		1
#else
#	define USE_PLUGIN_CODEC_PVR		0
#endif

#if !defined(PK_COMPILER_BUILD_COMPILER_D3D) || \
	!defined(PK_COMPILER_BUILD_COMPILER_UNKNOWN2)
#	error Configuration error: Should be defined in ps_config.h
#endif

#if !defined(USE_COMPILER_BACKEND_D3D)
#	if	(PK_COMPILER_BUILD_COMPILER_D3D != 0)
#		define	USE_COMPILER_BACKEND_D3D
#	endif
#endif
#if !defined(USE_COMPILER_BACKEND_UNKNOWN2)
#	if	(PK_COMPILER_BUILD_COMPILER_UNKNOWN2 != 0)
#		define	USE_COMPILER_BACKEND_UNKNOWN2
#	endif
#endif

//----------------------------------------------------------------------------

PK_PLUGIN_DECLARE(CCompilerBackendCPU_VM);
#if (PK_COMPILER_BUILD_COMPILER_ISPC != 0)
	PK_PLUGIN_DECLARE(CCompilerBackendCPU_ISPC);
#endif
#if defined(USE_COMPILER_BACKEND_D3D)
	PK_PLUGIN_DECLARE(CCompilerBackendGPU_D3D);
#endif
#if defined(USE_COMPILER_BACKEND_UNKNOWN2)
	PK_PLUGIN_DECLARE(CCompilerBackendGPU_PSSLC);
#endif
PK_PLUGIN_DECLARE(CImagePKIMCodec);
PK_PLUGIN_DECLARE(CImageDDSCodec);
PK_PLUGIN_DECLARE(CImagePNGCodec);
PK_PLUGIN_DECLARE(CImageTGACodec);
PK_PLUGIN_DECLARE(CImageJPEGCodec);
PK_PLUGIN_DECLARE(CImageHDRCodec);
#if USE_PLUGIN_CODEC_PKM
	PK_PLUGIN_DECLARE(CImagePKMCodec);
#endif
#if USE_PLUGIN_CODEC_PVR
	PK_PLUGIN_DECLARE(CImagePVRCodec);
#endif

//----------------------------------------------------------------------------

namespace
{
	struct	SPlugin
	{
		PK_NAMESPACE::IPluginModule	*(*m_Startup)();
		PK_NAMESPACE::IPluginModule	*(*m_Get)();
		void						(*m_Shutdown)();
		const char					*m_Path;
	};

	// List of PopcornFX plugins to load alongside PopcornFX runtime SDK
	// If your integration code overrides the SDK's default resource manager, you won't need to register image codecs
	// as it'll load your resources directly
#define	PLUGIN_DEF(__name, __path)		{ &PK_NAMESPACE::PK_GLUE(StartupPlugin_, __name), &PK_NAMESPACE::PK_GLUE(GetPlugin_, __name), &PK_NAMESPACE::PK_GLUE(ShutdownPlugin_, __name), __path }
	const SPlugin	g_Plugins[] =
	{
		PLUGIN_DEF(CCompilerBackendCPU_VM, "Plugins/compiler_backend_cpu_vm"),
#if	defined(USE_COMPILER_BACKEND_D3D)
		PLUGIN_DEF(CCompilerBackendGPU_D3D, "Plugins/compiler_backend_gpu_d3d"),
#endif
#if	defined(USE_COMPILER_BACKEND_UNKNOWN2)
		PLUGIN_DEF(CCompilerBackendGPU_PSSLC, "Plugins/compiler_backend_gpu_psslc"),
#endif
		PLUGIN_DEF(CImagePKIMCodec, "Plugins/codec_pkim"),
		PLUGIN_DEF(CImageDDSCodec, "Plugins/codec_dds"),
		PLUGIN_DEF(CImagePNGCodec, "Plugins/codec_png"),
		PLUGIN_DEF(CImageTGACodec, "Plugins/codec_tga"),
		PLUGIN_DEF(CImageJPEGCodec, "Plugins/codec_jpeg"),
		PLUGIN_DEF(CImageHDRCodec, "Plugins/codec_hdr"),
#	if USE_PLUGIN_CODEC_PKM
		PLUGIN_DEF(CImagePKMCodec, "Plugins/codec_pkm"),
#	endif
#	if USE_PLUGIN_CODEC_PVR
		PLUGIN_DEF(CImagePVRCodec, "Plugins/codec_pvr"),
#	endif
//#endif
	};
#undef PLUGIN_DEF
}

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

static void		AddStdOutLogListener(void *)
{
#if	(PK_LOG_ENABLED != 0)
	CLog::AddGlobalListener(PK_NEW(CLogListenerFile("popcorn.htm", "popcorn-engine logfile")));
#	if	(PK_LOG_DEV_LOGGERS_ENABLED != 0) && !defined(PK_RETAIL)
	CLog::AddGlobalListener(PK_NEW(CLogListenerDebug()));
#		if	defined(PK_WINAPI) && defined(PK_COMPILER_MSVC)
	CLog::AddGlobalListener(PK_NEW(CLogListenerStdOut()));
#		endif
#	endif
#endif	// (PK_LOG_ENABLED != 0)
}

//----------------------------------------------------------------------------

// This worker thread pool creation is very basic right now, we'll improve that in future beta versions
Threads::PAbstractPool		_CreateThreadPool()
{
	// You can either use the default PopcornFX thread pool, so you can specify amount of worker threads and their affinities.
	// Or create your own, and submit PopcornFX CPU tasks in your engine workers
	PWorkerThreadPool	pool = PK_NEW(CWorkerThreadPool);

	if (!PK_VERIFY(pool != null))
		return null;

#if	0//defined(PK_WINAPI)
	// Pin each worker to a physical HW thread.
	CWorkerThreadPool::SNumaWorkersConfig	config;
	const bool		success = pool->AddNUMAWorkers(config);
#else
	// Let the OS shedule our workers
	// leave 1 core for main thread
	const u32		processorCount = PKMax(CPU::Caps().ProcessAffinity().NumBitsSet(), 2U) - 1U;
	const bool		success = pool->AddFullAffinityWorkers(processorCount, CPU::Caps().ProcessAffinity(), CThreadManager::Priority_High);
#endif

	if (!success)
		return null;

	pool->StartWorkers();
	return pool;
}

//----------------------------------------------------------------------------

bool	PopcornStartup(bool logOnStdOut, File::FnNewFileSystem newFileSys, Scheduler::FnCreateThreadPool newThreadPool)
{
	SDllVersion		engineVersion;

	PK_ASSERT(engineVersion.Major == PK_VERSION_MAJOR);
	PK_ASSERT(engineVersion.Minor == PK_VERSION_MINOR);
#ifdef	PK_DEBUG
	PK_ASSERT(engineVersion.Debug == true);
#else
	PK_ASSERT(engineVersion.Debug == false);
#endif
	CPKKernel::Config	configKernel;

	if (logOnStdOut)
		configKernel.m_AddDefaultLogListeners = &AddStdOutLogListener;

	if (newThreadPool != null)
		configKernel.m_CreateThreadPool = newThreadPool;
	else
		configKernel.m_CreateThreadPool = &_CreateThreadPool;

	configKernel.m_NewFileSystem = newFileSys;

#if KR_PROFILER_ENABLED != 0
#	if defined(PK_BUILD_WITH_D3D12_SUPPORT) && (PK_BUILD_WITH_D3D12_SUPPORT != 0)
	configKernel.m_ProfilerRecordEventStartGPU = &PKPixBeginEvent;
	configKernel.m_ProfilerRecordEventEndGPU = &PKPixEndEvent;
#	elif defined(PK_BUILD_WITH_UNKNOWN2_SUPPORT) && (PK_BUILD_WITH_UNKNOWN2_SUPPORT != 0)
	configKernel.m_ProfilerRecordEventStartGPU = &PKRazorBeginEvent;
	configKernel.m_ProfilerRecordEventEndGPU = &PKRazorEndEvent;
#	endif
#endif

	// Startup all PopcornFX critical modules, each module's config can be overridden
	if (CPKKernel::Startup(engineVersion, configKernel) &&
		CPKBaseObject::Startup(engineVersion, CPKBaseObject::Config()) &&
		CPKEngineUtils::Startup(engineVersion, CPKEngineUtils::Config()) &&
		CPKCompiler::Startup(engineVersion, CPKCompiler::Config()) &&
		CPKImaging::Startup(engineVersion, CPKImaging::Config()) &&
		CPKGeometrics::Startup(engineVersion, CPKGeometrics::Config()) &&
		CPKParticles::Startup(engineVersion, CPKParticles::Config()) &&
		ParticleToolbox::Startup() &&
		CPKRenderHelpers::Startup(engineVersion, CPKRenderHelpers::Config()) &&
		CPKRHI::Startup(engineVersion, CPKRHI::Config()) &&						// Only necessary if your engine links/relies on PKRHI
		CPKSample::Startup(engineVersion, CPKSample::Config()) &&				// Only necessary if your engine links/relies on PKSample
		PK_VERIFY(Kernel::CheckStaticConfigFlags(Kernel::g_BaseStaticConfig, SKernelConfigFlags())))
	{
		for (u32 i = 0; i < PK_ARRAY_COUNT(g_Plugins); ++i)
		{
			IPluginModule		*plugin = (*g_Plugins[i].m_Startup)();
			if (plugin == null || !CPluginManager::PluginRegister(plugin, true, g_Plugins[i].m_Path))
			{
				CLog::Log(PK_INFO, "Failed to load plugin %s", g_Plugins[i].m_Path);
				return false;
			}
		}

		// Optional, don't place that if PopcornFX startup is done in your engine's main thread
		// As this is probably done by your engine
		CThreadManager::SetProcessPriority(CThreadManager::Process_High);

		// Specify your engine's coordinate system & unit-system
		CParticleManager::SetGlobalFrame(Frame_RightHand_Y_Up);
		CParticleManager::SetDistanceUnit(Units::Meter);
		return true;
	}

	PopcornShutdown();	// shutdown the modules we were able to load...
	return false;
}

//----------------------------------------------------------------------------

bool	PopcornShutdown()
{
	for (u32 i = 0; i < PK_ARRAY_COUNT(g_Plugins); ++i)
	{
		IPluginModule		*plugin = (*g_Plugins[i].m_Get)();
		if (plugin != null)
			CPluginManager::PluginRelease(plugin);
		(*g_Plugins[i].m_Shutdown)();
	}

	CPKSample::Shutdown();		// Only necessary if your engine links/relies on PKSample
	CPKRHI::Shutdown();			// Only necessary if your engine links/relies on PKRHI
	CPKRenderHelpers::Shutdown();
	ParticleToolbox::Shutdown();
	CPKParticles::Shutdown();
	CPKGeometrics::Shutdown();
	CPKImaging::Shutdown();
	CPKCompiler::Shutdown();
	CPKEngineUtils::Shutdown();
	CPKBaseObject::Shutdown();
	CPKKernel::Shutdown();
	return true;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
