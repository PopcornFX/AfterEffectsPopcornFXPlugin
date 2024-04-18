#pragma once

//----------------------------------------------------------------------------
// This program is the property of Persistant Studios SARL.
//
// You may not redistribute it and/or modify it under any conditions
// without written permission from Persistant Studios SARL, unless
// otherwise stated in the latest Persistant Studios Code License.
//
// See the Persistant Studios Code License for further details.
//----------------------------------------------------------------------------

#include <PK-SampleLib/ApiContextConfig.h>

#if (PK_BUILD_WITH_VULKAN_SUPPORT != 0)

#if	defined(PK_WINDOWS)
#	define PLATFORM_SURFACE_EXTENSION_NAME		VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#	define VK_USE_PLATFORM_WIN32_KHR
#elif	defined(PK_LINUX) && !defined(PK_GGP)
#	define PLATFORM_SURFACE_EXTENSION_NAME		VK_KHR_XLIB_SURFACE_EXTENSION_NAME
#	define VK_USE_PLATFORM_XLIB_KHR
#elif	defined(PK_MACOSX)
#	define PLATFORM_SURFACE_EXTENSION_NAME		VK_MVK_MACOS_SURFACE_EXTENSION_NAME
#	define VK_USE_PLATFORM_MACOS_MVK
#elif	defined(PK_GGP)
#	define PLATFORM_SURFACE_EXTENSION_NAME		VK_GGP_STREAM_DESCRIPTOR_SURFACE_EXTENSION_NAME
#	define VK_USE_PLATFORM_GGP
#else
#	error	Unrecognized Vulkan platform
#endif

#include <pk_rhi/include/vulkan/VulkanBasicContext.h>
#include <PK-SampleLib/ApiContext/IApiContext.h>
#include <PK-SampleLib/WindowContext/AWindowContext.h>

struct		SDL_Window;

#define		MAX_ENABLED_EXTENSIONS	16

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

VKAPI_ATTR VkBool32 VKAPI_CALL		VulkanDebugMessengerCallback(	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
																VkDebugUtilsMessageTypeFlagsEXT messageTypes,
																const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
																void *pUserData);
VKAPI_ATTR void* VKAPI_CALL			VulkanAllocate(	void *pUserData,
													size_t size,
													size_t alignment,
													VkSystemAllocationScope allocationScope);
VKAPI_ATTR void* VKAPI_CALL			VulkanReallocate(	void *pUserData,
														void *pOriginal,
														size_t size,
														size_t alignment,
														VkSystemAllocationScope allocationScope);
VKAPI_ATTR void VKAPI_CALL			VulkanFree(void *pUserData, void *pMemory);

//----------------------------------------------------------------------------

class	CVulkanContext : public IApiContext
{
public:
	CVulkanContext();
	~CVulkanContext();

	virtual bool									InitRenderApiContext(bool debug, PAbstractWindowContext windowApi) override;
	virtual bool									WaitAllRenderFinished() override;
	virtual CGuid									BeginFrame() override;
	virtual bool									EndFrame(void *renderToWait) override;
	virtual RHI::SApiContext						*GetRenderApiContext() override;
	// Add the index of the swap-chain to recreate here
	virtual bool									RecreateSwapChain(const CUint2 &ctxSize) override;
	virtual TMemoryView<const RHI::PRenderTarget>	GetCurrentSwapChain() override;

	//
	// THE FOLLOWING FUNCTIONS SHOULD BE CALLED IN THIS ORDER:
	//

	enum	ESwapChainOpResult
	{
		SwapChainOp_CriticalError,
		SwapChainOp_DeviceLost,
		SwapChainOp_Failed,
		SwapChainOp_Success,
	};

	// Utils to create the actual vkInstance object
	static bool		CreateInstance(	RHI::SVulkanBasicContext &basicCtx,
									const TMemoryView<const char *> &extensionsToEnable,
									const TMemoryView<const char *> &layersToEnable,
									char const *appName,
									u32 appVersion,
									char const *engineName,
									u32 engineVersion);
	// Utils to find the appropriate physical device and check its extensions & layers
	static bool		PickPhysicalDevice(	RHI::SVulkanBasicContext &basicCtx,
										const TMemoryView<const char *> &layersToEnable,
										const TMemoryView<const char *> &extensionsToEnable,
										bool							&outDebugLayersEnabled,
										bool							isOffscreen);
	// Bind a callback to the validation layer to display warnings and error messages
	static bool		CreateValidationLayerCallback(	RHI::SVulkanBasicContext &basicCtx,
													bool performanceWarning,
													PFN_vkDebugUtilsMessengerCallbackEXT debugCallbackToUse,
													PFN_vkDestroyDebugUtilsMessengerEXT &vkDestroyDebugMessengerCallback,
													VkDebugUtilsMessengerEXT &debugMessenger);
	// Creates the window surface
#if		defined(PK_WINDOWS)
	static bool		CreateWindowSurface(RHI::SVulkanBasicContext &basicCtx, HINSTANCE moduleHandle, HWND windowHandle);
#elif	defined(PK_LINUX) && !defined(PK_GGP)
	static bool		CreateWindowSurface(RHI::SVulkanBasicContext &basicCtx, ureg display, ureg window);
#elif	defined(PK_MACOSX)
	static bool		CreateWindowSurface(RHI::SVulkanBasicContext &basicCtx, void *view);
#elif	defined(PK_GGP)
	static bool		CreateWindowSurface(RHI::SVulkanBasicContext &basicCtx);
#else
#	error not implemented
#endif

	// Creates the queue and logical device
	static bool		CreateQueueAndLogicalDevice(RHI::SVulkanBasicContext &basicCtx,
												const TMemoryView<const char *> &extensionsToEnable,
												const TMemoryView<const char *> &layersToEnable,
												bool							isOffscreen);
												
	static bool		LoadExtensionFunctionPointers(RHI::SVulkanBasicContext &basicCtx);
	// Creates the swap-chain
	static ESwapChainOpResult	CreateSwapChain(RHI::SVulkanBasicContext &basicCtx,
													const CUint2 &windowSize,
													bool srgb,
													bool vSync,
													u32 swapChainIdx,
													bool isOffscreen);

	static void		DestroyApiData(	RHI::SVulkanBasicContext &basicCtx);

	static bool		DestroySwapChain(RHI::SVulkanBasicContext &basicCtx, u32 idx);

	//
	// Utils:
	//

	// Utils to get the instance creation options
	static bool						GetInstanceExtensionsToEnable(TArray<const char *> &extensions, bool enableValidationLayer, bool enableDebugNames, bool isOffscreen = false);
	static bool						GetInstanceValidationLayersToEnable(TArray<const char *> &layers);
	static bool						ExtensionsSupportedByDevice(VkPhysicalDevice device, const TMemoryView<const char *> &extensionsToEnable);
	static bool						InstanceLayersSupportedByDevice(VkPhysicalDevice device, const TMemoryView<const char *> &enabledLayers);
	static bool						SwapChainSupportedByDevice(	const RHI::SVulkanBasicContext &basicCtx,
																VkPhysicalDevice device,
																VkSurfaceCapabilitiesKHR &capabilities,
																TArray<VkSurfaceFormatKHR> &surfaceFormats,
																TArray<VkPresentModeKHR> &presentModes,
																u32 swapChainIdx);

	// Find the index of a queue that supports the flags and IFN the presentation
	static int						GetQueueFamilyIdx(	const RHI::SVulkanBasicContext &basicCtx,
														TArray<VkQueueFamilyProperties> &queueFamilyProperties,
														bool supportPresent,
														u32 queueFamilyFlags = 0xFFFFFFFF,
														u32 shouldNotSupport = 0);
	// Utils to create the swap-chain
	static VkSurfaceFormatKHR		GetOptimalSurfaceFormat(const TMemoryView<VkSurfaceFormatKHR> &surfaceFormats, bool srgb);
	static VkPresentModeKHR			GetOptimalPresentationMode(const TMemoryView<VkPresentModeKHR> &presentModes, bool vSync);
	static VkExtent2D				GetOptimalSwapChainExtent(	const VkSurfaceCapabilitiesKHR &capabilities,
																u32 windowsWidth,
																u32 windowsHeight);
	// Create the image views for the swap-chain
	static bool						CreateSwapChainImageViews(	const RHI::SVulkanBasicContext &basicCtx,
																const TMemoryView<VkImage> &scImages,
																TSemiDynamicArray<VkImageView, RHI::kMaxSwapChainImages> &scImageViews,
																u32 swapChainIdx);

	// Check if all the specified layers in "toCheckNames" exist in the "layers" parameter (can be VkLayerProperties or VkExtensionProperties)
	static bool						CheckLayerNamesExist(	const TMemoryView<const char *> &toCheckNames,
															const TMemoryView<const VkLayerProperties> &layers);
	static bool						CheckExtensionNamesExist(	const TMemoryView<const char *> &toCheckNames,
																const TMemoryView<const VkExtensionProperties> &extensions);

	//
	// Variables:
	//

	// Validation layer callback
	VkDebugUtilsMessengerEXT						m_DebugUtilsMessenger;

	// Information about the available queues in the device
	TArray<VkQueueFamilyProperties>					m_QueueFamilyProperties;

	// The device supported swap-chains
	TSemiDynamicArray<VkImage, 3>					m_SwapChainImages;
	TSemiDynamicArray<VkImageView, 3>				m_SwapChainImageViews;

	u32												m_CurrentSwapChainImage;

	// Dynamically loaded Vulkan functions for validation layer
	PFN_vkDestroyDebugUtilsMessengerEXT				m_FnVkDestroyDebugUtilsMessengerEXT;
//	PFN_vkDebugReportMessageEXT						m_vkDebugReportMessage;

	//This struct is the actual data that is shared between the Api context and the Api manager
	RHI::SVulkanBasicContext						m_ApiData;

	bool											m_VSync;
	bool											m_IsOffscreen;
private:
	static ESwapChainOpResult	CreateOffscreenSwapChain(RHI::SVulkanBasicContext &basicCtx,
													const CUint2 &windowSize,
													bool srgb,
													u32 swapChainIdx);
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

#endif // (PK_BUILD_WITH_VULKAN_SUPPORT != 0)
