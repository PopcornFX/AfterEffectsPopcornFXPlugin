//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"

#include "AEGP_RenderContext.h"
#include "AEGP_AEPKConversion.h"

#include "PopcornFX_Suite.h"

//AAE
#include <AE_Effect.h>
#include <AE_GeneralPlug.h>
#include <AE_EffectCB.h>
#include <AE_Macros.h>
#include <AE_EffectCBSuites.h>
#include <AEFX_SuiteHelper.h>
#include <AEFX_ChannelDepthTpl.h>
#include <AEGP_SuiteHandler.h>
#include <Param_Utils.h>

//CreateInternalWindow
#if	defined(PK_MACOSX)
#	include <Metal/Metal.h>
#	include <QuartzCore/CAMetalLayer.h>
#	include <Cocoa/Cocoa.h>
#else
#	include <WinUser.h>
#	include <wingdi.h>
#endif

#include <atomic>
#include <sstream>
//RHI
#include <pk_rhi/include/AllInterfaces.h>
#include <pk_rhi/include/Startup.h>

//D3D11
#if	(PK_BUILD_WITH_D3D11_SUPPORT != 0)
#include "RenderApi/AEGP_D3D11Context.h"
#endif
//D3D12
#if	(PK_BUILD_WITH_D3D12_SUPPORT != 0)
#include "RenderApi/AEGP_D3D12Context.h"
#endif
#if	(PK_BUILD_WITH_METAL_SUPPORT != 0)
#include "RenderApi/AEGP_MetalContext.h"
#endif

#include "RenderApi/AEGP_CopyPixels.h"

//Samples
#include <PK-SampleLib/SampleUtils.h>

__AEGP_PK_BEGIN

//----------------------------------------------------------------------------

CAAEBaseContext				*CAAERenderContext::m_AEGraphicContext = null;
Threads::CCriticalSection	CAAERenderContext::m_AEGraphicContextLock;

//----------------------------------------------------------------------------

CAAERenderContext::CAAERenderContext()
:	m_WindowHandle(null)
,	m_DeviceContext(null)
,	m_Width(0)
,	m_Height(0)
,	m_Initialized(false)
,	m_RHIRendering(null)
,	m_ShaderLoader(null)
{
}

//----------------------------------------------------------------------------

CAAERenderContext::~CAAERenderContext()
{
	PKSample::SPassDescription::s_PassDescriptions.Clean();
}

//----------------------------------------------------------------------------

bool	CAAERenderContext::InitializeIFN(RHI::EGraphicalApi api, const CString &className)
{
	if (m_Initialized == true)
	{
		return true;
	}
	m_API = api;

	if (!CreateInternalWindowIFN(className))
		return false;

	PK_SCOPEDLOCK(m_AEGraphicContextLock);
	if (m_AEGraphicContext == null)
	{
		switch (m_API)
		{
	#if (PK_BUILD_WITH_METAL_SUPPORT != 0)
		case (RHI::GApi_Metal):
			m_AEGraphicContext = PK_NEW(CAAEMetalContext);
			break;
	#endif
	#if (PK_BUILD_WITH_D3D11_SUPPORT != 0)
		case (RHI::GApi_D3D11):
			m_AEGraphicContext = PK_NEW(CAAED3D11Context);
			break;
	#endif
	#if (PK_BUILD_WITH_D3D12_SUPPORT != 0)
		case (RHI::GApi_D3D12):
			m_AEGraphicContext = PK_NEW(CAAED3D12Context);
			break;
	#endif
		default:
			CLog::Log(PK_ERROR, "No compatible API context could be created");
			PK_ASSERT_NOT_REACHED();
			return false;
			break;
		}
		if (!PK_VERIFY(m_AEGraphicContext != null))
			return false;
		if (!PK_VERIFY(m_AEGraphicContext->CreatePlatformContext(m_WindowHandle, m_DeviceContext)))
		{
			CLog::Log(PK_ERROR, "CreatePlatformContext failed");
			return false;
		}
		if (!PK_VERIFY(m_AEGraphicContext->InitIFN()))
		{
			CLog::Log(PK_ERROR, "Graphic context initialization failed");
			return false;
		}
	}
	m_Initialized = true;
	return true;
}

//----------------------------------------------------------------------------

bool	CAAERenderContext::Destroy()
{
	PK_SAFE_DELETE(m_RHIRendering);
	PK_SAFE_DELETE(m_AEGraphicContext);
	m_Initialized = false;
	return true;
}

//----------------------------------------------------------------------------
//									GETTER
//----------------------------------------------------------------------------

PKSample::CRHIParticleSceneRenderHelper	*CAAERenderContext::GetCurrentSceneRenderer()
{
	PK_ASSERT(m_RHIRendering != null);
	return m_RHIRendering;
}

//----------------------------------------------------------------------------

CUint2	CAAERenderContext::GetViewportSize() const
{
	return CUint2{ m_Width, m_Height };
}

//----------------------------------------------------------------------------

void CAAERenderContext::LogGraphicsErrors()
{
	m_AEGraphicContext->LogApiError();
}

//----------------------------------------------------------------------------

CAAEBaseContext	*CAAERenderContext::GetAEGraphicContext()
{
	return m_AEGraphicContext;
}

//----------------------------------------------------------------------------
//									Setter
//----------------------------------------------------------------------------

void	CAAERenderContext::SetShaderLoader(PKSample::CShaderLoader *shaderLoader)
{
	m_ShaderLoader = shaderLoader;
}

//----------------------------------------------------------------------------

PKSample::CShaderLoader *CAAERenderContext::GetShaderLoader()
{
	return m_ShaderLoader;
}

//----------------------------------------------------------------------------

void	CAAERenderContext::SetPostFXOptions(PKSample::SParticleSceneOptions &postFXOptions)
{
	m_SceneOptions = postFXOptions;
}

//----------------------------------------------------------------------------

bool	CAAERenderContext::SetAsCurrentContext()
{
	PK_ASSERT(m_DeviceContext != null);

	return m_AEGraphicContext->SetAsCurrent(m_DeviceContext);
}

//----------------------------------------------------------------------------

//Used for IMGUI Context
bool	CAAERenderContext::RenderFrameBegin(u32 width, u32 height)
{
	if (!PK_VERIFY(SetAsCurrentContext()))
		return false;
	if (m_Width != width || m_Height != height)
	{
		if (InitGraphicContext(m_Format, width, height) == false)
			return false;
	}
	//Frame 
	return m_AEGraphicContext->BeginFrame();
}

//----------------------------------------------------------------------------

bool	CAAERenderContext::RenderFrameEnd()
{
	return m_AEGraphicContext->EndFrame();
}

//----------------------------------------------------------------------------
//									AEE Interface
//----------------------------------------------------------------------------

bool	CAAERenderContext::InitGraphicContext(RHI::EPixelFormat rhiformat, u32 width, u32 height)
{
	PK_SCOPEDPROFILE();

	// Force the format to 32bpc for precision issues:
	rhiformat = RHI::EPixelFormat::FormatFloat32RGBA;

	bool updateRHISize = false;
	if (m_Format != rhiformat || m_Width != width || m_Height != height)
		updateRHISize = true;
	m_Format = rhiformat;
	m_Width = width;
	m_Height = height;

	RHI::PApiManager apiManager = m_AEGraphicContext->GetApiManager();

	if (!m_AEGraphicContext->CreateRenderTarget(m_Format, CUint3{ m_Width, m_Height, 1 }))
	{
		CLog::Log(PK_ERROR, "Graphic context CreateRenderTarget failed");
		return false;
	}

	bool newRHIRendering = false;
	if (m_RHIRendering == null)
	{
		m_RHIRendering = PK_NEW(PKSample::CRHIParticleSceneRenderHelper);
		if (!PK_VERIFY(m_RHIRendering != null))
			return false;
		if (!PK_VERIFY(m_ShaderLoader != null))
		{
			CLog::Log(PK_ERROR, "Shader loader was not properly set on the render context");
			return false;
		}

		if (!m_RHIRendering->Init(	apiManager, m_ShaderLoader, Resource::DefaultManager(),
									PKSample::CRHIParticleSceneRenderHelper::InitRP_All))
		{
			CLog::Log(PK_ERROR, "RHIRendering Initialisation failed");
			return false;
		}
		newRHIRendering = true;
	}
	if (updateRHISize || newRHIRendering)
	{
		if (!m_RHIRendering->Resize(m_AEGraphicContext->GetCurrentSwapChain()))
		{
			CLog::Log(PK_ERROR, "Particle render scene Resize failed");
			return false;
		}
		if (!m_RHIRendering->SetupPostFX_Distortion(m_SceneOptions.m_Distortion, newRHIRendering))
		{
			CLog::Log(PK_ERROR, "Particle render scene SetupPostFX_Distortion failed");
			return false;
		}
		if (!m_RHIRendering->SetupPostFX_ToneMapping(m_SceneOptions.m_ToneMapping, m_SceneOptions.m_Vignetting, true /*dithering*/, newRHIRendering, false))
		{
			CLog::Log(PK_ERROR, "Particle render scene SetupPostFX_ToneMapping failed");
			return false;
		}
		if (!m_RHIRendering->SetupPostFX_Bloom(m_SceneOptions.m_Bloom, newRHIRendering, true))
		{
			CLog::Log(PK_ERROR, "Particle render scene SetupPostFX_Bloom failed");
			return false;
		}
		if (!m_RHIRendering->SetupPostFX_FXAA(m_SceneOptions.m_FXAA, newRHIRendering, false))
		{
			CLog::Log(PK_ERROR, "Particle render scene SetupPostFX_FXAA failed");
			return false;
		}
		if (!m_RHIRendering->SetupShadows())
		{
			CLog::Log(PK_ERROR, "Particle render scene SetupShadows failed");
			return false;
		}
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CAAERenderContext::AERenderFrameBegin(SAAEIOData &AAEData, bool getBackground /*=true*/)
{
	PK_SCOPEDPROFILE();
	PF_PixelFormat		format = PF_PixelFormat_INVALID;
	RHI::EPixelFormat	rhiFormat = RHI::EPixelFormat::FormatUnknown;
	AEGP_SuiteHandler	suites(AAEData.m_InData->pica_basicP);

	PK_ASSERT(m_AEGraphicContext != null);
	m_AEGraphicContext->SetAsCurrent(m_DeviceContext);
	{
		PK_NAMEDSCOPEDPROFILE("checkout_layer_pixels");
		AAEData.m_ExtraData.m_SmartRenderData->cb->checkout_layer_pixels(AAEData.m_InData->effect_ref, 0, &m_InputWorld);
		if (!PK_VERIFY(m_InputWorld != null))
		{
			CLog::Log(PK_ERROR, "Checkout AE input world failed");
			return false;
		}
		AAEData.m_ExtraData.m_SmartRenderData->cb->checkout_output(AAEData.m_InData->effect_ref, &m_OutputWorld);
		if (!PK_VERIFY(m_OutputWorld != null))
		{
			CLog::Log(PK_ERROR, "Checkout AE output world failed");
			ResetCheckedOutWorlds(AAEData);
			return false;
		}
	}

	AAEData.m_WorldSuite->PF_GetPixelFormat(m_InputWorld, &format);
	
	rhiFormat = AAEToPK(format);

	u32 width = (u32)m_InputWorld->width;
	u32 height = (u32)m_InputWorld->height;
	
	m_AAEFormat = format;

	if (m_Width != width || m_Height != height)
	{
		if (InitGraphicContext(rhiFormat, width, height) == false)
		{
			CLog::Log(PK_ERROR, "InitGraphicContext failed");
			ResetCheckedOutWorlds(AAEData);
			return false;
		}
	}
	
	//Frame
	if (!PK_VERIFY(m_AEGraphicContext->BeginFrame()))
	{
		CLog::Log(PK_ERROR, "Graphic context BeginFrame failed");
		ResetCheckedOutWorlds(AAEData);
		return false;
	}

	if (getBackground)
	{
		if (!GetCompositingBuffer(AAEData, suites, m_InputWorld, m_OutputWorld, format))
		{
			CLog::Log(PK_ERROR, "GetCompositingBuffer failed");
			ResetCheckedOutWorlds(AAEData);
			m_AEGraphicContext->EndFrame();
			return false;
		}
	}
	else
		m_RHIRendering->SetBackGroundTexture(null);
	return true;
}

//----------------------------------------------------------------------------

bool	CAAERenderContext::AERenderFrameEnd(SAAEIOData &AAEData)
{
	PK_SCOPEDPROFILE();
	AEGP_SuiteHandler suites(AAEData.m_InData->pica_basicP);

	m_AEGraphicContext->EndFrame();

	if (m_InputWorld == null || m_OutputWorld == null)
	{
		ResetCheckedOutWorlds(AAEData);
		return false;
	}
	RenderToSAAEWorld(AAEData, suites, m_InputWorld, m_OutputWorld, m_AAEFormat);
	ResetCheckedOutWorlds(AAEData);
	return true;
}

//----------------------------------------------------------------------------

bool	CAAERenderContext::RenderToSAAEWorld(SAAEIOData &AAEData, AEGP_SuiteHandler &suiteHandler, PF_EffectWorld *inputWorld, PF_EffectWorld *effectWorld, PF_PixelFormat format)
{
	PK_SCOPEDPROFILE();

	u32		requiredBufferSize = GetPixelSizeFromPixelFormat(m_Format) * inputWorld->width * inputWorld->height;
	if (m_DownloadBuffer == null || m_DownloadBufferSize < requiredBufferSize)
	{
		m_DownloadBufferSize = requiredBufferSize;
		if (m_DownloadBuffer != null)
			m_DownloadBuffer = null;
		m_DownloadBuffer = CRefCountedMemoryBuffer::Alloc(requiredBufferSize);
		PK_ASSERT(m_DownloadBuffer != null);
	}

	if (!m_AEGraphicContext->FillRenderBuffer(m_DownloadBuffer, m_RHIRendering->GetFinalFrameBuffers(0), m_Format, m_Width, m_Height, m_Width))
	{
		CLog::Log(PK_ERROR, "FillRenderBuffer failed");
		return false;
	}
	

	PF_Err		res = PF_Err_NONE;

	switch (format)
	{
	case	PF_PixelFormat_ARGB128:
	{
		PK_NAMEDSCOPEDPROFILE("CopyPixelOut32");
		SCopyPixel					refcon = { m_DownloadBuffer, inputWorld, m_Gamma, m_IsOverride, m_AlphaValue };
		
		res = suiteHandler.IterateFloatSuite1()->iterate(	AAEData.m_InData,
															0,
															inputWorld->height,
															inputWorld,
															null,
															reinterpret_cast<void*>(&refcon),
															CopyPixelOut32,
															effectWorld);
		break;
	}
	case	PF_PixelFormat_ARGB64:
	{
		PK_NAMEDSCOPEDPROFILE("CopyPixelOut16");
		SCopyPixel					refcon = { m_DownloadBuffer, inputWorld, m_Gamma, m_IsOverride, m_AlphaValue };
		
		res = suiteHandler.Iterate16Suite1()->iterate(	AAEData.m_InData,
														0,
														inputWorld->height,
														inputWorld,
														null,
														reinterpret_cast<void*>(&refcon),
														CopyPixelOut16,
														effectWorld);
		break;
	}

	case	PF_PixelFormat_ARGB32:
	{
		PK_NAMEDSCOPEDPROFILE("CopyPixelOut8");
		SCopyPixel					refcon = { m_DownloadBuffer, inputWorld, m_Gamma, m_IsOverride, m_AlphaValue };

		res = suiteHandler.Iterate8Suite1()->iterate(	AAEData.m_InData,
														0,
														inputWorld->height,
														inputWorld,
														null,
														reinterpret_cast<void*>(&refcon),
														CopyPixelOut8,
														effectWorld);
		break;
	}
	default:
		PK_ASSERT(true);
		break;
	}

	if (res != PF_Err_NONE)
	{
		AAEData.m_ReturnCode = res;
		return false;
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CAAERenderContext::GetCompositingBuffer(SAAEIOData &AAEData, AEGP_SuiteHandler &suiteHandler, PF_EffectWorld *inputWorld, PF_EffectWorld *effectWorld, PF_PixelFormat format)
{
	PK_SCOPEDPROFILE();
	PF_Err				res = PF_Err_NONE;
	RHI::EPixelFormat	rhiFormat = RHI::FormatFloat32RGBA;

	PF_Rect		rect = inputWorld->extent_hint;
	PF_Point	origin;

	origin.h = (A_short)(AAEData.m_InData->output_origin_x);
	origin.v = (A_short)(AAEData.m_InData->output_origin_y);

	// We always copy the pixels in 32 bpc
	u32		requiredBufferSize = sizeof(CFloat4) * inputWorld->width * inputWorld->height;
	if (m_UploadBuffer == null || m_UploadBufferSize < requiredBufferSize)
	{
		m_UploadBufferSize = requiredBufferSize;
		m_UploadBuffer = CRefCountedMemoryBuffer::Alloc(requiredBufferSize);
		PK_ASSERT(m_UploadBuffer != null);
	}
	void				*srcBuffer = m_UploadBuffer->Data<void>();

	switch (format)
	{
	case	PF_PixelFormat_ARGB128:
	{
		PK_NAMEDSCOPEDPROFILE("CopyPixelIn32_TryCatch");
		SCopyPixel					refcon = { m_UploadBuffer, inputWorld, m_Gamma, m_IsOverride, m_AlphaValue };

		try
		{
			PK_NAMEDSCOPEDPROFILE("CopyPixelIn32");
			res = suiteHandler.IterateFloatSuite1()->iterate_origin_non_clip_src(AAEData.m_InData, 0, inputWorld->height, inputWorld, &rect, &origin,
																				 reinterpret_cast<void*>(&refcon), CopyPixelIn32, effectWorld);
		}
		catch (PF_Err& thrownErr)
		{
			res = thrownErr;
		}
		catch (...)
		{
			res = PF_Err_OUT_OF_MEMORY;
		}
		if (res == A_Err_NONE)
		{
			if (!m_AEGraphicContext->FillCompositingTexture(srcBuffer, rhiFormat, inputWorld->width, inputWorld->height, inputWorld->rowbytes / sizeof(PF_Pixel32)))
				res = A_Err_GENERIC;
		}
		break;
	}
	case	PF_PixelFormat_ARGB64:
	{
		PK_NAMEDSCOPEDPROFILE("CopyPixelIn16_TryCatch");
		SCopyPixel						refcon = { m_UploadBuffer, inputWorld, m_Gamma, m_IsOverride, m_AlphaValue };

		try
		{
			PK_NAMEDSCOPEDPROFILE("CopyPixelIn16");
			res = suiteHandler.Iterate16Suite1()->iterate_origin_non_clip_src(AAEData.m_InData, 0, inputWorld->height, inputWorld, &rect, &origin,
																			  reinterpret_cast<void*>(&refcon), CopyPixelIn16, effectWorld);
		}
		catch (PF_Err& thrownErr)
		{
			res = thrownErr;
		}
		catch (...)
		{
			res = PF_Err_OUT_OF_MEMORY;
		}
		if (res == A_Err_NONE)
		{
			if (!m_AEGraphicContext->FillCompositingTexture(srcBuffer, rhiFormat, inputWorld->width, inputWorld->height, inputWorld->rowbytes / sizeof(PF_Pixel32)))
				res = A_Err_GENERIC;
		}
		break;
	}

	case	PF_PixelFormat_ARGB32:
	{
		PK_NAMEDSCOPEDPROFILE("CopyPixelIn8_TryCatch");
		SCopyPixel					refcon = { m_UploadBuffer, inputWorld, m_Gamma, m_IsOverride, m_AlphaValue };

		try
		{
			PK_NAMEDSCOPEDPROFILE("CopyPixelIn8");
			res = suiteHandler.Iterate8Suite1()->iterate_origin_non_clip_src(AAEData.m_InData, 0, inputWorld->height, inputWorld, &rect, &origin,
																			 reinterpret_cast<void*>(&refcon), CopyPixelIn8, effectWorld);
		}
		catch (PF_Err& thrownErr)
		{
			res = thrownErr;
		}
		catch (...)
		{
			res = PF_Err_OUT_OF_MEMORY;
		}
		if (res == A_Err_NONE)
		{
			if (!m_AEGraphicContext->FillCompositingTexture(srcBuffer, rhiFormat, inputWorld->width, inputWorld->height, inputWorld->rowbytes / sizeof(PF_Pixel32)))
				res = A_Err_GENERIC;
		}
		break;
	}
	default:
		break;
	}
	if (res)
	{
		AAEData.m_ReturnCode = res;
		if (res == PF_Interrupt_CANCEL)
			return false;
		CLog::Log(PK_WARN, "Unknown error");
		return false;
	}

	if (res == A_Err_NONE)
		m_RHIRendering->SetBackGroundTexture(m_AEGraphicContext->GetCompositingTexture());

	if (res != PF_Err_NONE)
	{
		AAEData.m_ReturnCode = res;
		return false;
	}
	return true;
}

//----------------------------------------------------------------------------

CUint2	CAAERenderContext::GetContextSize()
{
	return CUint2(m_Width, m_Height);
}

//----------------------------------------------------------------------------

void	CAAERenderContext::ResetCheckedOutWorlds(SAAEIOData &AAEData)
{
	AAEData.m_ExtraData.m_SmartRenderData->cb->checkin_layer_pixels(AAEData.m_InData->effect_ref, 0);
	m_InputWorld = null;
	m_OutputWorld = null;
}

//----------------------------------------------------------------------------

bool	CAAERenderContext::CreateInternalWindowIFN(const CString& className)
{
	if (m_WindowHandle != null)
		return true;
#if		defined(PK_MACOSX)
	(void)className;
	NSRect	frameRect = NSMakeRect(100, 100 , 256, 256);
	NSView	*view = [[NSView alloc] initWithFrame:frameRect];
	[view setHidden:NO];
	[view setNeedsDisplay:YES];

	NSWindow	*myWindow = [[NSWindow alloc] initWithContentRect:NSMakeRect(100,100,256,256) styleMask:NSTitledWindowMask backing:NSBackingStoreBuffered defer:NO];
	[myWindow setContentView:view];
	//[myWindow makeKeyAndOrderFront:self];

	m_WindowHandle = (void*)myWindow;
	m_DeviceContext = null;
	return m_WindowHandle != null;
#else
	MSG		uMsg;

	::memset(&uMsg, 0, sizeof(uMsg));

	// tricky, windows needs a unique class name for each window instance
	// to derive a unique name, a pointer to "this" is used
	static std::atomic_int	S_cnt;
	std::stringstream		ss;

	ss << " PK_Win_Class" << S_cnt.load();
	S_cnt++;
	CString name = className + CString(ss.str().c_str());

	WNDCLASSEX winClass;
	::memset(&winClass, 0, sizeof(winClass));
	winClass.lpszClassName = name.Data();
	winClass.cbSize = sizeof(WNDCLASSEX);
	winClass.style = CS_HREDRAW | CS_VREDRAW;
	winClass.lpfnWndProc = ::DefWindowProc;
	winClass.hInstance = GetModuleHandle(null);
	winClass.hCursor = ::LoadCursor(null, IDC_ARROW);
	winClass.hbrBackground = (HBRUSH)::GetStockObject(BLACK_BRUSH);

	if (!(::RegisterClassEx(&winClass)))
	{
		return false;
	}

	HWND hwnd = ::CreateWindowEx(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE,
		name.Data(),
		"PopcornFX context",
		0, 0,
		0, 8, 8,
		null,
		null,
		null,
		null);

	PK_ASSERT(hwnd != null);
	m_WindowHandle = (void*)hwnd;
	m_DeviceContext = (void*)::GetDC(hwnd);
	return m_WindowHandle != null && m_DeviceContext != null;
#endif
}

//----------------------------------------------------------------------------

__AEGP_PK_END
