//> includes
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_initializers.h>
#include <vk_types.h>

#include "VkBootstrap.h"

#include <chrono>
#include <thread>

#ifndef NDEBUG
constexpr bool enableValidationLayers = true;
#else
constexpr bool enableValidationLayers = false;
#endif


VulkanEngine* loadedEngine = nullptr;

VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }
void VulkanEngine::init()
{
    // only one engine initialization is allowed with the application.
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    _init._window = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        _windowExtent.width,
        _windowExtent.height,
        window_flags);

	// initialize vulkan
	initVulkan();

	initSwapchain();

	initCommands();

	initSyncStructures();
	

    // everything went fine
    _isInitialized = true;
}

void VulkanEngine::cleanup()
{
    if (_isInitialized) {

        destroySwapchain();

        vkb::destroy_device(_init._vkbDevice);
        vkb::destroy_surface(_init._vkbInstance, _init._surface);

		// destroy_instance also destroys the debug messenger
		vkb::destroy_instance(_init._vkbInstance);


        SDL_DestroyWindow(_init._window);
    }

    // clear engine pointer
    loadedEngine = nullptr;
}

void VulkanEngine::draw()
{
    // nothing yet
}

void VulkanEngine::run()
{
    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit) {
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_QUIT)
                bQuit = true;

            if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    stop_rendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
                    stop_rendering = false;
                }
            }
        }

        // do not draw if we are minimized
        if (stop_rendering) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        draw();
    }
}

void VulkanEngine::initVulkan()
{
	/**********************************************************
	* VULKAN INITIALIZATION
    *
	* This section initializes the Vulkan instance using VkBootstrap, and sets up
	* the debug messenger.
	***********************************************************/

	// Use Vulkan Bootstrap to create the instance
	vkb::InstanceBuilder builder;

	// Create the Vulkan instance, with basic debug features
	auto inst_ret = builder.set_app_name("Vulkan Engine")
        .request_validation_layers(enableValidationLayers)
        .require_api_version(1, 3, 0)
        .use_default_debug_messenger()
		.build();

	vkb::Instance vkb_inst = inst_ret.value();
	// store the instance and debug messenger handles
    _init._vkbInstance = vkb_inst;
    //init._instance = vkb_inst.instance;
    _init._debugMessenger = vkb_inst.debug_messenger;

    /**********************************************************
	* VULKAN SURFACE CREATION AND DEVICE SELECTION
    *
	* This section creates the Vulkan surface using SDL and selects a suitable physical device (GPU).
    **********************************************************/

	// Create the Vulkan surface using SDL, throw error if it fails
    if (SDL_Vulkan_CreateSurface(_init._window, _init._vkbInstance.instance, &_init._surface) == SDL_FALSE)
    {
		fmt::print("Failed to create Vulkan surface.\n");
		abort();
    }


    /**********************************************************
	* PHYSICAL AND LOGICAL DEVICE SELECTION
    *
	* This section selects a suitable physical device (GPU) and creates a logical device.
    **********************************************************/

	// Vulkan 1.3 features we want to use
	VkPhysicalDeviceVulkan13Features features13 = {};
	features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features13.dynamicRendering = VK_TRUE;
	features13.synchronization2 = VK_TRUE;

	// Vulkan 1.2 features we want to use
	VkPhysicalDeviceVulkan12Features features12 = {};
	features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	features12.bufferDeviceAddress = VK_TRUE;
	features12.descriptorIndexing = VK_TRUE;

    // Select a GPU 
	vkb::PhysicalDeviceSelector selector{ vkb_inst };

    

   auto physicalDevices = selector
        .set_minimum_version(1, 3)
        .set_required_features_13(features13)
        .set_required_features_12(features12)
        .set_surface(_init._surface)
        .prefer_gpu_device_type()
        .select_devices();

   if (!physicalDevices) {
       fmt::print("Failed to select physical device: {}\n", physicalDevices.error().message());
       abort();
   }

   vkb::PhysicalDevice selectedDevice;

   // print the name of the selected device
   for (auto& pd : physicalDevices.value()) {
       fmt::print("Available GPU: {}\n", pd.name);
       if (pd.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
           selectedDevice = pd;
           break;
	   }
   }

   fmt::println("");

   if (selectedDevice == nullptr) {
	   fmt::print("No discrete GPU found, using first available GPU: {}\n", physicalDevices.value()[0].name);
	   selectedDevice = physicalDevices.value()[0];
   }

	fmt::print("Selected GPU: {}\n", selectedDevice.properties.deviceName);

	// Create the logical device
	vkb::DeviceBuilder deviceBuilder{ selectedDevice };

	vkb::Device vkbDevice = deviceBuilder.build().value();

	// Get the VkDevice handle
	//init._device = vkbDevice.device;
    _init._vkbDevice = vkbDevice;
    _init._physicalDevice = selectedDevice.physical_device;

}

void VulkanEngine::initSwapchain()
{
	createSwapchain(_windowExtent.width, _windowExtent.height);
}
void VulkanEngine::initCommands()
{
    //nothing yet
}
void VulkanEngine::initSyncStructures()
{
    //nothing yet
}

void VulkanEngine::createSwapchain(uint32_t width, uint32_t height)
{

    /**********************************************************
	* SWAPCHAIN CREATION
    *
	* This section creates the swapchain using VkBootstrap, specifying the desired format,
    **********************************************************/

	// Use Vulkan Bootstrap to create the swapchain
	vkb::SwapchainBuilder swapchainBuilder{ _init._physicalDevice, _init._vkbDevice.device, _init._surface };

    _init._swapchainImageFormat = VK_FORMAT_B8G8R8A8_SRGB;

    auto vkbSwapchain = swapchainBuilder 
		.set_desired_format(VkSurfaceFormatKHR{ .format = _init._swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR }) // 8-bit SRGB
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR) // vsync
        .set_desired_extent(width, height) // use window size
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT) // we want to blit to the swapchain
        .build()
        .value();

    _init._vkbSwapchain = vkbSwapchain;
    _init._swapchainExtent = vkbSwapchain.extent;

	// store swapchain and its related images
	//init._swapchain = vkbSwapchain.swapchain;
    _init._swapchainImages = vkbSwapchain.get_images().value();
    _init._swapchainImageViews = vkbSwapchain.get_image_views().value();


}

void VulkanEngine::destroySwapchain()
{
    _init._vkbSwapchain.destroy_image_views(_init._swapchainImageViews);
    vkb::destroy_swapchain(_init._vkbSwapchain);


	
}

// end of vk_engine.cpp