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

#include "ApiContextConfig.h"

#if (PK_BUILD_WITH_VULKAN_SUPPORT != 0)

#include "VulkanContext.h"

#include <pk_rhi/include/vulkan/VulkanRenderTarget.h>
#include <pk_rhi/include/vulkan/VulkanPopcornEnumConversion.h>

#include <stdlib.h>
#include <assert.h>

#include "WindowContext/SdlContext/SdlContext.h"

#if	(PK_BUILD_WITH_SDL != 0)
#	if defined(PK_COMPILER_CLANG_CL)
#		pragma clang diagnostic push
#		pragma clang diagnostic ignored "-Wpragma-pack"
#	endif // defined(PK_COMPILER_CLANG_CL)
#	include <SDL_syswm.h>
#	if defined(PK_COMPILER_CLANG_CL)
#		pragma clang diagnostic pop
#	endif // defined(PK_COMPILER_CLANG_CL)
#endif // (PK_BUILD_WITH_SDL != 0)

#ifdef PK_NX
#	include "pk_kernel/layer_0/kr_mem/include/pv_mem.h"
#endif

#define		USE_CUSTOM_ALLOCATOR

//----------------------------------------------------------------------------

#if defined(PK_GGP)

#define VK_GET_INSTANCE_PROC_ADDR(functionPointer, instance, functionName)						\
	static PFN_##functionName functionPointer;													\
																								\
	if (functionPointer == null)																\
	{																							\
		functionPointer = (PFN_##functionName)vkGetInstanceProcAddr(instance, #functionName);	\
		if (functionPointer == null)															\
			CLog::Log(PK_ERROR, "Error getting VkInstance function: %s", #functionName);		\
	}

#endif

//----------------------------------------------------------------------------

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

namespace	MacOSxUtils
{
	void	MakeViewMetalCompatible(void *nsview);
	void	*SDLViewFromWindow(void *window);
}

//----------------------------------------------------------------------------

static void	*kVulkanInvalidMallocValue = reinterpret_cast<void *>(static_cast<ureg>(0xFFFFFFFF));

//----------------------------------------------------------------------------

#if	(KR_PROFILER_ENABLED != 0)
static const char	*_VulkanGetAllocationScope(VkSystemAllocationScope allocationScope)
{
	const char	*scope = null;

	switch (allocationScope)
	{
	case VK_SYSTEM_ALLOCATION_SCOPE_COMMAND:
		scope = "VK_SYSTEM_ALLOCATION_SCOPE_COMMAND";
		break;
	case VK_SYSTEM_ALLOCATION_SCOPE_OBJECT:
		scope = "VK_SYSTEM_ALLOCATION_SCOPE_OBJECT";
		break;
	case VK_SYSTEM_ALLOCATION_SCOPE_CACHE:
		scope = "VK_SYSTEM_ALLOCATION_SCOPE_CACHE";
		break;
	case VK_SYSTEM_ALLOCATION_SCOPE_DEVICE:
		scope = "VK_SYSTEM_ALLOCATION_SCOPE_DEVICE";
		break;
	case VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE:
		scope = "VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE";
		break;
	default:
		break;
	}
	return scope;
}
#endif	// (KR_PROFILER_ENABLED == 0)

//----------------------------------------------------------------------------

VKAPI_ATTR VkBool32 VKAPI_CALL	VulkanDebugMessengerCallback(	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
																VkDebugUtilsMessageTypeFlagsEXT messageTypes,
																const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
																void *pUserData)
{
	(void)pUserData;
	CLog::ELogLevel			logLevel = CLog::Level_Info;
	const char				*levelName = "???";

	if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0)
	{
		levelName = "Error";
		logLevel = PKMax(logLevel, CLog::Level_Error);
	}
	else if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0)
	{
		levelName = "Warn";
		logLevel = PKMax(logLevel, CLog::Level_Warning);
	}
	else if ((messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) != 0)
	{
		levelName = "Perf";
		logLevel = PKMax(logLevel, CLog::Level_Warning);
	}
	else if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) != 0)
	{
		levelName = "Info";
		logLevel = PKMax(logLevel, CLog::Level_Info);
	}
	else
	{
		levelName = "????";
		logLevel = PKMax(logLevel, CLog::Level_Warning);
	}

	const CString	message = CString::Format("Vulkan %s (%s code %d): %s", levelName, pCallbackData->pMessageIdName, pCallbackData->messageIdNumber, pCallbackData->pMessage);

	CLog::Log(logLevel, PK_LOG_MODULE_CLASS_GUID_NAME(), "%s", message.Data());

	const bool		isVulkanError = (logLevel == CLog::Level_Error);

	if (isVulkanError)
	{
		PK_RELEASE_ASSERT_MESSAGE(!isVulkanError, "%s", message.Data());
//		return VK_TRUE; // abort !
		return VK_FALSE; //For the moment, we do not abort because there is a bug in the validation layer with my drivers
	}

	return VK_FALSE;
}

//----------------------------------------------------------------------------

#ifdef PK_NX
void	*NXAllocate(size_t size, size_t alignment, void *pUserData)
{
	(void)pUserData;
	return aligned_alloc(alignment, nn::util::align_up(size, alignment));
}

//----------------------------------------------------------------------------

void	*NXReallocate(void *addr, size_t newSize, void *userPtr)
{
	(void)userPtr;
	return realloc(addr, newSize);
}

//----------------------------------------------------------------------------

void NXFree(void *addr, void *userPtr)
{
	(void)userPtr;
	free(addr);
}
#endif

//----------------------------------------------------------------------------

VKAPI_ATTR void* VKAPI_CALL	VulkanAllocate(void *pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
{
	(void)pUserData; (void)allocationScope;
	PK_NAMEDSCOPEDPROFILE_C(_VulkanGetAllocationScope(allocationScope), CFloat3(1, 1, 1));
	if (size != 0)
		return PK_MALLOC_ALIGNED(static_cast<u32>(size), static_cast<u32>(alignment));
	else
		return kVulkanInvalidMallocValue;
}

//----------------------------------------------------------------------------

VKAPI_ATTR void* VKAPI_CALL	VulkanReallocate(void *pUserData, void *pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
{
	(void)pUserData; (void)allocationScope;
	PK_NAMEDSCOPEDPROFILE_C(_VulkanGetAllocationScope(allocationScope), CFloat3(1, 1, 1));

	if (pOriginal == kVulkanInvalidMallocValue)
	{
		if (size == 0)
			return kVulkanInvalidMallocValue;
		else
			return PK_MALLOC_ALIGNED(static_cast<u32>(size), static_cast<u32>(alignment));
	}
	else
	{
		if (size == 0)
		{
			PK_FREE(pOriginal);
			return kVulkanInvalidMallocValue;
		}
		else
		{
			return PK_REALLOC_ALIGNED(pOriginal, static_cast<u32>(size), static_cast<u32>(alignment));
		}
	}
}

//----------------------------------------------------------------------------

VKAPI_ATTR void VKAPI_CALL	VulkanFree(void *pUserData, void *pMemory)
{
	(void)pUserData;
	if (pMemory != kVulkanInvalidMallocValue)
		PK_FREE(pMemory);
}

//----------------------------------------------------------------------------

CVulkanContext::CVulkanContext()
:	m_DebugUtilsMessenger(VK_NULL_HANDLE)
,	m_FnVkDestroyDebugUtilsMessengerEXT(VK_NULL_HANDLE)
,	m_IsOffscreen(false)
{
	m_ApiData.m_Api = RHI::GApi_Vulkan;
}

//----------------------------------------------------------------------------

CVulkanContext::~CVulkanContext()
{
	if (m_DebugUtilsMessenger != VK_NULL_HANDLE && m_FnVkDestroyDebugUtilsMessengerEXT != null)
	{
		m_FnVkDestroyDebugUtilsMessengerEXT(m_ApiData.m_Instance, m_DebugUtilsMessenger, m_ApiData.m_Allocator);
		m_DebugUtilsMessenger = VK_NULL_HANDLE;
	}
	DestroyApiData(m_ApiData);
}

//----------------------------------------------------------------------------

bool	CVulkanContext::InitRenderApiContext(bool debug, PAbstractWindowContext windowApi)
{
	(void)debug;
	bool		err = false;
	CUint2		windowSize = windowApi->GetDrawableSize();

	// Init the Api data
	m_ApiData.m_Instance = VK_NULL_HANDLE;
	m_ApiData.m_PhysicalDevice = VK_NULL_HANDLE;
	m_ApiData.m_LogicalDevice = VK_NULL_HANDLE;
	m_ApiData.m_GraphicalQueue = VK_NULL_HANDLE;
	m_ApiData.m_GraphicalQueueFamily = -1;
	m_ApiData.m_PresentQueue = VK_NULL_HANDLE;
	m_ApiData.m_PresentQueueFamily = -1;
	m_ApiData.m_Allocator = null;
	m_ApiData.m_UsePipelineCache = true;
#ifdef	USE_CUSTOM_ALLOCATOR
	m_ApiData.m_Allocator = PK_NEW(VkAllocationCallbacks);
	m_ApiData.m_Allocator->pfnAllocation = VulkanAllocate;
	m_ApiData.m_Allocator->pfnReallocation = VulkanReallocate;
	m_ApiData.m_Allocator->pfnFree = VulkanFree;
#ifdef PK_NX
	nv::SetGraphicsAllocator(NXAllocate, NXFree, NXReallocate, null);
	nv::SetGraphicsDevtoolsAllocator(NXAllocate, NXFree, NXReallocate, null);
#endif // PK_NX
#endif // USE_CUSTOM_ALLOCATOR

#ifdef PK_NX
	u32 memorySize = 256u * 1024u * 1024u;
	void* pGraphicsMemory = PK_MALLOC_ALIGNED(memorySize, 4096);
	nv::InitializeGraphics(pGraphicsMemory, memorySize);

	nn::vi::Initialize();

	nn::vi::Display	*display;
	nn::vi::Layer	*layer;

	nn::Result result = nn::vi::OpenDefaultDisplay(&display);
	PK_ASSERT(result.IsSuccess());

	result = nn::vi::CreateLayer(&layer, display, windowSize.x(), windowSize.y());
	PK_ASSERT(result.IsSuccess());
#endif

	// Init Vulkan
	bool					enableValidationLayer = false;
	bool					enableDebugNames = false;
	bool					performanceWarnings = false;
	TArray<const char *>	instanceExtensions;
	TArray<const char *>	deviceExtensions;
	TArray<const char *>	layers;

#ifndef PK_RETAIL
	enableDebugNames = true;
#endif
#ifdef PK_DEBUG
	enableValidationLayer = true;
#endif
	m_VSync = false;

	if (windowApi->GetContextApi() == PKSample::Context_Offscreen)
	{
		m_IsOffscreen = true;
	}
	else
	{
		deviceExtensions.PushBack(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	}

	// First we get the extensions to enable in the Vulkan instance
	if (!GetInstanceExtensionsToEnable(instanceExtensions, enableValidationLayer, enableDebugNames, m_IsOffscreen))
		return false;
	// We then get the layers to enable in the instance
	if (enableValidationLayer)
	{
		if (!GetInstanceValidationLayersToEnable(layers))
			enableValidationLayer = false;
	}
	// We then create the instance using those extensions and layers
	err = CreateInstance(m_ApiData, instanceExtensions, layers, "PopcornFX Sample", 1, "PopcornFX", 2);
	if (err == false)
		return false;
	if (windowApi->GetContextApi() == PKSample::Context_Sdl)
	{
#if	(PK_BUILD_WITH_SDL != 0)
		SDL_SysWMinfo	info;
		SDL_Window		*window = static_cast<CSdlContext*>(windowApi.Get())->SdlGetWindow();

		SDL_VERSION(&info.version);
		SDL_GetWindowWMInfo(window, &info);

#	if		defined(PK_WINDOWS)
		err = CreateWindowSurface(m_ApiData, GetModuleHandle(null), info.info.win.window);
#	elif	defined(PK_LINUX)
		err = CreateWindowSurface(m_ApiData, (ureg)info.info.x11.display, (ureg)info.info.x11.window);
#	elif	defined(PK_MACOSX)
		err = CreateWindowSurface(m_ApiData, MacOSxUtils::SDLViewFromWindow(info.info.cocoa.window));
#	else
#		error not implemented
#	endif

#endif // (PK_BUILD_WITH_SDL != 0)
	}
#if defined(PK_GGP)
	else if (windowApi->GetContextApi() == PKSample::Context_Glfw)
	{
		err = CreateWindowSurface(m_ApiData);
	}
#endif
#if defined(PK_NX)
	else if (windowApi->GetContextApi() == PKSample::Context_NX)
	{
		void	*window;
		GetNativeWindow(&window, layer);
		if (!CreateWindowSurface(m_ApiData, window))
			return false;
	}
#endif
	else if (windowApi->GetContextApi() == PKSample::Context_Offscreen)
	{
		if (!m_ApiData.m_SwapChains.PushBack().Valid())
			return false;
		m_ApiData.m_SwapChainCount = m_ApiData.m_SwapChains.Count();
	}
	else
	{
		CLog::Log(PK_ERROR, "Vulkan does not handle window context of this type");
		return false;
	}
	if (err == false)
		return false;
	err = PickPhysicalDevice(m_ApiData, layers, deviceExtensions, enableValidationLayer, m_IsOffscreen);
	if (err == false)
		return false;
	if (enableValidationLayer)
	{
		err = CreateValidationLayerCallback(m_ApiData,
											performanceWarnings,
											VulkanDebugMessengerCallback,
											m_FnVkDestroyDebugUtilsMessengerEXT,
											m_DebugUtilsMessenger);
		if (err == false)
			return false;
	}
	err = CreateQueueAndLogicalDevice(m_ApiData, deviceExtensions, layers, m_IsOffscreen);
	if (err == false)
		return false;

	return CreateSwapChain(m_ApiData, windowSize, true, m_VSync, m_ApiData.m_SwapChainCount - 1, m_IsOffscreen) == CVulkanContext::SwapChainOp_Success;
}

//----------------------------------------------------------------------------

bool	CVulkanContext::WaitAllRenderFinished()
{
	if (m_ApiData.m_LogicalDevice != VK_NULL_HANDLE &&
		vkDeviceWaitIdle(m_ApiData.m_LogicalDevice) != VK_SUCCESS)
	{
		CLog::Log(PK_INFO, "vkDeviceWaitIdle failure");
		return false;
	}
	return true;
}

//----------------------------------------------------------------------------

CGuid	CVulkanContext::BeginFrame()
{
#if defined(PK_GGP)
	m_ApiData.m_PlatformSpecificData.m_FrameToken = GgpIssueFrameToken();
#endif
	if (m_IsOffscreen)
	{
		m_CurrentSwapChainImage = (m_CurrentSwapChainImage + 1) % 2;
		return m_CurrentSwapChainImage;
	}

	VkResult	res;

	res = vkAcquireNextImageKHR(m_ApiData.m_LogicalDevice,
								m_ApiData.m_SwapChains[0].m_SwapChain,
								TNumericTraits<u64>::Max(),
								m_ApiData.m_SwapChains[0].m_SwapChainImageAvailable,
								VK_NULL_HANDLE, &m_CurrentSwapChainImage);
	if (res == VK_SUBOPTIMAL_KHR)
	{
		CLog::Log(PK_ERROR, "vkAcquireNextImageKHR sub optimal");
		return CGuid::INVALID;
	}
	else if (PK_VK_FAILED_STR(res, "vkAcquireNextImageKHR"))
	{
		return CGuid::INVALID;
	}
	return m_CurrentSwapChainImage;
}

//----------------------------------------------------------------------------

bool	CVulkanContext::EndFrame(void *renderToWait)
{
	if (m_IsOffscreen)
		return true;
	// Present the RT
	VkSwapchainKHR		swapChains[] = { m_ApiData.m_SwapChains[0].m_SwapChain };
	VkPresentInfoKHR	presentInfo = {};
	VkSemaphore			renderFinished;

	renderFinished = *static_cast<VkSemaphore*>(renderToWait);
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinished;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &m_CurrentSwapChainImage;
	presentInfo.pResults = null;

#if defined(PK_GGP)
	VkPresentFrameTokenGGP	frameTokenMetadata = {};

	frameTokenMetadata.sType = VK_STRUCTURE_TYPE_PRESENT_FRAME_TOKEN_GGP;
	frameTokenMetadata.pNext = null;
	frameTokenMetadata.frameToken = m_ApiData.m_PlatformSpecificData.m_FrameToken;

	presentInfo.pNext = &frameTokenMetadata;
#endif

	if (PK_VK_FAILED(vkQueuePresentKHR(m_ApiData.m_PresentQueue, &presentInfo)))
		return false;

	return true;
}

//----------------------------------------------------------------------------

bool	CVulkanContext::RecreateSwapChain(const CUint2 &ctxSize)
{
	PK_ASSERT(m_ApiData.m_SwapChains.Count() == 1);
	if (!PK_VERIFY(!m_ApiData.m_SwapChains.Empty()))
		return false;
	// We wait for the device to be Idle before recreating the swap chain
	if (PK_VK_FAILED(vkDeviceWaitIdle(m_ApiData.m_LogicalDevice)))
		return false;
	if (CreateSwapChain(m_ApiData, ctxSize, true, m_VSync, 0, m_IsOffscreen) == false)
		return false;
	return true;
}

//----------------------------------------------------------------------------

TMemoryView<const RHI::PRenderTarget>	CVulkanContext::GetCurrentSwapChain()
{
	return m_ApiData.m_SwapChains[0].m_SwapChainRenderTargets;
}

//----------------------------------------------------------------------------

RHI::SApiContext	*CVulkanContext::GetRenderApiContext()
{
	return &m_ApiData;
}

//----------------------------------------------------------------------------

bool	CVulkanContext::CreateInstance(	RHI::SVulkanBasicContext &basicCtx,
										const TMemoryView<const char *> &extensionsToEnable,
										const TMemoryView<const char *> &layersToEnable,
										char const *appName,
										u32 appVersion,
										char const *engineName,
										u32 engineVersion)
{
	VkApplicationInfo	appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = appName;
	appInfo.applicationVersion = appVersion;
	appInfo.pEngineName = engineName;
	appInfo.engineVersion = engineVersion;
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	if (!layersToEnable.Empty())
	{
		createInfo.enabledLayerCount = layersToEnable.Count();
		createInfo.ppEnabledLayerNames = layersToEnable.Data();
	}
	createInfo.enabledExtensionCount = extensionsToEnable.Count();
	createInfo.ppEnabledExtensionNames = extensionsToEnable.Data();

	VkResult	err;
	err = vkCreateInstance(&createInfo, basicCtx.m_Allocator, &basicCtx.m_Instance);
	if (err != VK_SUCCESS)
	{
		if (err == VK_ERROR_INCOMPATIBLE_DRIVER)
			CLog::Log(PK_ERROR, "vkCreateInstance Failure: Cannot find a compatible Vulkan installable client driver.");
		else if (err == VK_ERROR_EXTENSION_NOT_PRESENT)
			CLog::Log(PK_ERROR, "vkCreateInstance Failure: Cannot find a specified extension library.");
		else
			CLog::Log(PK_ERROR, "vkCreateInstance Failure: %s: Do you have a compatible Vulkan installable client driver (ICD) installed?.", RHI::VkResultToString(err));
		return false;
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CVulkanContext::PickPhysicalDevice(	RHI::SVulkanBasicContext		&basicCtx,
											const TMemoryView<const char *> &layersToEnable,
											const TMemoryView<const char *> &extensionsToEnable,
											bool							&debugLayersEnabled,
											bool							isOffscreen)
{
	u32		gpuCount = 0;
	bool	deviceFound = false;

	if (PK_VK_FAILED(vkEnumeratePhysicalDevices(basicCtx.m_Instance, &gpuCount, null)))
		return false;

	if (!PK_VERIFY_MESSAGE(gpuCount > 0, "vkEnumeratePhysicalDevices reported zero accessible devices."))
		return false;

	TSemiDynamicArray<VkPhysicalDevice, 6>		physicalDevices;
	if (!physicalDevices.Resize(gpuCount))
		return false;

	if (PK_VK_FAILED(vkEnumeratePhysicalDevices(basicCtx.m_Instance, &gpuCount, physicalDevices.RawDataPointer())))
		gpuCount = 0;

	CGuid		bestPhysicalDevice;

	for (u32 i = 0; i < gpuCount && deviceFound == false; ++i)
	{
		// We are not going to use those here
		VkSurfaceCapabilitiesKHR		capabilities;
		TArray<VkSurfaceFormatKHR>		surfaceFormats;
		TArray<VkPresentModeKHR>		presentModes;

		// Check that the required extensions and layers are supported by the device
		bool	extensionSupported = ExtensionsSupportedByDevice(physicalDevices[i], extensionsToEnable);
		bool	layerSupported = InstanceLayersSupportedByDevice(physicalDevices[i], layersToEnable);
		bool	swapChainSupported = isOffscreen ? true : SwapChainSupportedByDevice(basicCtx, physicalDevices[i], capabilities, surfaceFormats, presentModes, 0);

		if (extensionSupported && swapChainSupported)
		{
			if (layerSupported)
			{
				basicCtx.m_PhysicalDevice = physicalDevices[i];
				deviceFound = true;
			}
			else
			{
				// If the debug layers are not handled, we don't really care we can still use this GPU if we don't find any other:
				bestPhysicalDevice = i;
			}
		}
	}

	if (deviceFound == false)
	{
		if (bestPhysicalDevice.Valid())
		{
			// We found a device but it does not support the validation layers:
			debugLayersEnabled = false;
			basicCtx.m_PhysicalDevice = physicalDevices[bestPhysicalDevice];
			CLog::Log(PK_WARN, "GPU does not support the validation layers");
		}
		else
		{
			CLog::Log(PK_ERROR, "Could not find any suitable physical device");
			return false;
		}
	}

	vkGetPhysicalDeviceProperties(basicCtx.m_PhysicalDevice, &basicCtx.m_DeviceProperties); // Unused for the moment
	vkGetPhysicalDeviceMemoryProperties(basicCtx.m_PhysicalDevice, &basicCtx.m_DeviceMemoryProperties);
	return true;
}

//----------------------------------------------------------------------------

bool	CVulkanContext::CreateValidationLayerCallback(	RHI::SVulkanBasicContext &basicCtx,
														bool performanceWarning,
														PFN_vkDebugUtilsMessengerCallbackEXT debugCallbackToUse,
														PFN_vkDestroyDebugUtilsMessengerEXT &vkDestroyDebugMessengerCallback,
														VkDebugUtilsMessengerEXT &debugMessenger)
{
	PFN_vkCreateDebugUtilsMessengerEXT	vkCreateDebugUtilsMessenger;

	vkCreateDebugUtilsMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(basicCtx.m_Instance, "vkCreateDebugUtilsMessengerEXT"));
	vkDestroyDebugMessengerCallback = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(basicCtx.m_Instance, "vkDestroyDebugUtilsMessengerEXT"));
	if (!PK_VERIFY_MESSAGE(vkCreateDebugUtilsMessenger != null, "GetProcAddr: Unable to find vkCreateDebugUtilsMessengerEXT"))
		return false;
	if (!PK_VERIFY_MESSAGE(vkDestroyDebugMessengerCallback != null, "GetProcAddr: Unable to find vkDestroyDebugUtilsMessengerEXT"))
		return false;

	//m_vkDebugReportMessage = (PFN_vkDebugReportMessageEXT)vkGetInstanceProcAddr(basicCtx.m_Instance, "vkDebugReportMessageEXT");
	//if (m_vkDebugReportMessage == null)
	//{
	//	CLog::Log(PK_WARN, "GetProcAddr: Unable to find vkDebugReportMessageEXT");
	//}

	VkDebugUtilsMessengerCreateInfoEXT dbgCreateInfo{};
	dbgCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

	dbgCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
	if (performanceWarning)
		dbgCreateInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	//dbgCreateInfo.flags |= VK_DEBUG_REPORT_INFORMATION_BIT_EXT;
	//dbgCreateInfo.flags |= VK_DEBUG_REPORT_DEBUG_BIT_EXT;
	dbgCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

	dbgCreateInfo.pfnUserCallback = debugCallbackToUse;
	dbgCreateInfo.pUserData = null;
	dbgCreateInfo.pNext = null;
	if (PK_VK_FAILED(vkCreateDebugUtilsMessenger(basicCtx.m_Instance, &dbgCreateInfo, basicCtx.m_Allocator, &debugMessenger)))
		return false;
	return true;
}

//----------------------------------------------------------------------------

static bool	_BeforeSurfaceCreation(RHI::SVulkanBasicContext &basicCtx)
{
	if (!basicCtx.m_SwapChains.PushBack().Valid())
		return false;
	basicCtx.m_SwapChainCount = basicCtx.m_SwapChains.Count();
	return true;
}

//----------------------------------------------------------------------------

static bool	_AfterSurfaceCreation(RHI::SVulkanBasicContext &basicCtx)
{
	// If the physical device was already chosen
	if (basicCtx.m_PhysicalDevice != VK_NULL_HANDLE)
	{
		// We check if the swap chain is compatible with the physical device
		VkBool32		supported;

		if (PK_VK_FAILED(vkGetPhysicalDeviceSurfaceSupportKHR(	basicCtx.m_PhysicalDevice,
																basicCtx.m_GraphicalQueueFamily,
																basicCtx.m_SwapChains.Last().m_Surface,
																&supported)))
			return false;
		return supported == VK_TRUE;
	}
	return true;
}

//----------------------------------------------------------------------------

#if	defined(PK_WINDOWS)

bool	CVulkanContext::CreateWindowSurface(RHI::SVulkanBasicContext &basicCtx, HINSTANCE moduleHandle, HWND windowHandle)
{
	if (!_BeforeSurfaceCreation(basicCtx))
		return false;
	VkWin32SurfaceCreateInfoKHR		createInfo;

	createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	createInfo.pNext = null;
	createInfo.flags = 0;
	createInfo.hinstance = moduleHandle;
	createInfo.hwnd = windowHandle;

	if (PK_VK_FAILED(vkCreateWin32SurfaceKHR(basicCtx.m_Instance, &createInfo, basicCtx.m_Allocator, &basicCtx.m_SwapChains.Last().m_Surface)))
		return false;
	return _AfterSurfaceCreation(basicCtx);
}

#elif	defined(PK_LINUX) && !defined(PK_GGP)

bool	CVulkanContext::CreateWindowSurface(RHI::SVulkanBasicContext &basicCtx, ureg display, ureg window)
{
	if (!_BeforeSurfaceCreation(basicCtx))
		return false;
	VkXlibSurfaceCreateInfoKHR	createInfo;

	createInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
	createInfo.pNext = null;
	createInfo.flags = 0;
	createInfo.dpy = (Display *)display;
	createInfo.window = (Window)window;

	if (PK_VK_FAILED(vkCreateXlibSurfaceKHR(basicCtx.m_Instance, &createInfo, basicCtx.m_Allocator, &basicCtx.m_SwapChains.Last().m_Surface)))
		return false;
	return _AfterSurfaceCreation(basicCtx);
}

#elif	defined(PK_MACOSX)

bool	CVulkanContext::CreateWindowSurface(RHI::SVulkanBasicContext &basicCtx, void *view)
{
	if (!_BeforeSurfaceCreation(basicCtx))
		return false;
	VkMacOSSurfaceCreateInfoMVK	createInfo;

	createInfo.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
	createInfo.pNext = null;
	createInfo.flags = 0;
	MacOSxUtils::MakeViewMetalCompatible(view);
	createInfo.pView = view;

	if (PK_VK_FAILED(vkCreateMacOSSurfaceMVK(basicCtx.m_Instance, &createInfo, basicCtx.m_Allocator, &basicCtx.m_SwapChains.Last().m_Surface)))
		return false;
	return _AfterSurfaceCreation(basicCtx);
}

#elif	defined(PK_GGP)

bool	CVulkanContext::CreateWindowSurface(RHI::SVulkanBasicContext &basicCtx)
{
	if (!_BeforeSurfaceCreation(basicCtx))
		return false;

	VkStreamDescriptorSurfaceCreateInfoGGP createInfo;

	createInfo.sType = VK_STRUCTURE_TYPE_STREAM_DESCRIPTOR_SURFACE_CREATE_INFO_GGP;
	createInfo.pNext = null;
	createInfo.flags = 0;
	createInfo.streamDescriptor = 1;

	VK_GET_INSTANCE_PROC_ADDR(fpCreateStreamDescriptorSurfaceGGP, basicCtx.m_Instance, vkCreateStreamDescriptorSurfaceGGP);

	if (PK_VK_FAILED(fpCreateStreamDescriptorSurfaceGGP(basicCtx.m_Instance, &createInfo, basicCtx.m_Allocator, &basicCtx.m_SwapChains.Last().m_Surface)))
		return false;

	return _AfterSurfaceCreation(basicCtx);
}

#elif	defined(PK_NX)
bool	CVulkanContext::CreateWindowSurface(RHI::SVulkanBasicContext &basicCtx, void *window)
{
	if (!_BeforeSurfaceCreation(basicCtx))
		return false;

	VkViSurfaceCreateInfoNN	createInfo;

	createInfo.sType = VK_STRUCTURE_TYPE_VI_SURFACE_CREATE_INFO_NN;
	createInfo.pNext = null;
	createInfo.flags = 0;
	createInfo.window = window;

	if (PK_VK_FAILED(vkCreateViSurfaceNN(basicCtx.m_Instance, &createInfo, basicCtx.m_Allocator, &basicCtx.m_SwapChains.Last().m_Surface)))
		return false;

	return _AfterSurfaceCreation(basicCtx);
}
#else
#	error not implemented
#endif

//----------------------------------------------------------------------------

bool	CVulkanContext::CreateQueueAndLogicalDevice(RHI::SVulkanBasicContext			&basicCtx,
													const TMemoryView<const char *>		&extensionsToEnable,
													const TMemoryView<const char *>		&layersToEnable,
													bool								isOffscreen)
{

	TSemiDynamicArray<VkDeviceQueueCreateInfo, 3>	queueCreateInfo;
	TSemiDynamicArray<int, 3>						queueFamilies;
	float											queuePriority = 1.0f;
	TArray<VkQueueFamilyProperties>					queueFamilyProperties;

	// Check if we can find a queue that supports graphics and present
	basicCtx.m_PresentQueueFamily = basicCtx.m_GraphicalQueueFamily = GetQueueFamilyIdx(basicCtx, queueFamilyProperties, !isOffscreen, VK_QUEUE_GRAPHICS_BIT);
	// If it does not exist, create two queues
	if (basicCtx.m_GraphicalQueueFamily == -1)
	{
		if (!queueFamilies.Resize(2))
			return false;
		basicCtx.m_PresentQueueFamily = GetQueueFamilyIdx(basicCtx, queueFamilyProperties, true);
		basicCtx.m_GraphicalQueueFamily = GetQueueFamilyIdx(basicCtx, queueFamilyProperties, false, VK_QUEUE_GRAPHICS_BIT);
		queueFamilies[0] = basicCtx.m_PresentQueueFamily;
		queueFamilies[1] = basicCtx.m_GraphicalQueueFamily;
	}
	else // Create only one queue that supports both
	{
		if (!queueFamilies.PushBack(basicCtx.m_PresentQueueFamily).Valid())
			return false;
	}

	// Use of multiple transfer queue not handled for the moment

	//int		transferQueue = GetQueueFamilyIdx(false, VK_QUEUE_TRANSFER_BIT, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
	//if (transferQueue != -1)
	//{
	//	queueFamilies.PushBack(transferQueue);
	//	m_ApiData.m_TransferQueueFamilies.PushBack(transferQueue);
	//}

	//if (m_ApiData.m_TransferQueueFamilies.Empty())
	//	m_ApiData.m_TransferQueueFamilies.PushBack(m_ApiData.m_GraphicalQueueFamily);

	if (!queueCreateInfo.Resize(queueFamilies.Count()))
		return false;
	Mem::Clear(queueCreateInfo.RawDataPointer(), queueCreateInfo.SizeInBytes());
	for (u32 i = 0; i < queueCreateInfo.Count(); ++i)
	{
		queueCreateInfo[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo[i].queueFamilyIndex = queueFamilies[i];
		queueCreateInfo[i].queueCount = 1;
		queueCreateInfo[i].pQueuePriorities = &queuePriority;
	}

	// We do not need any specific device feature
	VkPhysicalDeviceFeatures	deviceFeatures = {};

	deviceFeatures.wideLines = VK_TRUE;
	deviceFeatures.fillModeNonSolid = VK_TRUE;
	deviceFeatures.geometryShader = VK_TRUE;
	deviceFeatures.samplerAnisotropy = VK_TRUE;
	deviceFeatures.textureCompressionBC = VK_TRUE;
	deviceFeatures.depthClamp = VK_TRUE;
	deviceFeatures.shaderStorageImageWriteWithoutFormat = VK_TRUE;

	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = queueCreateInfo.Count();
	createInfo.pQueueCreateInfos = queueCreateInfo.RawDataPointer();
	createInfo.pEnabledFeatures = &deviceFeatures;
	// We create the appropriate logical device with the right extensions (especially VK_KHR_SWAPCHAIN_EXTENSION_NAME)
	createInfo.enabledExtensionCount = extensionsToEnable.Count();
	createInfo.ppEnabledExtensionNames = extensionsToEnable.Data();
	// and the validation layer that we got from VulkanGetInstanceValidationLayersToEnable
	createInfo.enabledLayerCount = layersToEnable.Count();
	createInfo.ppEnabledLayerNames = layersToEnable.Data();

	if (PK_VK_FAILED(vkCreateDevice(basicCtx.m_PhysicalDevice, &createInfo, basicCtx.m_Allocator, &basicCtx.m_LogicalDevice)))
		return false;

	vkGetDeviceQueue(basicCtx.m_LogicalDevice, basicCtx.m_GraphicalQueueFamily, 0, &basicCtx.m_GraphicalQueue);
	vkGetDeviceQueue(basicCtx.m_LogicalDevice, basicCtx.m_PresentQueueFamily, 0, &basicCtx.m_PresentQueue);
	//m_ApiData.m_TransferQueues.Resize(m_ApiData.m_TransferQueueFamilies.Count());
	//for (u32 i = 0; i < m_ApiData.m_TransferQueueFamilies.Count(); ++i)
	//{
	//	vkGetDeviceQueue(m_ApiData.m_LogicalDevice, m_ApiData.m_TransferQueueFamilies[i], 0, &m_ApiData.m_TransferQueues[i]);
	//}
	return PK_VERIFY(LoadExtensionFunctionPointers(basicCtx));
}

//----------------------------------------------------------------------------

bool	CVulkanContext::LoadExtensionFunctionPointers(RHI::SVulkanBasicContext &basicCtx)
{
#ifndef PK_RETAIL
	basicCtx.m_PfnSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(basicCtx.m_Instance, "vkSetDebugUtilsObjectNameEXT");
	if (!PK_VERIFY_MESSAGE(basicCtx.m_PfnSetDebugUtilsObjectNameEXT != null, "GetProcAddr: Unable to find vkCreateDebugUtilsMessengerEXT"))
		return false;
#else
	(void)basicCtx;
#endif
	return true;
}

//----------------------------------------------------------------------------

CVulkanContext::ESwapChainOpResult	CVulkanContext::CreateSwapChain(RHI::SVulkanBasicContext &basicCtx,
																		const CUint2 &windowSize,
																		bool srgb,
																		bool vSync,
																		u32 swapChainIdx,
																		bool isOffscreen)
{
	if (isOffscreen)
	{
		return CreateOffscreenSwapChain(basicCtx, windowSize, srgb, swapChainIdx);
	}
	VkSurfaceCapabilitiesKHR									capabilities;
	TArray<VkSurfaceFormatKHR>									surfaceFormats;
	TArray<VkPresentModeKHR>									presentModes;
	TSemiDynamicArray<VkImage, RHI::kMaxSwapChainImages>		swapChainImages;
	TSemiDynamicArray<VkImageView, RHI::kMaxSwapChainImages>	swapChainImageViews;

	// If the swap chain already exist, we need to get once again all the options
	if (!SwapChainSupportedByDevice(basicCtx, basicCtx.m_PhysicalDevice, capabilities, surfaceFormats, presentModes, swapChainIdx))
		return SwapChainOp_CriticalError;
	VkSurfaceFormatKHR surfaceFormat = GetOptimalSurfaceFormat(surfaceFormats, srgb);
	VkPresentModeKHR presentMode = GetOptimalPresentationMode(presentModes, vSync);
	VkExtent2D extent = GetOptimalSwapChainExtent(capabilities, windowSize.x(), windowSize.y());

	u32		imageCount = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
	{
		imageCount = capabilities.maxImageCount;
	}
	imageCount = PKMin(imageCount, static_cast<u32>(RHI::kMaxSwapChainImages));

	VkSwapchainKHR				newSwapChain;
	PK_ASSERT(basicCtx.m_GraphicalQueueFamily >= 0 && basicCtx.m_PresentQueueFamily >= 0);
	u32							queues[2] = { u32(basicCtx.m_GraphicalQueueFamily), u32(basicCtx.m_PresentQueueFamily) };
	VkSwapchainCreateInfoKHR	createInfo = {};

	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = basicCtx.m_SwapChains[swapChainIdx].m_Surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if (basicCtx.m_GraphicalQueueFamily != basicCtx.m_PresentQueueFamily)
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queues;
	}
	else
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}
	createInfo.preTransform = capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = basicCtx.m_SwapChains[swapChainIdx].m_SwapChain;

	// Create the semaphore to synchronize the swap-chain
	if (basicCtx.m_SwapChains[swapChainIdx].m_SwapChainImageAvailable == VK_NULL_HANDLE)
	{
		VkSemaphoreCreateInfo	semaphoreInfo = {};

		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		if (PK_VK_FAILED(vkCreateSemaphore(	basicCtx.m_LogicalDevice,
											&semaphoreInfo,
											basicCtx.m_Allocator,
											&basicCtx.m_SwapChains[swapChainIdx].m_SwapChainImageAvailable)))
			return SwapChainOp_CriticalError;
	}
	imageCount = PKMin(imageCount, static_cast<u32>(RHI::kMaxSwapChainImages));

	// We raise a fatal error when we cannot create the swap-chain and the device is lost
	VkResult	createSwapChainResult = vkCreateSwapchainKHR(basicCtx.m_LogicalDevice, &createInfo, basicCtx.m_Allocator, &newSwapChain);
	if (createSwapChainResult == VK_ERROR_DEVICE_LOST)
		return SwapChainOp_DeviceLost;
	else if (createSwapChainResult == VK_ERROR_INITIALIZATION_FAILED)
		return SwapChainOp_Failed;
	else if (createSwapChainResult != VK_SUCCESS)
		return SwapChainOp_CriticalError;

	if (PK_VK_FAILED(vkGetSwapchainImagesKHR(basicCtx.m_LogicalDevice, newSwapChain, &imageCount, null)))
		return SwapChainOp_CriticalError;

	if (!swapChainImages.Resize(imageCount))
		return SwapChainOp_CriticalError;

	if (PK_VK_FAILED(vkGetSwapchainImagesKHR(basicCtx.m_LogicalDevice, newSwapChain, &imageCount, swapChainImages.RawDataPointer())))
		return SwapChainOp_CriticalError;

	if (basicCtx.m_SwapChains[swapChainIdx].m_SwapChain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(basicCtx.m_LogicalDevice, basicCtx.m_SwapChains[swapChainIdx].m_SwapChain, basicCtx.m_Allocator);
	}

	basicCtx.m_SwapChains[swapChainIdx].m_SwapChain = newSwapChain;
	basicCtx.m_SwapChains[swapChainIdx].m_SwapChainFormat = surfaceFormat;
	basicCtx.m_SwapChains[swapChainIdx].m_SwapChainExtent = extent;

	if (!CreateSwapChainImageViews(basicCtx, swapChainImages, swapChainImageViews, swapChainIdx))
		return SwapChainOp_CriticalError;

	// We then construct the swap chain render target list
	if (!basicCtx.m_SwapChains[swapChainIdx].m_SwapChainRenderTargets.Resize(imageCount))
		return SwapChainOp_CriticalError;
	for (u32 i = 0; i < imageCount; ++i)
	{
		RHI::CVulkanRenderTarget		*toAdd = PK_NEW(RHI::CVulkanRenderTarget(RHI::SRHIResourceInfos("VulkanContext Swap Chain")));
		toAdd->VulkanSetRenderTarget(swapChainImageViews[i], surfaceFormat.format, extent, true);
		basicCtx.m_SwapChains[swapChainIdx].m_SwapChainRenderTargets[i] = toAdd;
	}
	basicCtx.m_BufferingMode = RHI::ContextDoubleBuffering;
	return SwapChainOp_Success;
}

//----------------------------------------------------------------------------

CVulkanContext::ESwapChainOpResult	CVulkanContext::CreateOffscreenSwapChain(RHI::SVulkanBasicContext &basicCtx,
																		const CUint2 &windowSize,
																		bool srgb,
																		u32 swapChainIdx)
{
	TSemiDynamicArray<VkImage, RHI::kMaxSwapChainImages>		&m_OffscreenSwapchainImages = basicCtx.m_SwapChains[swapChainIdx].m_OffscreenSwapchainImages;
	TSemiDynamicArray<VkDeviceMemory, RHI::kMaxSwapChainImages>	&m_OffscreenSwapchainImagesMemory = basicCtx.m_SwapChains[swapChainIdx].m_OffscreenSwapchainImagesMemory;
	TSemiDynamicArray<VkImageView, RHI::kMaxSwapChainImages>	offscreenSwapchainImageViews;
	const u32													imageCount = 2;
	const VkFormat												defaultFormat = srgb ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_B8G8R8A8_UNORM;
	const VkSurfaceFormatKHR									offscreenFormat{ defaultFormat, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
	const VkExtent3D											extent{ windowSize.x(), windowSize.y(), 1 };
	const u32													queue = u32(basicCtx.m_GraphicalQueueFamily);

	basicCtx.m_SwapChains[swapChainIdx].m_SwapChainFormat = offscreenFormat;
	basicCtx.m_SwapChains[swapChainIdx].m_SwapChainExtent = VkExtent2D{ extent.width, extent.height };

	if (!(m_OffscreenSwapchainImages.Resize(imageCount) && m_OffscreenSwapchainImagesMemory.Resize(imageCount) && offscreenSwapchainImageViews.Resize(imageCount)))
		return SwapChainOp_CriticalError;

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = offscreenFormat.format;
	imageInfo.extent = extent;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.queueFamilyIndexCount = 1;
	imageInfo.pQueueFamilyIndices = &queue;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	for (u32 i = 0; i < imageCount; i++)
	{
		vkCreateImage(basicCtx.m_LogicalDevice, &imageInfo, basicCtx.m_Allocator, &m_OffscreenSwapchainImages[i]);

		VkMemoryRequirements		memRequirements;
		vkGetImageMemoryRequirements(basicCtx.m_LogicalDevice, m_OffscreenSwapchainImages[i], &memRequirements);
		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;

		CGuid	memoryTypeIndex;
		for (u32 j = 0; j < basicCtx.m_DeviceMemoryProperties.memoryTypeCount; j++)
		{
			if ((memRequirements.memoryTypeBits & (1 << j)) &&
				(basicCtx.m_DeviceMemoryProperties.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
			{
				memoryTypeIndex = j;
			}
		}
		if (!PK_VERIFY(memoryTypeIndex.Valid()))
			return SwapChainOp_CriticalError;

		allocInfo.memoryTypeIndex = memoryTypeIndex;
		if (PK_VK_FAILED(vkAllocateMemory(basicCtx.m_LogicalDevice, &allocInfo, basicCtx.m_Allocator, &m_OffscreenSwapchainImagesMemory[i])))
			return SwapChainOp_CriticalError;
		if (PK_VK_FAILED(vkBindImageMemory(basicCtx.m_LogicalDevice, m_OffscreenSwapchainImages[i], m_OffscreenSwapchainImagesMemory[i], 0)))
			return SwapChainOp_CriticalError;
	}

	if (!CreateSwapChainImageViews(basicCtx, TMemoryView<VkImage>(m_OffscreenSwapchainImages), offscreenSwapchainImageViews, swapChainIdx))
		return SwapChainOp_CriticalError;

	if (!basicCtx.m_SwapChains[swapChainIdx].m_SwapChainRenderTargets.Resize(imageCount))
		return SwapChainOp_CriticalError;

	for (u32 i = 0; i < imageCount; ++i)
	{
		RHI::CVulkanRenderTarget		*toAdd = PK_NEW(RHI::CVulkanRenderTarget(RHI::SRHIResourceInfos("VulkanContext Offscreen Render Target")));
		toAdd->VulkanSetRenderTarget(offscreenSwapchainImageViews[i], basicCtx.m_SwapChains[swapChainIdx].m_SwapChainFormat.format, basicCtx.m_SwapChains[swapChainIdx].m_SwapChainExtent, true);
		basicCtx.m_SwapChains[swapChainIdx].m_SwapChainRenderTargets[i] = toAdd;
	}
	basicCtx.m_BufferingMode = RHI::ContextDoubleBuffering;

	if (basicCtx.m_SwapChains[swapChainIdx].m_SwapChainImageAvailable == VK_NULL_HANDLE)
	{
		VkSemaphoreCreateInfo	semaphoreInfo = {};

		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		if (PK_VK_FAILED(vkCreateSemaphore(	basicCtx.m_LogicalDevice,
											&semaphoreInfo,
											basicCtx.m_Allocator,
											&basicCtx.m_SwapChains[swapChainIdx].m_SwapChainImageAvailable)))
			return SwapChainOp_CriticalError;
	}
	return SwapChainOp_Success;
}

//----------------------------------------------------------------------------

void	CVulkanContext::DestroyApiData(	RHI::SVulkanBasicContext &basicCtx)
{
	for (u32 i = 0; i < basicCtx.m_SwapChains.Count(); ++i)
	{
		DestroySwapChain(basicCtx, i);
	}

	RHI::SVulkanBasicContext::SUnsafePools	&unsafePool = basicCtx.m_Pool;

	if (unsafePool.m_CmdPool != VK_NULL_HANDLE)
	{
		vkDestroyCommandPool(basicCtx.m_LogicalDevice, unsafePool.m_CmdPool, basicCtx.m_Allocator);
		unsafePool.m_CmdPool = VK_NULL_HANDLE;
	}
	if (unsafePool.m_CmdPoolTransient != VK_NULL_HANDLE)
	{
		vkDestroyCommandPool(basicCtx.m_LogicalDevice, unsafePool.m_CmdPoolTransient, basicCtx.m_Allocator);
		unsafePool.m_CmdPoolTransient = VK_NULL_HANDLE;
	}
	if (unsafePool.m_CmdPoolTransientReset != VK_NULL_HANDLE)
	{
		vkDestroyCommandPool(basicCtx.m_LogicalDevice, unsafePool.m_CmdPoolTransientReset, basicCtx.m_Allocator);
		unsafePool.m_CmdPoolTransientReset = VK_NULL_HANDLE;
	}
	if (unsafePool.m_DescriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(basicCtx.m_LogicalDevice, unsafePool.m_DescriptorPool, basicCtx.m_Allocator);
		unsafePool.m_DescriptorPool = VK_NULL_HANDLE;
	}
	vkDestroyDevice(basicCtx.m_LogicalDevice, basicCtx.m_Allocator);
	basicCtx.m_LogicalDevice = VK_NULL_HANDLE;
	// We then destroy the instance
	vkDestroyInstance(basicCtx.m_Instance, basicCtx.m_Allocator);
	basicCtx.m_Instance = VK_NULL_HANDLE;
	PK_SAFE_DELETE(basicCtx.m_Allocator);

}

//----------------------------------------------------------------------------

bool	CVulkanContext::DestroySwapChain(RHI::SVulkanBasicContext &basicCtx, u32 idx)
{
	if (basicCtx.m_SwapChains[idx].m_SwapChainImageAvailable != VK_NULL_HANDLE)
	{
		vkDestroySemaphore(basicCtx.m_LogicalDevice, basicCtx.m_SwapChains[idx].m_SwapChainImageAvailable, basicCtx.m_Allocator);
		basicCtx.m_SwapChains[idx].m_SwapChainImageAvailable = VK_NULL_HANDLE;
	}

	// Offscren Images
	for (VkImage &image: basicCtx.m_SwapChains[idx].m_OffscreenSwapchainImages)
	{
		if (image != VK_NULL_HANDLE)
		{
			vkDestroyImage(basicCtx.m_LogicalDevice, image, null);
			image = VK_NULL_HANDLE;
		}
	}
	for (VkDeviceMemory &memory: basicCtx.m_SwapChains[idx].m_OffscreenSwapchainImagesMemory)
	{
		if (memory != VK_NULL_HANDLE)
		{
			vkFreeMemory(basicCtx.m_LogicalDevice, memory, null);
			memory = VK_NULL_HANDLE;
		}
	}

	// We destroy everything before the Vulkan instance
	basicCtx.m_SwapChains[idx].m_SwapChainRenderTargets.Clear();

	if (basicCtx.m_SwapChains[idx].m_SwapChain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(basicCtx.m_LogicalDevice, basicCtx.m_SwapChains[idx].m_SwapChain, basicCtx.m_Allocator);
		basicCtx.m_SwapChains[idx].m_SwapChain = VK_NULL_HANDLE;
	}

	if (basicCtx.m_SwapChains[idx].m_Surface != VK_NULL_HANDLE)
	{
		vkDestroySurfaceKHR(basicCtx.m_Instance, basicCtx.m_SwapChains[idx].m_Surface, basicCtx.m_Allocator);
		basicCtx.m_SwapChains[idx].m_Surface = VK_NULL_HANDLE;
	}

	basicCtx.m_SwapChains.Remove(idx);
	basicCtx.m_SwapChainCount = basicCtx.m_SwapChains.Count();
	return true;
}

//----------------------------------------------------------------------------
//
//	Private members
//
//----------------------------------------------------------------------------

bool	CVulkanContext::GetInstanceExtensionsToEnable(TArray<const char *> &extensions, bool enableValidationLayer, bool enableDebugNames, bool isOffscreen /* = false*/)
{
	bool		instanceExtensionsFound = false;
	u32			instanceExtensionCount = 0;

	TArray<const char*> requiredInstanceExtensions;

	if (!isOffscreen)
	{
		requiredInstanceExtensions.PushBack(VK_KHR_SURFACE_EXTENSION_NAME);
		requiredInstanceExtensions.PushBack(PLATFORM_SURFACE_EXTENSION_NAME);
	}
	if (enableValidationLayer || enableDebugNames)
	{
		requiredInstanceExtensions.PushBack(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	if (PK_VK_FAILED(vkEnumerateInstanceExtensionProperties(null, &instanceExtensionCount, null)))
		return false;

	if (instanceExtensionCount > 0)
	{
		TArray<VkExtensionProperties>	instanceExtensions;

		if (!instanceExtensions.Resize(instanceExtensionCount))
			return false;
		if (PK_VK_FAILED(vkEnumerateInstanceExtensionProperties(null, &instanceExtensionCount, instanceExtensions.RawDataPointer())))
			return false;
		instanceExtensionsFound = CheckExtensionNamesExist(requiredInstanceExtensions, instanceExtensions);

		if (instanceExtensionsFound)
		{
			if (!extensions.Resize(requiredInstanceExtensions.Count()))
				return false;
			for (u32 i = 0; i < extensions.Count(); ++i)
			{
					extensions[i] = requiredInstanceExtensions[i];
			}
		}
	}
	else
	{
		CLog::Log(PK_ERROR, "vkEnumerateInstanceExtensionProperties failed to find required extensions.");
		return false;
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CVulkanContext::GetInstanceValidationLayersToEnable(TArray<const char *> &layers)
{
	bool	validationLayerFound = false;
	u32		instanceLayerCount = 0;

	if (PK_VK_FAILED(vkEnumerateInstanceLayerProperties(&instanceLayerCount, null)))
		return false;

	static char	const	*validationLayersToEnable[] =
	{
		"VK_LAYER_LUNARG_standard_validation",
		"VK_LAYER_LUNARG_core_validation",
		"VK_LAYER_GOOGLE_threading",
		"VK_LAYER_LUNARG_parameter_validation",
		"VK_LAYER_LUNARG_device_limits",
		"VK_LAYER_LUNARG_object_tracker",
		"VK_LAYER_LUNARG_image",
		"VK_LAYER_LUNARG_core_validation",
		"VK_LAYER_LUNARG_swapchain",
		"VK_LAYER_GOOGLE_unique_objects",
		"VK_LAYER_KHRONOS_validation"
	};

	if (instanceLayerCount > 0)
	{
		TArray<VkLayerProperties>	instanceLayers;

		if (!instanceLayers.Resize(instanceLayerCount))
			return false;
		if (PK_VK_FAILED(vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayers.RawDataPointer())))
			return false;

		for (u32 j = 0; j < PK_ARRAY_COUNT(validationLayersToEnable); ++j)
		{
			if (CheckLayerNamesExist(TMemoryView<const char*>(validationLayersToEnable[j]), instanceLayers))
			{
				if (!layers.PushBack(validationLayersToEnable[j]).Valid())
					return false;
				validationLayerFound = true;
			}
		}
	}
	if (validationLayerFound == false)
	{
		CLog::Log(PK_ERROR, "vkEnumerateInstanceLayerProperties failed to find any requested validation layer.");
		return false;
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CVulkanContext::ExtensionsSupportedByDevice(VkPhysicalDevice device, const TMemoryView<const char *> &extensionsToEnable)
{
	u32		deviceExtensionCount = 0;

	if (PK_VK_FAILED(vkEnumerateDeviceExtensionProperties(device, null, &deviceExtensionCount, null)))
		return false;
	if (deviceExtensionCount > 0)
	{
		TArray<VkExtensionProperties>	deviceExtensions;

		if (!deviceExtensions.Resize(deviceExtensionCount))
			return false;
		if (PK_VK_FAILED(vkEnumerateDeviceExtensionProperties(device, null, &deviceExtensionCount, deviceExtensions.RawDataPointer())))
			return false;
		return CheckExtensionNamesExist(extensionsToEnable, deviceExtensions);
	}
	return extensionsToEnable.Empty();
}

//----------------------------------------------------------------------------

bool	CVulkanContext::InstanceLayersSupportedByDevice(VkPhysicalDevice device, const TMemoryView<const char *> &enabledLayers)
{
	u32	deviceLayerCount = 0;

	if (PK_VK_FAILED(vkEnumerateDeviceLayerProperties(device, &deviceLayerCount, null)))
		return false;
	if (deviceLayerCount > 0)
	{
		TArray<VkLayerProperties>	deviceLayers;

		if (!deviceLayers.Resize(deviceLayerCount))
			return false;
		if (PK_VK_FAILED(vkEnumerateDeviceLayerProperties(device, &deviceLayerCount, deviceLayers.RawDataPointer())))
			return false;
		return CheckLayerNamesExist(enabledLayers, deviceLayers);
	}
	return enabledLayers.Empty();
}

//----------------------------------------------------------------------------

bool	CVulkanContext::SwapChainSupportedByDevice(	const RHI::SVulkanBasicContext	&basicCtx,
													VkPhysicalDevice				device,
													VkSurfaceCapabilitiesKHR		&capabilities,
													TArray<VkSurfaceFormatKHR>		&surfaceFormats,
													TArray<VkPresentModeKHR>		&presentModes,
													u32								swapChainIdx)
{
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, basicCtx.m_SwapChains[swapChainIdx].m_Surface, &capabilities);

	u32	formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, basicCtx.m_SwapChains[swapChainIdx].m_Surface, &formatCount, null);

	if (formatCount != 0)
	{
		if (!surfaceFormats.Resize(formatCount))
			return false;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, basicCtx.m_SwapChains[swapChainIdx].m_Surface, &formatCount, surfaceFormats.RawDataPointer());
	}
	else
	{
		return false;
	}

	u32	presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, basicCtx.m_SwapChains[swapChainIdx].m_Surface, &presentModeCount, null);

	if (presentModeCount != 0)
	{
		if (!presentModes.Resize(presentModeCount))
			return false;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, basicCtx.m_SwapChains[swapChainIdx].m_Surface, &presentModeCount, presentModes.RawDataPointer());
	}
	else
	{
		return false;
	}
	return true;
}

//----------------------------------------------------------------------------

int		CVulkanContext::GetQueueFamilyIdx(	const RHI::SVulkanBasicContext &basicCtx,
											TArray<VkQueueFamilyProperties> &queueFamilyProperties,
											bool supportPresent,
											u32 queueFamilyFlags,
											u32 shouldNotSupport)
{
	if (queueFamilyProperties.Empty())
	{
		u32	queueCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(basicCtx.m_PhysicalDevice, &queueCount, null);
		if (!queueFamilyProperties.Resize(queueCount))
			return -1;
		vkGetPhysicalDeviceQueueFamilyProperties(basicCtx.m_PhysicalDevice, &queueCount, queueFamilyProperties.RawDataPointer());
	}
	for (u32 i = 0; i < queueFamilyProperties.Count(); ++i)
	{
		VkQueueFamilyProperties		&currentFamily = queueFamilyProperties[i];
		// We just require a graphic queue
		if (currentFamily.queueCount > 0 && (currentFamily.queueFlags & queueFamilyFlags) != 0 && (currentFamily.queueFlags & shouldNotSupport) == 0)
		{
			VkBool32	supported;
			if (!supportPresent ||
				(!PK_VK_FAILED(vkGetPhysicalDeviceSurfaceSupportKHR(basicCtx.m_PhysicalDevice, i, basicCtx.m_SwapChains[0].m_Surface, &supported)) &&
				supported))
			{
				return i;
			}
		}
	}
	return -1;
}

//----------------------------------------------------------------------------

VkSurfaceFormatKHR	CVulkanContext::GetOptimalSurfaceFormat(const TMemoryView<VkSurfaceFormatKHR> &surfaceFormats, bool srgb)
{
	const VkFormat	defaultFormat = srgb ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_B8G8R8A8_UNORM;
	if (surfaceFormats.Count() == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED)
	{
		VkSurfaceFormatKHR	surfaceFormat;
		surfaceFormat.format = defaultFormat;
		surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		return surfaceFormat;
	}

	for (u32 i = 0; i < surfaceFormats.Count(); ++i)
	{
		if (surfaceFormats[i].format == defaultFormat && surfaceFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return surfaceFormats[i];
		}
	}
	return surfaceFormats[0];
}

//----------------------------------------------------------------------------

VkPresentModeKHR	CVulkanContext::GetOptimalPresentationMode(const TMemoryView<VkPresentModeKHR> &presentModes, bool vSync)
{
	if (vSync)
		return VK_PRESENT_MODE_FIFO_KHR;
	for (u32 i = 0; i < presentModes.Count(); ++i)
	{
		if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return presentModes[i];
		}
		if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
		{
			return presentModes[i];
		}
	}
	return VK_PRESENT_MODE_FIFO_KHR;
}

//----------------------------------------------------------------------------

VkExtent2D	CVulkanContext::GetOptimalSwapChainExtent(	const VkSurfaceCapabilitiesKHR	&capabilities,
														u32								windowsWidth,
														u32								windowsHeight)
{
	if (capabilities.currentExtent.width != TNumericTraits<u32>::Max())
	{
		return capabilities.currentExtent;
	}
	else
	{
		VkExtent2D	actualExtent;
		actualExtent.width = PKClamp(windowsWidth, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = PKClamp(windowsHeight, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
		return actualExtent;
	}
}

//----------------------------------------------------------------------------

bool	CVulkanContext::CreateSwapChainImageViews(	const RHI::SVulkanBasicContext								&basicCtx,
													const TMemoryView<VkImage>									&scImages,
													TSemiDynamicArray<VkImageView, RHI::kMaxSwapChainImages>	&scImageViews,
													u32															swapChainIdx)
{
	if (!scImageViews.Resize(scImages.Count()))
		return false;

	for (u32 i = 0; i < scImages.Count(); i++)
	{
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = scImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = basicCtx.m_SwapChains[swapChainIdx].m_SwapChainFormat.format;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		if (PK_VK_FAILED(vkCreateImageView(basicCtx.m_LogicalDevice, &createInfo, basicCtx.m_Allocator, &scImageViews[i])))
			return false;
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CVulkanContext::CheckLayerNamesExist(	const TMemoryView<const char *>				&toCheckNames,
												const TMemoryView<const VkLayerProperties>	&layers)
{
	for (u32 i = 0; i < toCheckNames.Count(); i++)
	{
		bool	found = false;
		for (u32 j = 0; j < layers.Count(); j++)
		{
			if (!strcmp(toCheckNames[i], layers[j].layerName))
			{
				found = true;
				break;
			}
		}
		if (!found)
		{
			CLog::Log(PK_ERROR, "Cannot find layer: %s", toCheckNames[i]);
			return false;
		}
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CVulkanContext::CheckExtensionNamesExist(	const TMemoryView<const char *>					&toCheckNames,
													const TMemoryView<const VkExtensionProperties>	&extensions)
{
	for (u32 i = 0; i < toCheckNames.Count(); i++)
	{
		bool	found = false;
		for (u32 j = 0; j < extensions.Count(); j++)
		{
			if (!strcmp(toCheckNames[i], extensions[j].extensionName))
			{
				found = true;
				break;
			}
		}
		if (!found)
		{
			CLog::Log(PK_ERROR, "Cannot find extension: %s", toCheckNames[i]);
			return false;
		}
	}
	return true;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

#endif // (PK_BUILD_WITH_VULKAN != 0)
