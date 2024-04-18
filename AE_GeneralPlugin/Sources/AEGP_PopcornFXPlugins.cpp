//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"

#include "AEGP_PopcornFXPlugins.h"

#include <pk_particles/include/ps_config.h>
#include <pk_kernel/include/kr_plugins.h>

#define		USE_COMPILER_BACKEND
#define		USE_IMAGE_PLUGINS
#define		USE_IMAGE_PLUGIN_TIFF
#define		USE_IMAGE_PLUGIN_PKM
#define		USE_IMAGE_PLUGIN_PVR
#define		USE_FBXIMPORTER

//------------------------------------------------------------------------------
//
//    toolkit to load/unload PKFX runtime plugins
//
//------------------------------------------------------------------------------

#ifdef	PK_PLUGINS_STATIC
#	if defined(USE_COMPILER_BACKEND)
PK_PLUGIN_DECLARE(CCompilerBackendCPU_VM);
#		if defined(PK_PARTICLES_UPDATER_USE_D3D12) || defined(PK_PARTICLES_UPDATER_USE_D3D11)
PK_PLUGIN_DECLARE(CCompilerBackendGPU_D3D);
#		endif
#	endif


#	if defined(USE_IMAGE_PLUGINS)
PK_PLUGIN_DECLARE(CImageDDSCodec);
PK_PLUGIN_DECLARE(CImagePNGCodec);
PK_PLUGIN_DECLARE(CImageTGACodec);
PK_PLUGIN_DECLARE(CImageJPEGCodec);
PK_PLUGIN_DECLARE(CImageHDRCodec);
#		if defined(USE_IMAGE_PLUGIN_TIFF)
PK_PLUGIN_DECLARE(CImageTIFFCodec);
#		endif
#		if defined(USE_IMAGE_PLUGIN_PKM)
PK_PLUGIN_DECLARE(CImagePKMCodec);
#		endif
#		if defined(USE_IMAGE_PLUGIN_PVR)
PK_PLUGIN_DECLARE(CImagePVRCodec);
#		endif
#	endif

#	if defined(USE_FBXIMPORTER)
PK_PLUGIN_DECLARE(CMeshCodecFBX);
#	endif

#	if defined(PK_DEBUG)
#		define	PK_PLUGIN_POSTFIX_BUILD		"_D"
#	else
#		define	PK_PLUGIN_POSTFIX_BUILD		""
#	endif
#	if	defined(PK_WINDOWS)
#		define	PK_PLUGIN_POSTFIX_EXT		".dll"
#	else
#		define	PK_PLUGIN_POSTFIX_EXT		""
#	endif
#endif

__AEGP_PK_BEGIN
//------------------------------------------------------------------------------

static u32    g_LoadedPlugins = 0;

//-----------------------------------------------------------------------------
//
//    Loads selected plugins
//
//-----------------------------------------------------------------------------

bool	PopcornRegisterPlugins(u32 selected /*= 0*/)
{
	PK_ASSERT(g_LoadedPlugins == 0);

	bool	success = true;
#ifndef	PK_PLUGINS_STATIC
	// plugins are .dll
	PopcornFX::CPluginManager::RegisterDirectory("Plugins", false);

#else
	// plugins are linked statically
	if (selected & EPlugin_CompilerBackendVM)
	{
		const char	*backendPath = "Plugins/CBCPU_VM" PK_PLUGIN_POSTFIX_BUILD PK_PLUGIN_POSTFIX_EXT;
		IPluginModule	*backend = StartupPlugin_CCompilerBackendCPU_VM();
		success &= (backend != null && PopcornFX::CPluginManager::PluginRegister(backend, true, backendPath));
	}
#	if (PK_PARTICLES_UPDATER_USE_D3D12 != 0 || PK_PARTICLES_UPDATER_USE_D3D11 != 0)
	if (selected & EPlugin_CompilerBackendD3D)
	{
		const char	*backendPath = "Plugins/CBGPU_D3D" PK_PLUGIN_POSTFIX_BUILD PK_PLUGIN_POSTFIX_EXT;
		IPluginModule	*backend = StartupPlugin_CCompilerBackendGPU_D3D();
		success &= (backend != null && CPluginManager::PluginRegister(backend, true, backendPath));
	}
#	endif
#	ifdef USE_IMAGE_PLUGINS
	if (selected & EPlugin_ImageCodecDDS)
	{
		const char		*codecPathDDS = "Plugins/image_codec_dds" PK_PLUGIN_POSTFIX_BUILD PK_PLUGIN_POSTFIX_EXT;
		IPluginModule	*codecDDS = StartupPlugin_CImageDDSCodec();
		success &= (codecDDS != null && PopcornFX::CPluginManager::PluginRegister(codecDDS, true, codecPathDDS));
	}

	if (selected & EPlugin_ImageCodecPNG)
	{
		const char		*codecPathPNG = "Plugins/image_codec_png" PK_PLUGIN_POSTFIX_BUILD PK_PLUGIN_POSTFIX_EXT;
		IPluginModule	*codecPNG = StartupPlugin_CImagePNGCodec();
		success &= (codecPNG != null && PopcornFX::CPluginManager::PluginRegister(codecPNG, true, codecPathPNG));
	}

	if (selected & EPlugin_ImageCodecJPG)
	{
		const char		*codecPathJPG = "Plugins/image_codec_jpeg" PK_PLUGIN_POSTFIX_BUILD PK_PLUGIN_POSTFIX_EXT;
		IPluginModule	*codecJPG = StartupPlugin_CImageJPEGCodec();
		success &= (codecJPG != null && PopcornFX::CPluginManager::PluginRegister(codecJPG, true, codecPathJPG));
	}

	if (selected & EPlugin_ImageCodecTGA)
	{
		const char		*codecPathTGA = "Plugins/image_codec_tga" PK_PLUGIN_POSTFIX_BUILD PK_PLUGIN_POSTFIX_EXT;
		IPluginModule	*codecTGA = StartupPlugin_CImageTGACodec();
		success &= (codecTGA != null && PopcornFX::CPluginManager::PluginRegister(codecTGA, true, codecPathTGA));
	}

	if (selected & EPlugin_ImageCodecHDR)
	{
		const char		*codecPathHDR = "Plugins/image_codec_hdr" PK_PLUGIN_POSTFIX_BUILD PK_PLUGIN_POSTFIX_EXT;
		IPluginModule	*codecHDR = StartupPlugin_CImageHDRCodec();
		success &= (codecHDR != null && PopcornFX::CPluginManager::PluginRegister(codecHDR, true, codecPathHDR));
	}
#	endif

#	if defined(USE_IMAGE_PLUGIN_TIFF)
	if (selected & EPlugin_ImageCodecTIFF)
	{
		const char		*codecPathTIFF = "Plugins/image_codec_tiff" PK_PLUGIN_POSTFIX_BUILD PK_PLUGIN_POSTFIX_EXT;
		IPluginModule	*codecTIFF = StartupPlugin_CImageTIFFCodec();
		success &= (codecTIFF != null && PopcornFX::CPluginManager::PluginRegister(codecTIFF, true, codecPathTIFF));
	}
#	endif

#	if defined(USE_IMAGE_PLUGIN_PKM)
	if (selected & EPlugin_ImageCodecPKM)
	{
		const char		*codecPathPKM = "Plugins/image_codec_pkm" PK_PLUGIN_POSTFIX_BUILD PK_PLUGIN_POSTFIX_EXT;
		IPluginModule	*codecPKM = StartupPlugin_CImagePKMCodec();
		success &= (codecPKM != null && PopcornFX::CPluginManager::PluginRegister(codecPKM, true, codecPathPKM));
	}
#	endif

#	if defined(USE_IMAGE_PLUGIN_PVR)
	if (selected & EPlugin_ImageCodecPVR)
	{
		const char		*codecPathPVR = "Plugins/image_codec_pvr" PK_PLUGIN_POSTFIX_BUILD PK_PLUGIN_POSTFIX_EXT;
		IPluginModule	*codecPVR = StartupPlugin_CImagePVRCodec();
		success &= (codecPVR != null && PopcornFX::CPluginManager::PluginRegister(codecPVR, true, codecPathPVR));
	}
#	endif

#	if defined(USE_FBXIMPORTER)
	if (selected & EPlugin_MeshCodecFBX)
	{
		const char		*codecPath = "Plugins/MeshCodecFBX" PK_PLUGIN_POSTFIX_BUILD PK_PLUGIN_POSTFIX_EXT;
		IPluginModule	*codec = StartupPlugin_CMeshCodecFBX();
		success &= (codec != null && PopcornFX::CPluginManager::PluginRegister(codec, true, codecPath));
	}
#	endif
#endif

	g_LoadedPlugins = selected;
	return success;
}

//-----------------------------------------------------------------------------
//
//    Unloads all plugins
//
//-----------------------------------------------------------------------------

void		PopcornUnregisterPlugins()
{
	// unregister plugins:
#ifdef	PK_PLUGINS_STATIC
	if (g_LoadedPlugins & EPlugin_CompilerBackendVM)
	{
		IPluginModule		*backend = GetPlugin_CCompilerBackendCPU_VM();
		(backend != null && PopcornFX::CPluginManager::PluginRelease(backend));
		ShutdownPlugin_CCompilerBackendCPU_VM();
	}

#	if (PK_PARTICLES_UPDATER_USE_D3D12 != 0 || PK_PARTICLES_UPDATER_USE_D3D11 != 0)
	if (g_LoadedPlugins & EPlugin_CompilerBackendD3D)
	{
		IPluginModule		*backend = GetPlugin_CCompilerBackendGPU_D3D();
		(backend != null && PopcornFX::CPluginManager::PluginRelease(backend));
		ShutdownPlugin_CCompilerBackendGPU_D3D();
	}
#	endif
#	ifdef USE_IMAGE_PLUGINS
	if (g_LoadedPlugins & EPlugin_ImageCodecDDS)
	{
		IPluginModule		*codecDDS = GetPlugin_CImageDDSCodec();
		(codecDDS != null && PopcornFX::CPluginManager::PluginRelease(codecDDS));
		ShutdownPlugin_CImageDDSCodec();
	}

	if (g_LoadedPlugins & EPlugin_ImageCodecPNG)
	{
		IPluginModule		*codecPNG = GetPlugin_CImagePNGCodec();
		(codecPNG != null && PopcornFX::CPluginManager::PluginRelease(codecPNG));
		ShutdownPlugin_CImagePNGCodec();
	}

	if (g_LoadedPlugins & EPlugin_ImageCodecJPG)
	{
		IPluginModule		*codecJPG = GetPlugin_CImageJPEGCodec();
		(codecJPG != null && PopcornFX::CPluginManager::PluginRelease(codecJPG));
		ShutdownPlugin_CImageJPEGCodec();
	}

	if (g_LoadedPlugins & EPlugin_ImageCodecTGA)
	{
		IPluginModule		*codecTGA = GetPlugin_CImageTGACodec();
		(codecTGA != null && PopcornFX::CPluginManager::PluginRelease(codecTGA));
		ShutdownPlugin_CImageTGACodec();
	}

	if (g_LoadedPlugins & EPlugin_ImageCodecHDR)
	{
		IPluginModule		*codecHDR = GetPlugin_CImageHDRCodec();
		(codecHDR != null && PopcornFX::CPluginManager::PluginRelease(codecHDR));
		ShutdownPlugin_CImageHDRCodec();
	}
#	endif

#	if defined(USE_IMAGE_PLUGIN_TIFF)
	if (g_LoadedPlugins & EPlugin_ImageCodecTIFF)
	{
		IPluginModule		*codecTIFF = GetPlugin_CImageTIFFCodec();
		(codecTIFF != null && PopcornFX::CPluginManager::PluginRelease(codecTIFF));
		ShutdownPlugin_CImageTIFFCodec();
	}
#	endif

#	if defined(USE_IMAGE_PLUGIN_PKM)
	if (g_LoadedPlugins & EPlugin_ImageCodecPKM)
	{
		IPluginModule		*codecPKM = GetPlugin_CImagePKMCodec();
		(codecPKM != null && PopcornFX::CPluginManager::PluginRelease(codecPKM));
		ShutdownPlugin_CImagePKMCodec();
	}
#	endif

#	if defined(USE_IMAGE_PLUGIN_PVR)
	if (g_LoadedPlugins & EPlugin_ImageCodecPVR)
	{
		IPluginModule		*codecPVR = GetPlugin_CImagePVRCodec();
		(codecPVR != null && PopcornFX::CPluginManager::PluginRelease(codecPVR));
		ShutdownPlugin_CImagePVRCodec();
	}
#	endif

#	if defined(USE_FBXIMPORTER)
	if (g_LoadedPlugins & EPlugin_MeshCodecFBX)
	{
		IPluginModule		*codec = GetPlugin_CMeshCodecFBX();
		(codec != null && PopcornFX::CPluginManager::PluginRelease(codec));
		ShutdownPlugin_CMeshCodecFBX();
	}
#	endif
#endif
	g_LoadedPlugins = 0;
}

//------------------------------------------------------------------------------
__AEGP_PK_END
